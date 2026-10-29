#pragma once
#include <JuceHeader.h>
#include "TMUCore.h"

// Biblioteca compartida por todas las instancias de TMU abiertas en FL Studio.
// Escanea las carpetas de soundfonts en segundo plano.
struct PresetEntry
{
    juce::String file;        // ruta completa al .sf2
    juce::String source;      // nombre del soundfont (sin extension)
    juce::String name;        // nombre del sonido
    int bank = 0, program = 0, phdrIndex = 0, category = tmu::Other;
};

class PresetLibrary : public juce::ChangeBroadcaster,
                      private juce::Thread
{
public:
    PresetLibrary();
    ~PresetLibrary() override;

    static juce::Array<juce::File> getSearchFolders();
    static juce::File getUserFolder();

    void rescan();
    bool isScanning() const { return scanning.load(); }

    // copia segura de la lista actual (ordenada A-Z)
    std::shared_ptr<const std::vector<PresetEntry>> getEntries() const;

private:
    void run() override;

    mutable juce::CriticalSection lock;
    std::shared_ptr<const std::vector<PresetEntry>> entries;
    std::atomic<bool> scanning { false };
};
