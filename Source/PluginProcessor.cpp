#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "tsf.h"

TMUProcessor::TMUProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      juce::Thread ("TMU loader"),
      params (*this, nullptr, "TMU", createLayout())
{
    volumeParam = params.getRawParameterValue ("volume");
    transposeParam = params.getRawParameterValue ("transpose");
    modParam = params.getRawParameterValue ("mod");
    filterParam = params.getRawParameterValue ("filter");
    for (auto& m : ccMap) m = -1;
    for (auto& v : pendingCC) v = -1.0f;
    startTimerHz (60);
    for (int i = 0; i < 128; ++i) noteMap[(size_t) i] = i;
    startThread (juce::Thread::Priority::normal);
    library->addChangeListener (this);
    if (! library->isScanning())
        changeListenerCallback (nullptr);   // la biblioteca ya estaba cargada por otra instancia
}

TMUProcessor::~TMUProcessor()
{
    stopTimer();
    library->removeChangeListener (this);
    signalThreadShouldExit();
    notify();
    stopThread (5000);
    if (synth != nullptr) tsf_close (synth);
}

juce::AudioProcessorValueTreeState::ParameterLayout TMUProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "volume", 1 }, "Volume",
                    juce::NormalisableRange<float> (-36.0f, 12.0f, 0.1f), 0.0f,
                    juce::AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "transpose", 1 }, "Transpose", -24, 24, 0,
                    juce::AudioParameterIntAttributes().withLabel ("st")));
    // Mod: vibrato como la rueda de modulacion (la rueda CC1 del teclado tambien lo mueve)
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "mod", 1 }, "Mod",
                    juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f,
                    juce::AudioParameterFloatAttributes().withLabel ("%")
                        .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })));
    // Filtro: centro sin filtro, izquierda corta agudos, derecha corta graves (igual que rmx_)
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "filter", 1 }, "Filter",
                    juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f,
                    juce::AudioParameterFloatAttributes()
                        .withStringFromValueFunction ([] (float v, int)
                        {
                            if (std::abs (v) < 0.01f) return juce::String ("OFF");
                            return v < 0 ? "HIGH CUT " + juce::String (juce::roundToInt (-v * 100)) + "%"
                                         : "LOW CUT " + juce::String (juce::roundToInt (v * 100)) + "%";
                        })));
    return layout;
}

bool TMUProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void TMUProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    renderBuffer.assign ((size_t) juce::jmax (512, samplesPerBlock) * 2, 0.0f);
    gainSmoothed.reset (sampleRate, 0.03);
    gainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (volumeParam->load(), -36.0f));

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (512, samplesPerBlock), 2 };
    lowCut.prepare (spec);  lowCut.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    highCut.prepare (spec); highCut.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    lowCut.setResonance (0.75f); highCut.setResonance (0.75f);
    filterSmoothed.reset (sampleRate, 0.05);
    filterSmoothed.setCurrentAndTargetValue (filterParam->load());
    modSmoothed.reset (sampleRate, 0.05);
    modSmoothed.setCurrentAndTargetValue (0.0f);
    lfoPhase = 0.0;

    const juce::SpinLock::ScopedLockType sl (synthLock);
    if (synth != nullptr)
    {
        tsf_set_output (synth, TSF_STEREO_INTERLEAVED, (int) sampleRate, 0.0f);
        tsf_reset (synth);
    }
}

void TMUProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    // MIDI learn y controladores asignados (se aplican a los parametros desde el hilo de la interfaz)
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (! m.isController()) continue;
        const int cc = m.getControllerNumber();
        if (cc == 1) modWheel = (float) m.getControllerValue() / 127.0f;

        const int learn = learningParam.load();
        if (learn >= 0 && cc != 64 && cc < 120)
        {
            for (auto& slot : ccMap) if (slot.load() == learn) slot = -1;   // un solo CC por perilla
            ccMap[(size_t) cc] = learn;
            learningParam = -1;
        }
        const int target = ccMap[(size_t) cc].load();
        if (target >= 0 && target < numLearnable)
            pendingCC[(size_t) target] = (float) m.getControllerValue() / 127.0f;
    }

    const float vol = volumeParam->load();
    gainSmoothed.setTargetValue (vol <= -35.9f ? 0.0f : juce::Decibels::decibelsToGain (vol));
    const int transpose = (int) transposeParam->load();

    const juce::SpinLock::ScopedTryLockType sl (synthLock);
    if (! sl.isLocked() || synth == nullptr)
        return;

    if (transpose != lastTranspose)
    {
        tsf_channel_note_off_all (synth, 0);
        lastTranspose = transpose;
    }

    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    modSmoothed.setTargetValue (juce::jmax (modParam->load(), modWheel.load()));
    const double lfoInc = juce::MathConstants<double>::twoPi * 5.5 / currentSampleRate.load();

    auto render = [&] (int start, int count)
    {
        while (count > 0)
        {
            // vibrato: se actualiza cada 32 muestras (hasta medio semitono, como la rueda de mod de un GM)
            const float depth = modSmoothed.getNextValue();
            if (count > 1) modSmoothed.skip (juce::jmin (count, 32) - 1);
            const float tune = depth > 0.0005f ? 0.5f * depth * (float) std::sin (lfoPhase) : 0.0f;
            tsf_channel_set_tuning (synth, 0, tune);
            lfoPhase = std::fmod (lfoPhase + lfoInc * juce::jmin (count, 32), juce::MathConstants<double>::twoPi);

            const int chunk = juce::jmin (count, 32, (int) renderBuffer.size() / 2);
            tsf_render_float (synth, renderBuffer.data(), chunk, 0);
            for (int i = 0; i < chunk; ++i)
            {
                const float g = gainSmoothed.getNextValue();
                const float l = renderBuffer[(size_t) i * 2] * g, r = renderBuffer[(size_t) i * 2 + 1] * g;
                if (right != nullptr) { left[start + i] = l; right[start + i] = r; }
                else                    left[start + i] = 0.5f * (l + r);
            }
            start += chunk;
            count -= chunk;
        }
    };

    int pos = 0;
    for (const auto meta : midi)
    {
        const int evPos = juce::jlimit (0, numSamples, meta.samplePosition);
        if (evPos > pos) { render (pos, evPos - pos); pos = evPos; }

        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            const int key = juce::jlimit (0, 127, m.getNoteNumber() + transpose);
            noteMap[(size_t) m.getNoteNumber()] = key;
            tsf_channel_note_on (synth, 0, key, m.getFloatVelocity());
        }
        else if (m.isNoteOff())
        {
            tsf_channel_note_off (synth, 0, noteMap[(size_t) m.getNoteNumber()]);
        }
        else if (m.isPitchWheel())
        {
            tsf_channel_set_pitchwheel (synth, 0, m.getPitchWheelValue());
        }
        else if (m.isAllNotesOff())
        {
            tsf_channel_note_off_all (synth, 0);
        }
        else if (m.isAllSoundOff())
        {
            tsf_channel_sounds_off_all (synth, 0);
        }
        else if (m.isController())
        {
            const int cc = m.getControllerNumber();
            if (cc != 1 && cc != 6 && cc != 38 && cc != 100 && cc != 101 && ccMap[(size_t) cc].load() < 0)
                tsf_channel_midi_control (synth, 0, cc, m.getControllerValue());
        }
    }
    if (pos < numSamples) render (pos, numSamples - pos);

    // ---- filtro de salida
    filterSmoothed.setTargetValue (filterParam->load());
    const float fv = filterSmoothed.skip (numSamples);
    if (std::abs (fv) > 0.005f)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        if (fv < 0)
        {
            highCut.setCutoffFrequency (20000.0f * std::pow (200.0f / 20000.0f, -fv));
            highCut.process (ctx);
        }
        else
        {
            lowCut.setCutoffFrequency (20.0f * std::pow (2000.0f / 20.0f, fv));
            lowCut.process (ctx);
        }
    }
}

//==============================================================================
const char* TMUProcessor::learnableIds[TMUProcessor::numLearnable] = { "volume", "mod", "filter", "transpose" };

void TMUProcessor::timerCallback()
{
    // aplicar los controladores MIDI asignados a los parametros (avisando a FL)
    for (int i = 0; i < numLearnable; ++i)
    {
        const float v = pendingCC[(size_t) i].exchange (-1.0f);
        if (v >= 0.0f)
            if (auto* prm = params.getParameter (learnableIds[i]))
                prm->setValueNotifyingHost (v);
    }
}

void TMUProcessor::startLearning (const juce::String& paramId)
{
    for (int i = 0; i < numLearnable; ++i)
        if (paramId == learnableIds[i]) learningParam = i;
}

void TMUProcessor::clearLearning (const juce::String& paramId)
{
    for (int i = 0; i < numLearnable; ++i)
        if (paramId == learnableIds[i])
        {
            for (auto& slot : ccMap) if (slot.load() == i) slot = -1;
            if (learningParam.load() == i) learningParam = -1;
        }
}

int TMUProcessor::getAssignedCC (const juce::String& paramId) const
{
    for (int i = 0; i < numLearnable; ++i)
        if (paramId == learnableIds[i])
            for (int cc = 0; cc < 128; ++cc)
                if (ccMap[(size_t) cc].load() == i) return cc;
    return -1;
}

juce::String TMUProcessor::getLearningParam() const
{
    const int l = learningParam.load();
    return l >= 0 ? juce::String (learnableIds[l]) : juce::String();
}

//==============================================================================
void TMUProcessor::loadPreset (const PresetEntry& e)
{
    {
        const juce::ScopedLock sl (infoLock);
        pending = e;
        hasPending = true;
        loaded.loading = true;
    }
    notify();
}

TMUProcessor::LoadedInfo TMUProcessor::getLoadedInfo() const
{
    const juce::ScopedLock sl (infoLock);
    return loaded;
}

void TMUProcessor::run()
{
    while (! threadShouldExit())
    {
        PresetEntry job;
        {
            const juce::ScopedLock sl (infoLock);
            if (hasPending) { job = pending; hasPending = false; }
            else job.phdrIndex = -1;
        }

        if (job.phdrIndex < 0) { wait (-1); continue; }

        std::vector<uint8_t> sf2;
        std::string err;
        tsf* fresh = nullptr;

        if (! juce::File (job.file).existsAsFile())
            err = "No se encontro el archivo";
        else if (tmu::extractPreset (job.file.toStdString(), job.phdrIndex, sf2, err))
        {
            fresh = tsf_load_memory (sf2.data(), (int) sf2.size());
            if (fresh == nullptr) err = "No se pudo leer el sonido";
        }

        if (fresh != nullptr)
        {
            tsf_set_output (fresh, TSF_STEREO_INTERLEAVED, (int) currentSampleRate.load(), 0.0f);
            tsf_set_max_voices (fresh, 96);
            tsf_channel_set_presetindex (fresh, 0, 0);
            tsf_channel_set_pitchrange (fresh, 0, 2.0f);

            tsf* old = nullptr;
            {
                const juce::SpinLock::ScopedLockType sl (synthLock);
                old = synth;
                synth = fresh;
                tsf_set_output (synth, TSF_STEREO_INTERLEAVED, (int) currentSampleRate.load(), 0.0f);
            }
            if (old != nullptr) tsf_close (old);
        }

        const juce::ScopedLock sl (infoLock);
        if (fresh != nullptr)
        {
            loaded.file = job.file; loaded.source = job.source; loaded.name = job.name;
            loaded.phdrIndex = job.phdrIndex; loaded.category = job.category;
            loaded.error = {};
        }
        else
        {
            loaded.error = juce::String (err) + ": " + job.name;
        }
        loaded.loading = hasPending;
    }
}

void TMUProcessor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Instancia nueva sin sonido elegido: cargar el primer piano de la biblioteca.
    {
        const juce::ScopedLock sl (infoLock);
        if (loaded.phdrIndex >= 0 || hasPending || loaded.loading) return;
    }
    auto list = library->getEntries();
    const PresetEntry* pick = nullptr;
    for (auto& e : *list)
        if (e.category == tmu::Pianos && e.name.containsIgnoreCase ("grand")) { pick = &e; break; }
    if (pick == nullptr && ! list->empty()) pick = &list->front();
    if (pick != nullptr) loadPreset (*pick);
}

//==============================================================================
void TMUProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = params.copyState();
    juce::ValueTree root ("TMUState");
    root.addChild (state, -1, nullptr);

    const auto info = getLoadedInfo();
    root.setProperty ("file", info.file, nullptr);
    root.setProperty ("source", info.source, nullptr);
    root.setProperty ("name", info.name, nullptr);
    root.setProperty ("phdr", info.phdrIndex, nullptr);
    root.setProperty ("category", info.category, nullptr);
    root.setProperty ("uiView", uiView, nullptr);
    root.setProperty ("uiGroup", uiGroup, nullptr);
    root.setProperty ("uiSearch", uiSearch, nullptr);
    juce::StringArray map;
    for (int cc = 0; cc < 128; ++cc)
        if (ccMap[(size_t) cc].load() >= 0) map.add (juce::String (cc) + ":" + learnableIds[ccMap[(size_t) cc].load()]);
    root.setProperty ("midiMap", map.joinIntoString (","), nullptr);

    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void TMUProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("TMUState")) return;

    auto p = root.getChildWithName (params.state.getType());
    if (p.isValid()) params.replaceState (p);

    uiView = root.getProperty ("uiView", "type").toString();
    uiGroup = root.getProperty ("uiGroup").toString();
    uiSearch = root.getProperty ("uiSearch").toString();
    for (auto& m : ccMap) m = -1;
    for (auto& item : juce::StringArray::fromTokens (root.getProperty ("midiMap").toString(), ",", ""))
    {
        const int cc = item.upToFirstOccurrenceOf (":", false, false).getIntValue();
        const auto id = item.fromFirstOccurrenceOf (":", false, false);
        for (int i = 0; i < numLearnable; ++i)
            if (id == learnableIds[i] && cc >= 0 && cc < 128) ccMap[(size_t) cc] = i;
    }

    PresetEntry e;
    e.file = root.getProperty ("file").toString();
    e.source = root.getProperty ("source").toString();
    e.name = root.getProperty ("name").toString();
    e.phdrIndex = (int) root.getProperty ("phdr", -1);
    e.category = (int) root.getProperty ("category", (int) tmu::Other);

    if (e.phdrIndex >= 0 && e.file.isNotEmpty())
    {
        // Si el archivo se movio (por ej. se reinstalo en otra carpeta), buscarlo por nombre.
        if (! juce::File (e.file).existsAsFile())
        {
            const auto fileName = juce::File (e.file).getFileName();
            for (auto& dir : PresetLibrary::getSearchFolders())
                for (const auto& it : juce::RangedDirectoryIterator (dir, true, fileName, juce::File::findFiles))
                    { e.file = it.getFile().getFullPathName(); break; }
        }
        loadPreset (e);
    }
}

juce::AudioProcessorEditor* TMUProcessor::createEditor()
{
    return new TMUEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TMUProcessor();
}
