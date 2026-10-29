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
    rateParam = params.getRawParameterValue ("rate");
    swingParam = params.getRawParameterValue ("swing");
    humanParam = params.getRawParameterValue ("humanize");
    velCurveParam = params.getRawParameterValue ("velcurve");
    scheduled.reserve (1024);
    due.reserve (1024);
    for (auto& m : ccMap) m = -1;
    for (auto& v : pendingCC) v = -1.0f;
    startTimerHz (30);
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
    // LFO de la modulacion (vibrato + filtro que se mueve)
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "rate", 1 }, "Rate",
                    juce::NormalisableRange<float> (0.1f, 12.0f, 0.01f, 0.45f), 5.5f,
                    juce::AudioParameterFloatAttributes().withLabel ("Hz")));
    // Swing: 0 = recto (50%), 1 = 75% (atrasa los contratiempos de 16avos)
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "swing", 1 }, "Swing",
                    juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f,
                    juce::AudioParameterFloatAttributes()
                        .withStringFromValueFunction ([] (float v, int) { return juce::String (50 + juce::roundToInt (v * 25.0f)) + " %"; })));
    // Humanize: varia velocity y un poco el tiempo de cada nota
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "humanize", 1 }, "Humanize",
                    juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f,
                    juce::AudioParameterFloatAttributes()
                        .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "velcurve", 1 }, "Velocity",
                    juce::StringArray { "Suave", "Normal", "Dura", "Fija" }, 1));
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
    modFilter.prepare (spec);
    modFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    modFilter.setResonance (1.1f);   // un poco de resonancia, como el filtro de un rompler
    scheduled.clear();
    notesHeld = 0;

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
    const double sr = currentSampleRate.load();
    const juce::int64 blockStart = sampleCounter;
    sampleCounter += numSamples;

    // ---- tempo y posicion de FL (para el swing)
    double ppqAtStart = -1.0, bpm = 120.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto p = pos->getPpqPosition()) ppqAtStart = *p;
            if (auto b = pos->getBpm()) bpm = juce::jmax (20.0, *b);
        }
    const double samplesPerBeat = sr * 60.0 / bpm;
    const float swing = swingParam->load(), human = humanParam->load();
    const int curve = (int) velCurveParam->load();

    // ---- pasar el MIDI entrante a la cola (con swing / humanize / curva de velocity)
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const juce::int64 t = blockStart + juce::jlimit (0, numSamples, meta.samplePosition);
        const auto* raw = m.getRawData();
        if (scheduled.size() >= 1000) break;

        if (m.isNoteOn())
        {
            float v = m.getFloatVelocity();
            switch (curve)
            {
                case 0:  v = std::pow (v, 0.6f); break;   // suave: toca fuerte aunque toques despacio
                case 2:  v = std::pow (v, 1.7f); break;   // dura: hay que pegarle para que suene fuerte
                case 3:  v = 100.0f / 127.0f; break;      // fija
                default: break;
            }
            juce::int64 delay = 0;
            if (human > 0.0f)
            {
                v *= 1.0f + (rng.nextFloat() - 0.5f) * 0.5f * human;   // hasta +-25 %
                if (playing) delay += (juce::int64) (rng.nextFloat() * human * 0.018 * sr);   // hasta 18 ms tarde
            }
            if (swing > 0.0f && playing && ppqAtStart >= 0.0)
            {
                // nota sobre un contratiempo de 16avo -> se atrasa (como el swing de una MPC)
                const double ppq = ppqAtStart + (double) (t - blockStart) / samplesPerBeat;
                const double six = ppq * 4.0;
                const double idx = std::floor (six + 0.12);
                if (((long long) idx & 1) == 1 && std::abs (six - idx) < 0.12)
                    delay += (juce::int64) (swing * 0.5 * samplesPerBeat * 0.25);
            }
            noteDelay[(size_t) m.getNoteNumber()] = delay;
            scheduled.push_back ({ t + delay, raw[0], raw[1], raw[2], juce::jlimit (0.01f, 1.0f, v) });
        }
        else if (m.isNoteOff())
            scheduled.push_back ({ t + noteDelay[(size_t) m.getNoteNumber()], raw[0], raw[1], 0, 0.0f });
        else if (m.getRawDataSize() <= 3)
            scheduled.push_back ({ t, raw[0], m.getRawDataSize() > 1 ? raw[1] : (juce::uint8) 0, m.getRawDataSize() > 2 ? raw[2] : (juce::uint8) 0, 0.0f });
    }

    // eventos que tocan en este bloque
    due.clear();
    for (auto& e : scheduled) if (e.time < blockStart + numSamples) due.push_back (e);
    scheduled.erase (std::remove_if (scheduled.begin(), scheduled.end(), [&] (const Event& e) { return e.time < blockStart + numSamples; }), scheduled.end());
    std::stable_sort (due.begin(), due.end(), [] (const Event& a, const Event& b) { return a.time < b.time; });

    const juce::SpinLock::ScopedTryLockType sl (synthLock);
    if (! sl.isLocked() || synth == nullptr)
        return;

    if (transpose != lastTranspose)
    {
        tsf_channel_note_off_all (synth, 0);
        lastTranspose = transpose;
        notesHeld = 0;
    }

    // Sin notas sonando ni eventos: no hay nada que calcular (asi 10 TMU quietos no gastan CPU)
    filterSmoothed.setTargetValue (filterParam->load());
    modSmoothed.setTargetValue (juce::jmax (modParam->load(), modWheel.load()));
    if (due.empty() && tsf_active_voice_count (synth) == 0 && ! filterSmoothed.isSmoothing())
    {
        gainSmoothed.skip (numSamples);
        modSmoothed.skip (numSamples);
        if (! wasSilent) { lowCut.reset(); highCut.reset(); modFilter.reset(); wasSilent = true; }
        return;
    }
    wasSilent = false;

    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    const double lfoInc = juce::MathConstants<double>::twoPi * (double) rateParam->load() / sr;
    const double delaySamples = 0.25 * sr, fadeSamples = 0.6 * sr;   // vibrato estilo Roland: espera y despues crece

    auto render = [&] (int start, int count)
    {
        while (count > 0)
        {
            // modulacion: se actualiza cada 32 muestras. Sin modulacion se calcula el bloque de una vez.
            const bool modOn = modSmoothed.isSmoothing() || modSmoothed.getTargetValue() > 0.0005f;
            const int span = modOn ? juce::jmin (count, 32) : count;
            const float depth = modSmoothed.getNextValue();
            if (span > 1) modSmoothed.skip (span - 1);

            float fade = 0.0f, lfo = 0.0f;
            if (modOn)
            {
                const double since = (double) (blockStart + start - modTriggerTime);
                fade = (float) juce::jlimit (0.0, 1.0, (since - delaySamples) / fadeSamples);
                lfo = (float) std::sin (lfoPhase);
                lfoPhase = std::fmod (lfoPhase + lfoInc * span, juce::MathConstants<double>::twoPi);
            }
            const float amount = depth * fade;
            tsf_channel_set_tuning (synth, 0, amount > 0.0005f ? 0.5f * amount * lfo : 0.0f);

            const int chunk = juce::jmin (span, (int) renderBuffer.size() / 2);
            tsf_render_float (synth, renderBuffer.data(), chunk, 0);
            for (int i = 0; i < chunk; ++i)
            {
                const float g = gainSmoothed.getNextValue();
                const float l = renderBuffer[(size_t) i * 2] * g, r = renderBuffer[(size_t) i * 2 + 1] * g;
                if (right != nullptr) { left[start + i] = l; right[start + i] = r; }
                else                    left[start + i] = 0.5f * (l + r);
            }

            // filtro que se mueve con el LFO (abierto cuando el LFO esta abajo, cerrado arriba)
            if (amount > 0.002f)
            {
                modFilter.setCutoffFrequency (juce::jlimit (300.0f, 18000.0f, 18000.0f * std::pow (2.0f, -amount * (1.0f + lfo) * 2.6f)));
                juce::dsp::AudioBlock<float> blk (buffer);
                auto sub = blk.getSubBlock ((size_t) start, (size_t) chunk);
                juce::dsp::ProcessContextReplacing<float> ctx (sub);
                modFilter.process (ctx);
            }
            start += chunk;
            count -= chunk;
        }
    };

    int pos = 0;
    for (const auto& e : due)
    {
        const int evPos = (int) juce::jlimit<juce::int64> (0, numSamples, e.time - blockStart);
        if (evPos > pos) { render (pos, evPos - pos); pos = evPos; }

        const int type = e.status & 0xf0;
        if (type == 0x90 && e.d2 > 0)
        {
            const int key = juce::jlimit (0, 127, (int) e.d1 + transpose);
            noteMap[(size_t) e.d1] = key;
            if (notesHeld <= 0) modTriggerTime = e.time;   // frase nueva: el vibrato vuelve a esperar
            ++notesHeld;
            tsf_channel_note_on (synth, 0, key, e.vel);
        }
        else if (type == 0x80 || (type == 0x90 && e.d2 == 0))
        {
            notesHeld = juce::jmax (0, notesHeld - 1);
            tsf_channel_note_off (synth, 0, noteMap[(size_t) e.d1]);
        }
        else if (type == 0xE0)
        {
            tsf_channel_set_pitchwheel (synth, 0, (int) e.d1 | ((int) e.d2 << 7));
        }
        else if (type == 0xB0)
        {
            const int cc = e.d1;
            if (cc == 123 || cc == 120) { tsf_channel_note_off_all (synth, 0); notesHeld = 0; }
            else if (cc != 1 && cc != 6 && cc != 38 && cc != 100 && cc != 101 && ccMap[(size_t) cc].load() < 0)
                tsf_channel_midi_control (synth, 0, cc, e.d2);
        }
    }
    if (pos < numSamples) render (pos, numSamples - pos);

    // ---- filtro de salida (perilla FILTRO)
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
const char* TMUProcessor::learnableIds[TMUProcessor::numLearnable] = { "volume", "mod", "filter", "transpose", "rate", "swing", "humanize" };

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
