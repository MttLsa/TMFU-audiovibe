#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <vector>

// =====================================================================================
//  Tonal analysis of one sample - the same analysis as in Groovulation (Analysis.cpp):
//   - tuning: deviation from A = 440 Hz in cents, from the phase of the high-resolution spectral peaks
//   - chroma: tuning-compensated harmonic-sum pitch-class profile (HPCP-like), 55 Hz .. 5 kHz
//   - key: correlation with the Krumhansl-Kessler + Temperley major / minor profiles (24 keys)
//   - fundamental: median YIN f0 of the voiced frames; for basses the f0 histogram is mixed into the chroma
//     and the most frequent fundamental is preferred as the root
//   - key from the file name ("bass Gmin" -> G minor, "pad D" -> D major, "La minore" -> A minor)
// =====================================================================================
namespace keyanalysis
{
    struct Result
    {
        bool ok = false;
        int keyRoot = -1;           // 0..11, -1 = atonal / silent
        bool minor = false;
        float keyConf = 0.0f;       // 0..1
        float tuneCents = 0.0f;     // tuning of the sample relative to A = 440 Hz (cents)
        float f0 = 0.0f;            // fundamental (Hz), 0 = none
        float voiced = 0.0f;        // fraction of pitched frames
        std::array<float, 12> chroma {};   // absolute pitch classes (sum = 1)
        int nameRoot = -1;          // key read from the file name, -1 = none
        bool nameMinor = false;
    };

    // mono or stereo sample (R may be empty), any sample rate; 'name' = file name without extension
    Result analyse (const std::vector<float>& L, const std::vector<float>& R, double sampleRate, const juce::String& name);

    bool keyFromName (const juce::String& fileName, int& root, bool& minor);
    bool nameSaysBass (const juce::String& fileName);
    juce::String keyName (int root, bool minor);     // "C min", "F# maj", "-"

    // serialisation for the per-folder cache file
    juce::String toString (const Result& r);
    bool fromString (const juce::String& s, Result& r);
}
