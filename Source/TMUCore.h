// TMU core: lectura de SoundFonts (.sf2), clasificacion por tipo y extraccion de un solo preset.
// No depende de JUCE, asi se puede probar por separado.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace tmu
{
enum Category
{
    Pianos = 0, Keys, Guitars, Bass, Strings, Brass, Woodwinds, Leads, Pads, Chip,
    Bells, Voices, DrumKits, Percussion, FX, Ethnic, Other, NumCategories
};

const char* categoryName (int c);   // nombre para mostrar (español)

struct RawPreset
{
    std::string name;
    int bank = 0, program = 0;
    int phdrIndex = 0;   // indice dentro del archivo, identifica el preset
};

// Lee solo la lista de presets (rapido: no toca las muestras de audio).
bool readPresetList (const std::string& path, std::vector<RawPreset>& out);

// Clasifica un nombre por palabras clave. Devuelve -1 si no hay coincidencia.
int categoryFromName (const std::string& presetName);

// Categoria segun el numero de programa General MIDI.
int categoryFromGM (int bank, int program);

// Clasifica todos los presets de un archivo (decide si el archivo sigue el estandar GM).
std::vector<int> categorizeFile (const std::vector<RawPreset>& presets);

// Presets basura ("-----", "EOP", vacios) que no se muestran.
bool isJunkName (const std::string& presetName);

// Arma en memoria un .sf2 minimo que contiene SOLO el preset pedido y sus muestras.
// Asi un soundfont de 1 GB no se carga entero para tocar un solo sonido.
bool extractPreset (const std::string& path, int phdrIndex, std::vector<uint8_t>& outSf2, std::string& error);
}
