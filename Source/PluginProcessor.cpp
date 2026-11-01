#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "tsf.h"

TMUProcessor::TMUProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      juce::Thread ("TMU loader"),
      params (*this, nullptr, "TMU", createLayout())
{
    for (int k = 0; k < numLayers; ++k)
    {
        auto& L = layers[(size_t) k];
        L.onParam        = params.getRawParameterValue (layerOnId (k));
        L.volParam       = params.getRawParameterValue (layerParamId ("volume", k));
        L.transposeParam = params.getRawParameterValue (layerParamId ("transpose", k));
        L.modParam       = params.getRawParameterValue (layerParamId ("mod", k));
        L.filterParam    = params.getRawParameterValue (layerParamId ("filter", k));
        L.rateParam      = params.getRawParameterValue (layerParamId ("rate", k));
        L.swingParam     = params.getRawParameterValue (layerParamId ("swing", k));
        L.humanParam     = params.getRawParameterValue (layerParamId ("humanize", k));
        L.velCurveParam  = params.getRawParameterValue (layerParamId ("velcurve", k));
        L.scheduled.reserve (1024);
        L.due.reserve (1024);
        for (int i = 0; i < 128; ++i) L.noteMap[(size_t) i] = i;
    }
    for (auto& m : ccMap) m = -1;
    for (auto& v : pendingCC) v = -1.0f;
    startTimerHz (30);
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
    for (auto& L : layers)
        if (L.synth != nullptr) { tsf_close (L.synth); L.synth = nullptr; }
}

juce::AudioProcessorValueTreeState::ParameterLayout TMUProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Los 8 controles de cada capa. La capa 1 conserva los ids de siempre (volume, mod, ...) para que los
    // proyectos y la automatizacion de antes sigan andando; las otras son "l2_volume", "l2_mod", ...
    for (int k = 0; k < numLayers; ++k)
    {
        const int ver = k == 0 ? 1 : 2;   // version 2 = parametros agregados en la primera actualizacion
        const juce::String pre = k == 0 ? juce::String() : "Layer " + juce::String (k + 1) + " ";
        auto id = [&] (const char* base) { return juce::ParameterID { layerParamId (base, k), ver }; };

        layout.add (std::make_unique<juce::AudioParameterFloat> (id ("volume"), pre + "Volume",
                        juce::NormalisableRange<float> (-36.0f, 12.0f, 0.1f), 0.0f,
                        juce::AudioParameterFloatAttributes().withLabel ("dB")));
        layout.add (std::make_unique<juce::AudioParameterInt> (id ("transpose"), pre + "Transpose", -24, 24, 0,
                        juce::AudioParameterIntAttributes().withLabel ("st")));
        // Mod: vibrato como la rueda de modulacion (la rueda CC1 del teclado tambien lo mueve)
        layout.add (std::make_unique<juce::AudioParameterFloat> (id ("mod"), pre + "Mod",
                        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f,
                        juce::AudioParameterFloatAttributes().withLabel ("%")
                            .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })));
        // Filtro: centro sin filtro, izquierda corta agudos, derecha corta graves (igual que rmx_)
        layout.add (std::make_unique<juce::AudioParameterFloat> (id ("filter"), pre + "Filter",
                        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f,
                        juce::AudioParameterFloatAttributes()
                            .withStringFromValueFunction ([] (float v, int)
                            {
                                if (std::abs (v) < 0.01f) return juce::String ("OFF");
                                return v < 0 ? "HIGH CUT " + juce::String (juce::roundToInt (-v * 100)) + "%"
                                             : "LOW CUT " + juce::String (juce::roundToInt (v * 100)) + "%";
                            })));
        // LFO de la modulacion (vibrato + filtro que se mueve)
        layout.add (std::make_unique<juce::AudioParameterFloat> (id ("rate"), pre + "Rate",
                        juce::NormalisableRange<float> (0.1f, 12.0f, 0.01f, 0.45f), 5.5f,
                        juce::AudioParameterFloatAttributes().withLabel ("Hz")));
        // Swing: 0 = recto (50%), 1 = 75% (atrasa los contratiempos de 16avos)
        layout.add (std::make_unique<juce::AudioParameterFloat> (id ("swing"), pre + "Swing",
                        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f,
                        juce::AudioParameterFloatAttributes()
                            .withStringFromValueFunction ([] (float v, int) { return juce::String (50 + juce::roundToInt (v * 25.0f)) + " %"; })));
        // Humanize: varia velocity y un poco el tiempo de cada nota
        layout.add (std::make_unique<juce::AudioParameterFloat> (id ("humanize"), pre + "Humanize",
                        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f,
                        juce::AudioParameterFloatAttributes()
                            .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })));
        layout.add (std::make_unique<juce::AudioParameterChoice> (id ("velcurve"), pre + "Velocity",
                        juce::StringArray { "Suave", "Normal", "Dura", "Fija" }, 1));
    }
    // ON de cada capa
    for (int k = 0; k < numLayers; ++k)
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { layerOnId (k), 2 },
                        "Layer " + juce::String (k + 1) + " On", true));
    return layout;
}

bool TMUProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

// ganancia que tiene que tener una capa: 0 si esta apagada o con el volumen al piso
static float layerTarget (const std::atomic<float>* on, const std::atomic<float>* vol)
{
    if (on == nullptr || vol == nullptr || on->load() < 0.5f) return 0.0f;
    const float db = vol->load();
    return db <= -35.9f ? 0.0f : juce::Decibels::decibelsToGain (db);
}

void TMUProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    const int maxBlock = juce::jmax (512, samplesPerBlock);
    renderBuffer.assign ((size_t) maxBlock * 2, 0.0f);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlock, 2 };
    for (auto& L : layers)
    {
        L.gain.reset (sampleRate, 0.03);
        L.gain.setCurrentAndTargetValue (layerTarget (L.onParam, L.volParam));
        L.lowCut.prepare (spec);  L.lowCut.setType (juce::dsp::StateVariableTPTFilterType::highpass);
        L.highCut.prepare (spec); L.highCut.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        L.lowCut.setResonance (0.75f); L.highCut.setResonance (0.75f);
        L.filterSmoothed.reset (sampleRate, 0.05);
        L.filterSmoothed.setCurrentAndTargetValue (L.filterParam->load());
        L.modSmoothed.reset (sampleRate, 0.05);
        L.modSmoothed.setCurrentAndTargetValue (0.0f);
        L.lfoPhase = 0.0;
        L.modFilter.prepare (spec);
        L.modFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        L.modFilter.setResonance (1.1f);   // un poco de resonancia, como el filtro de un rompler
        L.scheduled.clear();
        L.notesHeld = 0;
        L.audio.setSize (2, maxBlock, false, true, false);
    }

    const juce::SpinLock::ScopedLockType sl (synthLock);
    for (auto& L : layers)
        if (L.synth != nullptr)
        {
            tsf_set_output (L.synth, TSF_STEREO_INTERLEAVED, (int) sampleRate, 0.0f);
            tsf_reset (L.synth);
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

    // ---- pasar el MIDI entrante a la cola de CADA capa (con el swing / humanize / curva de velocity de esa capa)
    for (auto& L : layers)
    {
        const float swing = L.swingParam->load(), human = L.humanParam->load();
        const int curve = (int) L.velCurveParam->load();
        const bool layerOn = L.onParam->load() >= 0.5f;

        for (const auto meta : midi)
        {
            const auto m = meta.getMessage();
            const juce::int64 t = blockStart + juce::jlimit (0, numSamples, meta.samplePosition);
            const auto* raw = m.getRawData();
            if (L.scheduled.size() >= 1000) break;

            if (m.isNoteOn())
            {
                if (! layerOn) continue;   // una capa apagada no recibe notas nuevas (si los note off, para que no queden colgadas)
                float v = m.getFloatVelocity();
                switch (curve)
                {
                    case 0:  v = std::pow (v, 0.6f); break;   // suave: toca fuerte aunque toques despacio
                    case 2:  v = std::pow (v, 1.4f); break;   // dura: hay que pegarle para que suene fuerte
                    case 3:  v = 100.0f / 127.0f; break;      // fija
                    default: break;
                }
                juce::int64 delay = 0;
                if (human > 0.0f)
                {
                    v += (rng.nextFloat() * 2.0f - 1.0f) * 0.22f * human;   // hasta +-28 de velocity (de 127)
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
                L.noteDelay[(size_t) m.getNoteNumber()] = delay;
                L.scheduled.push_back ({ t + delay, raw[0], raw[1], raw[2], juce::jlimit (0.01f, 1.0f, v) });
            }
            else if (m.isNoteOff())
                L.scheduled.push_back ({ t + L.noteDelay[(size_t) m.getNoteNumber()], raw[0], raw[1], 0, 0.0f });
            else if (m.getRawDataSize() <= 3)
                L.scheduled.push_back ({ t, raw[0], m.getRawDataSize() > 1 ? raw[1] : (juce::uint8) 0, m.getRawDataSize() > 2 ? raw[2] : (juce::uint8) 0, 0.0f });
        }

        // eventos que tocan en este bloque
        L.due.clear();
        for (auto& e : L.scheduled) if (e.time < blockStart + numSamples) L.due.push_back (e);
        L.scheduled.erase (std::remove_if (L.scheduled.begin(), L.scheduled.end(),
                                           [&] (const Event& e) { return e.time < blockStart + numSamples; }), L.scheduled.end());
        std::stable_sort (L.due.begin(), L.due.end(), [] (const Event& a, const Event& b) { return a.time < b.time; });
    }

    const juce::SpinLock::ScopedTryLockType sl (synthLock);
    if (! sl.isLocked())
        return;

    const int numCh = buffer.getNumChannels();
    if (numCh < 1 || renderBuffer.size() < 2)
        return;   // prepareToPlay todavia no corrio

    // cada capa se calcula aparte (con sus filtros y su volumen) y despues se suman
    for (int k = 0; k < numLayers; ++k)
    {
        auto& L = layers[(size_t) k];
        if (L.synth == nullptr) continue;
        if (L.audio.getNumChannels() != numCh || L.audio.getNumSamples() < numSamples)
            L.audio.setSize (numCh, juce::jmax (512, numSamples), false, false, false);
        if (processLayer (k, numSamples, blockStart, sr))
            for (int ch = 0; ch < numCh; ++ch)
                buffer.addFrom (ch, 0, L.audio, ch, 0, numSamples);
    }

    if (! license->isActive())   // TMU sin activar: no suena hasta poner el serial
        buffer.clear();
}

bool TMUProcessor::processLayer (int k, int numSamples, juce::int64 blockStart, double sr)
{
    auto& L = layers[(size_t) k];
    const int transpose = (int) L.transposeParam->load();
    if (transpose != L.lastTranspose)
    {
        tsf_channel_note_off_all (L.synth, 0);
        L.lastTranspose = transpose;
        L.notesHeld = 0;
    }

    // Sin notas sonando ni eventos: no hay nada que calcular (asi 10 TMU quietos no gastan CPU)
    L.gain.setTargetValue (layerTarget (L.onParam, L.volParam));
    L.filterSmoothed.setTargetValue (L.filterParam->load());
    L.modSmoothed.setTargetValue (juce::jmax (L.modParam->load(), modWheel.load()));
    if (L.due.empty() && tsf_active_voice_count (L.synth) == 0 && ! L.filterSmoothed.isSmoothing())
    {
        L.gain.skip (numSamples);
        L.modSmoothed.skip (numSamples);
        if (! L.wasSilent) { L.lowCut.reset(); L.highCut.reset(); L.modFilter.reset(); L.wasSilent = true; }
        return false;
    }
    L.wasSilent = false;

    auto& out = L.audio;
    out.clear (0, numSamples);
    auto* left = out.getWritePointer (0);
    auto* right = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;
    juce::dsp::AudioBlock<float> outBlock (out.getArrayOfWritePointers(), (size_t) out.getNumChannels(), (size_t) numSamples);
    const double lfoInc = juce::MathConstants<double>::twoPi * (double) L.rateParam->load() / sr;
    const double delaySamples = 0.25 * sr, fadeSamples = 0.6 * sr;   // vibrato estilo Roland: espera y despues crece

    auto render = [&] (int start, int count)
    {
        while (count > 0)
        {
            // modulacion: se actualiza cada 32 muestras. Sin modulacion se calcula el bloque de una vez.
            const bool modOn = L.modSmoothed.isSmoothing() || L.modSmoothed.getTargetValue() > 0.0005f;
            const int span = modOn ? juce::jmin (count, 32) : count;
            const float depth = L.modSmoothed.getNextValue();
            if (span > 1) L.modSmoothed.skip (span - 1);

            float fade = 0.0f, lfo = 0.0f;
            if (modOn)
            {
                const double since = (double) (blockStart + start - L.modTriggerTime);
                fade = (float) juce::jlimit (0.0, 1.0, (since - delaySamples) / fadeSamples);
                lfo = (float) std::sin (L.lfoPhase);
                L.lfoPhase = std::fmod (L.lfoPhase + lfoInc * span, juce::MathConstants<double>::twoPi);
            }
            const float amount = depth * fade;
            tsf_channel_set_tuning (L.synth, 0, amount > 0.0005f ? 0.5f * amount * lfo : 0.0f);

            const int chunk = juce::jmin (span, (int) renderBuffer.size() / 2);
            tsf_render_float (L.synth, renderBuffer.data(), chunk, 0);
            for (int i = 0; i < chunk; ++i)
            {
                const float g = L.gain.getNextValue();
                const float l = renderBuffer[(size_t) i * 2] * g, r = renderBuffer[(size_t) i * 2 + 1] * g;
                if (right != nullptr) { left[start + i] = l; right[start + i] = r; }
                else                    left[start + i] = 0.5f * (l + r);
            }

            // filtro que se mueve con el LFO (abierto cuando el LFO esta abajo, cerrado arriba)
            if (amount > 0.002f)
            {
                L.modFilter.setCutoffFrequency (juce::jlimit (300.0f, 18000.0f, 18000.0f * std::pow (2.0f, -amount * (1.0f + lfo) * 2.6f)));
                auto sub = outBlock.getSubBlock ((size_t) start, (size_t) chunk);
                juce::dsp::ProcessContextReplacing<float> ctx (sub);
                L.modFilter.process (ctx);
            }
            start += chunk;
            count -= chunk;
        }
    };

    int pos = 0;
    for (const auto& e : L.due)
    {
        const int evPos = (int) juce::jlimit<juce::int64> (0, numSamples, e.time - blockStart);
        if (evPos > pos) { render (pos, evPos - pos); pos = evPos; }

        const int type = e.status & 0xf0;
        if (type == 0x90 && e.d2 > 0)
        {
            const int key = juce::jlimit (0, 127, (int) e.d1 + transpose);
            L.noteMap[(size_t) e.d1] = key;
            if (L.notesHeld <= 0) L.modTriggerTime = e.time;   // frase nueva: el vibrato vuelve a esperar
            ++L.notesHeld;
            if (k == getActiveLayer())   // la pantalla muestra la velocity de la capa elegida
            {
                lastVelocity = juce::jlimit (1, 127, juce::roundToInt (e.vel * 127.0f));
                ++noteCounter;
            }
            // TinySoundFont responde a la velocity en forma lineal (casi no se nota la diferencia).
            // Al cuadrado se parece a la respuesta de un sampler / rompler: tocar suave suena de verdad mas suave.
            tsf_channel_note_on (L.synth, 0, key, e.vel * e.vel);
        }
        else if (type == 0x80 || (type == 0x90 && e.d2 == 0))
        {
            L.notesHeld = juce::jmax (0, L.notesHeld - 1);
            tsf_channel_note_off (L.synth, 0, L.noteMap[(size_t) e.d1]);
        }
        else if (type == 0xE0)
        {
            tsf_channel_set_pitchwheel (L.synth, 0, (int) e.d1 | ((int) e.d2 << 7));
        }
        else if (type == 0xB0)
        {
            const int cc = e.d1;
            if (cc == 123 || cc == 120) { tsf_channel_note_off_all (L.synth, 0); L.notesHeld = 0; }
            else if (cc != 1 && cc != 6 && cc != 38 && cc != 100 && cc != 101 && ccMap[(size_t) cc].load() < 0)
                tsf_channel_midi_control (L.synth, 0, cc, e.d2);
        }
    }
    if (pos < numSamples) render (pos, numSamples - pos);

    // ---- filtro de salida (perilla FILTRO de esta capa)
    const float fv = L.filterSmoothed.skip (numSamples);
    if (std::abs (fv) > 0.005f)
    {
        juce::dsp::ProcessContextReplacing<float> ctx (outBlock);
        if (fv < 0)
        {
            L.highCut.setCutoffFrequency (20000.0f * std::pow (200.0f / 20000.0f, -fv));
            L.highCut.process (ctx);
        }
        else
        {
            L.lowCut.setCutoffFrequency (20.0f * std::pow (2000.0f / 20.0f, fv));
            L.lowCut.process (ctx);
        }
    }
    return true;
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
            if (auto* prm = params.getParameter (layerParamId (learnableIds[i], getActiveLayer())))   // la perilla de la capa elegida
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
void TMUProcessor::loadPreset (const PresetEntry& e, int layer)
{
    const int k = layer < 0 ? getActiveLayer() : juce::jlimit (0, numLayers - 1, layer);
    {
        const juce::ScopedLock sl (infoLock);
        auto& L = layers[(size_t) k];
        L.pending = e;
        L.hasPending = true;
        L.requested = e;
        L.hasRequested = true;
        L.info.loading = true;
    }
    notify();
}

void TMUProcessor::resetLayerParams (int layer)
{
    // al sumar (o vaciar) una capa, sus controles vuelven a 0 / valor de fabrica y queda prendida
    if (layer < 0 || layer >= numLayers) return;
    auto setDefault = [this] (const juce::String& id, float normalised)
    {
        if (auto* prm = params.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (normalised < 0.0f ? prm->getDefaultValue() : normalised);
            prm->endChangeGesture();
        }
    };
    for (auto* base : { "volume", "transpose", "mod", "filter", "rate", "swing", "humanize", "velcurve" })
        setDefault (layerParamId (base, layer), -1.0f);
    setDefault (layerOnId (layer), 1.0f);
}

void TMUProcessor::clearLayer (int layer)
{
    if (layer < 0 || layer >= numLayers) return;
    tsf* old = nullptr;
    {
        const juce::SpinLock::ScopedLockType sl (synthLock);
        old = layers[(size_t) layer].synth;
        layers[(size_t) layer].synth = nullptr;
    }
    if (old != nullptr) tsf_close (old);

    const juce::ScopedLock sl (infoLock);
    auto& L = layers[(size_t) layer];
    L.info = {};
    L.pending = {};
    L.requested = {};
    L.hasPending = false;
    L.hasRequested = false;
}

TMUProcessor::LoadedInfo TMUProcessor::getLoadedInfo (int layer) const
{
    const juce::ScopedLock sl (infoLock);
    return layers[(size_t) juce::jlimit (0, numLayers - 1, layer)].info;
}

void TMUProcessor::run()
{
    while (! threadShouldExit())
    {
        PresetEntry job;
        int k = -1;
        {
            const juce::ScopedLock sl (infoLock);
            for (int i = 0; i < numLayers; ++i)
                if (layers[(size_t) i].hasPending)
                {
                    k = i;
                    job = layers[(size_t) i].pending;
                    layers[(size_t) i].hasPending = false;
                    break;
                }
        }

        if (k < 0) { wait (-1); continue; }

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
            tsf_set_max_voices (fresh, k == 0 ? 96 : 64);
            tsf_channel_set_presetindex (fresh, 0, 0);
            tsf_channel_set_pitchrange (fresh, 0, 2.0f);

            tsf* old = nullptr;
            {
                const juce::SpinLock::ScopedLockType sl (synthLock);
                old = layers[(size_t) k].synth;
                layers[(size_t) k].synth = fresh;
                tsf_set_output (fresh, TSF_STEREO_INTERLEAVED, (int) currentSampleRate.load(), 0.0f);
            }
            if (old != nullptr) tsf_close (old);
        }

        const juce::ScopedLock sl (infoLock);
        auto& L = layers[(size_t) k];
        if (fresh != nullptr)
        {
            L.info.file = job.file; L.info.source = job.source; L.info.name = job.name;
            L.info.phdrIndex = job.phdrIndex; L.info.category = job.category;
            L.info.error = {};
        }
        else
        {
            L.info.error = juce::String (err) + ": " + job.name;
        }
        L.info.loading = L.hasPending;
    }
}

void TMUProcessor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Instancia nueva sin sonido elegido: cargar el primer piano de la biblioteca (en la capa 1).
    {
        const juce::ScopedLock sl (infoLock);
        const auto& L = layers[0];
        if (L.info.phdrIndex >= 0 || L.hasPending || L.hasRequested || L.info.loading) return;
    }
    auto list = library->getEntries();
    const PresetEntry* pick = nullptr;
    for (auto& e : *list)
        if (e.category == tmu::Pianos && e.name.containsIgnoreCase ("grand")) { pick = &e; break; }
    if (pick == nullptr && ! list->empty()) pick = &list->front();
    if (pick != nullptr) loadPreset (*pick, 0);
}

//==============================================================================
void TMUProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = params.copyState();
    juce::ValueTree root ("TMUState");
    root.addChild (state, -1, nullptr);

    // lo que hay que guardar de una capa: el ultimo sonido pedido (aunque todavia se este cargando)
    auto layerState = [this] (int k)
    {
        const juce::ScopedLock sl (infoLock);
        const auto& L = layers[(size_t) k];
        LoadedInfo r = L.info;
        if (L.hasRequested)
        {
            r.file = L.requested.file; r.source = L.requested.source; r.name = L.requested.name;
            r.phdrIndex = L.requested.phdrIndex; r.category = L.requested.category;
        }
        return r;
    };

    // capa 1: las mismas propiedades de siempre (los proyectos viejos y nuevos se leen igual)
    const auto info = layerState (0);
    root.setProperty ("file", info.file, nullptr);
    root.setProperty ("source", info.source, nullptr);
    root.setProperty ("name", info.name, nullptr);
    root.setProperty ("phdr", info.phdrIndex, nullptr);
    root.setProperty ("category", info.category, nullptr);
    root.setProperty ("uiView", uiView, nullptr);
    root.setProperty ("uiGroup", uiGroup, nullptr);
    root.setProperty ("uiSearch", uiSearch, nullptr);
    root.setProperty ("uiLayer", getActiveLayer(), nullptr);
    // capas 2 a 4
    for (int k = 1; k < numLayers; ++k)
    {
        const auto li = layerState (k);
        if (li.phdrIndex < 0 || li.file.isEmpty()) continue;
        juce::ValueTree v ("Layer");
        v.setProperty ("index", k, nullptr);
        v.setProperty ("file", li.file, nullptr);
        v.setProperty ("source", li.source, nullptr);
        v.setProperty ("name", li.name, nullptr);
        v.setProperty ("phdr", li.phdrIndex, nullptr);
        v.setProperty ("category", li.category, nullptr);
        root.addChild (v, -1, nullptr);
    }
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
    setActiveLayer ((int) root.getProperty ("uiLayer", 0));
    for (auto& m : ccMap) m = -1;
    for (auto& item : juce::StringArray::fromTokens (root.getProperty ("midiMap").toString(), ",", ""))
    {
        const int cc = item.upToFirstOccurrenceOf (":", false, false).getIntValue();
        const auto id = item.fromFirstOccurrenceOf (":", false, false);
        for (int i = 0; i < numLearnable; ++i)
            if (id == learnableIds[i] && cc >= 0 && cc < 128) ccMap[(size_t) cc] = i;
    }

    // Si el archivo se movio (por ej. se reinstalo en otra carpeta), buscarlo por nombre.
    auto restore = [this] (const juce::ValueTree& v, int layer)
    {
        PresetEntry e;
        e.file = v.getProperty ("file").toString();
        e.source = v.getProperty ("source").toString();
        e.name = v.getProperty ("name").toString();
        e.phdrIndex = (int) v.getProperty ("phdr", -1);
        e.category = (int) v.getProperty ("category", (int) tmu::Other);

        if (e.phdrIndex < 0 || e.file.isEmpty()) return false;
        if (! juce::File (e.file).existsAsFile())
        {
            const auto fileName = juce::File (e.file).getFileName();
            for (auto& dir : PresetLibrary::getSearchFolders())
                for (const auto& it : juce::RangedDirectoryIterator (dir, true, fileName, juce::File::findFiles))
                    { e.file = it.getFile().getFullPathName(); break; }
        }
        loadPreset (e, layer);
        return true;
    };

    restore (root, 0);
    for (int k = 1; k < numLayers; ++k)
    {
        bool found = false;
        for (int i = 0; i < root.getNumChildren(); ++i)
        {
            auto v = root.getChild (i);
            if (v.hasType ("Layer") && (int) v.getProperty ("index", -1) == k)
            {
                found = restore (v, k);
                break;
            }
        }
        if (! found) clearLayer (k);
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
