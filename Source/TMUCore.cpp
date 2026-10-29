#include "TMUCore.h"
#include <cstdio>
#include <cstring>
#include <cctype>
#include <map>
#include <memory>
#include <algorithm>
#include <cstdlib>

#if defined(_WIN32)
 #define tmu_fseek _fseeki64
 #define tmu_ftell _ftelli64
 typedef long long tmu_off;
#else
 #define tmu_fseek fseeko
 #define tmu_ftell ftello
 typedef off_t tmu_off;
#endif

namespace tmu
{
//==============================================================================
const char* categoryName (int c)
{
    static const char* names[] = {
        "Pianos", "Teclados y Organos", "Guitarras", "Bajos", "Cuerdas", "Metales", "Vientos",
        "Synth Leads", "Pads y Atmosferas", "Chiptune / 8-bit", "Campanas y Mallets",
        "Voces y Coros", "Kits de Bateria", "Percusion", "FX", "Etnicos", "Otros" };
    return (c >= 0 && c < NumCategories) ? names[c] : "Otros";
}

//==============================================================================
namespace
{
struct FileCloser { void operator() (FILE* f) const { if (f) fclose (f); } };
using FilePtr = std::unique_ptr<FILE, FileCloser>;

uint32_t rd32 (const uint8_t* p) { return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24); }
uint16_t rd16 (const uint8_t* p) { return (uint16_t) (p[0] | (p[1] << 8)); }

struct Hydra
{
    std::vector<uint8_t> phdr, pbag, pmod, pgen, inst, ibag, imod, igen, shdr;
    tmu_off smplOffset = -1;
    uint32_t smplBytes = 0;
};

// Recorre los chunks RIFF. Si wantSamples=false igual guarda la posicion de smpl.
bool readHydra (FILE* f, Hydra& h, bool onlyPhdr)
{
    tmu_fseek (f, 0, SEEK_END);
    const tmu_off fileSize = tmu_ftell (f);
    tmu_fseek (f, 0, SEEK_SET);

    uint8_t hdr[12];
    if (fread (hdr, 1, 12, f) != 12 || memcmp (hdr, "RIFF", 4) != 0 || memcmp (hdr + 8, "sfbk", 4) != 0)
        return false;

    tmu_off riffEnd = std::min<tmu_off> (fileSize, (tmu_off) 8 + rd32 (hdr + 4));
    tmu_off pos = 12;

    while (pos + 8 <= riffEnd)
    {
        tmu_fseek (f, pos, SEEK_SET);
        uint8_t ch[12];
        if (fread (ch, 1, 8, f) != 8) break;
        const uint32_t size = rd32 (ch + 4);
        const tmu_off start = pos + 8;
        const tmu_off end = std::min<tmu_off> (riffEnd, start + size);

        if (memcmp (ch, "LIST", 4) == 0 && fread (ch + 8, 1, 4, f) == 4)
        {
            const bool isPdta = memcmp (ch + 8, "pdta", 4) == 0;
            const bool isSdta = memcmp (ch + 8, "sdta", 4) == 0;
            tmu_off sp = start + 4;

            while ((isPdta || isSdta) && sp + 8 <= end)
            {
                tmu_fseek (f, sp, SEEK_SET);
                uint8_t sc[8];
                if (fread (sc, 1, 8, f) != 8) break;
                uint32_t ssize = rd32 (sc + 4);
                const tmu_off sstart = sp + 8;
                if (sstart + (tmu_off) ssize > end) ssize = (uint32_t) std::max<tmu_off> (0, end - sstart);

                if (isSdta && memcmp (sc, "smpl", 4) == 0)
                {
                    h.smplOffset = sstart;
                    h.smplBytes = ssize;
                }
                else if (isPdta && ssize < 64u * 1024u * 1024u)
                {
                    std::vector<uint8_t>* dst = nullptr;
                    if      (memcmp (sc, "phdr", 4) == 0) dst = &h.phdr;
                    else if (! onlyPhdr)
                    {
                        if      (memcmp (sc, "pbag", 4) == 0) dst = &h.pbag;
                        else if (memcmp (sc, "pmod", 4) == 0) dst = &h.pmod;
                        else if (memcmp (sc, "pgen", 4) == 0) dst = &h.pgen;
                        else if (memcmp (sc, "inst", 4) == 0) dst = &h.inst;
                        else if (memcmp (sc, "ibag", 4) == 0) dst = &h.ibag;
                        else if (memcmp (sc, "imod", 4) == 0) dst = &h.imod;
                        else if (memcmp (sc, "igen", 4) == 0) dst = &h.igen;
                        else if (memcmp (sc, "shdr", 4) == 0) dst = &h.shdr;
                    }
                    if (dst != nullptr)
                    {
                        dst->resize (ssize);
                        if (ssize > 0 && fread (dst->data(), 1, ssize, f) != ssize) dst->clear();
                    }
                }
                sp = sstart + ssize + (ssize & 1);
            }
        }
        pos = start + size + (size & 1);
    }
    return ! h.phdr.empty();
}

std::string trimName (const uint8_t* p, size_t maxLen)
{
    std::string s;
    for (size_t i = 0; i < maxLen && p[i] != 0; ++i)
    {
        unsigned char c = p[i];
        s += (c >= 32 && c < 127) ? (char) c : ' ';
    }
    while (! s.empty() && s.back() == ' ') s.pop_back();
    size_t a = 0;
    while (a < s.size() && s[a] == ' ') ++a;
    return s.substr (a);
}
} // namespace

//==============================================================================
bool readPresetList (const std::string& path, std::vector<RawPreset>& out)
{
    out.clear();
    FilePtr f (fopen (path.c_str(), "rb"));
    if (! f) return false;

    Hydra h;
    if (! readHydra (f.get(), h, false)) return false;

    const size_t n = h.phdr.size() / 38, nPbag = h.pbag.size() / 4, nPgen = h.pgen.size() / 4;
    for (size_t i = 0; i + 1 < n; ++i)   // el ultimo es el terminador "EOP"
    {
        const uint8_t* r = h.phdr.data() + i * 38;

        // descartar presets vacios (sin ningun instrumento asignado)
        bool hasInstrument = false;
        size_t b0 = rd16 (r + 24), b1 = rd16 (r + 38 + 24);
        for (size_t b = b0; b < b1 && b + 1 < nPbag && ! hasInstrument; ++b)
        {
            size_t g0 = rd16 (h.pbag.data() + b * 4), g1 = std::min<size_t> (rd16 (h.pbag.data() + (b + 1) * 4), nPgen);
            for (size_t g = g0; g < g1; ++g)
                if (rd16 (h.pgen.data() + g * 4) == 41) { hasInstrument = true; break; }
        }
        if (! hasInstrument) continue;

        RawPreset p;
        p.name = trimName (r, 20);
        p.program = rd16 (r + 20);
        p.bank = rd16 (r + 22);
        p.phdrIndex = (int) i;
        out.push_back (p);
    }
    return true;
}

//==============================================================================
// Clasificacion por nombre.
// Se normaliza el nombre (minusculas, separando "DrawbarOrgan" -> "drawbar organ")
// y se busca cada palabra clave al comienzo de una palabra. Gana la coincidencia
// que aparece MAS A LA DERECHA (en ingles el sustantivo principal va al final:
// "Synth Bass" es un bajo, "Bass Drum" es percusion, "Perc Organ" es un organo).
namespace
{
std::string normalise (const std::string& in)
{
    std::string s = " ";
    char prev = ' ';
    for (char c : in)
    {
        const bool lowerToUpper = std::islower ((unsigned char) prev) && std::isupper ((unsigned char) c);
        const bool alphaDigit = (std::isalpha ((unsigned char) prev) && std::isdigit ((unsigned char) c))
                             || (std::isdigit ((unsigned char) prev) && std::isalpha ((unsigned char) c));
        if (lowerToUpper || alphaDigit) s += ' ';
        s += std::isalnum ((unsigned char) c) ? (char) std::tolower ((unsigned char) c) : ' ';
        prev = c;
    }
    s += ' ';
    // comprimir espacios
    std::string r;
    for (char c : s) if (! (c == ' ' && ! r.empty() && r.back() == ' ')) r += c;
    return r;
}

struct Rule { const char* pattern; int category; bool wholeWord; };

// pattern: palabras separadas por espacio. wholeWord=true exige que termine en limite de palabra.
const Rule rules[] = {
    // --- Pianos
    { "piano", Pianos, false }, { "grand", Pianos, true }, { "steinway", Pianos, false }, { "upright", Pianos, false },
    { "honky", Pianos, false }, { "pno", Pianos, true }, { "yamaha", Pianos, true }, { "bosendorfer", Pianos, false },
    // --- Teclados / organos
    { "e piano", Keys, false }, { "ep", Keys, true }, { "el piano", Keys, false }, { "electric piano", Keys, false },
    { "epiano", Keys, false }, { "rhodes", Keys, false }, { "wurli", Keys, false }, { "dx", Keys, true },
    { "clav", Keys, false }, { "harpsi", Keys, false }, { "cembalo", Keys, false }, { "organ", Keys, false },
    { "accordion", Keys, false }, { "accordian", Keys, false }, { "harmonica", Keys, false }, { "harmonium", Keys, false },
    { "bandoneon", Keys, false }, { "melodica", Keys, false }, { "drawbar", Keys, false }, { "hammond", Keys, false },
    { "tine", Keys, false }, { "keys", Keys, true }, { "calliope", Leads, false },
    // --- Guitarras
    { "guitar", Guitars, false }, { "gtr", Guitars, false }, { "gt", Guitars, true }, { "gtr", Guitars, false },
    { "overdrive", Guitars, false }, { "distortion", Guitars, false }, { "dist gt", Guitars, false },
    { "nylon", Guitars, false }, { "steel gt", Guitars, false }, { "jazz gt", Guitars, false }, { "clean gt", Guitars, false },
    { "muted gt", Guitars, false }, { "power chord", Guitars, false }, { "feedback", Guitars, false }, { "strat", Guitars, true },
    { "les paul", Guitars, false }, { "mandolin", Guitars, false }, { "ukulele", Guitars, false }, { "uke", Guitars, true },
    { "lute", Ethnic, true }, 
    // --- Bajos
    { "bass", Bass, false }, { "bss", Bass, true }, { "808", Bass, true }, { "fretless", Bass, false }, { "slap", Bass, false },
    { "sub", Bass, true }, { "reese", Bass, false }, { "bassline", Bass, false },
    // --- Cuerdas
    { "string", Strings, false }, { "strg", Strings, false }, { "str", Strings, true }, { "strs", Strings, true },
    { "violin", Strings, false }, { "viola", Strings, false }, { "cello", Strings, false }, { "contrabass", Strings, false },
    { "double bass", Strings, false }, { "fiddle", Strings, false }, { "pizz", Strings, false }, { "tremolo", Strings, false },
    { "harp", Strings, true }, { "harps", Strings, true }, { "orchestra", Strings, true }, { "orchestral", Strings, true },
    { "ensemble", Strings, true }, { "quartet", Strings, false }, { "legato", Strings, false }, { "spiccato", Strings, false },
    { "staccato", Strings, false }, { "arco", Strings, true }, { "symphon", Strings, false }, { "viol", Strings, true },
    // --- Metales
    { "brass", Brass, false }, { "trumpet", Brass, false }, { "trombone", Brass, false }, { "tuba", Brass, true },
    { "horn", Brass, true }, { "horns", Brass, true }, { "french horn", Brass, false }, { "flugel", Brass, false },
    { "cornet", Brass, false }, { "tpt", Brass, true }, { "trp", Brass, true }, { "fanfare", Brass, false },
    { "sousaphone", Brass, false }, { "euphonium", Brass, false }, { "bugle", Brass, false }, { "brs", Brass, true },
    // --- Vientos
    { "flute", Woodwinds, false }, { "flt", Woodwinds, true }, { "piccolo", Woodwinds, false }, { "recorder", Woodwinds, false },
    { "sax", Woodwinds, false }, { "clarinet", Woodwinds, false }, { "oboe", Woodwinds, false }, { "bassoon", Woodwinds, false },
    { "english horn", Woodwinds, false }, { "pan flute", Woodwinds, false }, { "panflute", Woodwinds, false },
    { "pan pipe", Woodwinds, false }, { "panpipe", Woodwinds, false }, { "ocarina", Woodwinds, false },
    { "whistle", Woodwinds, false }, { "blown bottle", Woodwinds, false }, { "bottle", Woodwinds, false },
    { "reed", Woodwinds, true }, { "reeds", Woodwinds, true }, { "woodwind", Woodwinds, false }, { "fife", Woodwinds, true },
    { "kazoo", Woodwinds, false }, { "pipe", Woodwinds, true }, { "pipes", Woodwinds, true }, { "wind", Woodwinds, true },
    // --- Synth leads
    { "lead", Leads, false }, { "ld", Leads, true }, { "synth", Leads, false }, { "syn", Leads, true }, { "chiff", Leads, false },
    { "charang", Leads, false }, { "pluck", Leads, false }, { "stab", Leads, false },
    { "saw", Leads, true }, { "sawtooth", Leads, false }, { "supersaw", Leads, false }, { "analog", Leads, false },
    { "moog", Leads, false }, { "juno", Leads, false }, { "jupiter", Leads, false }, { "polysynth", Pads, false },
    { "arp", Leads, true }, { "bell lead", Leads, false }, { "fifths", Leads, false }, { "acid", Leads, true },
    { "wobble", Leads, false }, { "sync", Leads, true }, { "hoover", Leads, false },
    // --- Pads
    { "pad", Pads, true }, { "pads", Pads, true }, { "warm", Pads, true }, { "halo", Pads, true }, { "sweep", Pads, true },
    { "atmos", Pads, false }, { "ambient", Pads, false }, { "ambience", Pads, false }, { "fantasia", Pads, false },
    { "soundtrack", Pads, false }, { "new age", Pads, false }, { "bowed", Pads, false }, { "metallic", Pads, false },
    { "brightness", Pads, false }, { "goblin", Pads, false }, { "echoes", Pads, false }, { "echo drops", Pads, false },
    { "sci fi", Pads, false }, { "space", Pads, true }, { "texture", Pads, false }, { "drone", Pads, false },
    { "crystal", Bells, true }, { "dream", Pads, true }, { "ethereal", Pads, false }, { "evolving", Pads, false },
    // --- Chiptune
    { "square", Chip, false }, { "sqr", Chip, true }, { "pulse", Chip, false }, { "triangle", Chip, false },
    { "tri", Chip, true }, { "sine", Chip, false }, { "saw wave", Chip, false }, { "8 bit", Chip, false },
    { "8bit", Chip, false }, { "chip", Chip, false }, { "nes", Chip, true }, { "famicom", Chip, false },
    { "gb", Chip, true }, { "gameboy", Chip, false }, { "game boy", Chip, false }, { "psg", Chip, true },
    { "fm", Chip, true }, { "ym", Chip, true }, { "sid", Chip, true }, { "wave", Chip, true }, { "waveform", Chip, false },
    { "duty", Chip, false }, { "bleep", Chip, false }, { "blip", Chip, false },
    { "beep", Chip, false },
    // --- Campanas / mallets
    { "bell", Bells, false }, { "chime", Bells, false }, { "glock", Bells, false }, { "celest", Bells, false },
    { "vibes", Bells, true }, { "vibraphone", Bells, false }, { "marimba", Bells, false }, { "xylo", Bells, false },
    { "music box", Bells, false }, { "musicbox", Bells, false }, { "tubular", Bells, false }, { "kalimba", Bells, false },
    { "steel drum", Bells, false }, { "steeldrum", Bells, false }, { "carillon", Bells, false }, { "dulcimer", Bells, false },
    { "mallet", Bells, false }, { "glass", Bells, false }, { "tinkle", Bells, false }, { "celesta", Bells, false },
    { "handbell", Bells, false }, { "gamelan", Bells, false }, { "toy", Bells, true }, { "crotales", Bells, false },
    // --- Voces
    { "choir", Voices, false }, { "chorus", Voices, true }, { "voice", Voices, false }, { "vox", Voices, true },
    { "vocal", Voices, false }, { "aah", Voices, false }, { "ahh", Voices, false }, { "aahs", Voices, true },
    { "ooh", Voices, false }, { "oohs", Voices, true }, { "oh", Voices, true }, { "ah", Voices, true },
    { "doo", Voices, true }, { "scat", Voices, true }, { "hum", Voices, true }, { "humming", Voices, false },
    { "sing", Voices, false }, { "soprano", Voices, true }, { "alto", Voices, true }, { "tenor", Voices, true },
    { "chant", Voices, false }, { "vocoder", Voices, false }, { "talk", Voices, false }, { "yeah", Voices, false },
    { "hey", Voices, true }, { "shout", Voices, false }, { "laugh", Voices, false }, { "scream", Voices, false },
    { "soprano sax", Woodwinds, false }, { "alto sax", Woodwinds, false }, { "tenor sax", Woodwinds, false },
    // --- Kits de bateria
    { "kit", DrumKits, false }, { "drum set", DrumKits, false }, { "drumset", DrumKits, false }, { "drumkit", DrumKits, false },
    { "drums", DrumKits, true }, { "percussion set", DrumKits, false }, { "perc set", DrumKits, false },
    { "standard set", DrumKits, false }, { "std set", DrumKits, false }, { "808 set", DrumKits, false },
    { "909", DrumKits, true }, { "tr 808", DrumKits, false }, { "tr 909", DrumKits, false }, { "drum map", DrumKits, false },
    { "brush set", DrumKits, false }, { "jazz set", DrumKits, false }, { "room set", DrumKits, false },
    { "power set", DrumKits, false }, { "electronic set", DrumKits, false }, { "orchestra set", DrumKits, false },
    // --- Percusion
    { "kick", Percussion, false }, { "bd", Percussion, true }, { "snare", Percussion, false }, { "snr", Percussion, true },
    { "sd", Percussion, true }, { "hihat", Percussion, false }, { "hi hat", Percussion, false }, { "hh", Percussion, true },
    { "hat", Percussion, true }, { "hats", Percussion, true }, { "cymbal", Percussion, false }, { "symbol", Percussion, true },
    { "crash", Percussion, false }, { "ride", Percussion, true }, { "splash", Percussion, false }, { "china", Percussion, true },
    { "tom", Percussion, true }, { "toms", Percussion, true }, { "clap", Percussion, false }, { "conga", Percussion, false },
    { "bongo", Percussion, false }, { "timbal", Percussion, false }, { "cowbell", Percussion, false },
    { "shaker", Percussion, false }, { "tambourin", Percussion, false }, { "agogo", Percussion, false },
    { "woodblock", Percussion, false }, { "wood block", Percussion, false }, { "timpani", Percussion, false },
    { "timp", Percussion, true }, { "taiko", Percussion, false }, { "drum", Percussion, false }, { "perc", Percussion, false },
    { "claves", Percussion, false }, { "clave", Percussion, true }, { "cabasa", Percussion, false }, { "guiro", Percussion, false },
    { "maracas", Percussion, false }, { "rimshot", Percussion, false }, { "rim", Percussion, true }, { "snap", Percussion, false },
    { "bata", Percussion, true }, { "djembe", Percussion, false }, { "cajon", Percussion, false }, { "castanet", Percussion, false },
    { "triangle", Chip, false }, { "gong", Percussion, false }, { "cuica", Percussion, false }, { "surdo", Percussion, false },
    { "timbale", Percussion, false }, { "click", Percussion, true }, { "metronome", Percussion, false }, { "stick", Percussion, false },
    { "steel drum", Bells, false }, { "steel drums", Bells, false },
    // --- FX
    { "fx", FX, true }, { "sfx", FX, true }, { "se", FX, true }, { "effect", FX, false }, { "noise", FX, false },
    { "seashore", FX, false }, { "rain", FX, true }, { "thunder", FX, false }, { "bird", FX, false }, { "tweet", FX, false },
    { "telephone", FX, false }, { "phone", FX, true }, { "helicopter", FX, false }, { "applause", FX, false },
    { "gunshot", FX, false }, { "gun", FX, true }, { "explosion", FX, false }, { "explode", FX, false }, { "laser", FX, false },
    { "siren", FX, false }, { "scratch", FX, false }, { "breath", FX, false }, { "fret noise", FX, false },
    { "reverse", FX, false }, { "rev cym", FX, false }, { "heart beat", FX, false }, { "heartbeat", FX, false },
    { "car", FX, true }, { "door", FX, false }, { "footstep", FX, false }, { "jet", FX, true }, { "horse", FX, false },
    { "dog", FX, true }, { "bark", FX, true }, { "meow", FX, false }, { "camera", FX, false }, { "whirr", FX, false },
    { "waves", FX, true }, { "ocean", FX, false }, { "stream", FX, false }, { "bubble", FX, false }, { "splash fx", FX, false },
    { "hit", FX, true }, { "orchestra hit", FX, false }, { "orch hit", FX, false }, { "impact", FX, false },
    { "riser", FX, false }, { "whoosh", FX, false }, { "zap", FX, true }, { "coin", FX, true }, { "jump", FX, true },
    { "1up", FX, false }, { "1 up", FX, false }, { "alarm", FX, false }, { "beeper", FX, false }, { "ring", FX, true },
    { "punch", FX, true }, { "slash", FX, true }, { "sword", FX, false }, { "engine", FX, false }, { "train", FX, true },
    { "oof", FX, true }, { "glitch", FX, false }, { "static", FX, false }, { "vinyl", FX, false }, { "tape", FX, true },
    { "wind chime", Bells, false }, { "crowd", FX, false }, { "cheer", FX, false }, { "waterphone", FX, false },
    { "bass clar", Woodwinds, false }, { "clar", Woodwinds, false }, { "vibra slap", Percussion, false },
    { "shenai", Ethnic, false }, { "didj", Ethnic, false }, { "shofar", Brass, false }, { "likembe", Bells, false },
    { "ohh", Voices, true }, { "tp", Brass, true }, { "tb", Brass, true }, { "fl", Woodwinds, true },
    // --- Etnicos
    { "sitar", Ethnic, false }, { "banjo", Ethnic, false }, { "shamisen", Ethnic, false }, { "koto", Ethnic, false },
    { "bagpipe", Ethnic, false }, { "shanai", Ethnic, false }, { "shehnai", Ethnic, false }, { "shakuhachi", Ethnic, false },
    { "erhu", Ethnic, false }, { "oud", Ethnic, true }, { "tabla", Ethnic, false }, { "balalaika", Ethnic, false },
    { "bouzouki", Ethnic, false }, { "pipa", Ethnic, true }, { "guzheng", Ethnic, false }, { "didgeridoo", Ethnic, false },
    { "zither", Ethnic, false }, { "santur", Ethnic, false }, { "hulusi", Ethnic, false }, { "dizi", Ethnic, true },
    { "kora", Ethnic, true }, { "charango", Ethnic, false }, { "bandura", Ethnic, false }, { "duduk", Ethnic, false },
};

bool matchAt (const std::string& s, size_t pos, const std::string& pat, bool wholeWord)
{
    if (s.compare (pos, pat.size(), pat) != 0) return false;
    if (! wholeWord) return true;
    const size_t e = pos + pat.size();
    return e >= s.size() || s[e] == ' ';
}
} // namespace

int categoryFromName (const std::string& presetName)
{
    const std::string s = normalise (presetName);

    // Nombres de ripeos de GBA: "Type 1 @0x..." (1,2 = onda cuadrada, 3 = wave, 4 = ruido)
    if (s.compare (0, 6, " type ") == 0)
    {
        const int t = std::atoi (s.c_str() + 6);
        if (t == 128) return DrumKits;
        if ((t & 7) >= 1 && (t & 7) <= 4) return Chip;
    }

    int best = -1;
    size_t bestEnd = 0, bestLen = 0;

    for (const auto& r : rules)
    {
        const std::string pat = r.pattern;
        for (size_t p = 1; p + pat.size() <= s.size(); ++p)
        {
            if (s[p - 1] != ' ') continue;   // tiene que empezar una palabra
            if (! matchAt (s, p, pat, r.wholeWord)) continue;

            // fin de la palabra donde termina la coincidencia
            size_t e = p + pat.size();
            while (e < s.size() && s[e] != ' ') ++e;

            if (best < 0 || e > bestEnd || (e == bestEnd && pat.size() > bestLen))
            {
                best = r.category; bestEnd = e; bestLen = pat.size();
            }
        }
    }
    return best;
}

int categoryFromGM (int bank, int program)
{
    if (bank == 128) return DrumKits;
    const int p = program & 127;
    if (p <= 3)   return Pianos;
    if (p <= 7)   return Keys;
    if (p <= 15)  return Bells;
    if (p <= 23)  return Keys;
    if (p <= 31)  return Guitars;
    if (p <= 39)  return Bass;
    if (p <= 46)  return Strings;
    if (p == 47)  return Percussion;
    if (p <= 51)  return Strings;
    if (p <= 54)  return Voices;
    if (p == 55)  return FX;
    if (p <= 63)  return Brass;
    if (p <= 79)  return Woodwinds;
    if (p <= 87)  return Leads;
    if (p <= 103) return Pads;
    if (p <= 111) return Ethnic;
    if (p == 112 || p == 114) return Bells;
    if (p <= 118) return Percussion;
    return FX;
}

bool isJunkName (const std::string& n)
{
    int alnum = 0;
    for (char c : n) if (std::isalnum ((unsigned char) c)) ++alnum;
    if (alnum == 0) return true;
    std::string l; for (char c : n) l += (char) std::tolower ((unsigned char) c);
    if (l.rfind ("unused", 0) == 0) return true;
    return l == "eop" || l == "eoi" || l == "eos" || l == "unused" || l == "empty" || l == "untitled" || l == "null";
}

std::vector<int> categorizeFile (const std::vector<RawPreset>& presets)
{
    std::vector<int> byName (presets.size()), out (presets.size());
    int named = 0, agree = 0;

    for (size_t i = 0; i < presets.size(); ++i)
    {
        byName[i] = categoryFromName (presets[i].name);
        if (byName[i] >= 0 && presets[i].bank < 120)
        {
            ++named;
            const int gm = categoryFromGM (presets[i].bank, presets[i].program);
            if (gm == byName[i]
                || (gm == Pads && byName[i] == Leads) || (gm == Leads && (byName[i] == Chip || byName[i] == Pads))
                || (gm == Keys && byName[i] == Pianos) || (gm == Pianos && byName[i] == Keys))
                ++agree;
        }
    }
    // Si los nombres coinciden bastante con el numero GM, el archivo sigue el estandar
    // y se puede usar el numero de programa para los presets sin nombre claro.
    const bool gmLike = named >= 8 && agree * 100 >= named * 45;

    for (size_t i = 0; i < presets.size(); ++i)
    {
        const auto& p = presets[i];
        if (p.bank == 128 && (byName[i] < 0 || byName[i] == Percussion || byName[i] == Bass))
                                              out[i] = DrumKits;           // banco 128 = baterias
        else if (byName[i] >= 0)              out[i] = byName[i];
        else if (gmLike)                      out[i] = categoryFromGM (p.bank, p.program);
        else                                  out[i] = Other;
    }
    return out;
}

//==============================================================================
// Extraccion de un preset a un .sf2 minimo en memoria.
namespace
{
void put32 (std::vector<uint8_t>& v, uint32_t x) { for (int i = 0; i < 4; ++i) v.push_back ((uint8_t) (x >> (8 * i))); }
void put16 (std::vector<uint8_t>& v, uint16_t x) { v.push_back ((uint8_t) x); v.push_back ((uint8_t) (x >> 8)); }
void set16 (uint8_t* p, uint16_t x) { p[0] = (uint8_t) x; p[1] = (uint8_t) (x >> 8); }
void set32 (uint8_t* p, uint32_t x) { for (int i = 0; i < 4; ++i) p[i] = (uint8_t) (x >> (8 * i)); }

void addChunk (std::vector<uint8_t>& out, const char* id, const std::vector<uint8_t>& data)
{
    out.insert (out.end(), id, id + 4);
    put32 (out, (uint32_t) data.size());
    out.insert (out.end(), data.begin(), data.end());
    if (data.size() & 1) out.push_back (0);
}

std::vector<uint8_t> makeList (const char* type, const std::vector<uint8_t>& body)
{
    std::vector<uint8_t> v (type, type + 4);
    v.insert (v.end(), body.begin(), body.end());
    return v;
}
} // namespace

bool extractPreset (const std::string& path, int phdrIndex, std::vector<uint8_t>& outSf2, std::string& error)
{
    outSf2.clear();
    FilePtr f (fopen (path.c_str(), "rb"));
    if (! f) { error = "No se pudo abrir el archivo"; return false; }

    Hydra h;
    if (! readHydra (f.get(), h, false) || h.smplOffset < 0)
    { error = "Soundfont invalido"; return false; }

    const size_t nPhdr = h.phdr.size() / 38, nPbag = h.pbag.size() / 4, nPgen = h.pgen.size() / 4, nPmod = h.pmod.size() / 10;
    const size_t nInst = h.inst.size() / 22, nIbag = h.ibag.size() / 4, nIgen = h.igen.size() / 4, nImod = h.imod.size() / 10;
    const size_t nShdr = h.shdr.size() / 46;
    const uint32_t smplCount = h.smplBytes / 2;

    if (phdrIndex < 0 || (size_t) phdrIndex + 1 >= nPhdr || nPbag < 1 || nInst < 1 || nIbag < 1 || nShdr < 1)
    { error = "Preset no encontrado"; return false; }

    auto clampIdx = [] (size_t v, size_t n) { return std::min (v, n); };

    std::vector<uint8_t> phdr, pbag, pmod, pgen, inst, ibag, imod, igen, shdr, smpl;
    std::map<int, int> instMap, smplMap;
    std::vector<int> instOrder, smplOrder;

    // ---- preset
    const uint8_t* ph = h.phdr.data() + (size_t) phdrIndex * 38;
    const uint8_t* phNext = ph + 38;
    size_t b0 = clampIdx (rd16 (ph + 24), nPbag - 1), b1 = clampIdx (rd16 (phNext + 24), nPbag - 1);
    if (b1 < b0) b1 = b0;

    for (size_t b = b0; b < b1; ++b)
    {
        put16 (pbag, (uint16_t) (pgen.size() / 4));
        put16 (pbag, (uint16_t) (pmod.size() / 10));

        size_t g0 = clampIdx (rd16 (h.pbag.data() + b * 4), nPgen), g1 = clampIdx (rd16 (h.pbag.data() + (b + 1) * 4), nPgen);
        for (size_t g = g0; g < g1; ++g)
        {
            const uint8_t* gp = h.pgen.data() + g * 4;
            uint16_t oper = rd16 (gp), amount = rd16 (gp + 2);
            if (oper == 41)   // instrument
            {
                if (amount + 1u >= nInst) continue;
                auto it = instMap.find (amount);
                if (it == instMap.end()) { it = instMap.emplace (amount, (int) instOrder.size()).first; instOrder.push_back (amount); }
                amount = (uint16_t) it->second;
            }
            put16 (pgen, oper); put16 (pgen, amount);
        }
        size_t m0 = clampIdx (rd16 (h.pbag.data() + b * 4 + 2), nPmod), m1 = clampIdx (rd16 (h.pbag.data() + (b + 1) * 4 + 2), nPmod);
        for (size_t m = m0; m < m1; ++m) pmod.insert (pmod.end(), h.pmod.data() + m * 10, h.pmod.data() + m * 10 + 10);
    }
    if (instOrder.empty()) { error = "El preset no tiene instrumentos"; return false; }

    // ---- instrumentos
    for (int oldI : instOrder)
    {
        const uint8_t* ir = h.inst.data() + (size_t) oldI * 22;
        std::vector<uint8_t> rec (ir, ir + 22);
        set16 (rec.data() + 20, (uint16_t) (ibag.size() / 4));
        inst.insert (inst.end(), rec.begin(), rec.end());

        size_t ib0 = clampIdx (rd16 (ir + 20), nIbag - 1), ib1 = clampIdx (rd16 (ir + 22 + 20), nIbag - 1);
        if (ib1 < ib0) ib1 = ib0;
        for (size_t b = ib0; b < ib1; ++b)
        {
            put16 (ibag, (uint16_t) (igen.size() / 4));
            put16 (ibag, (uint16_t) (imod.size() / 10));
            size_t g0 = clampIdx (rd16 (h.ibag.data() + b * 4), nIgen), g1 = clampIdx (rd16 (h.ibag.data() + (b + 1) * 4), nIgen);
            for (size_t g = g0; g < g1; ++g)
            {
                const uint8_t* gp = h.igen.data() + g * 4;
                uint16_t oper = rd16 (gp), amount = rd16 (gp + 2);
                if (oper == 53)   // sampleID
                {
                    if (amount + 1u >= nShdr) continue;
                    const uint8_t* sr = h.shdr.data() + (size_t) amount * 46;
                    uint32_t s = rd32 (sr + 20), e = rd32 (sr + 24);
                    if (e > smplCount) e = smplCount;
                    if (s >= e || (rd16 (sr + 44) & 0x8000)) continue;   // vacia o en ROM
                    auto it = smplMap.find (amount);
                    if (it == smplMap.end()) { it = smplMap.emplace (amount, (int) smplOrder.size()).first; smplOrder.push_back (amount); }
                    amount = (uint16_t) it->second;
                }
                put16 (igen, oper); put16 (igen, amount);
            }
            size_t m0 = clampIdx (rd16 (h.ibag.data() + b * 4 + 2), nImod), m1 = clampIdx (rd16 (h.ibag.data() + (b + 1) * 4 + 2), nImod);
            for (size_t m = m0; m < m1; ++m) imod.insert (imod.end(), h.imod.data() + m * 10, h.imod.data() + m * 10 + 10);
        }
    }
    if (smplOrder.empty()) { error = "El preset no tiene muestras"; return false; }

    // ---- muestras
    uint64_t totalFrames = 0;
    for (int oldS : smplOrder)
    {
        const uint8_t* sr = h.shdr.data() + (size_t) oldS * 46;
        uint32_t s = rd32 (sr + 20), e = std::min (rd32 (sr + 24), smplCount);
        totalFrames += (e - s) + 46;
    }
    if (totalFrames > 200ull * 1024 * 1024) { error = "Preset demasiado grande"; return false; }
    smpl.reserve ((size_t) totalFrames * 2);

    for (int oldS : smplOrder)
    {
        const uint8_t* sr = h.shdr.data() + (size_t) oldS * 46;
        uint32_t s = rd32 (sr + 20), e = std::min (rd32 (sr + 24), smplCount);
        uint32_t ls = rd32 (sr + 28), le = rd32 (sr + 32);
        const uint32_t len = e - s;
        const uint32_t newStart = (uint32_t) (smpl.size() / 2);

        const size_t off = smpl.size();
        smpl.resize (off + (size_t) len * 2);
        tmu_fseek (f.get(), h.smplOffset + (tmu_off) s * 2, SEEK_SET);
        if (fread (smpl.data() + off, 1, (size_t) len * 2, f.get()) != (size_t) len * 2)
        { error = "Error leyendo muestras"; return false; }
        smpl.resize (smpl.size() + 46 * 2, 0);   // relleno de silencio que pide el formato

        auto rel = [&] (uint32_t x) { x = std::max (s, std::min (x, e)); return newStart + (x - s); };
        std::vector<uint8_t> rec (sr, sr + 46);
        set32 (rec.data() + 20, newStart);
        set32 (rec.data() + 24, newStart + len);
        set32 (rec.data() + 28, rel (ls));
        set32 (rec.data() + 32, rel (le));
        uint16_t link = rd16 (sr + 42), type = rd16 (sr + 44);
        auto lit = smplMap.find (link);
        if ((type & 0x0E) != 0 && lit != smplMap.end()) link = (uint16_t) lit->second;
        else { link = 0; type = 1; }
        set16 (rec.data() + 42, link);
        set16 (rec.data() + 44, type);
        shdr.insert (shdr.end(), rec.begin(), rec.end());
    }

    // ---- registros finales (terminadores)
    {
        std::vector<uint8_t> rec (ph, ph + 38);
        set16 (rec.data() + 20, 0);   // programa 0
        set16 (rec.data() + 22, 0);   // banco 0
        set16 (rec.data() + 24, 0);
        phdr.insert (phdr.end(), rec.begin(), rec.end());
        std::vector<uint8_t> eop (38, 0);
        memcpy (eop.data(), "EOP", 3);
        set16 (eop.data() + 24, (uint16_t) (pbag.size() / 4));
        phdr.insert (phdr.end(), eop.begin(), eop.end());
    }
    put16 (pbag, (uint16_t) (pgen.size() / 4)); put16 (pbag, (uint16_t) (pmod.size() / 10));
    pmod.resize (pmod.size() + 10, 0);
    pgen.resize (pgen.size() + 4, 0);
    {
        std::vector<uint8_t> eoi (22, 0);
        memcpy (eoi.data(), "EOI", 3);
        set16 (eoi.data() + 20, (uint16_t) (ibag.size() / 4));
        inst.insert (inst.end(), eoi.begin(), eoi.end());
    }
    put16 (ibag, (uint16_t) (igen.size() / 4)); put16 (ibag, (uint16_t) (imod.size() / 10));
    imod.resize (imod.size() + 10, 0);
    igen.resize (igen.size() + 4, 0);
    {
        std::vector<uint8_t> eos (46, 0);
        memcpy (eos.data(), "EOS", 3);
        shdr.insert (shdr.end(), eos.begin(), eos.end());
    }
    if (pgen.size() / 4 > 65535 || igen.size() / 4 > 65535 || ibag.size() / 4 > 65535)
    { error = "Preset demasiado complejo"; return false; }

    // ---- armar el RIFF
    std::vector<uint8_t> info, sdta, pdta;
    { std::vector<uint8_t> ifil; put16 (ifil, 2); put16 (ifil, 1); addChunk (info, "ifil", ifil); }
    { std::vector<uint8_t> isng { 'E','M','U','8','0','0','0',0 }; addChunk (info, "isng", isng); }
    { std::vector<uint8_t> inam { 'T','M','U',0 }; addChunk (info, "INAM", inam); }
    addChunk (sdta, "smpl", smpl);
    addChunk (pdta, "phdr", phdr); addChunk (pdta, "pbag", pbag); addChunk (pdta, "pmod", pmod);
    addChunk (pdta, "pgen", pgen); addChunk (pdta, "inst", inst); addChunk (pdta, "ibag", ibag);
    addChunk (pdta, "imod", imod); addChunk (pdta, "igen", igen); addChunk (pdta, "shdr", shdr);

    std::vector<uint8_t> body { 's','f','b','k' };
    addChunk (body, "LIST", makeList ("INFO", info));
    addChunk (body, "LIST", makeList ("sdta", sdta));
    addChunk (body, "LIST", makeList ("pdta", pdta));

    outSf2.reserve (body.size() + 8);
    addChunk (outSf2, "RIFF", body);
    return true;
}
} // namespace tmu
