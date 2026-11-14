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
    constexpr int numSteps         = 64;   // max steps per pattern (both sections)
    constexpr int numLimePatterns  = 4;    // patterns A … D (as in Limelite)

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
    inline const char* bLevelAlias = "b_gain";   // Section B master = OUT Gain

    // ---------------- Section B (Limelite 1.3 architecture) ----------------
    constexpr int numLimeSteps   = 64;
    constexpr int numLimeVoices  = 6;
    enum LimeVoice { kick = 0, snare, hat, tie, pod, pulp };
    inline const char* limeVoiceName (int v) { static const char* n[] { "Kick", "Snare", "Hat", "Tie", "Pod", "Pulp" }; return n[v]; }
    inline const char* limeVoiceKey  (int v) { static const char* n[] { "kick", "snare", "hat", "tie", "pod", "pulp" }; return n[v]; }
    inline juce::String b (int voice, const char* key) { return juce::String ("b_") + limeVoiceKey (voice) + "_" + key; }

    // per-voice keys (see Params.cpp for which voice owns which key)
    namespace lv
    {
        inline const char *on = "on", *gain = "gain", *acc = "acc", *pan = "pan", *wav = "wav", *tune = "tune",
                          *st = "st", *hold = "h", *dcy = "d", *sat = "sat", *shp = "shp", *sm = "sm", *mod = "mod",
                          *lo = "lo", *ft = "ft", *cut = "cut", *res = "res", *flam = "flam",
                          *cd = "cd", *od = "od", *link = "link",
                          *vamt = "vamt", *port = "port", *att = "a", *fm = "fm", *mp = "mp", *noise = "noise", *vel = "vel",
                          *zap = "zap",
                          *trkLo = "lo", *trkHi = "hi", *src = "src", *inGain = "in", *osc = "osc",
                          *comb = "comb", *fb = "fb", *freq = "freq";
    }

    // sequencer / remix / global B
    inline const char* bPattern = "b_pattern";   // A-D
    inline const char* bLen     = "b_len";
    inline const char* bStart   = "b_start";
    inline const char* bTieLen  = "b_tie_len";
    inline const char* bPodLen  = "b_pod_len";
    inline const char* bModLen  = "b_mod_len";
    inline const char* bTieStr  = "b_tie_str";
    inline const char* bPodStr  = "b_pod_str";
    inline const char* bModStr  = "b_mod_str";
    inline const char* bSwing   = "b_swing";
    inline const char* bHalf    = "b_half";
    inline const char* bChainOn = "b_chain_on";
    inline const char* bChainLen= "b_chain_len";
    inline juce::String bChainSlot (int i) { return "b_chain_" + juce::String (i + 1); }
    inline const char* bLock    = "b_lock";
    inline const char* bRemix   = "b_remix";      // remix pattern 0..127
    inline const char* bRemixRate = "b_remix_rate";
    inline const char* bShift   = "b_shift";
    // OUT
    inline const char* bDirt = "b_dirt", *bGain = "b_gain", *bTune = "b_tune", *bNorm = "b_norm", *bNormAmp = "b_norm_amp",
                      *bNormRel = "b_norm_rel", *bNormSm = "b_norm_sm";
    // FILTER
    inline const char* bFltCut = "b_flt_cut", *bFltTrk = "b_flt_trk", *bFltSm = "b_flt_sm", *bFltMod = "b_flt_mod";
    // EQ
    inline const char* bEqHi = "b_eq_hi", *bEqMid = "b_eq_mid", *bEqLo = "b_eq_lo", *bEqAmp = "b_eq_amp", *bEqSplit = "b_eq_split";
    // MAGIC
    inline const char* bMagPitch = "b_mag_pitch", *bMagGrain = "b_mag_gr", *bMagMod = "b_mag_mod", *bMagRmx = "b_mag_rmx",
                      *bMagMix = "b_mag_mix", *bMagNotch = "b_mag_notch";
    // DELAY
    inline const char* bDlyOne = "b_dly_one", *bDlyTwo = "b_dly_two", *bDlyFb = "b_dly_fb", *bDlyGain = "b_dly_gain",
                      *bDlyLfo = "b_dly_lfo", *bDlyFM = "b_dly_fm", *bDlyDM = "b_dly_dm", *bDlyWide = "b_dly_wide",
                      *bDlyCut = "b_dly_cut", *bDlyRes = "b_dly_res", *bDlyLo = "b_dly_lo";
    // XFX
    inline const char* bXMode = "b_x_mode", *bXParam = "b_x_x", *bXAdd = "b_x_y", *bXDly = "b_x_dly", *bXGain = "b_x_gain",
                      *bXLo = "b_x_lo", *bXHi = "b_x_hi", *bXSpd = "b_x_spd", *bXLen = "b_x_len", *bXPlay = "b_x_play",
                      *bXRec = "b_x_rec", *bXSm = "b_x_sm";
    // MIX
    inline const char* bFxMix = "b_fx_mix", *bModInput = "b_mod_input", *bModMix = "b_mod_mix";

    inline const juce::StringArray& xfxModeNames()
    {
        static const juce::StringArray s { "off", "highpass", "bandpass", "lowpass", "phaser", "vowel", "flanger", "fm", "reson",
                                           "feedback", "reflex", "reverb", "ufo", "rainman", "distort", "ringmod", "lofi",
                                           "pitchshift", "gate", "chop", "slice", "freeze", "spin" };
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

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
