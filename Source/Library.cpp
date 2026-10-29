#include "Library.h"

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
   #if JUCE_MAC
    dirs.add (juce::File ("/Library/Application Support/TMU/SoundFonts"));          // lo instala el .pkg
    dirs.add (juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                 .getChildFile ("Library/Application Support/TMU/SoundFonts"));
   #elif JUCE_WINDOWS
    dirs.add (juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory).getChildFile ("TMU/SoundFonts"));
   #else
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
                list->push_back (std::move (e));
            }
        }
    }

    std::stable_sort (list->begin(), list->end(), [] (const PresetEntry& a, const PresetEntry& b)
    {
        const int c = a.name.compareNatural (b.name);
        return c != 0 ? c < 0 : a.source.compareNatural (b.source) < 0;
    });

    {
        const juce::ScopedLock sl (lock);
        entries = list;
    }
    scanning = false;
    sendChangeMessage();
}
