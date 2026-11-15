#pragma once
#include <array>
#include <cmath>
#include <juce_core/juce_core.h>

// Scales as in Ableton Live's "Scale" MIDI effect / Live 12 scale list (intervals in semitones from the root).
namespace scales
{
    struct Scale { const char* name; std::array<int, 12> steps; int count; };

    inline const std::array<Scale, 34>& all()
    {
        static const std::array<Scale, 34> s {{
            { "Major",              { 0, 2, 4, 5, 7, 9, 11 }, 7 },
            { "Minor",              { 0, 2, 3, 5, 7, 8, 10 }, 7 },
            { "Dorian",             { 0, 2, 3, 5, 7, 9, 10 }, 7 },
            { "Mixolydian",         { 0, 2, 4, 5, 7, 9, 10 }, 7 },
            { "Lydian",             { 0, 2, 4, 6, 7, 9, 11 }, 7 },
            { "Phrygian",           { 0, 1, 3, 5, 7, 8, 10 }, 7 },
            { "Locrian",            { 0, 1, 3, 5, 6, 8, 10 }, 7 },
            { "Whole Tone",         { 0, 2, 4, 6, 8, 10 }, 6 },
            { "Half-whole Dim.",    { 0, 1, 3, 4, 6, 7, 9, 10 }, 8 },
            { "Whole-half Dim.",    { 0, 2, 3, 5, 6, 8, 9, 11 }, 8 },
            { "Minor Blues",        { 0, 3, 5, 6, 7, 10 }, 6 },
            { "Minor Pentatonic",   { 0, 3, 5, 7, 10 }, 5 },
            { "Major Pentatonic",   { 0, 2, 4, 7, 9 }, 5 },
            { "Harmonic Minor",     { 0, 2, 3, 5, 7, 8, 11 }, 7 },
            { "Harmonic Major",     { 0, 2, 4, 5, 7, 8, 11 }, 7 },
            { "Dorian #4",          { 0, 2, 3, 6, 7, 9, 10 }, 7 },
            { "Phrygian Dominant",  { 0, 1, 4, 5, 7, 8, 10 }, 7 },
            { "Melodic Minor",      { 0, 2, 3, 5, 7, 9, 11 }, 7 },
            { "Lydian Augmented",   { 0, 2, 4, 6, 8, 9, 11 }, 7 },
            { "Lydian Dominant",    { 0, 2, 4, 6, 7, 9, 10 }, 7 },
            { "Super Locrian",      { 0, 1, 3, 4, 6, 8, 10 }, 7 },
            { "8-Tone Spanish",     { 0, 1, 3, 4, 5, 6, 8, 10 }, 8 },
            { "Bhairav",            { 0, 1, 4, 5, 7, 8, 11 }, 7 },
            { "Hungarian Minor",    { 0, 2, 3, 6, 7, 8, 11 }, 7 },
            { "Hirajoshi",          { 0, 2, 3, 7, 8 }, 5 },
            { "In-Sen",             { 0, 1, 5, 7, 10 }, 5 },
            { "Iwato",              { 0, 1, 5, 6, 10 }, 5 },
            { "Kumoi",              { 0, 2, 3, 7, 9 }, 5 },
            { "Pelog Selisir",      { 0, 1, 3, 7, 8 }, 5 },
            { "Pelog Tembung",      { 0, 1, 5, 7, 8 }, 5 },
            { "Messiaen 3",         { 0, 2, 3, 4, 6, 7, 8, 10, 11 }, 9 },
            { "Messiaen 4",         { 0, 1, 2, 5, 6, 7, 8, 11 }, 8 },
            { "Messiaen 5",         { 0, 1, 5, 6, 7, 11 }, 6 },
            { "Chromatic",          { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }, 12 },
        }};
        return s;
    }

    inline juce::StringArray names()
    {
        juce::StringArray n;
        for (auto& s : all()) n.add (s.name);
        return n;
    }

    inline juce::StringArray rootNames() { return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; }

    // 12-bit pitch-class mask of a scale on a root
    inline std::array<bool, 12> mask (int root, int scaleIndex)
    {
        std::array<bool, 12> m {};
        const auto& s = all()[(size_t) juce::jlimit (0, (int) all().size() - 1, scaleIndex)];
        for (int i = 0; i < s.count; ++i) m[(size_t) ((root + s.steps[(size_t) i]) % 12)] = true;
        return m;
    }

    // Nearest pitch class (as a signed semitone move, ties -> downwards) from 'pc' that lies in the scale.
    inline int snapMove (int pc, const std::array<bool, 12>& m)
    {
        for (int d = 0; d <= 6; ++d)
        {
            if (m[(size_t) (((pc - d) % 12 + 12) % 12)]) return -d;
            if (m[(size_t) ((pc + d) % 12)]) return d;
        }
        return 0;
    }

    // Best transposition (-6..+5 semitones) of a loop into a scale.
    //   chroma : the loop's key-weighted pitch-class profile (sum 1), root = detected tonic
    //   The move maximises the energy of the loop that falls on scale notes; ties prefer putting the loop's
    //   tonic on the scale root, then the smallest move. E.g. scale C Major: E major loop -> -4, D major -> -2,
    //   A minor -> 0 (already in C major, relative minor).
    inline int bestShift (const std::array<float, 12>& chroma, int loopRoot, int scaleRoot, int scaleIndex)
    {
        const auto m = mask (scaleRoot, scaleIndex);
        int best = 0; float bestScore = -1.0e9f;
        for (int sh = -6; sh <= 5; ++sh)
        {
            float score = 0.0f;
            for (int pc = 0; pc < 12; ++pc)
                score += m[(size_t) (((pc + sh) % 12 + 12) % 12)] ? chroma[(size_t) pc] : -chroma[(size_t) pc];
            if ((((loopRoot + sh) % 12 + 12) % 12) == scaleRoot) score += 0.08f;   // tonic on the scale root
            else if (m[(size_t) (((loopRoot + sh) % 12 + 12) % 12)]) score += 0.03f; // tonic at least in the scale
            score -= 0.004f * (float) std::abs (sh);
            if (score > bestScore) { bestScore = score; best = sh; }
        }
        return best;
    }
}
