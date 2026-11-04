// Idioma de los plugins de Viszeversza: espanol / ingles, se cambia desde adentro del plugin.
// La eleccion se guarda en un archivo (Application Support/Viszeversza/language.txt), asi vale para todos los plugins.
// Sin eleccion guardada: usa el idioma del sistema (espanol si es "es", ingles en cualquier otro caso).
// Los textos del codigo estan en espanol; TR("texto") devuelve la traduccion si el idioma es ingles y la tabla (LangEN.h) la tiene.
#pragma once
#include <JuceHeader.h>
#include <string>
#include <unordered_map>
#include "LangEN.h"

namespace lang
{
enum Id { ES = 0, EN = 1 };

inline juce::File settingsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Viszeversza").getChildFile ("language.txt");
}

inline Id& state()
{
    static Id v = []
    {
        const auto f = settingsFile();
        if (f.existsAsFile())
        {
            const auto t = f.loadFileAsString().trim().toLowerCase();
            if (t == "en") return EN;
            if (t == "es") return ES;
        }
        return juce::SystemStats::getUserLanguage().startsWithIgnoreCase ("es") ? ES : EN;
    }();
    return v;
}

inline Id get() { return state(); }

inline void set (Id id)
{
    state() = id;
    const auto f = settingsFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText (id == EN ? "en" : "es");
}

inline juce::String tr (const char* es)
{
    if (state() == EN)
    {
        const auto& t = table();
        const auto it = t.find (es);
        if (it != t.end()) return juce::String (juce::CharPointer_UTF8 (it->second));
    }
    return juce::String (juce::CharPointer_UTF8 (es));
}

inline juce::String tr (const juce::String& es) { return tr (es.toRawUTF8()); }
} // namespace lang

#define TR(s) lang::tr (s)
