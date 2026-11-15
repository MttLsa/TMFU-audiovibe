#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Params.h"
#include <array>
#include <vector>

// =====================================================================================
//  Loop analysis (runs once per file while scanning, results are cached in the database)
//
//  - Type (module 1..9): file / folder name keywords first, audio classifier as fallback
//  - Tempo & length: BPM from the name if present, otherwise duration -> power-of-two bar count
//    checked against an onset-autocorrelation tempo estimate
//  - Key: tuning-compensated chroma (HPCP-like) + Krumhansl-Kessler / Temperley profile correlation;
//    for basses the chroma is combined with a YIN fundamental-frequency histogram
//  - Fundamental: median YIN f0 of the voiced frames
//  - Timbre/character descriptor for "similar loops": MFCC mean/std, spectral shape, dynamics,
//    16-step rhythm pattern, key-relative chroma (see LoopLibrary for the distance + clustering)
// =====================================================================================
namespace analysis
{
    constexpr int numFeatures = 67;

    struct Result
    {
        bool ok = false;
        double sampleRate = 44100.0;
        int64_t numSamples = 0;
        int channels = 1;
        double duration = 0.0;

        int type = -1;              // 0..8, final type
        bool typeFromName = false;
        double bpm = 120.0;
        int beats = 4;              // loop length in quarter notes
        bool bpmFromName = false;

        int keyRoot = -1;           // 0..11, -1 = atonal
        bool minor = false;
        float keyConf = 0.0f;
        float tuneCents = 0.0f;     // deviation of the loop's tuning from A=440 (cents)
        int nameRoot = -1;          // key from the file name
        bool nameMinor = false;
        float f0 = 0.0f;            // fundamental (Hz), 0 = none
        float voiced = 0.0f;        // fraction of pitched frames
        float kickScore = 0.0f;     // 0..1: how clearly the loop contains a kick drum (low-band transients)
        std::array<float, 12> chroma {};   // absolute pitch classes (sum = 1)
        std::vector<float> features;       // numFeatures values
        std::vector<float> onsets;         // onset positions as fractions of the loop (0..1)
    };

    // full analysis of one file (thread-safe, no shared state)
    Result analyse (juce::AudioFormatManager& fm, const juce::File& file, const juce::String& pathForKeywords);

    // keyword classifier on file + folder names (module index); returns -1 if nothing matched
    int typeFromName (const juce::String& pathText);
    bool nameSaysNoKick (const juce::String& pathText);
    // final module type: vocals / fx names are final, "no kick" -> Bottom, drum material with an audible
    // kick -> Kick (the audio wins over the name), "kick" without a kick -> Bottom
    constexpr float kickThreshold = 0.35f;
    int decideType (int nameType, const Result& r, bool noKickName);
    // key from a name ("bass rocket league Gmin" -> G minor, "pad D" -> D major); false if none
    bool keyFromName (const juce::String& name, int& root, bool& minor);
    // BPM from a name ("120bpm", "128 BPM", "_95_"), 0 if none
    double bpmFromName (const juce::String& name);
    // onsets of an already loaded mono signal (fractions of its length)
    std::vector<float> detectOnsets (const float* x, int n, double sr);

    juce::String keyName (int root, bool minor);
    juce::String noteName (float hz);
}
