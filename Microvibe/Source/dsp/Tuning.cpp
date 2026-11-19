#include "Tuning.h"
#include "../../third_party/MTS-ESP/libMTSClient.h"
#include <cmath>

namespace tuning
{
    const juce::StringArray& builtinNames()
    {
        static const juce::StringArray s {
            "Chromatic 12-TET", "Major", "Minor", "Dorian", "Phrygian", "Minor Pentatonic", "Whole Tone",
            "19-EDO", "24-EDO (quarter tones)", "31-EDO", "53-EDO",
            "Just 5-limit Major", "Just 7-limit", "Harmonic 8-16", "Bohlen-Pierce", "Pelog", "Slendro",
            "Maqam Rast", "Maqam Bayati", "Partch 43", "User (.scl)" };
        return s;
    }

    static double ratioCents (double n, double d) { return 1200.0 * std::log2 (n / d); }

    static Scale make (const juce::String& name, std::initializer_list<double> c, double period = 1200.0)
    {
        Scale s; s.name = name; s.period = period; s.size = 0;
        for (double x : c) if (s.size < 256) s.cents[(size_t) s.size++] = x;
        return s;
    }
    static Scale edo (const juce::String& name, int n, double period = 1200.0)
    {
        Scale s; s.name = name; s.period = period; s.size = n;
        for (int k = 0; k < n; ++k) s.cents[(size_t) k] = period * k / n;
        return s;
    }

    Scale builtin (int i)
    {
        switch (i)
        {
            case 0:  return edo ("Chromatic 12-TET", 12);
            case 1:  return make ("Major", { 0, 200, 400, 500, 700, 900, 1100 });
            case 2:  return make ("Minor", { 0, 200, 300, 500, 700, 800, 1000 });
            case 3:  return make ("Dorian", { 0, 200, 300, 500, 700, 900, 1000 });
            case 4:  return make ("Phrygian", { 0, 100, 300, 500, 700, 800, 1000 });
            case 5:  return make ("Minor Pentatonic", { 0, 300, 500, 700, 1000 });
            case 6:  return make ("Whole Tone", { 0, 200, 400, 600, 800, 1000 });
            case 7:  return edo ("19-EDO", 19);
            case 8:  return edo ("24-EDO", 24);
            case 9:  return edo ("31-EDO", 31);
            case 10: return edo ("53-EDO", 53);
            case 11: return make ("Just 5-limit Major", { 0, ratioCents (9, 8), ratioCents (5, 4), ratioCents (4, 3), ratioCents (3, 2), ratioCents (5, 3), ratioCents (15, 8) });
            case 12: return make ("Just 7-limit", { 0, ratioCents (8, 7), ratioCents (7, 6), ratioCents (5, 4), ratioCents (4, 3), ratioCents (7, 5),
                                                    ratioCents (3, 2), ratioCents (8, 5), ratioCents (7, 4), ratioCents (9, 5) });
            case 13:
            {
                Scale s; s.name = "Harmonic 8-16"; s.size = 8;
                for (int n = 8; n < 16; ++n) s.cents[(size_t) (n - 8)] = ratioCents (n, 8);
                return s;
            }
            case 14: return edo ("Bohlen-Pierce", 13, ratioCents (3, 1));
            case 15: return make ("Pelog", { 0, 120, 270, 540, 670, 785, 950 });
            case 16: return make ("Slendro", { 0, 231, 474, 717, 955 });
            case 17: return make ("Maqam Rast", { 0, 200, 350, 500, 700, 900, 1050 });
            case 18: return make ("Maqam Bayati", { 0, 150, 300, 500, 700, 800, 1000 });
            case 19:
            {
                static const int r[][2] = { {1,1},{81,80},{33,32},{21,20},{16,15},{12,11},{11,10},{10,9},{9,8},{8,7},{7,6},{32,27},{6,5},{11,9},
                    {5,4},{14,11},{9,7},{21,16},{4,3},{27,20},{11,8},{7,5},{10,7},{16,11},{40,27},{3,2},{32,21},{14,9},{11,7},{8,5},{18,11},
                    {5,3},{27,16},{12,7},{7,4},{16,9},{9,5},{20,11},{11,6},{15,8},{40,21},{64,33},{160,81} };
                Scale s; s.name = "Partch 43"; s.size = 43;
                for (int k = 0; k < 43; ++k) s.cents[(size_t) k] = ratioCents (r[k][0], r[k][1]);
                return s;
            }
            default: return edo ("Chromatic 12-TET", 12);
        }
    }

    // ------------------------------------------------------------------------------------- Scala
    static juce::StringArray contentLines (const juce::String& text)
    {
        juce::StringArray lines, out;
        lines.addLines (text);
        for (auto l : lines) { l = l.trim(); if (l.startsWithChar ('!')) continue; out.add (l); }
        return out;
    }

    static bool parsePitch (juce::String t, double& cents)
    {
        t = t.trim().upToFirstOccurrenceOf (" ", false, false).upToFirstOccurrenceOf ("\t", false, false);
        if (t.isEmpty()) return false;
        if (t.containsChar ('.')) { cents = t.getDoubleValue(); return true; }
        double n = 0, d = 1;
        if (t.containsChar ('/')) { n = t.upToFirstOccurrenceOf ("/", false, false).getDoubleValue(); d = t.fromFirstOccurrenceOf ("/", false, false).getDoubleValue(); }
        else n = t.getDoubleValue();
        if (n <= 0 || d <= 0) return false;
        cents = ratioCents (n, d);
        return true;
    }

    bool parseScl (const juce::String& text, Scale& out, juce::String& error)
    {
        auto lines = contentLines (text);
        if (lines.size() < 2) { error = "file .scl troppo corto"; return false; }
        out = Scale(); out.name = lines[0].isNotEmpty() ? lines[0] : juce::String ("User scale");
        const int n = lines[1].getIntValue();
        if (n < 1 || n > 255 || lines.size() < 2 + n) { error = "numero di note non valido"; return false; }
        std::vector<double> c { 0.0 };
        double period = 1200.0;
        for (int k = 0; k < n; ++k)
        {
            double v = 0;
            if (! parsePitch (lines[2 + k], v)) { error = "riga non valida: " + lines[2 + k]; return false; }
            if (k == n - 1) period = v; else c.push_back (v);
        }
        if (period <= 1.0) { error = "periodo non valido"; return false; }
        std::vector<double> d;
        for (double x : c) { x = std::fmod (x, period); if (x < 0) x += period; d.push_back (x); }
        std::sort (d.begin(), d.end());
        d.erase (std::unique (d.begin(), d.end(), [] (double a, double b) { return std::abs (a - b) < 1.0e-6; }), d.end());
        out.size = (int) std::min<size_t> (256, d.size());
        for (int k = 0; k < out.size; ++k) out.cents[(size_t) k] = d[(size_t) k];
        out.period = period;
        return true;
    }

    bool parseKbm (const juce::String& text, const Scale& scl, double& tonicSemis, juce::String& error)
    {
        auto lines = contentLines (text);
        if (lines.size() < 7) { error = "file .kbm troppo corto"; return false; }
        const int mapSize = lines[0].getIntValue();
        const int middle = lines[3].getIntValue();
        const int refNote = lines[4].getIntValue();
        const double refHz = lines[5].getDoubleValue();
        const int octDeg = lines[6].getIntValue();
        if (refHz <= 0) { error = "frequenza di riferimento non valida"; return false; }
        // scale degree of the reference note (relative to the middle note)
        int degree = refNote - middle;
        if (mapSize > 0)
        {
            const int off = refNote - middle;
            const int oct = (int) std::floor ((double) off / mapSize);
            const int idx = off - oct * mapSize;
            const juce::String m = lines.size() > 7 + idx ? lines[7 + idx].trim() : juce::String ("x");
            if (m.startsWithChar ('x')) { error = "nota di riferimento non mappata"; return false; }
            degree = m.getIntValue() + oct * (octDeg > 0 ? octDeg : scl.size);
        }
        const int per = (int) std::floor ((double) degree / scl.size);
        const int idx = degree - per * scl.size;
        const double cents = per * scl.period + scl.cents[(size_t) juce::jlimit (0, scl.size - 1, idx)];
        const double tonicHz = refHz / std::exp2 (cents / 1200.0);
        tonicSemis = 12.0 * std::log2 (tonicHz / 261.6255653);
        return true;
    }

    // ------------------------------------------------------------------------------------- Tuning
    Tuning::Tuning() { mts = MTS_RegisterClient(); cur = builtin (0); }
    Tuning::~Tuning() { if (mts) MTS_DeregisterClient (mts); }

    bool Tuning::loadUser (const juce::String& sclText, const juce::String& kbmText, juce::String& error)
    {
        Scale s;
        if (! parseScl (sclText, s, error)) return false;
        if (kbmText.isNotEmpty())
        {
            double t = 0;
            if (! parseKbm (kbmText, s, t, error)) return false;
            s.tonicSemis = t; s.tonicFromKbm = true;
        }
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            user = s;
        }
        userScaleName = s.name;
        ++userVersion;
        return true;
    }

    void Tuning::update (int builtinIndex, int root, bool useMts)
    {
        const int userIdx = builtinNames().size() - 1;
        const int ver = userVersion.load();
        if (builtinIndex != curBuiltin || root != curRoot || (builtinIndex == userIdx && ver != seenVersion))
        {
            if (builtinIndex == userIdx)
            {
                const juce::SpinLock::ScopedTryLockType sl (lock);
                if (sl.isLocked())
                {
                    cur = user.size > 0 && ver > 0 ? user : builtin (0);
                    seenVersion = ver; curBuiltin = builtinIndex; curRoot = root;
                    if (! cur.tonicFromKbm) cur.tonicSemis = root;
                }
            }
            else
            {
                cur = builtin (builtinIndex);
                cur.tonicSemis = root;
                curBuiltin = builtinIndex; curRoot = root;
            }
        }
        mtsOn = useMts && mts != nullptr && MTS_HasMaster (mts);
        if (mtsOn && (mtsCounter++ % 8) == 0)
        {
            mtsCount = 0;
            for (int n = 0; n < 128; ++n)
            {
                if (MTS_ShouldFilterNote (mts, (char) n, -1)) continue;
                mtsPitch[(size_t) mtsCount++] = (n - 60) + MTS_RetuningInSemitones (mts, (char) n, -1);
            }
            std::sort (mtsPitch.begin(), mtsPitch.begin() + mtsCount);
            if (mtsCount == 0) mtsOn = false;
        }
    }

    double Tuning::nearest (double semis) const
    {
        if (mtsOn)
        {
            auto it = std::lower_bound (mtsPitch.begin(), mtsPitch.begin() + mtsCount, semis);
            if (it == mtsPitch.begin()) return *it;
            if (it == mtsPitch.begin() + mtsCount) return *(it - 1);
            return (semis - *(it - 1)) < (*it - semis) ? *(it - 1) : *it;
        }
        const double c = (semis - cur.tonicSemis) * 100.0;
        const double k = std::floor (c / cur.period);
        const double rem = c - k * cur.period;
        double best = 0, bestD = 1.0e9;
        for (int i = 0; i <= cur.size; ++i)
        {
            const double v = i < cur.size ? cur.cents[(size_t) i] : cur.period;
            const double d = std::abs (v - rem);
            if (d < bestD) { bestD = d; best = v; }
        }
        return cur.tonicSemis + (k * cur.period + best) / 100.0;
    }

    double Tuning::snap (double semis, float amount) const
    {
        if (amount <= 0.0001f) return semis;
        const double q = nearest (semis);
        return semis + (q - semis) * amount;
    }
}
