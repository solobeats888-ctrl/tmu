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
    juce::int64 addedMs = 0;  // cuando se vio por primera vez el .sf2 (ms); 0 o 1 = ya estaba antes de la actualizacion
    juce::String key;         // "archivo.sf2|indice|nombre": identifica el sonido en favoritos

    // Cuantos dias un .sf2 recien agregado figura como "reciente" (pintado de otro color y en el grupo RECIENTES)
    static constexpr juce::int64 recentDays = 14;
    bool isRecent() const
    {
        return addedMs > 1 && juce::Time::currentTimeMillis() - addedMs < recentDays * 24 * 60 * 60 * 1000;
    }

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

    // Nombres propios: el usuario le puede cambiar el nombre a un sonido. El archivo .sf2 no se toca,
    // solo se guarda "clave del sonido -> nombre que quiere ver". Vacio = sigue el nombre original.
    juce::String getCustomName (const juce::String& key) const;
    void setCustomName (const juce::String& key, const juce::String& newName);   // vacio = volver al original
    juce::String shownName (const juce::String& key, const juce::String& original) const;   // nombre propio si hay, si no el original

    // Soundfonts ya vistos (para marcar los nuevos). La clave es "nombre.sf2" + tamano.
    // Devuelve 0 si ese archivo nunca se vio; 1 si ya estaba cuando se instalo esta funcion; si no, cuando se agrego (ms).
    juce::int64 getFirstSeen (const juce::String& fileKey) const;
    bool isSeenBaselineDone() const;
    void recordSeen (const std::map<juce::String, juce::int64>& newFiles);   // guarda y marca la lista base como hecha

private:
    void save();
    juce::File file;
    mutable juce::CriticalSection lock;
    std::map<juce::String, int> stars;
    std::map<juce::String, juce::String> customNames;
    std::map<juce::String, juce::int64> seen;
    bool seenBaselineDone = false;
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
    juce::SharedResourcePointer<UserPrefs> prefs;   // para saber que .sf2 son nuevos
};
