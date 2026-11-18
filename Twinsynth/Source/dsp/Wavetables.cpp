#include "Wavetables.h"
#include <juce_dsp/juce_dsp.h>

namespace
{
    constexpr int N = WavetableBank::size;
    constexpr int H = N / 2 - 1;          // 1023 harmonics
    constexpr float PI = 3.14159265358979f;

    float sq (float x) { return x * x; }

    // Spectrum from a time-domain cycle (forward FFT) -> magnitude + phase per harmonic
    void spectrumFromTime (juce::dsp::FFT& fft, const std::vector<float>& cycle, std::vector<float>& re, std::vector<float>& im)
    {
        std::vector<float> buf ((size_t) N * 2, 0.0f);
        std::copy (cycle.begin(), cycle.end(), buf.begin());
        fft.performRealOnlyForwardTransform (buf.data());
        for (int h = 0; h <= H; ++h) { re[(size_t) h] = buf[(size_t) (2 * h)]; im[(size_t) h] = buf[(size_t) (2 * h + 1)]; }
        re[0] = im[0] = 0.0f;   // no DC
    }

    // amplitudes of sine harmonics -> complex bins
    void spectrumFromAmps (const std::vector<float>& amp, std::vector<float>& re, std::vector<float>& im)
    {
        for (int h = 0; h <= H; ++h) { re[(size_t) h] = 0.0f; im[(size_t) h] = h == 0 ? 0.0f : -amp[(size_t) h] * (float) N * 0.5f; }
    }

    float formant (float hz, float f1, float f2, float f3)
    {
        auto pk = [] (float x, float c, float bw) { return std::exp (-sq ((x - c) / bw)); };
        return 0.05f + pk (hz, f1, 90.0f) + 0.7f * pk (hz, f2, 120.0f) + 0.35f * pk (hz, f3, 180.0f);
    }

    // Builds one frame (t = table, x = 0..1 position) either as harmonic amplitudes or as a time cycle
    void buildFrame (int t, float x, juce::dsp::FFT& fft, std::vector<float>& re, std::vector<float>& im)
    {
        std::vector<float> amp ((size_t) H + 1, 0.0f);
        std::vector<float> cyc ((size_t) N, 0.0f);
        bool timeDomain = false;
        switch (t)
        {
            case 0: // Analog Morph: sine -> triangle -> saw -> square
            {
                const float seg = x * 3.0f;
                for (int h = 1; h <= H; ++h)
                {
                    const float sine = h == 1 ? 1.0f : 0.0f;
                    const float tri = (h % 2) ? (8.0f / (PI * PI)) / (float) (h * h) * (((h - 1) / 2) % 2 ? -1.0f : 1.0f) : 0.0f;
                    const float saw = (2.0f / PI) / (float) h;
                    const float sqr = (h % 2) ? (4.0f / PI) / (float) h : 0.0f;
                    float a;
                    if (seg < 1.0f) a = sine + (tri - sine) * seg;
                    else if (seg < 2.0f) a = tri + (saw - tri) * (seg - 1.0f);
                    else a = saw + (sqr - saw) * (seg - 2.0f);
                    amp[(size_t) h] = a;
                }
                break;
            }
            case 1: // Pulse Width 50 % -> 4 %
            {
                const float w = 0.5f - 0.46f * x;
                for (int h = 1; h <= H; ++h) amp[(size_t) h] = (4.0f / (PI * (float) h)) * std::sin (PI * (float) h * w);
                break;
            }
            case 2: // PPG-style digital: moving harmonic band over a soft fundamental
            {
                const float c = 2.0f + x * 46.0f, bw = 2.0f + x * 6.0f;
                for (int h = 1; h <= H; ++h)
                    amp[(size_t) h] = (h == 1 ? 0.6f : 0.0f) + std::exp (-sq (((float) h - c) / bw)) * (1.0f / std::sqrt ((float) h)) * 1.4f;
                break;
            }
            case 3: // Vocal a-e-i-o-u (formants on a 110 Hz saw)
            {
                static const float F[5][3] = { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 }, { 570, 840, 2410 }, { 300, 870, 2240 } };
                const float p = x * 4.0f; const int i0 = std::min (3, (int) p); const float fr = p - (float) i0;
                float f1 = F[i0][0] + (F[i0 + 1][0] - F[i0][0]) * fr, f2 = F[i0][1] + (F[i0 + 1][1] - F[i0][1]) * fr, f3 = F[i0][2] + (F[i0 + 1][2] - F[i0][2]) * fr;
                for (int h = 1; h <= H; ++h) amp[(size_t) h] = (1.0f / (float) h) * formant (110.0f * (float) h, f1, f2, f3) * 2.0f;
                break;
            }
            case 4: // Bell / metallic partial sets
            {
                static const int P[] = { 1, 3, 5, 7, 9, 11, 14, 17, 19, 23, 27, 31, 37 };
                for (int k = 0; k < 13; ++k)
                    amp[(size_t) P[k]] = std::pow (0.78f - 0.35f * (1.0f - x), (float) k) * (k == 0 ? 1.0f : 0.8f);
                break;
            }
            case 5: // Organ registrations (16', 8', 5 1/3', 4', 2 2/3', 2', 1 3/5', 1 1/3', 1')
            {
                static const int hs[9] = { 1, 2, 3, 4, 6, 8, 10, 12, 16 };
                static const float reg[4][9] = { { 8, 8, 8, 0, 0, 0, 0, 0, 0 }, { 8, 8, 8, 8, 0, 0, 0, 0, 0 },
                                                 { 8, 6, 8, 6, 4, 4, 0, 3, 2 }, { 8, 8, 8, 8, 8, 8, 8, 8, 8 } };
                const float p = x * 3.0f; const int i0 = std::min (2, (int) p); const float fr = p - (float) i0;
                for (int k = 0; k < 9; ++k) amp[(size_t) hs[k]] = (reg[i0][k] + (reg[i0 + 1][k] - reg[i0][k]) * fr) / 8.0f;
                break;
            }
            case 6: // Hollow: odd harmonics, brightness grows
                for (int h = 1; h <= H; h += 2) amp[(size_t) h] = std::pow ((float) h, -(2.2f - 1.4f * x));
                break;
            case 7: // Hard-sync sweep (slave ratio 1 -> 8)
            {
                timeDomain = true;
                const float r = 1.0f + x * 7.0f;
                for (int i = 0; i < N; ++i) { float ph = (float) i / N * r; ph -= std::floor (ph); cyc[(size_t) i] = 2.0f * ph - 1.0f; }
                break;
            }
            case 8: // Growl: FM sine with rising index (modern bass)
            {
                timeDomain = true;
                const float idx = x * 7.0f;
                for (int i = 0; i < N; ++i) { const float ph = (float) i / N; cyc[(size_t) i] = std::sin (2 * PI * ph + idx * std::sin (2 * PI * 2.0f * ph) + 0.4f * idx * std::sin (2 * PI * 3.0f * ph)); }
                break;
            }
            case 9: // Bit steps: saw quantised to fewer and fewer levels (band-limited)
            {
                timeDomain = true;
                const float levels = std::pow (2.0f, 7.0f - 5.5f * x);
                for (int i = 0; i < N; ++i) { const float s = 2.0f * (float) i / N - 1.0f; cyc[(size_t) i] = std::round (s * levels) / levels; }
                break;
            }
            case 10: // Strings: saw with ensemble comb notches
                for (int h = 1; h <= H; ++h) amp[(size_t) h] = (1.0f / (float) h) * (0.55f + 0.45f * std::cos ((float) h * (0.15f + x * 0.9f)));
                break;
            default: // 11 Reso sweep: saw with a resonant peak that moves up
            {
                const float c = 2.0f + x * 30.0f;
                for (int h = 1; h <= H; ++h) amp[(size_t) h] = (1.0f / (float) h) * (0.4f + 3.0f * std::exp (-sq (((float) h - c) / 1.5f)));
                break;
            }
        }
        if (timeDomain) spectrumFromTime (fft, cyc, re, im);
        else            spectrumFromAmps (amp, re, im);
    }
}

const WavetableBank& WavetableBank::get() { static WavetableBank b; return b; }

WavetableBank::WavetableBank()
{
    data.assign ((size_t) numTables * numFrames * numLevels * (size + 1), 0.0f);
    juce::dsp::FFT fft (11);   // 2048
    std::vector<float> re ((size_t) H + 1), im ((size_t) H + 1), buf ((size_t) N * 2);
    for (int t = 0; t < numTables; ++t)
        for (int f = 0; f < numFrames; ++f)
        {
            buildFrame (t, (float) f / (numFrames - 1), fft, re, im);
            float gain = 1.0f;
            for (int l = 0; l < numLevels; ++l)
            {
                const int maxH = 1024 >> l;
                std::fill (buf.begin(), buf.end(), 0.0f);
                for (int h = 1; h <= std::min (maxH, H); ++h) { buf[(size_t) (2 * h)] = re[(size_t) h]; buf[(size_t) (2 * h + 1)] = im[(size_t) h]; }
                fft.performRealOnlyInverseTransform (buf.data());
                if (l == 0)
                {
                    float pk = 0.0f;
                    for (int i = 0; i < N; ++i) pk = std::max (pk, std::abs (buf[(size_t) i]));
                    gain = pk > 1.0e-9f ? 0.95f / pk : 0.0f;
                }
                float* dst = data.data() + ((size_t) ((t * numFrames + f) * numLevels + l)) * (size + 1);
                for (int i = 0; i < N; ++i) dst[i] = buf[(size_t) i] * gain;
                dst[N] = dst[0];
            }
        }
}
