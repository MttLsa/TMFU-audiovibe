#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// =====================================================================================
//  Parameter IDs
//  Section A  ("a…")  : 8-channel synthetic drum machine (Microtonic architecture)
//  Section B  ("b…")  : 5-part sample groovebox        (Limelite architecture)
//  Global     ("g…")  : A/B crossfader, master cutoff filter, master volume, clock
// =====================================================================================
namespace params
{
    constexpr int numMicroChannels = 8;
    constexpr int numMicroPatterns = 12;   // A … L
    constexpr int numSteps         = 16;
    constexpr int numLimePatterns  = 8;    // snapshot slots 1 … 8

    // ---------------- Section A : per-channel drum patch ----------------
    // Suffixes, combined as "a<ch>_<suffix>" with ch = 1..8
    namespace mt
    {
        inline const char* oscWave   = "osc_wave";
        inline const char* oscFreq   = "osc_freq";
        inline const char* oscAtk    = "osc_atk";
        inline const char* oscDcy    = "osc_dcy";
        inline const char* pmMode    = "pm_mode";
        inline const char* pmAmt     = "pm_amt";
        inline const char* pmRate    = "pm_rate";
        inline const char* nMode     = "n_mode";
        inline const char* nFreq     = "n_freq";
        inline const char* nQ        = "n_q";
        inline const char* nStereo   = "n_stereo";
        inline const char* nEnv      = "n_env";
        inline const char* nAtk      = "n_atk";
        inline const char* nDcy      = "n_dcy";
        inline const char* mix       = "mix";
        inline const char* dist      = "dist";
        inline const char* eqFreq    = "eq_freq";
        inline const char* eqGain    = "eq_gain";
        inline const char* level     = "level";
        inline const char* pan       = "pan";
        inline const char* choke     = "choke";
        inline const char* mute      = "mute";
        inline const char* velOsc    = "vel_osc";
        inline const char* velNoise  = "vel_noise";
        inline const char* velMod    = "vel_mod";

        // all numeric patch parameters (morphable / copyable); mute is excluded
        inline const std::vector<const char*>& patchSuffixes()
        {
            static const std::vector<const char*> s {
                oscWave, oscFreq, oscAtk, oscDcy, pmMode, pmAmt, pmRate,
                nMode, nFreq, nQ, nStereo, nEnv, nAtk, nDcy,
                mix, dist, eqFreq, eqGain, level, pan, choke,
                velOsc, velNoise, velMod };
            return s;
        }
    }

    inline juce::String a (int ch, const char* suffix) { return "a" + juce::String (ch + 1) + "_" + suffix; }

    // Section A globals
    inline const char* aPattern  = "a_pattern";
    inline const char* aSwing    = "a_swing";
    inline const char* aFillRate = "a_fillrate";
    inline const char* aFill     = "a_fill";
    inline const char* aMorph    = "a_morph";
    inline const char* aLevel    = "a_level";

    // ---------------- Section B ----------------
    enum LimePart { kick = 0, snare, hat, tie, pod, numLimeParts };
    inline const char* limePartName (int p)
    {
        static const char* n[] { "Kick", "Snare", "Hat", "Tie", "Pod" };
        return n[p];
    }
    inline const char* limePartKey (int p)
    {
        static const char* n[] { "kick", "snare", "hat", "tie", "pod" };
        return n[p];
    }
    inline juce::String b (int part, const char* suffix) { return juce::String ("b_") + limePartKey (part) + "_" + suffix; }

    // per-part suffixes (not every part uses every suffix, see Params.cpp)
    namespace lm
    {
        inline const char* smp     = "smp";      // sample (hat: closed)
        inline const char* smpOpen = "smpo";     // hat: open sample
        inline const char* tune    = "tune";
        inline const char* dcy     = "dcy";      // hat: closed decay
        inline const char* dcyOpen = "dcyo";     // hat: open decay
        inline const char* link    = "link";     // hat: choke link
        inline const char* fmode   = "fmode";    // snare: LP/BP/HP
        inline const char* cut     = "cut";
        inline const char* res     = "res";
        inline const char* lowcut  = "lowcut";   // kick
        inline const char* fmFreq  = "fmfreq";   // tie
        inline const char* fmAmt   = "fmamt";    // tie
        inline const char* zap     = "zap";      // pod
        inline const char* zapDcy  = "zapdcy";   // pod
        inline const char* lvl     = "lvl";
        inline const char* pan     = "pan";
        inline const char* send    = "send";
        inline const char* mute    = "mute";
    }

    inline const char* bPattern  = "b_pattern";
    inline const char* bLen      = "b_len";
    inline const char* bTieLen   = "b_tie_len";
    inline const char* bPodLen   = "b_pod_len";
    inline const char* bModLen   = "b_mod_len";
    inline const char* bTieStr   = "b_tie_str";
    inline const char* bPodStr   = "b_pod_str";
    inline const char* bModStr   = "b_mod_str";
    inline const char* bShuffle  = "b_shuffle";
    inline const char* bAccent   = "b_accent";
    inline const char* bGain     = "b_gain";
    inline const char* bPitch    = "b_pitch";
    inline const char* bDirt     = "b_dirt";
    inline const char* bNorm     = "b_norm";
    inline const char* bLfoRate  = "b_lfo_rate";
    inline const char* bLfoFM    = "b_lfo_fm";
    inline const char* bLfoDM    = "b_lfo_dm";
    inline const char* bDlyTime  = "b_dly_time";
    inline const char* bDlyFb    = "b_dly_fb";
    inline const char* bDlyDamp  = "b_dly_damp";
    inline const char* bDlyLvl   = "b_dly_lvl";
    inline const char* bFltCut   = "b_flt_cut";
    inline const char* bFltRes   = "b_flt_res";
    inline const char* bModDest  = "b_mod_dest";
    inline const char* bModAmt   = "b_mod_amt";

    inline const juce::StringArray& delayTimeNames()
    {
        static const juce::StringArray s { "1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8", "1/4T", "1/8D", "1/4", "1/4D", "1/2" };
        return s;
    }
    inline double delayTimeBeats (int idx)
    {
        static const double v[] { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.375, 0.5, 2.0 / 3.0, 0.75, 1.0, 1.5, 2.0 };
        return v[juce::jlimit (0, 10, idx)];
    }

    enum ModDest { mdOff = 0, mdKickTune, mdKickDecay, mdSnareTune, mdSnareDecay, mdSnareCut, mdHatTune, mdHatDecay,
                   mdTieTune, mdTieFM, mdPodTune, mdPodZap, mdFilter, mdDelaySend, mdDelayFb, numModDest };
    inline const juce::StringArray& modDestNames()
    {
        static const juce::StringArray s { "Off", "Kick Tune", "Kick Decay", "Snare Tune", "Snare Decay", "Snare Cutoff",
                                           "Hat Tune", "Hat Decay", "Tie Tune", "Tie FM", "Pod Tune", "Pod Zap",
                                           "Filter Cutoff", "Delay Send", "Delay Feedback" };
        return s;
    }

    // ---------------- Global ----------------
    inline const char* gXfade  = "g_xfade";
    inline const char* gCutoff = "g_cutoff";
    inline const char* gMode   = "g_mode";
    inline const char* gRes    = "g_res";
    inline const char* gMaster = "g_master";
    inline const char* gTempo  = "g_tempo";
    inline const char* gRun    = "g_run";

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout (int numBankSamples, const juce::StringArray& sampleNames,
                                                                     const int defaultSample[numLimeParts], int defaultHatOpen);
}
