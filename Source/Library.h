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
    int hidden = 0;           // 0 = visible, 1 = repetido de otro sonido, 2 = no suena
    juce::String key;         // "archivo.sf2|indice|nombre": identifica el sonido en favoritos

    static juce::String makeKey (const juce::String& fileName, int phdr, const juce::String& presetName)
    {
        return fileName + "|" + juce::String (phdr) + "|" + presetName;
    }
};

// Favoritos (1 a 3 estrellas) y opciones del usuario, compartidos por todas las instancias
// y guardados en ~/Library/Application Support/TMU/TMU-favoritos.xml
class UserPrefs
{
public:
    UserPrefs();
    int getStars (const juce::String& key) const;
    void setStars (const juce::String& key, int stars);
    int numFavourites() const;
    bool getShowDuplicates() const;
    void setShowDuplicates (bool);
    bool getTutorialSeen() const;
    void setTutorialSeen (bool);
    int getVersion() const { return version.load(); }   // cambia cada vez que se modifica algo

private:
    void save();
    juce::File file;
    mutable juce::CriticalSection lock;
    std::map<juce::String, int> stars;
    bool showDuplicates = false;
    bool tutorialSeen = false;
    std::atomic<int> version { 0 };
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
