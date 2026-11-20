#include "KeyAnalysis.h"
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <algorithm>

using namespace juce;

// Port of the tonal part of Groovulation's analysis (Source/dsp/Analysis.cpp): same constants, same algorithm.
namespace keyanalysis
{
namespace
{
    constexpr double kTargetSr = 22050.0;
    constexpr int kOrder = 11, kN = 1 << kOrder, kHop = 512;
    constexpr double kPi = 3.14159265358979323846;

    // Krumhansl-Kessler and Temperley (Kostka-Payne) key profiles
    const float kkMaj[12] { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
    const float kkMin[12] { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };
    const float tpMaj[12] { 0.748f, 0.060f, 0.488f, 0.082f, 0.670f, 0.460f, 0.096f, 0.715f, 0.104f, 0.366f, 0.057f, 0.400f };
    const float tpMin[12] { 0.712f, 0.084f, 0.474f, 0.618f, 0.049f, 0.460f, 0.105f, 0.747f, 0.404f, 0.067f, 0.133f, 0.330f };

    std::array<float, 12> zNorm (const float* p)
    {
        std::array<float, 12> r {};
        float m = 0; for (int i = 0; i < 12; ++i) m += p[i]; m /= 12;
        float s = 0; for (int i = 0; i < 12; ++i) s += (p[i] - m) * (p[i] - m); s = std::sqrt (s / 12) + 1e-9f;
        for (int i = 0; i < 12; ++i) r[(size_t) i] = (p[i] - m) / s;
        return r;
    }

    float corrKey (const std::array<float, 12>& chroma, int root, bool minor)
    {
        static const auto a = zNorm (kkMaj), b = zNorm (kkMin), c = zNorm (tpMaj), d = zNorm (tpMin);
        const auto& p1 = minor ? b : a; const auto& p2 = minor ? d : c;
        float m = 0; for (auto v : chroma) m += v; m /= 12;
        float s = 0; for (auto v : chroma) s += (v - m) * (v - m); s = std::sqrt (s / 12) + 1e-9f;
        float r = 0;
        for (int i = 0; i < 12; ++i)
        {
            const float x = (chroma[(size_t) ((i + root) % 12)] - m) / s;
            r += x * 0.5f * (p1[(size_t) i] + p2[(size_t) i]);
        }
        return r / 12.0f;
    }

    String lowerWords (const String& s)
    {
        String out;
        juce_wchar prev = 0;
        for (auto ch : s)
        {
            if (! CharacterFunctions::isLetterOrDigit (ch)) { out << ' '; prev = ' '; continue; }
            if (CharacterFunctions::isUpperCase (ch) && CharacterFunctions::isLowerCase (prev)) out << ' ';
            if (CharacterFunctions::isDigit (ch) != CharacterFunctions::isDigit (prev) && prev != ' ' && prev != 0) out << ' ';
            out << CharacterFunctions::toLowerCase (ch);
            prev = ch;
        }
        return " " + out + " ";
    }
    bool hasWord (const String& text, const char* w) { return text.contains (" " + String (w) + " "); }

    float median (std::vector<float> v)
    {
        if (v.empty()) return 0.0f;
        std::nth_element (v.begin(), v.begin() + (long) v.size() / 2, v.end());
        return v[v.size() / 2];
    }

    // name type as in Groovulation's keyword classifier: 1 = bass, 0 = other tonal (pad / chords / synth), -1 = unknown
    int tonalNameType (const String& name)
    {
        static const char* bassW[]  { "bass", "basses", "bassline", "basslines", "sub", "subs", "808", "reese", "bs" };
        static const char* padW[]   { "pad", "pads", "atmo", "atmos", "atmosphere", "ambient", "ambience", "drone", "drones", "string", "strings", "texture", "textures", "swell", "swells", "soundscape" };
        static const char* chordW[] { "chord", "chords", "stab", "stabs", "keys", "key", "piano", "pianos", "rhodes", "organ", "organs", "guitar", "guitars", "gtr", "epiano", "ep", "wurli", "wurlitzer", "brass", "horn", "horns", "clav" };
        static const char* synthW[] { "synth", "synths", "lead", "leads", "arp", "arps", "arpeggio", "pluck", "plucks", "seq", "sequence", "melody", "melodies", "mel", "bell", "bells", "hook", "riff", "riffs", "melodic" };
        const String t = lowerWords (name);
        auto anyIn = [&t] (const char* const* list, size_t n) { for (size_t i = 0; i < n; ++i) if (hasWord (t, list[i])) return true; return false; };
        if (anyIn (bassW, std::size (bassW))) return 1;
        if (anyIn (padW, std::size (padW)) || anyIn (chordW, std::size (chordW)) || anyIn (synthW, std::size (synthW))) return 0;
        return -1;
    }
}

bool nameSaysBass (const String& fileName) { return tonalNameType (fileName) == 1; }

bool keyFromName (const String& fileName, int& rootOut, bool& minorOut)
{
    StringArray tokens;
    tokens.addTokens (fileName.replaceCharacters ("_-.,()[]{}+&'", "             "), " ", "");
    tokens.removeEmptyStrings();
    for (int ti = tokens.size(); --ti >= 0;)   // the key is usually at the end of the name
    {
        String t = tokens[ti].trim();
        int pc = -1, len = 0;
        static const std::pair<const char*, int> solf[] { { "sol", 7 }, { "do", 0 }, { "re", 2 }, { "mi", 4 }, { "fa", 5 }, { "la", 9 }, { "si", 11 } };
        for (auto& sf : solf)
        {
            const int L = (int) strlen (sf.first);
            if (t.startsWithIgnoreCase (sf.first) && (t.length() == L || ! CharacterFunctions::isLetter (t[L])
                                                      || t.substring (L).startsWithIgnoreCase ("m") || t[L] == 'b'))
            { pc = sf.second; len = L; break; }
        }
        if (pc < 0)
        {
            const juce_wchar c = t[0], up = CharacterFunctions::toUpperCase (c);
            if (up >= 'A' && up <= 'G' && (c == up || t.length() > 1))
            {
                static const int base[] { 9, 11, 0, 2, 4, 5, 7 };
                pc = base[up - 'A']; len = 1;
                if (c != up && ! (t[1] == '#' || t[1] == 'b' || t[1] == 'm' || CharacterFunctions::isDigit (t[1]))) pc = -1;
            }
        }
        if (pc < 0) continue;
        String rest = t.substring (len);
        if (rest.startsWith ("#") || rest.startsWith (String::fromUTF8 ("\xe2\x99\xaf"))) { ++pc; rest = rest.substring (1); }
        else if (rest.startsWithIgnoreCase ("sharp")) { ++pc; rest = rest.substring (5); }
        else if (rest.startsWithIgnoreCase ("flat")) { --pc; rest = rest.substring (4); }
        else if (rest.startsWith (String::fromUTF8 ("\xe2\x99\xad"))) { --pc; rest = rest.substring (1); }
        else if (rest.startsWith ("b") && ! rest.startsWithIgnoreCase ("bass") && ! rest.startsWithIgnoreCase ("bpm")) { --pc; rest = rest.substring (1); }
        // quality: m / min / minor / minore -> minor; maj / major / maggiore / M / nothing -> major; digits & extensions allowed
        bool minor = false;
        const String q = rest;
        if (q.startsWith ("maj") || q.startsWithIgnoreCase ("major") || q.startsWithIgnoreCase ("magg") || q.startsWith ("M")) minor = false;
        else if (q.startsWithIgnoreCase ("minor") || q.startsWithIgnoreCase ("minore") || q.startsWithIgnoreCase ("min") || q.startsWith ("m")) minor = true;
        static const char* okWords[] { "minore", "minor", "min", "maggiore", "magg", "major", "maj", "m", "M", "dim", "aug", "sus", "add" };
        String chk = q;
        while (chk.isNotEmpty())
        {
            bool matched = false;
            for (auto* w : okWords) if (chk.startsWith (w)) { chk = chk.substring ((int) strlen (w)); matched = true; break; }
            if (! matched && (CharacterFunctions::isDigit (chk[0]) || chk[0] == '#' || chk[0] == 'b' || chk[0] == 'o')) { chk = chk.substring (1); matched = true; }
            if (! matched) break;
        }
        if (chk.isNotEmpty()) continue;
        if (ti + 1 < tokens.size())
        {
            const auto nx = tokens[ti + 1].toLowerCase();
            if (nx == "minor" || nx == "min" || nx == "minore" || nx == "m") minor = true;
        }
        // a lone capital letter at the start of the name is too ambiguous ("A loop") unless it is the last token
        if (len == 1 && rest.isEmpty() && ti == 0 && tokens.size() > 1) continue;
        rootOut = (pc % 12 + 12) % 12;
        minorOut = minor;
        return true;
    }
    return false;
}

String keyName (int root, bool minor)
{
    if (root < 0) return "-";
    static const char* n[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return String (n[root % 12]) + (minor ? " min" : " maj");
}

//==============================================================================
Result analyse (const std::vector<float>& Lch, const std::vector<float>& Rch, double sampleRate, const String& name)
{
    Result r;
    { int root; bool mi; if (keyFromName (name, root, mi)) { r.nameRoot = root; r.nameMinor = mi; } }
    const int len = (int) Lch.size();
    if (len < 64 || sampleRate <= 0) return r;

    // ---- mono mix, low-pass + decimate to ~22 kHz
    std::vector<float> mono ((size_t) len);
    for (int i = 0; i < len; ++i) mono[(size_t) i] = Rch.empty() ? Lch[(size_t) i] : 0.5f * (Lch[(size_t) i] + Rch[(size_t) i]);
    const int dec = jmax (1, (int) std::floor (sampleRate / kTargetSr + 0.25));
    const double sr = sampleRate / dec;
    std::vector<float> x;
    if (dec > 1)
    {
        IIRFilter f1, f2;
        f1.setCoefficients (IIRCoefficients::makeLowPass (sampleRate, sr * 0.45, 0.54));
        f2.setCoefficients (IIRCoefficients::makeLowPass (sampleRate, sr * 0.45, 1.31));
        f1.processSamples (mono.data(), len);
        f2.processSamples (mono.data(), len);
        x.resize ((size_t) (len / dec));
        for (size_t i = 0; i < x.size(); ++i) x[i] = mono[i * (size_t) dec];
    }
    else x = std::move (mono);
    const int n = (int) x.size();
    float peak = 0; for (auto v : x) peak = jmax (peak, std::abs (v));
    if (peak < 1e-5f) { r.ok = true; return r; }
    for (auto& v : x) v /= peak;

    // ---- band energies (STFT, 2048 points) - used to recognise basses
    double eLow = 0, eMid = 0, eHigh = 0;
    {
        dsp::FFT fft (kOrder);
        std::vector<float> win (kN), fbuf (2 * kN);
        for (int i = 0; i < kN; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * (float) kPi * i / kN);
        const double binHz = sr / kN;
        const int frames = jmax (1, (n - kN) / kHop + 1);
        for (int f = 0; f < frames; ++f)
        {
            const int off = f * kHop;
            for (int i = 0; i < kN; ++i) fbuf[(size_t) i] = ((off + i < n) ? x[(size_t) (off + i)] : 0.0f) * win[(size_t) i];
            std::fill (fbuf.begin() + kN, fbuf.end(), 0.0f);
            fft.performFrequencyOnlyForwardTransform (fbuf.data(), true);
            for (int k = 0; k < kN / 2 + 1; ++k)
            {
                const double p = (double) fbuf[(size_t) k] * fbuf[(size_t) k];
                const double hzk = k * binHz;
                if (hzk < 150) eLow += p; else if (hzk < 4000) eMid += p; else eHigh += p;
            }
        }
    }

    // ---- high-resolution spectral peaks (8192-point FFT, ~2.7 Hz bins) 55 Hz .. 5 kHz -> pitch-class evidence
    struct Peak { float midi, w; };
    std::vector<Peak> peaks;
    {
        constexpr int cOrder = 13, cN = 1 << cOrder, cHop = 2048;
        dsp::FFT cfft (cOrder);
        std::vector<float> cw (cN), cb (2 * cN);
        for (int i = 0; i < cN; ++i) cw[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * (float) kPi * i / cN);
        const double cBin = sr / cN;
        // (as in Groovulation; very short one-shots still get one analysis window)
        for (int off = 0; off == 0 || off + cN / 2 < n; off += cHop)
        {
            float e = 0;
            for (int i = 0; i < cN; ++i) { const float v = off + i < n ? x[(size_t) (off + i)] : 0.0f; cb[(size_t) i] = v * cw[(size_t) i]; e += v * v; }
            if (std::sqrt (e / cN) < 0.003f) continue;
            std::fill (cb.begin() + cN, cb.end(), 0.0f);
            cfft.performFrequencyOnlyForwardTransform (cb.data(), true);
            float mx = 0; for (int k = 1; k < cN / 2; ++k) mx = jmax (mx, cb[(size_t) k]);
            for (int k = jmax (2, (int) (55 / cBin)); k < (int) (5000 / cBin) && k + 1 < cN / 2; ++k)
            {
                const float a = cb[(size_t) k - 1], b = cb[(size_t) k], c = cb[(size_t) k + 1];
                if (b > a && b >= c && b > mx * 0.01f)
                {
                    const float d = 0.5f * (a - c) / (a - 2 * b + c + 1e-12f);   // parabolic interpolation
                    const float hzp = (float) ((k + d) * cBin);
                    peaks.push_back ({ 69.0f + 12.0f * std::log2 (hzp / 440.0f), b });
                }
            }
        }
    }

    // ---- YIN f0 on a coarser hop (FFT-based difference function)
    std::vector<float> f0s, f0w;
    {
        constexpr int W = 1024, tauMax = 1024, L = W + tauMax, fo = 12, FN = 1 << fo;
        dsp::FFT cfft (fo);
        std::vector<dsp::Complex<float>> A (FN), B (FN), C (FN), Af (FN), Bf (FN), Ct (FN);
        const int tauMin = (int) (sr / 1000.0), tauHi = jmin (tauMax - 2, (int) (sr / 30.0));
        std::vector<float> d (tauMax), cmnd (tauMax);
        int voicedCount = 0, total = 0;
        for (int off = 0; off + L < n; off += 2048)
        {
            float e = 0; for (int i = 0; i < W; ++i) e += x[(size_t) (off + i)] * x[(size_t) (off + i)];
            if (std::sqrt (e / W) < 0.01f) continue;
            ++total;
            for (int i = 0; i < FN; ++i) { A[(size_t) i] = { i < W ? x[(size_t) (off + i)] : 0.0f, 0.0f }; B[(size_t) i] = { i < L ? x[(size_t) (off + i)] : 0.0f, 0.0f }; }
            cfft.perform (A.data(), Af.data(), false);
            cfft.perform (B.data(), Bf.data(), false);
            for (int i = 0; i < FN; ++i) Ct[(size_t) i] = std::conj (Af[(size_t) i]) * Bf[(size_t) i];
            cfft.perform (Ct.data(), C.data(), true);
            const float scale = std::abs (C[0].real()) > 1e-12f ? e / C[0].real() : 0.0f;
            for (int i = 0; i < tauMax; ++i) C[(size_t) i] *= scale;
            float eTau = e;
            d[0] = 0;
            for (int tau = 1; tau < tauMax; ++tau)
            {
                eTau += x[(size_t) (off + tau + W - 1)] * x[(size_t) (off + tau + W - 1)] - x[(size_t) (off + tau - 1)] * x[(size_t) (off + tau - 1)];
                d[(size_t) tau] = jmax (0.0f, e + eTau - 2.0f * C[(size_t) tau].real());
            }
            float run = 0; cmnd[0] = 1;
            for (int tau = 1; tau < tauMax; ++tau) { run += d[(size_t) tau]; cmnd[(size_t) tau] = d[(size_t) tau] * tau / (run + 1e-12f); }
            int best = -1;
            for (int tau = jmax (2, tauMin); tau < tauHi; ++tau)
                if (cmnd[(size_t) tau] < 0.15f) { while (tau + 1 < tauHi && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau]) ++tau; best = tau; break; }
            if (best < 0)
            {
                float mn = 1e9f; int at = -1;
                for (int tau = jmax (2, tauMin); tau < tauHi; ++tau) if (cmnd[(size_t) tau] < mn) { mn = cmnd[(size_t) tau]; at = tau; }
                if (mn < 0.3f) best = at;
            }
            if (best > 1)
            {
                const float a = cmnd[(size_t) best - 1], b = cmnd[(size_t) best], c = cmnd[(size_t) best + 1];
                const float tf = (float) best + 0.5f * (a - c) / (a - 2 * b + c + 1e-12f);
                f0s.push_back ((float) (sr / tf));
                f0w.push_back ((1.0f - jmin (1.0f, b)) * std::sqrt (e / W));
                ++voicedCount;
            }
        }
        r.voiced = total > 0 ? (float) voicedCount / (float) total : 0.0f;
        if (! f0s.empty()) r.f0 = median (f0s);
    }

    // ---- tuning + chroma + key
    {
        double sx = 0, sy = 0;
        for (auto& p : peaks) { const double dev = p.midi - std::round (p.midi); sx += p.w * std::cos (2 * kPi * dev); sy += p.w * std::sin (2 * kPi * dev); }
        r.tuneCents = peaks.empty() ? 0.0f : (float) (std::atan2 (sy, sx) / (2 * kPi) * 100.0);
        std::array<double, 12> ch {};
        // harmonic-sum pitch-class profile: every peak also votes for the fundamentals it may belong to
        for (auto& p : peaks)
        {
            const double w = (double) p.w;
            for (int h = 1; h <= 4; ++h)
            {
                const double m = p.midi - 12.0 * std::log2 ((double) h) - r.tuneCents / 100.0;
                if (h > 1 && m < 28) break;
                const double hw = std::pow (0.6, h - 1);
                const int pc = ((int) std::lround (m) % 12 + 12) % 12;
                ch[(size_t) pc] += w * hw;
            }
        }
        std::array<double, 12> fh {};
        double fsum = 0;
        for (size_t i = 0; i < f0s.size(); ++i)
        {
            const double midi = 69.0 + 12.0 * std::log2 (f0s[i] / 440.0) - r.tuneCents / 100.0;
            fh[(size_t) (((int) std::lround (midi) % 12 + 12) % 12)] += f0w[i];
            fsum += f0w[i];
        }
        double csum = 0; for (auto v : ch) csum += v;
        if (csum <= 0 && fsum <= 0) { r.ok = true; return r; }   // no tonal evidence: atonal
        const double lowShare = eLow / (eLow + eMid + eHigh + 1e-12);
        const int nameType = tonalNameType (name);
        const bool bassy = r.f0 > 0 && r.f0 < 250 && r.voiced > 0.3f && (nameType == 1 || (nameType < 0 && lowShare > 0.35));
        const double fWeight = bassy ? 0.6 : 0.0;   // f0 of polyphonic material is unreliable
        for (int i = 0; i < 12; ++i)
        {
            const double a = csum > 0 ? ch[(size_t) i] / csum : 0.0;
            const double b = fsum > 0 ? fh[(size_t) i] / fsum : 0.0;
            r.chroma[(size_t) i] = (float) (fsum > 0 ? (1 - fWeight) * a + fWeight * b : a);
        }
        float best = -1e9f, second = -1e9f; int bestRoot = 0; bool bestMinor = false;
        for (int root = 0; root < 12; ++root)
            for (int mi = 0; mi < 2; ++mi)
            {
                const float c = corrKey (r.chroma, root, mi == 1);
                if (c > best) { second = best; best = c; bestRoot = root; bestMinor = mi == 1; }
                else if (c > second) second = c;
            }
        r.keyRoot = bestRoot; r.minor = bestMinor;
        r.keyConf = jlimit (0.0f, 1.0f, best * 0.6f + (best - second) * 4.0f);
        if (bassy)
        {
            int fr = 0; for (int i = 1; i < 12; ++i) if (fh[(size_t) i] > fh[(size_t) fr]) fr = i;
            if (fsum > 0 && fh[(size_t) fr] / fsum > 0.3)
            {
                const float cmaj = corrKey (r.chroma, fr, false), cmin = corrKey (r.chroma, fr, true);
                if (jmax (cmaj, cmin) > best - 0.25f) { r.keyRoot = fr; r.minor = cmin > cmaj; }
            }
        }
    }
    r.ok = true;
    return r;
}

//==============================================================================
String toString (const Result& r)
{
    StringArray t;
    t.add (String (r.keyRoot)); t.add (r.minor ? "1" : "0"); t.add (String (r.keyConf, 4)); t.add (String (r.tuneCents, 3));
    t.add (String (r.f0, 3)); t.add (String (r.voiced, 4));
    for (auto c : r.chroma) t.add (String (c, 5));
    return t.joinIntoString (" ");
}

bool fromString (const String& s, Result& r)
{
    StringArray t; t.addTokens (s, " ", ""); t.removeEmptyStrings();
    if (t.size() != 18) return false;
    r.keyRoot = t[0].getIntValue(); r.minor = t[1] == "1"; r.keyConf = t[2].getFloatValue(); r.tuneCents = t[3].getFloatValue();
    r.f0 = t[4].getFloatValue(); r.voiced = t[5].getFloatValue();
    for (int i = 0; i < 12; ++i) r.chroma[(size_t) i] = t[6 + i].getFloatValue();
    r.ok = true;
    return true;
}
}
