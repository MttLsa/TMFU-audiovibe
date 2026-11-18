#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// =====================================================================================================
//  TWINSYNTH parameter table.  X(enum, id, name, kind, min, max, default, skewCentre, unit, choices)
//  kind: F = float, I = int, B = bool, C = choice ('|' separated list).  skewCentre 0 = linear.
//  The ids are also the keys used by resources/presets.json (tools/gen_presets.py).
// =====================================================================================================
#define TS_PARAMS(X) \
 /* ------------------------------------------------ GLOBAL */ \
 X(gMaster,  "g_master",  "Master",          F, -36, 6, 0, 0, "dB", "") \
 X(gTrim,    "g_trim",    "Preset Trim",     F, -24, 24, 0, 0, "dB", "") \
 X(gMode,    "g_mode",    "Voice Mode",      C, 0, 2, 0, 0, "", "Poly|Mono|Legato") \
 X(gGlide,   "g_glide",   "Glide",           F, 0, 3000, 0, 200, "ms", "") \
 X(gBend,    "g_bend",    "Bend Range",      I, 0, 24, 2, 0, "st", "") \
 X(gVel,     "g_vel",     "Velocity",        F, 0, 1, 0.5f, 0, "", "") \
 X(gWidth,   "g_width",   "Stereo Width",    F, 0, 1, 0.6f, 0, "", "") \
 X(gMwVib,   "g_mwvib",   "MW Vibrato",      F, 0, 1, 0.3f, 0, "", "") \
 X(gMwCut,   "g_mwcut",   "MW Cutoff",       F, 0, 1, 0, 0, "", "") \
 X(gDrive,   "g_drive",   "Drive",           F, 0, 1, 0, 0, "", "") \
 X(gChoMode, "g_chomode", "Chorus",          C, 0, 4, 0, 0, "", "Off|I|II|I+II|Ensemble") \
 X(gChoMix,  "g_chomix",  "Chorus Mix",      F, 0, 1, 0.5f, 0, "", "") \
 X(gDlyDiv,  "g_dlydiv",  "Delay Time",      C, 0, 7, 3, 0, "", "1/16|1/8T|1/8|1/8D|1/4T|1/4|1/4D|1/2") \
 X(gDlyFb,   "g_dlyfb",   "Delay Feedback",  F, 0, 0.95f, 0.35f, 0, "", "") \
 X(gDlyMix,  "g_dlymix",  "Delay Mix",       F, 0, 1, 0, 0, "", "") \
 X(gDlyPing, "g_dlyping", "Ping-Pong",       B, 0, 1, 1, 0, "", "") \
 X(gRevSize, "g_revsize", "Reverb Size",     F, 0, 1, 0.6f, 0, "", "") \
 X(gRevDamp, "g_revdamp", "Reverb Damp",     F, 0, 1, 0.5f, 0, "", "") \
 X(gRevMix,  "g_revmix",  "Reverb Mix",      F, 0, 1, 0, 0, "", "") \
 X(gBpm,     "g_bpm",     "Tempo (no host)", F, 60, 200, 124, 0, "bpm", "") \
 /* ------------------------------------------------ SECTION A (analog) */ \
 X(aOn,      "a_on",      "A On",            B, 0, 1, 1, 0, "", "") \
 X(aLevel,   "a_level",   "A Level",         F, 0, 1, 0.8f, 0, "", "") \
 X(aPan,     "a_pan",     "A Pan",           F, -1, 1, 0, 0, "", "") \
 X(aO1Wave,  "a_o1wave",  "A Osc1 Wave",     C, 0, 3, 0, 0, "", "Saw|Pulse|Triangle|Sine") \
 X(aO1Oct,   "a_o1oct",   "A Osc1 Octave",   I, -3, 3, 0, 0, "", "") \
 X(aO1Pw,    "a_o1pw",    "A Osc1 PW",       F, 0.05f, 0.95f, 0.5f, 0, "", "") \
 X(aO1Lvl,   "a_o1lvl",   "A Osc1 Level",    F, 0, 1, 1, 0, "", "") \
 X(aO2Wave,  "a_o2wave",  "A Osc2 Wave",     C, 0, 3, 0, 0, "", "Saw|Pulse|Triangle|Sine") \
 X(aO2Oct,   "a_o2oct",   "A Osc2 Octave",   I, -3, 3, 0, 0, "", "") \
 X(aO2Semi,  "a_o2semi",  "A Osc2 Semi",     I, -12, 12, 0, 0, "st", "") \
 X(aO2Fine,  "a_o2fine",  "A Osc2 Fine",     F, -100, 100, 0, 0, "ct", "") \
 X(aO2Pw,    "a_o2pw",    "A Osc2 PW",       F, 0.05f, 0.95f, 0.5f, 0, "", "") \
 X(aO2Lvl,   "a_o2lvl",   "A Osc2 Level",    F, 0, 1, 0, 0, "", "") \
 X(aSync,    "a_sync",    "A Sync",          B, 0, 1, 0, 0, "", "") \
 X(aXmod,    "a_xmod",    "A Cross Mod",     F, 0, 1, 0, 0, "", "") \
 X(aRing,    "a_ring",    "A Ring Mod",      F, 0, 1, 0, 0, "", "") \
 X(aSub,     "a_sub",     "A Sub Osc",       F, 0, 1, 0, 0, "", "") \
 X(aNoise,   "a_noise",   "A Noise",         F, 0, 1, 0, 0, "", "") \
 X(aUni,     "a_uni",     "A Unison",        I, 1, 7, 1, 0, "", "") \
 X(aDetune,  "a_detune",  "A Detune",        F, 0, 1, 0.2f, 0, "", "") \
 X(aDrift,   "a_drift",   "A Drift",         F, 0, 1, 0.15f, 0, "", "") \
 X(aPwm,     "a_pwm",     "A PWM (LFO)",     F, 0, 1, 0, 0, "", "") \
 X(aFltType, "a_flttype", "A Filter Type",   C, 0, 4, 0, 0, "", "LP24|LP18|LP12|BP12|HP12") \
 X(aCut,     "a_cut",     "A Cutoff",        F, 20, 20000, 8000, 1000, "Hz", "") \
 X(aRes,     "a_res",     "A Resonance",     F, 0, 1, 0.1f, 0, "", "") \
 X(aDrive,   "a_drive",   "A Drive",         F, 0, 1, 0.1f, 0, "", "") \
 X(aKey,     "a_key",     "A Key Track",     F, 0, 1, 0.5f, 0, "", "") \
 X(aEnv,     "a_env",     "A Env Amount",    F, -1, 1, 0.3f, 0, "", "") \
 X(aVelF,    "a_velf",    "A Vel>Filter",    F, 0, 1, 0.2f, 0, "", "") \
 X(aFA,      "a_fa",      "A F.Attack",      F, 0.5f, 10000, 2, 300, "ms", "") \
 X(aFD,      "a_fd",      "A F.Decay",       F, 0.5f, 10000, 400, 300, "ms", "") \
 X(aFS,      "a_fs",      "A F.Sustain",     F, 0, 1, 0.4f, 0, "", "") \
 X(aFR,      "a_fr",      "A F.Release",     F, 0.5f, 10000, 300, 300, "ms", "") \
 X(aAA,      "a_aa",      "A Attack",        F, 0.5f, 10000, 2, 300, "ms", "") \
 X(aAD,      "a_ad",      "A Decay",         F, 0.5f, 10000, 500, 300, "ms", "") \
 X(aAS,      "a_as",      "A Sustain",       F, 0, 1, 1, 0, "", "") \
 X(aAR,      "a_ar",      "A Release",       F, 0.5f, 10000, 250, 300, "ms", "") \
 X(aLfoShape,"a_lfoshape","A LFO Shape",     C, 0, 5, 0, 0, "", "Triangle|Sine|Saw Up|Saw Down|Square|S&H") \
 X(aLfoRate, "a_lforate", "A LFO Rate",      F, 0.05f, 30, 4, 3, "Hz", "") \
 X(aLfoDelay,"a_lfodelay","A LFO Delay",     F, 0, 5000, 0, 500, "ms", "") \
 X(aLfoPitch,"a_lfopitch","A LFO>Pitch",     F, 0, 12, 0, 1, "st", "") \
 X(aLfoCut,  "a_lfocut",  "A LFO>Cutoff",    F, 0, 1, 0, 0, "", "") \
 X(aPenv,    "a_penv",    "A Pitch Env",     F, -48, 48, 0, 0, "st", "") \
 X(aPdec,    "a_pdec",    "A Pitch Decay",   F, 1, 3000, 100, 150, "ms", "") \
 X(aAccent,  "a_accent",  "A Accent",        F, 0, 1, 0.5f, 0, "", "") \
 /* ------------------------------------------------ SECTION D (digital) */ \
 X(dOn,      "d_on",      "D On",            B, 0, 1, 0, 0, "", "") \
 X(dLevel,   "d_level",   "D Level",         F, 0, 1, 0.8f, 0, "", "") \
 X(dPan,     "d_pan",     "D Pan",           F, -1, 1, 0, 0, "", "") \
 X(dEngine,  "d_engine",  "D Engine",        C, 0, 4, 0, 0, "", "FM|Wavetable|Supersaw|Phase Dist|Additive") \
 X(dOct,     "d_oct",     "D Octave",        I, -3, 3, 0, 0, "", "") \
 X(dSemi,    "d_semi",    "D Semi",          I, -12, 12, 0, 0, "st", "") \
 X(dFine,    "d_fine",    "D Fine",          F, -100, 100, 0, 0, "ct", "") \
 X(dUni,     "d_uni",     "D Unison",        I, 1, 7, 1, 0, "", "") \
 X(dDetune,  "d_detune",  "D Detune",        F, 0, 1, 0.2f, 0, "", "") \
 X(dNoise,   "d_noise",   "D Noise",         F, 0, 1, 0, 0, "", "") \
 X(dAlg,     "d_alg",     "FM Algorithm",    C, 0, 7, 0, 0, "", "1 Stack|2 Y-Stack|3 Branch|4 Branch B|5 Two Pairs|6 One-to-Three|7 Pair+2|8 Additive") \
 X(dFb,      "d_fb",      "FM Feedback",     F, 0, 1, 0, 0, "", "") \
 X(dR1,      "d_r1",      "Op1 Ratio",       F, 0.5f, 16, 1, 3, "", "") \
 X(dR2,      "d_r2",      "Op2 Ratio",       F, 0.5f, 16, 1, 3, "", "") \
 X(dR3,      "d_r3",      "Op3 Ratio",       F, 0.5f, 16, 1, 3, "", "") \
 X(dR4,      "d_r4",      "Op4 Ratio",       F, 0.5f, 16, 1, 3, "", "") \
 X(dL1,      "d_l1",      "Op1 Level",       F, 0, 1, 1, 0, "", "") \
 X(dL2,      "d_l2",      "Op2 Level",       F, 0, 1, 0.5f, 0, "", "") \
 X(dL3,      "d_l3",      "Op3 Level",       F, 0, 1, 0, 0, "", "") \
 X(dL4,      "d_l4",      "Op4 Level",       F, 0, 1, 0, 0, "", "") \
 X(dE1,      "d_e1",      "Op1 Decay",       F, 5, 20000, 1000, 800, "ms", "") \
 X(dE2,      "d_e2",      "Op2 Decay",       F, 5, 20000, 1000, 800, "ms", "") \
 X(dE3,      "d_e3",      "Op3 Decay",       F, 5, 20000, 1000, 800, "ms", "") \
 X(dE4,      "d_e4",      "Op4 Decay",       F, 5, 20000, 1000, 800, "ms", "") \
 X(dS1,      "d_s1",      "Op1 Sustain",     F, 0, 1, 1, 0, "", "") \
 X(dS2,      "d_s2",      "Op2 Sustain",     F, 0, 1, 0.5f, 0, "", "") \
 X(dS3,      "d_s3",      "Op3 Sustain",     F, 0, 1, 0.5f, 0, "", "") \
 X(dS4,      "d_s4",      "Op4 Sustain",     F, 0, 1, 0.5f, 0, "", "") \
 X(dFmVel,   "d_fmvel",   "FM Vel>Index",    F, 0, 1, 0.5f, 0, "", "") \
 X(dFmAtk,   "d_fmatk",   "FM Mod Attack",   F, 0.5f, 5000, 1, 100, "ms", "") \
 X(dWt,      "d_wt",      "Wavetable",       C, 0, 11, 0, 0, "", "Analog Morph|Pulse Width|PPG Digital|Vocal|Bell|Organ|Hollow|Sync Sweep|Growl|Bit Steps|Strings|Reso Sweep") \
 X(dWPos,    "d_wpos",    "WT Position",     F, 0, 1, 0, 0, "", "") \
 X(dWEnv,    "d_wenv",    "WT Env",          F, -1, 1, 0, 0, "", "") \
 X(dWLfo,    "d_wlfo",    "WT LFO",          F, 0, 1, 0, 0, "", "") \
 X(dSsDet,   "d_ssdet",   "Supersaw Detune", F, 0, 1, 0.5f, 0, "", "") \
 X(dSsMix,   "d_ssmix",   "Supersaw Mix",    F, 0, 1, 0.6f, 0, "", "") \
 X(dPdW,     "d_pdw",     "PD Wave",         C, 0, 6, 0, 0, "", "Saw|Square|Pulse|Double Sine|Reso Saw|Reso Tri|Reso Trap") \
 X(dPdAmt,   "d_pdamt",   "PD Amount",       F, 0, 1, 0.5f, 0, "", "") \
 X(dPdEnv,   "d_pdenv",   "PD Env",          F, -1, 1, 0, 0, "", "") \
 X(dAddM,    "d_addm",    "Additive Model",  C, 0, 10, 0, 0, "", "House Organ|Drawbar Organ|E.Piano|Piano|Strings|Choir|Bell|Mallet|Glass|Brass|Soft Pad") \
 X(dBright,  "d_bright",  "Brightness",      F, 0, 1, 0.5f, 0, "", "") \
 X(dTilt,    "d_tilt",    "Partial Decay",   F, 0, 1, 0.5f, 0, "", "") \
 X(dInh,     "d_inh",     "Inharmonicity",   F, 0, 1, 0, 0, "", "") \
 X(dEven,    "d_even",    "Even Partials",   F, 0, 1, 1, 0, "", "") \
 X(dFltType, "d_flttype", "D Filter Type",   C, 0, 5, 1, 0, "", "Off|LP12|LP24|BP|HP|Notch") \
 X(dCut,     "d_cut",     "D Cutoff",        F, 20, 20000, 20000, 1000, "Hz", "") \
 X(dRes,     "d_res",     "D Resonance",     F, 0, 1, 0.1f, 0, "", "") \
 X(dKey,     "d_key",     "D Key Track",     F, 0, 1, 0.5f, 0, "", "") \
 X(dEnv,     "d_env",     "D Env Amount",    F, -1, 1, 0, 0, "", "") \
 X(dVelF,    "d_velf",    "D Vel>Filter",    F, 0, 1, 0.2f, 0, "", "") \
 X(dFA,      "d_fa",      "D F.Attack",      F, 0.5f, 10000, 2, 300, "ms", "") \
 X(dFD,      "d_fd",      "D F.Decay",       F, 0.5f, 10000, 400, 300, "ms", "") \
 X(dFS,      "d_fs",      "D F.Sustain",     F, 0, 1, 0.4f, 0, "", "") \
 X(dFR,      "d_fr",      "D F.Release",     F, 0.5f, 10000, 300, 300, "ms", "") \
 X(dAA,      "d_aa",      "D Attack",        F, 0.5f, 10000, 2, 300, "ms", "") \
 X(dAD,      "d_ad",      "D Decay",         F, 0.5f, 10000, 500, 300, "ms", "") \
 X(dAS,      "d_as",      "D Sustain",       F, 0, 1, 1, 0, "", "") \
 X(dAR,      "d_ar",      "D Release",       F, 0.5f, 10000, 250, 300, "ms", "") \
 X(dLfoShape,"d_lfoshape","D LFO Shape",     C, 0, 5, 0, 0, "", "Triangle|Sine|Saw Up|Saw Down|Square|S&H") \
 X(dLfoRate, "d_lforate", "D LFO Rate",      F, 0.05f, 30, 4, 3, "Hz", "") \
 X(dLfoDelay,"d_lfodelay","D LFO Delay",     F, 0, 5000, 0, 500, "ms", "") \
 X(dLfoPitch,"d_lfopitch","D LFO>Pitch",     F, 0, 12, 0, 1, "st", "") \
 X(dLfoCut,  "d_lfocut",  "D LFO>Cutoff",    F, 0, 1, 0, 0, "", "") \
 X(dLfoTim,  "d_lfotim",  "D LFO>Timbre",    F, 0, 1, 0, 0, "", "") \
 X(dPenv,    "d_penv",    "D Pitch Env",     F, -48, 48, 0, 0, "st", "") \
 X(dPdec,    "d_pdec",    "D Pitch Decay",   F, 1, 3000, 100, 150, "ms", "") \
 X(dBits,    "d_bits",    "D Bit Crush",     F, 0, 1, 0, 0, "", "") \
 X(dSrr,     "d_srr",     "D Rate Reduce",   F, 0, 1, 0, 0, "", "")

namespace params
{
    enum ID : int
    {
       #define TS_ENUM(e, ...) e,
        TS_PARAMS (TS_ENUM)
       #undef TS_ENUM
        NUM
    };

    enum Kind { F, I, B, C };

    struct Def
    {
        const char* id; const char* name; Kind kind;
        float mn, mx, def, centre; const char* unit; const char* choices;
    };

    const Def& def (int i);
    int indexOf (const juce::String& id);   // -1 if unknown
    juce::StringArray choicesOf (int i);
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
