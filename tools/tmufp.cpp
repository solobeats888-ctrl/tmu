// Huella de audio de cada preset (para encontrar sonidos repetidos).
// Uso: tmufp lista.txt desde budget_seg salida.tsv
//   lista.txt: una ruta .sf2 por linea. Procesa desde el archivo N hasta agotar el tiempo y
//   escribe en stdout "NEXT <n>" (o "NEXT DONE").
#define TSF_IMPLEMENTATION
#include "tsf.h"
#include "TMUCore.h"
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using namespace tmu;

static void fft (std::vector<std::complex<float>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const float ang = -2.0f * (float) M_PI / (float) len;
        const std::complex<float> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<float> w (1);
            for (size_t k = 0; k < len / 2; ++k)
            {
                auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v; a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

int main (int argc, char** argv)
{
    if (argc < 5) return 2;
    std::vector<std::string> files;
    { std::ifstream in (argv[1]); std::string l; while (std::getline (in, l)) if (! l.empty()) files.push_back (l); }
    size_t idx = (size_t) atoi (argv[2]);
    const double budget = atof (argv[3]);
    FILE* out = fopen (argv[4], "a");
    const auto t0 = std::chrono::steady_clock::now();

    const int sr = 22050, N = 1024, bands = 20;
    const int notes[] = { 36, 48, 60, 72 };
    const int onLen = (int) (0.30 * sr), offLen = (int) (0.15 * sr);
    std::vector<float> win (N);
    for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * (float) M_PI * (float) i / (float) N);
    std::vector<int> edges (bands + 1);
    for (int b = 0; b <= bands; ++b) edges[(size_t) b] = (int) std::round (60.0 * std::pow (10000.0 / 60.0, (double) b / bands) * N / sr);

    for (; idx < files.size(); ++idx)
    {
        if (std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() > budget) break;
        const auto& path = files[idx];
        std::vector<RawPreset> ps;
        if (! readPresetList (path, ps)) continue;

        for (auto& p : ps)
        {
            if (isJunkName (p.name)) continue;
            std::vector<uint8_t> buf; std::string err;
            if (! extractPreset (path, p.phdrIndex, buf, err)) { fprintf (out, "%s\t%d\tERR\t%zu\t\n", path.c_str(), p.phdrIndex, (size_t) 0); continue; }
            tsf* t = tsf_load_memory (buf.data(), (int) buf.size());
            if (! t) { fprintf (out, "%s\t%d\tERR\t%zu\t\n", path.c_str(), p.phdrIndex, buf.size()); continue; }
            tsf_set_output (t, TSF_MONO, sr, 0);
            tsf_set_max_voices (t, 64);
            tsf_channel_set_presetindex (t, 0, 0);

            std::vector<float> feat;
            uint64_t h = 1469598103934665603ull;
            double total = 0;
            std::vector<float> seg ((size_t) (onLen + offLen));
            for (int n : notes)
            {
                tsf_channel_note_on (t, 0, n, 0.8f);
                tsf_render_float (t, seg.data(), onLen, 0);
                tsf_channel_note_off (t, 0, n);
                tsf_render_float (t, seg.data() + onLen, offLen, 0);
                tsf_channel_sounds_off_all (t, 0);
                for (float v : seg)
                {
                    total += (double) v * v;
                    const int16_t q = (int16_t) std::lround (std::max (-1.0f, std::min (1.0f, v)) * 8000.0f);
                    h = (h ^ (uint16_t) q) * 1099511628211ull;
                }
                for (size_t f = 0; f + N <= seg.size(); f += N)
                {
                    std::vector<std::complex<float>> a ((size_t) N);
                    for (int i = 0; i < N; ++i) a[(size_t) i] = seg[f + (size_t) i] * win[(size_t) i];
                    fft (a);
                    for (int b = 0; b < bands; ++b)
                    {
                        double e = 0;
                        for (int k = edges[(size_t) b]; k <= std::max (edges[(size_t) b], edges[(size_t) b + 1] - 1); ++k) e += std::norm (a[(size_t) k]);
                        feat.push_back ((float) std::log10 (1e-9 + e));
                    }
                }
            }
            tsf_close (t);
            const double rms = std::sqrt (total / (4.0 * (onLen + offLen)));
            fprintf (out, "%s\t%d\t%016llx\t%zu\t%.6f\t", path.c_str(), p.phdrIndex, (unsigned long long) h, buf.size(), rms);
            for (size_t i = 0; i < feat.size(); ++i) fprintf (out, i ? ",%.2f" : "%.2f", feat[i]);
            fprintf (out, "\n");
        }
        fflush (out);
    }
    fclose (out);
    if (idx >= files.size()) printf ("NEXT DONE\n"); else printf ("NEXT %zu\n", idx);
    return 0;
}
