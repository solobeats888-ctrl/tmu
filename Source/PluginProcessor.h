#pragma once
#include <JuceHeader.h>
#include "Library.h"

struct tsf;

class TMUProcessor : public juce::AudioProcessor,
                     private juce::Thread,
                     private juce::ChangeListener
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
    // Carga un sonido en segundo plano (no corta el audio).
    void loadPreset (const PresetEntry& e);

    struct LoadedInfo { juce::String file, source, name; int phdrIndex = -1, category = tmu::Other; juce::String error; bool loading = false; };
    LoadedInfo getLoadedInfo() const;

    juce::AudioProcessorValueTreeState params;
    juce::MidiKeyboardState keyboardState;
    juce::SharedResourcePointer<PresetLibrary> library;

    // estado de la interfaz (se guarda con el proyecto)
    juce::String uiView { "type" }, uiGroup, uiSearch;

private:
    void run() override;   // hilo de carga
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::SpinLock synthLock;
    tsf* synth = nullptr;

    mutable juce::CriticalSection infoLock;
    LoadedInfo loaded;
    PresetEntry pending;
    bool hasPending = false;

    std::atomic<double> currentSampleRate { 44100.0 };
    std::vector<float> renderBuffer;
    juce::SmoothedValue<float> gainSmoothed;
    int lastTranspose = 0;
    std::array<int, 128> noteMap {};   // nota que realmente sono para cada tecla (por el transpose)

    std::atomic<float>* volumeParam = nullptr;
    std::atomic<float>* transposeParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TMUProcessor)
};
