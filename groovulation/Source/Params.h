#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// =====================================================================================
//  GROOVULATION parameters
//    per module   : "m<1..12>_<name>"
//    global/master: plain names
// =====================================================================================
namespace lb
{
    constexpr int numModules = 12;
    constexpr int numLfos = 3;

    enum ModuleType { kick = 0, snare, bottom, top, perc, custom, bass, chords, pad, synth, vocals, fx };
    enum Section { drums = 0, customSection, bassSection, melodic, voxfx };

    inline juce::String moduleName (int m)
    {
        static const char* n[] { "Kick", "Snare", "Bottom", "Top", "Percussion", "Custom", "Bass", "Melodic 01", "Melodic 02", "Melodic 03", "Vocals", "FX" };
        return n[juce::jlimit (0, numModules - 1, m)];
    }
    inline juce::String moduleSubtitle (int m)
    {
        static const char* n[] { "loops with kick", "snare / clap", "toms, low-mid, no kick", "top / hats", "percussion", "your own file",
                                 "bass", "chords & stabs", "pads", "synth", "vocals", "fx" };
        return n[juce::jlimit (0, numModules - 1, m)];
    }
    // factory folder of each module ("" = none: Custom)
    inline juce::String folderName (int m)
    {
        static const char* n[] { "01 Kick", "02 Snare-Clap", "03 Bottom", "04 Top", "05 Percussion", "", "06 Bass",
                                 "07 Melodic 01 Chords-Stabs", "08 Melodic 02 Pads", "09 Melodic 03 Synth", "10 Vocals", "11 FX" };
        return n[juce::jlimit (0, numModules - 1, m)];
    }
    inline Section sectionOf (int m)
    {
        return m < 5 ? drums : (m == custom ? customSection : (m == bass ? bassSection : (m <= synth ? melodic : voxfx)));
    }
    // modules whose loops follow SCALE (Custom: only when its loop is pitched)
    inline bool isTonal (int m) { return m == bass || m == chords || m == pad || m == synth || m == vocals; }
    // drums use the transient-slice ("Beats") engine, the rest the high quality time-stretch ("Complex")
    inline bool usesBeats (int m) { return m < 5; }

    inline juce::String pid (int module, const char* name) { return "m" + juce::String (module + 1) + "_" + name; }

    // ---------------- grid (display + loop length in grid steps -> polyrhythms)
    inline const juce::StringArray& gridNames() { static juce::StringArray s { "1/1", "1/2", "1/4", "1/8", "1/12", "1/16", "1/24", "1/32" }; return s; }
    inline double gridBeats (int i)
    {
        static const double b[] { 4.0, 2.0, 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
        return b[juce::jlimit (0, 7, i)];
    }
    constexpr int maxSteps = 128;   // 0 = original loop length

    // ---------------- tempo-synced divisions (delay time / LFO rate), in beats
    inline const juce::StringArray& syncNames()
    {
        static juce::StringArray s { "1/64", "1/32T", "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2", "1/1", "2 bars", "4 bars", "8 bars" };
        return s;
    }
    inline double syncBeats (int i)
    {
        static const double b[] { 1.0 / 16, 1.0 / 12, 1.0 / 8, 1.0 / 6, 0.25, 0.375, 1.0 / 3, 0.5, 0.75, 2.0 / 3, 1.0, 1.5, 2.0, 4.0, 8.0, 16.0, 32.0 };
        return b[juce::jlimit (0, 16, i)];
    }

    // ---------------- LFO
    inline const juce::StringArray& lfoShapes() { static juce::StringArray s { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H" }; return s; }
    enum LfoTarget { tNone = 0, tGain, tPan, tPitch, tTempo, tCutoff, tRes, tDirt, tDelayMix, tDelayFb, tReverbMix, tSliceStart, tSliceEnd, tLoop, numTargets };
    inline const juce::StringArray& lfoTargets()
    {
        static juce::StringArray s { "--", "Gain", "Pan", "Pitch", "Tempo", "Cutoff", "Resonance", "Dirt", "Delay Mix", "Delay FB", "Reverb Mix", "Slice Start", "Slice End", "Loop" };
        return s;
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // master section
    inline const juce::StringArray& brGridNames() { static juce::StringArray s { "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4", "1/2", "1 bar" }; return s; }
    inline double brGridBeats (int i) { static const double b[] { 0.125, 1.0 / 6, 0.25, 1.0 / 3, 0.5, 1.0, 2.0, 4.0 }; return b[juce::jlimit (0, 7, i)]; }
    inline const juce::StringArray& brIntervalNames() { static juce::StringArray s { "1/32", "1/16", "1/8", "1/4", "1/2", "1 bar", "2 bars", "4 bars" }; return s; }
    inline double brIntervalBeats (int i) { static const double b[] { 0.125, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0 }; return b[juce::jlimit (0, 7, i)]; }
}
