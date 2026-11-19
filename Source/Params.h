#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <algorithm>
#include <vector>

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
        inline const char* nudge     = "nudge";      // per-channel timing offset, ms (-50..+50)
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
    constexpr int numLimeVoices  = 5;
    enum LimeVoice { kick = 0, snare, hat, tie, pod };
    inline const char* limeVoiceName (int v) { static const char* n[] { "Kick", "Snare", "Hat", "Belt", "Capsule" }; return n[v]; }   // shown names (Tie -> Belt, Pod -> Capsule)
    inline const char* limeVoiceKey  (int v) { static const char* n[] { "kick", "snare", "hat", "tie", "pod" }; return n[v]; }
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
                          *comb = "comb", *fb = "fb", *freq = "freq", *scale = "scale", *nameKey = "namekey",
                          *nudge = "nudge";   // per-module timing offset, ms (-50..+50)
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
    inline const char* bDirt = "b_dirt", *bGain = "b_gain", *bTune = "b_tune";
    // NORM buttons: output normaliser of each section (A and B)
    inline const char* aNorm = "a_norm", *bNorm = "b_norm";
    // SEQ buttons: sequencer of each section on / off (independent of MIDI)
    inline const char* aSeq = "a_seq", *bSeq = "b_seq";
    // CLICK: transient sample layered on the Kick (channel 1) of Section A
    inline const char* aClickOn = "a_click_on", *aClickWav = "a_click_wav", *aClickGain = "a_click_gain",
                      *aClickTune = "a_click_tune", *aClickDcy = "a_click_dcy", *aClickHp = "a_click_hp";
    // FILTER
    inline const char* bFltCut = "b_flt_cut", *bFltTrk = "b_flt_trk", *bFltSm = "b_flt_sm", *bFltMod = "b_flt_mod";
    // EQ
    inline const char* bEqHi = "b_eq_hi", *bEqMid = "b_eq_mid", *bEqLo = "b_eq_lo", *bEqAmp = "b_eq_amp", *bEqSplit = "b_eq_split";
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

    // SCALE (shared by Tie and Pod): root + scale type, same scales as Ableton Live's Scale device
    inline const char* bScaleRoot = "b_scale_root";
    inline const char* bScaleType = "b_scale_type";
    struct ScaleDef { const char* name; std::vector<int> steps; };
    inline const std::vector<ScaleDef>& scales()
    {
        static const std::vector<ScaleDef> s {
            { "Major", { 0, 2, 4, 5, 7, 9, 11 } }, { "Minor", { 0, 2, 3, 5, 7, 8, 10 } },
            { "Dorian", { 0, 2, 3, 5, 7, 9, 10 } }, { "Mixolydian", { 0, 2, 4, 5, 7, 9, 10 } },
            { "Lydian", { 0, 2, 4, 6, 7, 9, 11 } }, { "Phrygian", { 0, 1, 3, 5, 7, 8, 10 } },
            { "Locrian", { 0, 1, 3, 5, 6, 8, 10 } }, { "Whole Tone", { 0, 2, 4, 6, 8, 10 } },
            { "Half-whole Dim.", { 0, 1, 3, 4, 6, 7, 9, 10 } }, { "Whole-half Dim.", { 0, 2, 3, 5, 6, 8, 9, 11 } },
            { "Minor Blues", { 0, 3, 5, 6, 7, 10 } }, { "Minor Pentatonic", { 0, 3, 5, 7, 10 } },
            { "Major Pentatonic", { 0, 2, 4, 7, 9 } }, { "Harmonic Minor", { 0, 2, 3, 5, 7, 8, 11 } },
            { "Harmonic Major", { 0, 2, 4, 5, 7, 8, 11 } }, { "Dorian #4", { 0, 2, 3, 6, 7, 9, 10 } },
            { "Phrygian Dominant", { 0, 1, 4, 5, 7, 8, 10 } }, { "Melodic Minor", { 0, 2, 3, 5, 7, 9, 11 } },
            { "Lydian Augmented", { 0, 2, 4, 6, 8, 9, 11 } }, { "Lydian Dominant", { 0, 2, 4, 6, 7, 9, 10 } },
            { "Super Locrian", { 0, 1, 3, 4, 6, 8, 10 } }, { "8-Tone Spanish", { 0, 1, 3, 4, 5, 6, 8, 10 } },
            { "Bhairav", { 0, 1, 4, 5, 7, 8, 11 } }, { "Hungarian Minor", { 0, 2, 3, 6, 7, 8, 11 } },
            { "Hirajoshi", { 0, 2, 3, 7, 8 } }, { "In-Sen", { 0, 1, 5, 7, 10 } }, { "Iwato", { 0, 1, 5, 6, 10 } },
            { "Kumoi", { 0, 2, 3, 7, 9 } }, { "Pelog Selisir", { 0, 1, 3, 7, 8 } }, { "Pelog Tembung", { 0, 1, 5, 7, 8 } },
            { "Messiaen 3", { 0, 2, 3, 4, 6, 7, 8, 10, 11 } }, { "Messiaen 4", { 0, 1, 2, 5, 6, 7, 8, 11 } },
            { "Messiaen 5", { 0, 1, 5, 6, 7, 11 } }, { "Messiaen 6", { 0, 2, 4, 5, 6, 8, 10, 11 } },
            { "Messiaen 7", { 0, 1, 2, 3, 5, 6, 7, 8, 9, 11 } } };
        return s;
    }
    inline bool inScale (int note, int root, int type)
    {
        const auto& st = scales()[(size_t) juce::jlimit (0, (int) scales().size() - 1, type)].steps;
        const int pc = ((note - root) % 12 + 12) % 12;
        return std::find (st.begin(), st.end(), pc) != st.end();
    }
    // nearest scale note to a (fractional) MIDI pitch
    inline int quantizeToScale (float pitch, int root, int type)
    {
        const int c = juce::roundToInt (pitch);
        for (int d = 0; d <= 12; ++d)
        {
            const int up = c + d, dn = c - d;
            const bool okUp = inScale (up, root, type), okDn = inScale (dn, root, type);
            if (okUp && okDn) return std::abs ((float) up - pitch) <= std::abs ((float) dn - pitch) ? up : dn;
            if (okUp) return up;
            if (okDn) return dn;
        }
        return c;
    }
    // move a scale note by n scale degrees
    inline int stepScale (int note, int degrees, int root, int type)
    {
        int n = note;
        for (int i = 0; i < std::abs (degrees); ++i)
        {
            do { n += degrees > 0 ? 1 : -1; } while (! inScale (n, root, type));
        }
        return n;
    }

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
    inline const char* gMidiTarget = "g_midi";    // which section the MIDI keyboard plays: 0 = A, 1 = B

    // ---------------- LFOs: 3 per section, each can modulate any parameter of its section ----------------
    constexpr int numLfos = 8;          // maximum per section; "LFO count" (1..8, default 3) sets how many are active
    inline juce::String lfoCountId (int section) { return section == 0 ? "a_lfo_count" : "b_lfo_count"; }
    // section 0 = A, 1 = B;  keys: target, shape, sync, div, ms, depth, phase
    inline juce::String lfoId (int section, int lfo, const char* key)
    {
        return juce::String (section == 0 ? "a" : "b") + "_lfo" + juce::String (lfo + 1) + "_" + key;
    }
    inline const juce::StringArray& lfoShapes()
    {
        static const juce::StringArray s { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H", "Random" };
        return s;
    }
    struct LfoDiv { const char* name; double beats; };
    inline const std::vector<LfoDiv>& lfoDivs()
    {
        static const std::vector<LfoDiv> d { { "8 bars", 32 }, { "4 bars", 16 }, { "2 bars", 8 }, { "1 bar", 4 }, { "1/2", 2 }, { "1/4", 1 },
                                             { "1/8", 0.5 }, { "1/16", 0.25 }, { "1/32", 0.125 }, { "1/2 T", 4.0 / 3.0 }, { "1/4 T", 2.0 / 3.0 },
                                             { "1/8 T", 1.0 / 3.0 }, { "1/16 T", 1.0 / 6.0 }, { "1/2 .", 3 }, { "1/4 .", 1.5 }, { "1/8 .", 0.75 } };
        return d;
    }
    // modulation targets of a section (every parameter of the section except the LFOs themselves and A Morph)
    struct LfoTarget { juce::String id, name, group; };
    const std::vector<LfoTarget>& lfoTargets (int section);

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
