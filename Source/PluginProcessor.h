#pragma once
#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include "Library.h"
#include "License.h"

struct tsf;

class TMUProcessor : public juce::AudioProcessor,
                     private juce::Thread,
                     private juce::ChangeListener,
                     private juce::Timer
{
public:
    TMUProcessor();
    ~TMUProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "TMU"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Capas (layering): hasta 4 sonidos tocando a la vez con el mismo MIDI, cada uno con su volumen y su ON.
    static constexpr int numLayers = 4;
    static juce::String layerOnId (int layer)  { return "l" + juce::String (layer + 1) + "on"; }
    static juce::String layerVolId (int layer) { return "l" + juce::String (layer + 1) + "vol"; }

    // Carga un sonido en segundo plano (no corta el audio). layer = -1: la capa elegida en la pantalla.
    void loadPreset (const PresetEntry& e, int layer = -1);
    void clearLayer (int layer);   // vacia una capa (la 1 no se puede vaciar desde la interfaz)

    struct LoadedInfo { juce::String file, source, name; int phdrIndex = -1, category = tmu::Other; juce::String error; bool loading = false; };
    LoadedInfo getLoadedInfo() const { return getLoadedInfo (getActiveLayer()); }   // de la capa elegida
    LoadedInfo getLoadedInfo (int layer) const;
    int getActiveLayer() const { return activeLayer.load(); }
    void setActiveLayer (int layer) { activeLayer = juce::jlimit (0, numLayers - 1, layer); }

    juce::AudioProcessorValueTreeState params;
    juce::MidiKeyboardState keyboardState;
    juce::SharedResourcePointer<PresetLibrary> library;

    // MIDI learn: click derecho en una perilla > Aprender controlador
    void startLearning (const juce::String& paramId);
    void clearLearning (const juce::String& paramId);
    int getAssignedCC (const juce::String& paramId) const;
    juce::String getLearningParam() const;

    // velocity de la ultima nota que sono (ya con la curva y el HUMAN), 1..127; -1 = ninguna
    int getLastVelocity() const { return lastVelocity.load(); }
    int getNoteCount() const { return noteCounter.load(); }

    // estado de la interfaz (se guarda con el proyecto)
    juce::String uiView { "type" }, uiGroup, uiSearch;

    juce::SharedResourcePointer<tmulicense::State> license;   // sin licencia: no suena

private:
    void run() override;   // hilo de carga
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    static constexpr int numLearnable = 7;
    static const char* learnableIds[numLearnable];
    std::array<std::atomic<int>, 128> ccMap;
    std::array<std::atomic<float>, numLearnable> pendingCC;
    std::atomic<int> learningParam { -1 };
    std::atomic<float> modWheel { 0.0f };
    std::atomic<float>* modParam = nullptr;
    std::atomic<float>* filterParam = nullptr;
    juce::SmoothedValue<float> modSmoothed, filterSmoothed;
    double lfoPhase = 0.0;
    bool wasSilent = false;
    juce::dsp::StateVariableTPTFilter<float> lowCut, highCut, modFilter;
    std::atomic<float>* rateParam = nullptr;
    std::atomic<float>* swingParam = nullptr;
    std::atomic<float>* humanParam = nullptr;
    std::atomic<float>* velCurveParam = nullptr;

    // eventos MIDI programados (para SWING y HUMANIZE, que atrasan notas)
    struct Event { juce::int64 time; juce::uint8 status, d1, d2; float vel; };
    std::vector<Event> scheduled, due;
    std::array<juce::int64, 128> noteDelay {};
    juce::int64 sampleCounter = 0;
    juce::int64 modTriggerTime = 0;   // cuando empezo la frase (para el vibrato con retardo estilo Roland)
    int notesHeld = 0;
    std::atomic<int> lastVelocity { -1 }, noteCounter { 0 };
    juce::Random rng;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // cada capa: su propio soundfont cargado + volumen y ON (parametros l1on, l1vol, ...)
    struct Layer
    {
        tsf* synth = nullptr;                 // protegido por synthLock
        std::atomic<float>* onParam = nullptr;
        std::atomic<float>* volParam = nullptr;
        juce::SmoothedValue<float> gain;      // solo hilo de audio
        LoadedInfo info;                      // protegido por infoLock
        PresetEntry pending;                  // proximo sonido a cargar
        bool hasPending = false;
        PresetEntry requested;                // ultimo sonido pedido (es lo que se guarda en el proyecto)
        bool hasRequested = false;
    };

    juce::SpinLock synthLock;
    mutable juce::CriticalSection infoLock;
    std::array<Layer, numLayers> layers;
    std::atomic<int> activeLayer { 0 };

    std::atomic<double> currentSampleRate { 44100.0 };
    std::vector<float> renderBuffer, layerBuffer;
    juce::SmoothedValue<float> gainSmoothed;
    int lastTranspose = 0;
    std::array<int, 128> noteMap {};   // nota que realmente sono para cada tecla (por el transpose)

    std::atomic<float>* volumeParam = nullptr;
    std::atomic<float>* transposeParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TMUProcessor)
};
