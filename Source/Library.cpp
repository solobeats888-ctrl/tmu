#include "Library.h"
#include <BinaryData.h>
#include <set>

//==============================================================================
UserPrefs::UserPrefs()
{
    file = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("Application Support/TMU/TMU-favoritos.xml");
   #if ! JUCE_MAC
    file = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("TMU/TMU-favoritos.xml");
   #endif
    if (auto xml = juce::XmlDocument::parse (file))
    {
        showDuplicates = xml->getBoolAttribute ("showDuplicates", false);
        tutorialSeen = xml->getBoolAttribute ("tutorialSeen", false);
        seenBaselineDone = xml->getBoolAttribute ("seenBaselineDone", false);
        for (auto* sf : xml->getChildWithTagNameIterator ("seen"))
        {
            const auto t = sf->getStringAttribute ("t").getLargeIntValue();
            if (t > 0) seen[sf->getStringAttribute ("key")] = t;
        }
        for (auto* f : xml->getChildWithTagNameIterator ("fav"))
        {
            const int n = juce::jlimit (0, 3, f->getIntAttribute ("stars"));
            if (n > 0) stars[f->getStringAttribute ("key")] = n;
        }
    }
}

int UserPrefs::getStars (const juce::String& key) const
{
    const juce::ScopedLock sl (lock);
    auto it = stars.find (key);
    return it == stars.end() ? 0 : it->second;
}

void UserPrefs::setStars (const juce::String& key, int n)
{
    {
        const juce::ScopedLock sl (lock);
        if (n <= 0) stars.erase (key); else stars[key] = juce::jmin (3, n);
    }
    save();
}

int UserPrefs::numFavourites() const
{
    const juce::ScopedLock sl (lock);
    return (int) stars.size();
}

bool UserPrefs::getShowDuplicates() const
{
    const juce::ScopedLock sl (lock);
    return showDuplicates;
}

void UserPrefs::setShowDuplicates (bool b)
{
    {
        const juce::ScopedLock sl (lock);
        showDuplicates = b;
    }
    save();
}

bool UserPrefs::getTutorialSeen() const
{
    const juce::ScopedLock sl (lock);
    return tutorialSeen;
}

void UserPrefs::setTutorialSeen (bool b)
{
    {
        const juce::ScopedLock sl (lock);
        tutorialSeen = b;
    }
    save();
}

juce::int64 UserPrefs::getFirstSeen (const juce::String& fileKey) const
{
    const juce::ScopedLock sl (lock);
    auto it = seen.find (fileKey);
    return it == seen.end() ? 0 : it->second;
}

bool UserPrefs::isSeenBaselineDone() const
{
    const juce::ScopedLock sl (lock);
    return seenBaselineDone;
}

void UserPrefs::recordSeen (const std::map<juce::String, juce::int64>& newFiles)
{
    {
        const juce::ScopedLock sl (lock);
        for (auto& [k, t] : newFiles) seen[k] = t;
        seenBaselineDone = true;
    }
    save();
}

void UserPrefs::save()
{
    juce::XmlElement xml ("TMUFavoritos");
    {
        const juce::ScopedLock sl (lock);
        xml.setAttribute ("showDuplicates", showDuplicates);
        xml.setAttribute ("tutorialSeen", tutorialSeen);
        xml.setAttribute ("seenBaselineDone", seenBaselineDone);
        for (auto& [k, t] : seen)
        {
            auto* sf = xml.createNewChildElement ("seen");
            sf->setAttribute ("key", k);
            sf->setAttribute ("t", juce::String (t));
        }
        for (auto& [k, n] : stars)
        {
            auto* f = xml.createNewChildElement ("fav");
            f->setAttribute ("key", k);
            f->setAttribute ("stars", n);
        }
    }
    file.getParentDirectory().createDirectory();
    xml.writeTo (file);
    ++version;
}

//==============================================================================
// Lista de sonidos repetidos / mudos de la biblioteca incluida (calculada comparando
// la huella de audio de cada sonido). Formato: archivo \t indice \t nombre \t motivo
static const std::map<juce::String, int>& knownHidden()
{
    static const std::map<juce::String, int> table = []
    {
        std::map<juce::String, int> t;
        const juce::String text (juce::CharPointer_UTF8 (BinaryData::repetidos_tsv), (size_t) BinaryData::repetidos_tsvSize);
        for (auto& line : juce::StringArray::fromLines (text))
        {
            auto parts = juce::StringArray::fromTokens (line, "\t", "");
            if (parts.size() < 4) continue;
            t[PresetEntry::makeKey (parts[0], parts[1].getIntValue(), parts[2])] = parts[3] == "mudo" ? 2 : 1;
        }
        return t;
    }();
    return table;
}

PresetLibrary::PresetLibrary() : juce::Thread ("TMU library scan")
{
    entries = std::make_shared<std::vector<PresetEntry>>();
    rescan();
}

PresetLibrary::~PresetLibrary()
{
    stopThread (5000);
}

juce::File PresetLibrary::getUserFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("TMU SoundFonts");
}

juce::Array<juce::File> PresetLibrary::getSearchFolders()
{
    juce::Array<juce::File> dirs;
    // TMU es solo la herramienta: no trae soundfonts. Cada uno pone los suyos en Musica/TMU SoundFonts.
   #if ! JUCE_MAC && ! JUCE_WINDOWS
    dirs.add (juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".local/share/TMU/SoundFonts"));
   #endif
    dirs.add (getUserFolder());   // ~/Music/TMU SoundFonts: para agregar soundfonts propios
    return dirs;
}

void PresetLibrary::rescan()
{
    if (scanning.exchange (true)) return;
    stopThread (2000);
    scanning = true;
    startThread (juce::Thread::Priority::background);
}

std::shared_ptr<const std::vector<PresetEntry>> PresetLibrary::getEntries() const
{
    const juce::ScopedLock sl (lock);
    return entries;
}

void PresetLibrary::run()
{
    auto list = std::make_shared<std::vector<PresetEntry>>();
    juce::StringArray seenFiles;

    // .sf2 nuevos: la primera vez que se instala esta funcion todo lo que ya habia queda como "viejo"
    // (1); lo que se agregue despues se anota con la fecha y figura en RECIENTES.
    const bool baselineDone = prefs->isSeenBaselineDone();
    const juce::int64 scanTime = juce::Time::currentTimeMillis();
    std::map<juce::String, juce::int64> newFiles;

    getUserFolder().createDirectory();

    for (auto& dir : getSearchFolders())
    {
        if (! dir.isDirectory()) continue;

        for (const auto& it : juce::RangedDirectoryIterator (dir, true, "*", juce::File::findFiles))
        {
            if (threadShouldExit()) { scanning = false; return; }

            const auto f = it.getFile();
            if (! f.getFileExtension().equalsIgnoreCase (".sf2")) continue;
            if (f.getFileName().startsWith ("._")) continue;   // archivos basura de macOS

            const auto key = f.getFileName().toLowerCase() + juce::String (f.getSize());
            if (seenFiles.contains (key)) continue;   // mismo archivo en dos carpetas
            seenFiles.add (key);

            juce::int64 addedMs = prefs->getFirstSeen (key);
            if (addedMs == 0)
            {
                addedMs = baselineDone ? scanTime : 1;
                newFiles[key] = addedMs;
            }

            std::vector<tmu::RawPreset> raw;
            if (! tmu::readPresetList (f.getFullPathName().toStdString(), raw)) continue;
            const auto cats = tmu::categorizeFile (raw);

            for (size_t i = 0; i < raw.size(); ++i)
            {
                if (tmu::isJunkName (raw[i].name)) continue;
                PresetEntry e;
                e.file = f.getFullPathName();
                e.source = f.getFileNameWithoutExtension();
                e.name = juce::String (raw[i].name);
                e.bank = raw[i].bank;
                e.program = raw[i].program;
                e.phdrIndex = raw[i].phdrIndex;
                e.category = cats[i];
                e.addedMs = addedMs;
                e.key = PresetEntry::makeKey (f.getFileName(), e.phdrIndex, e.name);
                auto hid = knownHidden().find (e.key);
                if (hid != knownHidden().end()) e.hidden = hid->second;
                list->push_back (std::move (e));
            }
        }
    }

    std::stable_sort (list->begin(), list->end(), [] (const PresetEntry& a, const PresetEntry& b)
    {
        const int c = a.name.compareNatural (b.name);
        return c != 0 ? c < 0 : a.source.compareNatural (b.source) < 0;
    });

    if (! newFiles.empty() || ! baselineDone)
        prefs->recordSeen (newFiles);

    {
        const juce::ScopedLock sl (lock);
        entries = list;
    }
    scanning = false;
    sendChangeMessage();
}
