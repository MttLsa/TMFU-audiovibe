#include "Analysis.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <numeric>
#include <algorithm>

using namespace juce;

namespace analysis
{
namespace
{
    constexpr double kTargetSr = 22050.0;
    constexpr int kOrder = 11, kN = 1 << kOrder, kHop = 512;
    constexpr int kMel = 40, kMfcc = 13;
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
        // separators -> spaces, camelCase split, lower case, padded with spaces for whole-word search
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
}

//==============================================================================
bool nameSaysNoKick (const String& pathText)
{
    const String t = lowerWords (File::createFileWithoutCheckingPath (pathText).getFileNameWithoutExtension());
    return t.contains (" no kick ") || t.contains (" nokick ") || t.contains (" without kick ") || t.contains (" kickless ")
        || t.contains (" no kicks ") || t.contains (" no bd ") || t.contains (" w o kick ");
}

int typeFromName (const String& pathText)
{
    using namespace lb;
    const String t = lowerWords (pathText);
    // order matters: vocals / fx first (they must never land in another module), "bass drum" is a kick,
    // "no kick" loops are bottoms, generic drums -> bottom
    static const char* vocalList[]  { "vocal", "vocals", "vox", "voice", "voices", "acapella", "acappella", "a cappella", "chant", "chants", "choir", "choirs",
                                  "adlib", "adlibs", "spoken", "phrase", "phrases", "sing", "singer", "rap", "hook vocal", "vocoder" };
    static const char* fxList[]     { "fx", "sfx", "riser", "risers", "uplifter", "uplifters", "downlifter", "downlifters", "sweep", "sweeps", "impact", "impacts",
                                  "whoosh", "whooshes", "transition", "transitions", "noise", "noises", "build", "buildup", "reverse", "reversed", "boom", "booms",
                                  "foley", "glitch", "glitches", "zap", "laser", "siren", "effect", "effects" };
    static const char* kickW[]  { "kick", "kicks", "kik", "bd", "bassdrum", "kck" };
    static const char* snareList[]  { "snare", "snares", "snr", "sd", "clap", "claps", "clp", "rim", "rimshot", "snap", "snaps" };
    static const char* topList[]    { "top", "tops", "hat", "hats", "hh", "hihat", "hihats", "hi hat", "hi hats", "cymbal", "cymbals", "ride", "rides", "shaker", "shakers", "oh", "ch", "crash", "openhat" };
    static const char* percList[]   { "perc", "percs", "percussion", "percussions", "conga", "congas", "bongo", "bongos", "tamb", "tambourine", "cowbell", "clave", "claves",
                                  "timbale", "timbales", "djembe", "tabla", "woodblock", "agogo", "guiro", "cabasa" };
    static const char* bassW[]  { "bass", "basses", "bassline", "basslines", "sub", "subs", "808", "reese", "bs" };
    static const char* padW[]   { "pad", "pads", "atmo", "atmos", "atmosphere", "ambient", "ambience", "drone", "drones", "string", "strings", "texture", "textures", "swell", "swells", "soundscape" };
    static const char* chordW[] { "chord", "chords", "stab", "stabs", "keys", "key", "piano", "pianos", "rhodes", "organ", "organs", "guitar", "guitars", "gtr", "epiano", "ep", "wurli", "wurlitzer", "brass", "horn", "horns", "clav" };
    static const char* synthW[] { "synth", "synths", "lead", "leads", "arp", "arps", "arpeggio", "pluck", "plucks", "seq", "sequence", "melody", "melodies", "mel", "bell", "bells", "hook", "riff", "riffs", "melodic" };
    static const char* drums[]  { "bottom", "bottoms", "btm", "tom", "toms", "drum", "drums", "drumloop", "beat", "beats", "groove", "grooves", "break", "breaks", "breakbeat", "full", "kit" };

    if (nameSaysNoKick (pathText)) return bottom;
    // the file name is the most specific: test it first, then the whole path
    const String fileOnly = lowerWords (File::createFileWithoutCheckingPath (pathText).getFileNameWithoutExtension());
    for (const String* text : { &fileOnly, &t })
    {
        auto anyIn = [text] (const char* const* list, size_t n) { for (size_t i = 0; i < n; ++i) if (hasWord (*text, list[i])) return true; return false; };
        if (anyIn (vocalList, std::size (vocalList))) return vocals;
        if (anyIn (fxList, std::size (fxList))) return fx;
        if (text->contains (" bass drum") || text->contains (" kick drum")) return kick;
        if (anyIn (kickW, std::size (kickW)) && ! anyIn (drums, std::size (drums))) return kick;
        if (anyIn (snareList, std::size (snareList))) return snare;
        if (anyIn (topList, std::size (topList))) return top;
        if (anyIn (percList, std::size (percList))) return perc;
        if (anyIn (bassW, std::size (bassW))) return bass;
        if (anyIn (padW, std::size (padW))) return pad;
        if (anyIn (chordW, std::size (chordW))) return chords;
        if (anyIn (synthW, std::size (synthW))) return synth;
        if (anyIn (drums, std::size (drums))) return bottom;
        if (anyIn (kickW, std::size (kickW))) return kick;
    }
    return -1;
}

int decideType (int nameType, const Result& r, bool noKickName)
{
    using namespace lb;
    // 1. vocals / fx by name are final; "no kick" loops are bottoms
    if (nameType == vocals || nameType == fx) return nameType;
    if (noKickName) return bottom;
    // 2. named tonal material keeps its type
    if (nameType == bass || nameType == chords || nameType == pad || nameType == synth) return nameType;
    // 3. the audio decides about the kick: drum material with an audible kick goes to KICK, whatever the name says
    const bool drumMaterial = nameType >= 0 ? nameType < 5 : r.voiced < 0.6f;
    if (drumMaterial && r.kickScore >= kickThreshold) return kick;
    // 4. named "kick" but no kick in the audio -> low drums without kick = BOTTOM
    if (nameType == kick) return bottom;
    return nameType;   // -1 = unknown: classified later by the nearest named neighbours
}

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
        String q = rest;
        if (q.startsWith ("maj") || q.startsWithIgnoreCase ("major") || q.startsWithIgnoreCase ("magg") || q.startsWith ("M")) minor = false;
        else if (q.startsWithIgnoreCase ("minor") || q.startsWithIgnoreCase ("minore") || q.startsWithIgnoreCase ("min") || q.startsWith ("m")) minor = true;
        // the rest of the token may only hold chord extensions / octave digits
        String tail = q.trimCharactersAtStart ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
        static const char* okWords[] { "minore", "minor", "min", "maggiore", "magg", "major", "maj", "m", "M", "dim", "aug", "sus", "add" };
        String chk = q;
        while (chk.isNotEmpty())
        {
            bool matched = false;
            for (auto* w : okWords) if (chk.startsWith (w)) { chk = chk.substring ((int) strlen (w)); matched = true; break; }
            if (! matched && (CharacterFunctions::isDigit (chk[0]) || chk[0] == '#' || chk[0] == 'b' || chk[0] == 'o')) { chk = chk.substring (1); matched = true; }
            if (! matched) break;
        }
        ignoreUnused (tail);
        if (chk.isNotEmpty()) continue;
        // also accept a separate following token "minor"/"min"/"major"
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

double bpmFromName (const String& name)
{
    const String t = lowerWords (name);
    StringArray tok; tok.addTokens (t, " ", ""); tok.removeEmptyStrings();
    for (int i = 0; i < tok.size(); ++i)
    {
        if (! tok[i].containsOnly ("0123456789.")) continue;
        const double v = tok[i].getDoubleValue();
        const bool tagged = (i + 1 < tok.size() && tok[i + 1] == "bpm") || (i > 0 && tok[i - 1] == "bpm");
        if (tagged && v >= 40 && v <= 300) return v;
    }
    // untagged 2-3 digit number in a plausible tempo range ("Loop_124_Am")
    for (int i = 0; i < tok.size(); ++i)
        if (tok[i].containsOnly ("0123456789") && tok[i].length() >= 2 && tok[i].length() <= 3)
        {
            const int v = tok[i].getIntValue();
            if (v >= 70 && v <= 190 && v != 808 && v != 909) return v;
        }
    return 0.0;
}

String keyName (int root, bool minor)
{
    if (root < 0) return "-";
    static const char* n[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return String (n[root % 12]) + (minor ? " min" : " maj");
}

String noteName (float hz)
{
    if (hz <= 0.0f) return "-";
    const float midi = 69.0f + 12.0f * std::log2 (hz / 440.0f);
    static const char* n[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int r = roundToInt (midi);
    const int cents = roundToInt ((midi - (float) r) * 100.0f);
    return String (n[((r % 12) + 12) % 12]) + String (r / 12 - 1) + (cents == 0 ? String() : (cents > 0 ? " +" : " ") + String (cents) + "c");
}

//==============================================================================
std::vector<float> detectOnsets (const float* x, int n, double sr)
{
    // energy-flux novelty on 3 bands (fast, used at load time for the transient shaper)
    std::vector<float> out;
    if (n < 256) return out;
    const int hop = jmax (64, (int) (sr * 0.005));
    const int frames = n / hop;
    std::vector<float> nov ((size_t) frames, 0.0f);
    float lpS = 0, hpPrev = 0, prevLo = 0, prevHi = 0, prevAll = 0;
    const float a = std::exp (-2.0f * (float) kPi * 200.0f / (float) sr);
    for (int f = 0; f < frames; ++f)
    {
        float eLo = 0, eHi = 0, eAll = 0;
        for (int i = f * hop; i < (f + 1) * hop; ++i)
        {
            lpS = a * lpS + (1 - a) * x[i];
            const float hp = x[i] - hpPrev; hpPrev = x[i];
            eLo += lpS * lpS; eHi += hp * hp; eAll += x[i] * x[i];
        }
        const float lo = std::log1p (1000.0f * eLo / (float) hop), hi = std::log1p (1000.0f * eHi / (float) hop), al = std::log1p (1000.0f * eAll / (float) hop);
        nov[(size_t) f] = jmax (0.0f, lo - prevLo) + jmax (0.0f, hi - prevHi) + 0.5f * jmax (0.0f, al - prevAll);
        prevLo = lo; prevHi = hi; prevAll = al;
    }
    const float mx = *std::max_element (nov.begin(), nov.end());
    if (mx <= 1e-6f) return out;
    const int w = jmax (2, (int) (0.05 * sr / hop)), minGap = jmax (1, (int) (0.045 * sr / hop));
    int last = -minGap;
    for (int f = 1; f + 1 < frames; ++f)
    {
        float mean = 0; int cnt = 0;
        for (int k = jmax (0, f - w); k <= jmin (frames - 1, f + w); ++k) { mean += nov[(size_t) k]; ++cnt; }
        mean /= (float) cnt;
        const float v = nov[(size_t) f];
        if (v > nov[(size_t) f - 1] && v >= nov[(size_t) f + 1] && v > mean * 1.5f + 0.08f * mx && f - last >= minGap)
        {
            out.push_back ((float) (f * hop) / (float) n);
            last = f;
        }
    }
    return out;
}

//==============================================================================
Result analyse (AudioFormatManager& fm, const File& file, const String& pathForKeywords)
{
    Result r;
    std::unique_ptr<AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples < 64 || reader->sampleRate <= 0) return r;
    r.sampleRate = reader->sampleRate;
    r.numSamples = reader->lengthInSamples;
    r.channels = (int) reader->numChannels;
    r.duration = (double) r.numSamples / r.sampleRate;

    const String fileName = file.getFileNameWithoutExtension();
    r.typeFromName = (r.type = typeFromName (pathForKeywords)) >= 0;
    r.bpmFromName = (r.bpm = bpmFromName (fileName)) > 0;
    r.nameRoot = -1;
    { int root; bool mi; if (keyFromName (fileName, root, mi)) { r.nameRoot = root; r.nameMinor = mi; } }

    // ---- read (max 100 s), mono mix, low-pass + decimate to ~22 kHz
    const int len = (int) std::min<int64> (r.numSamples, (int64) (r.sampleRate * 100.0));
    AudioBuffer<float> buf ((int) reader->numChannels, len);
    reader->read (&buf, 0, len, 0, true, true);
    std::vector<float> mono ((size_t) len, 0.0f);
    for (int c = 0; c < buf.getNumChannels(); ++c)
    {
        auto* d = buf.getReadPointer (c);
        for (int i = 0; i < len; ++i) mono[(size_t) i] += d[i] / (float) buf.getNumChannels();
    }
    const int dec = jmax (1, (int) std::floor (r.sampleRate / kTargetSr + 0.25));
    const double sr = r.sampleRate / dec;
    std::vector<float> x;
    if (dec > 1)
    {
        IIRFilter f1, f2;
        f1.setCoefficients (IIRCoefficients::makeLowPass (r.sampleRate, sr * 0.45, 0.54));
        f2.setCoefficients (IIRCoefficients::makeLowPass (r.sampleRate, sr * 0.45, 1.31));
        f1.processSamples (mono.data(), len);
        f2.processSamples (mono.data(), len);
        x.resize ((size_t) (len / dec));
        for (size_t i = 0; i < x.size(); ++i) x[i] = mono[i * (size_t) dec];
    }
    else x = std::move (mono);
    const int n = (int) x.size();
    float peak = 0; for (auto v : x) peak = jmax (peak, std::abs (v));
    if (peak < 1e-5f) { r.ok = true; if (r.type < 0) r.type = 2; r.features.assign (numFeatures, 0.0f); return r; }
    for (auto& v : x) v /= peak;   // level independent features

    // ---- STFT
    dsp::FFT fft (kOrder);
    std::vector<float> win (kN), fbuf (2 * kN);
    for (int i = 0; i < kN; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * (float) kPi * i / kN);
    const int bins = kN / 2 + 1;
    const double binHz = sr / kN;

    // mel filterbank
    auto hzToMel = [] (double f) { return 2595.0 * std::log10 (1.0 + f / 700.0); };
    auto melToHz = [] (double m) { return 700.0 * (std::pow (10.0, m / 2595.0) - 1.0); };
    std::vector<double> melEdges (kMel + 2);
    for (int i = 0; i < kMel + 2; ++i) melEdges[(size_t) i] = melToHz (hzToMel (30.0) + (hzToMel (jmin (10000.0, sr * 0.48)) - hzToMel (30.0)) * i / (kMel + 1));

    const int frames = jmax (1, (n - kN) / kHop + 1);
    std::vector<std::array<float, kMfcc>> mfcc ((size_t) frames);
    std::vector<float> cent ((size_t) frames), roll ((size_t) frames), flat ((size_t) frames), flux ((size_t) frames), rms ((size_t) frames), zcr ((size_t) frames);
    double eLow = 0, eMid = 0, eHigh = 0;
    struct Peak { float midi, w; };
    std::vector<Peak> peaks; peaks.reserve ((size_t) frames * 24);
    std::vector<float> prevMag ((size_t) bins, 0.0f), mag ((size_t) bins);

    for (int f = 0; f < frames; ++f)
    {
        const int off = f * kHop;
        float e = 0; int zc = 0;
        for (int i = 0; i < kN; ++i)
        {
            const float v = (off + i < n) ? x[(size_t) (off + i)] : 0.0f;
            fbuf[(size_t) i] = v * win[(size_t) i];
            e += v * v;
            if (i > 0 && off + i < n && ((x[(size_t) (off + i - 1)] < 0) != (v < 0))) ++zc;
        }
        std::fill (fbuf.begin() + kN, fbuf.end(), 0.0f);
        rms[(size_t) f] = std::sqrt (e / kN);
        zcr[(size_t) f] = (float) zc / kN;
        fft.performFrequencyOnlyForwardTransform (fbuf.data(), true);
        double sum = 0, wsum = 0, logSum = 0, lin = 0, fl = 0;
        int cnt = 0;
        for (int k = 0; k < bins; ++k)
        {
            mag[(size_t) k] = fbuf[(size_t) k];
            const double p = (double) mag[(size_t) k] * mag[(size_t) k];
            const double hzk = k * binHz;
            sum += p; wsum += p * hzk;
            if (hzk >= 30 && hzk <= 10000) { logSum += std::log (p + 1e-12); lin += p; ++cnt; }
            if (hzk < 150) eLow += p; else if (hzk < 4000) eMid += p; else eHigh += p;
            fl += jmax (0.0, std::sqrt (p + 1e-12) - (double) prevMag[(size_t) k]);
        }
        cent[(size_t) f] = (float) std::log2 (jmax (20.0, wsum / (sum + 1e-12)));
        double acc = 0; int rk = bins - 1;
        for (int k = 0; k < bins; ++k) { acc += (double) mag[(size_t) k] * mag[(size_t) k]; if (acc >= 0.85 * sum) { rk = k; break; } }
        roll[(size_t) f] = (float) std::log2 (jmax (20.0, rk * binHz));
        flat[(size_t) f] = cnt > 0 ? (float) (std::exp (logSum / cnt) / (lin / cnt + 1e-12)) : 0.0f;
        flux[(size_t) f] = (float) fl;
        for (int k = 0; k < bins; ++k) prevMag[(size_t) k] = mag[(size_t) k];

        // mel + DCT
        std::array<double, kMel> mel {};
        for (int b = 0; b < kMel; ++b)
        {
            const double lo = melEdges[(size_t) b], c = melEdges[(size_t) b + 1], hi = melEdges[(size_t) b + 2];
            double s = 0;
            for (int k = (int) (lo / binHz); k <= (int) (hi / binHz) && k < bins; ++k)
            {
                const double hzk = k * binHz;
                const double w = hzk < c ? (hzk - lo) / (c - lo) : (hi - hzk) / (hi - c);
                if (w > 0) s += w * (double) mag[(size_t) k] * mag[(size_t) k];
            }
            mel[(size_t) b] = std::log10 (s + 1e-10);
        }
        for (int c = 0; c < kMfcc; ++c)
        {
            double s = 0;
            for (int b = 0; b < kMel; ++b) s += mel[(size_t) b] * std::cos (kPi * c * (b + 0.5) / kMel);
            mfcc[(size_t) f][(size_t) c] = (float) s;
        }

    }

    // ---- high-resolution spectral peaks (8192-point FFT, ~2.7 Hz bins) 55 Hz .. 5 kHz -> pitch-class evidence
    {
        constexpr int cOrder = 13, cN = 1 << cOrder, cHop = 2048;
        dsp::FFT cfft (cOrder);
        std::vector<float> cw (cN), cb (2 * cN);
        for (int i = 0; i < cN; ++i) cw[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * (float) kPi * i / cN);
        const double cBin = sr / cN;
        for (int off = 0; off + cN / 2 < n; off += cHop)
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

    // ---- onsets (spectral flux novelty, adaptive threshold)
    {
        const float mx = *std::max_element (flux.begin(), flux.end());
        int last = -4;
        for (int f = 1; f + 1 < frames && mx > 0; ++f)
        {
            float mean = 0; int c = 0;
            for (int k = jmax (0, f - 6); k <= jmin (frames - 1, f + 6); ++k) { mean += flux[(size_t) k]; ++c; }
            mean /= (float) c;
            const float v = flux[(size_t) f];
            if (v > flux[(size_t) f - 1] && v >= flux[(size_t) f + 1] && v > mean * 1.4f + 0.06f * mx && f - last >= 3)
            { r.onsets.push_back ((float) ((double) (f * kHop + kN / 4) / n)); last = f; }
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
            cfft.perform (A.data(), Af.data(), false);   // (separate in/out buffers: not every FFT engine works in place)
            cfft.perform (B.data(), Bf.data(), false);
            for (int i = 0; i < FN; ++i) Ct[(size_t) i] = std::conj (Af[(size_t) i]) * Bf[(size_t) i];
            cfft.perform (Ct.data(), C.data(), true);
            // normalise: lag 0 of the cross-correlation equals the window energy (independent of FFT scaling)
            const float scale = std::abs (C[0].real()) > 1e-12f ? e / C[0].real() : 0.0f;
            for (int i = 0; i < tauMax; ++i) C[(size_t) i] *= scale;
            // energies of the shifted windows
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

    // ---- tuning + chroma
    {
        double sx = 0, sy = 0;
        for (auto& p : peaks) { const double dev = p.midi - std::round (p.midi); sx += p.w * std::cos (2 * kPi * dev); sy += p.w * std::sin (2 * kPi * dev); }
        r.tuneCents = (float) (std::atan2 (sy, sx) / (2 * kPi) * 100.0);
        std::array<double, 12> ch {};
        // harmonic-sum pitch-class profile: every peak also votes for the fundamentals it may belong to
        // (f/2, f/3, f/4, decaying weights), so the 3rd/5th harmonics of saws do not fake fifths / thirds
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
        // fundamental-frequency evidence (dominant in basses)
        std::array<double, 12> fh {};
        double fsum = 0;
        for (size_t i = 0; i < f0s.size(); ++i)
        {
            const double midi = 69.0 + 12.0 * std::log2 (f0s[i] / 440.0) - r.tuneCents / 100.0;
            fh[(size_t) (((int) std::lround (midi) % 12 + 12) % 12)] += f0w[i];
            fsum += f0w[i];
        }
        double csum = 0; for (auto v : ch) csum += v;
        const double lowShare = eLow / (eLow + eMid + eHigh + 1e-12);
        const bool bassy = r.f0 > 0 && r.f0 < 250 && r.voiced > 0.3f && (r.type == 5 || (r.type < 0 && lowShare > 0.35));
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
        // bass loops: the most frequent fundamental is usually the root
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

    // ---- kick detection: transients in the 30..100 Hz band (kick drums), independent of the name
    {
        IIRFilter lpK, lpK2, hpK;
        lpK.setCoefficients (IIRCoefficients::makeLowPass (sr, 100.0, 0.7));
        lpK2.setCoefficients (IIRCoefficients::makeLowPass (sr, 100.0, 0.7));
        hpK.setCoefficients (IIRCoefficients::makeHighPass (sr, 30.0, 0.7));
        std::vector<float> low (x);
        lpK.processSamples (low.data(), n); lpK2.processSamples (low.data(), n); hpK.processSamples (low.data(), n);
        const int hop = jmax (16, (int) (sr * 0.005));
        const int nf = n / hop;
        std::vector<float> envL ((size_t) nf), envA ((size_t) nf);
        double eL = 0, eA = 0;
        for (int f = 0; f < nf; ++f)
        {
            float a = 0, b = 0;
            for (int i = f * hop; i < (f + 1) * hop; ++i) { a += low[(size_t) i] * low[(size_t) i]; b += x[(size_t) i] * x[(size_t) i]; }
            envL[(size_t) f] = std::sqrt (a / hop); envA[(size_t) f] = std::sqrt (b / hop);
            eL += a; eA += b;
        }
        const float share = (float) (eL / (eA + 1e-12));
        float mxL = 0; for (auto v : envL) mxL = jmax (mxL, v);
        float mxA = 0; for (auto v : envA) mxA = jmax (mxA, v);
        // low transients: fast rise (x2.5 within 30 ms) to a loud peak, followed by a decay
        int hits = 0, last = -1000;
        const int rise = jmax (1, (int) (0.03 * sr / hop)), decay = jmax (2, (int) (0.12 * sr / hop));
        for (int f = rise; f + decay < nf; ++f)
        {
            const float v = envL[(size_t) f];
            if (v < 0.3f * mxL || v < 0.12f * mxA || f - last < decay) continue;
            if (v < envL[(size_t) f - 1] || v < envL[(size_t) f + 1]) continue;
            if (v > 2.5f * envL[(size_t) (f - rise)] && envL[(size_t) (f + decay)] < 0.6f * v) { ++hits; last = f; }
        }
        const double bars = jmax (0.25, r.duration * (r.bpm > 0 ? r.bpm : 120.0) / 240.0);
        const float perBar = (float) (hits / bars);
        r.kickScore = jlimit (0.0f, 1.0f, std::min (1.0f, share * 3.0f) * std::min (1.0f, perBar / 1.5f) * (mxL > 0.15f * mxA ? 1.0f : 0.3f));
    }

    // ---- tempo & length
    {
        // onset-envelope autocorrelation tempo (60..200 BPM, log-gaussian prior around 120)
        const double fps = sr / kHop;
        double estBpm = 0, bestScore = 0;
        for (int lag = (int) (fps * 60 / 200); lag <= (int) (fps * 60 / 60) && lag < frames; ++lag)
        {
            double s = 0; for (int f = 0; f + lag < frames; ++f) s += flux[(size_t) f] * flux[(size_t) (f + lag)];
            const double bpmL = 60.0 * fps / lag;
            s *= std::exp (-0.5 * std::pow (std::log2 (bpmL / 120.0) / 0.9, 2.0));
            if (s > bestScore) { bestScore = s; estBpm = bpmL; }
        }
        if (r.bpmFromName)
            r.beats = jmax (1, (int) std::lround (r.duration * r.bpm / 60.0));
        else
        {
            int bestBeats = 4; double bestErr = 1e9;
            for (int beats : { 4, 8, 16, 32, 64, 128 })
            {
                const double b = beats * 60.0 / r.duration;
                if (b < 55 || b > 210) continue;
                double err = std::abs (std::log2 (b / 120.0)) * 0.6;   // prior towards common tempos
                if (estBpm > 0)
                {
                    // the autocorrelation often locks on half / double tempo: octave errors are cheap
                    double best = 1e9;
                    for (int k = -2; k <= 2; ++k) best = std::min (best, std::abs (std::log2 (b / (estBpm * std::pow (2.0, k)))) + 0.15 * std::abs (k));
                    err += best;
                }
                if (err < bestErr) { bestErr = err; bestBeats = beats; }
            }
            if (bestErr > 1e8) bestBeats = jmax (1, (int) std::lround (r.duration * 2.0));   // assume ~120 BPM
            r.beats = bestBeats;
            r.bpm = r.beats * 60.0 / r.duration;
        }
    }

    // ---- feature vector
    r.features.assign (numFeatures, 0.0f);
    auto meanStd = [] (const std::vector<float>& v, float& m, float& s)
    {
        m = 0; for (auto a : v) m += a; m /= (float) std::max<size_t> (1, v.size());
        s = 0; for (auto a : v) s += (a - m) * (a - m); s = std::sqrt (s / (float) std::max<size_t> (1, v.size()));
    };
    for (int c = 0; c < kMfcc; ++c)
    {
        std::vector<float> col ((size_t) frames);
        for (int f = 0; f < frames; ++f) col[(size_t) f] = mfcc[(size_t) f][(size_t) c];
        meanStd (col, r.features[(size_t) c], r.features[(size_t) (kMfcc + c)]);
    }
    float m, s;
    meanStd (cent, m, s); r.features[26] = m; r.features[27] = s;
    meanStd (roll, m, s); r.features[28] = m;
    meanStd (flat, m, s); r.features[29] = m;
    float rm, rs; meanStd (rms, rm, rs);
    meanStd (flux, m, s); r.features[30] = m / (rm * kN * 0.05f + 1e-6f);
    r.features[31] = *std::max_element (rms.begin(), rms.end()) / (rm + 1e-6f);
    r.features[32] = rs / (rm + 1e-6f);
    meanStd (zcr, m, s); r.features[33] = m;
    const double eTot = eLow + eMid + eHigh + 1e-12;
    r.features[34] = (float) (eLow / eTot); r.features[35] = (float) (eMid / eTot); r.features[36] = (float) (eHigh / eTot);
    r.features[37] = (float) r.onsets.size() / (float) jmax (1, r.beats);
    if (! r.onsets.empty())
    {
        // 16-step (per bar) rhythm pattern
        float tot = 0;
        for (auto o : r.onsets)
        {
            const double beat = o * r.beats;
            const int step = ((int) std::floor (std::fmod (beat, 4.0) / 0.25 + 0.5)) % 16;
            r.features[(size_t) (38 + step)] += 1.0f; tot += 1.0f;
        }
        for (int i = 0; i < 16; ++i) r.features[(size_t) (38 + i)] /= tot;
    }
    for (int i = 0; i < 12; ++i) r.features[(size_t) (54 + i)] = r.chroma[(size_t) ((i + jmax (0, r.keyRoot)) % 12)];
    r.features[66] = r.voiced;

    // ---- type: names + kick detection; rule-based audio fallback (refined later by k-NN in the library)
    {
        const int nameType = r.type;
        r.type = decideType (nameType, r, nameSaysNoKick (pathForKeywords));
        r.typeFromName = nameType >= 0;
    }
    if (r.type < 0)
    {
        using namespace lb;
        const float lowR = r.features[34], highR = r.features[36], flatness = r.features[29];
        float chromaPeak = 0; for (auto v : r.chroma) chromaPeak = jmax (chromaPeak, v);
        int strong = 0; for (auto v : r.chroma) if (v > chromaPeak * 0.5f) ++strong;
        const bool tonal = r.voiced > 0.45f && flatness < 0.2f;
        const float density = r.features[37];
        if (tonal)
        {
            if (r.f0 > 0 && r.f0 < 180 && lowR > 0.3f) r.type = bass;
            else if (density < 0.5f && r.features[32] < 0.5f) r.type = pad;
            else if (strong >= 3) r.type = chords;
            else r.type = synth;
        }
        else
        {
            if (highR > 0.4f && lowR < 0.08f) r.type = top;
            else if (lowR > 0.1f) r.type = bottom;
            else if (density <= 1.1f) r.type = snare;
            else r.type = perc;
        }
    }
    r.ok = true;
    return r;
}
} // namespace analysis
