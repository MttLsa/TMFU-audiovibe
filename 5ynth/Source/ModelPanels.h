#pragma once
#include "Params.h"
#include <juce_core/juce_core.h>
#include <map>
#include <vector>

// =====================================================================================================
//  Front panels of the real instruments: for every synth model, the sections and control names of the original
//  hardware / software panel, each mapped to the 5ynth engine parameter that does that job.
//  Engine parameters that the original panel does not have stay out of the panel (a sound that uses one shows it
//  in the EXTRA section, so it can still be edited).
// =====================================================================================================
namespace panels
{
    struct Item { int suffix; const char* label; };
    struct Group
    {
        const char* title;
        std::vector<Item> items;
        int page = -1;     // engine the section belongs to (-1 = always); multi-engine instruments (D-50, M1…)
        bool fmGrid = false;   // the FM operator grid (algorithm, feedback, velocity + operators)
        int ops = 6;       // operators shown in the FM grid (4 on the 4-operator Yamahas)
        int cols = 0;
    };
    using Panel = std::vector<Group>;

    inline Group fm (const char* title, int ops = 6, int page = -1) { Group g { title, {}, page, true, ops }; return g; }

    inline const Panel* find (const juce::String& model)
    {
        using namespace params;
        static const std::map<juce::String, Panel> all = []
        {
            std::map<juce::String, Panel> m;
            // ------------------------------------------------------------------ Moog
            m["Moog modular"] = {
                { "921 OSCILLATOR 1", { { mOct, "Range" }, { mO1Wave, "Waveform" }, { mO1Pw, "Width" }, { mO1Lvl, "Level" } } },
                { "921 OSCILLATOR 2", { { mO2Oct, "Range" }, { mO2Fine, "Frequency" }, { mO2Wave, "Waveform" }, { mO2Lvl, "Level" } } },
                { "904A LOW PASS FILTER", { { mCut, "Fixed Control V." }, { mRes, "Regeneration" }, { mEnv, "Control In (Env)" }, { mKey, "Keyboard CV" } } },
                { "911 ENVELOPE (FILTER)", { { mFA, "T1 Attack" }, { mFD, "T2 Decay" }, { mFS, "Esus" }, { mFR, "T3 Release" } } },
                { "911 ENVELOPE (VCA)", { { mAA, "T1 Attack" }, { mAD, "T2 Decay" }, { mAS, "Esus" }, { mAR, "T3 Release" } } },
                { "MODULATION OSCILLATOR", { { mLfoShape, "Waveform" }, { mLfoRate, "Frequency" }, { mLfoPitch, "To Oscillators" } } },
            };
            m["Moog Minimoog Model D"] = {
                { "CONTROLLERS", { { mFine, "Tune" }, { mLfoRate, "Osc 3 Rate (Mod)" }, { mLfoPitch, "Osc Modulation" }, { mLfoCut, "Filter Modulation" } } },
                { "OSCILLATOR BANK", { { mOct, "Osc 1 Range" }, { mO1Wave, "Osc 1 Waveform" }, { mO2Oct, "Osc 2 Range" }, { mO2Fine, "Osc 2 Frequency" }, { mO2Wave, "Osc 2 Waveform" } } },
                { "MIXER", { { mO1Lvl, "Osc 1" }, { mO2Lvl, "Osc 2" }, { mNoise, "Noise" } } },
                { "MODIFIERS - FILTER", { { mCut, "Cutoff Frequency" }, { mRes, "Emphasis" }, { mEnv, "Amount of Contour" }, { mKey, "Keyboard Control" } } },
                { "FILTER CONTOUR", { { mFA, "Attack Time" }, { mFD, "Decay Time" }, { mFS, "Sustain Level" } } },
                { "LOUDNESS CONTOUR", { { mAA, "Attack Time" }, { mAD, "Decay Time" }, { mAS, "Sustain Level" } } },
            };
            m["Polymoog"] = {
                { "OSCILLATORS", { { mO1Wave, "Waveform" }, { mO1Pw, "Pulse Width" }, { mO2Wave, "Osc 2 Wave" }, { mO2Fine, "Osc 2 Tune" }, { mO2Lvl, "Osc 2 Level" }, { mSub, "Sub Octave" } } },
                { "MODULATION", { { mLfoShape, "Waveform" }, { mLfoRate, "Rate" }, { mLfoPitch, "Pitch Mod" }, { mLfoTim, "PW Mod" }, { mPwm, "PWM" } } },
                { "VCF", { { mCut, "Cutoff" }, { mRes, "Emphasis" }, { mEnv, "Contour Amount" } } },
                { "VCF CONTOUR", { { mFA, "Attack" }, { mFD, "Decay" }, { mFS, "Sustain" }, { mFR, "Release" } } },
                { "VCA CONTOUR", { { mAA, "Attack" }, { mAD, "Decay" }, { mAS, "Sustain" }, { mAR, "Release" } } },
            };
            m["Moog Source"] = {
                { "OSCILLATOR 1", { { mOct, "Octave" }, { mO1Wave, "Waveform" }, { mO1Pw, "Pulse Width" }, { mO1Lvl, "Level" } } },
                { "OSCILLATOR 2", { { mO2Oct, "Octave" }, { mO2Semi, "Frequency" }, { mO2Wave, "Waveform" }, { mO2Lvl, "Level" }, { mSync, "Sync" } } },
                { "MIXER", { { mSub, "Sub" }, { mNoise, "Noise" } } },
                { "FILTER", { { mCut, "Cutoff" }, { mRes, "Emphasis" }, { mEnv, "Contour Amount" }, { mKey, "Kbd Amount" } } },
                { "FILTER CONTOUR", { { mFA, "Attack" }, { mFD, "Decay" }, { mFS, "Sustain" }, { mFR, "Release" } } },
                { "LOUDNESS CONTOUR", { { mAA, "Attack" }, { mAD, "Decay" }, { mAS, "Sustain" }, { mAR, "Release" } } },
            };
            m["Moog Sub 37"] = {
                { "OSCILLATOR 1", { { mOct, "Octave" }, { mO1Wave, "Wave" }, { mO1Lvl, "Osc 1" } } },
                { "OSCILLATOR 2", { { mO2Oct, "Octave" }, { mO2Fine, "Frequency" }, { mO2Wave, "Wave" }, { mO2Lvl, "Osc 2" }, { mSync, "Hard Sync" } } },
                { "MIXER", { { mSub, "Sub Osc" }, { mNoise, "Noise" } } },
                { "FILTER", { { mCut, "Cutoff" }, { mRes, "Resonance" }, { mFDrive, "Multidrive" }, { mEnv, "EG Amt" }, { mKey, "KB Amt" } } },
                { "FILTER EG", { { mFA, "Attack" }, { mFD, "Decay" }, { mFS, "Sustain" }, { mFR, "Release" } } },
                { "AMP EG", { { mAA, "Attack" }, { mAD, "Decay" }, { mAS, "Sustain" }, { mAR, "Release" } } },
                { "LFO 1", { { mLfoShape, "Wave" }, { mLfoRate, "Rate" }, { mLfoPitch, "Pitch Amt" }, { mLfoCut, "Filter Amt" } } },
                { "PITCH EG", { { mPenv, "Pitch Amt" }, { mPdec, "Decay" } } },
            };
            // ------------------------------------------------------------------ early / experimental
            m["Electro-Theremin"] = {
                { "OSCILLATOR", { { mFine, "Pitch Trim" }, { mO1Wave, "Tone" }, { mO2Wave, "Tone 2" }, { mO2Fine, "Tone 2 Tune" }, { mO2Lvl, "Tone 2 Level" } } },
                { "TONE", { { mCut, "Brightness" }, { mRes, "Resonance" } } },
                { "EXPRESSION", { { mAA, "Swell" }, { mAR, "Fade" }, { mLfoRate, "Vibrato Rate" }, { mLfoPitch, "Vibrato Depth" }, { mLfoDelay, "Vibrato Delay" } } },
            };
            m["BBC Radiophonic oscillators"] = {
                { "OSCILLATORS", { { mOct, "Octave" }, { mO1Wave, "Osc 1 Wave" }, { mO2Wave, "Osc 2 Wave" }, { mO2Lvl, "Osc 2 Level" } } },
                { "FILTER", { { mCut, "Frequency" }, { mRes, "Resonance" }, { mEnv, "Envelope" } } },
                { "ENVELOPE (TAPE)", { { mFD, "Filter Decay" }, { mFS, "Filter Level" }, { mAA, "Attack" }, { mAD, "Decay" }, { mAS, "Level" }, { mAR, "Release" } } },
            };
            m["EMS VCS3 / Synthi AKS"] = {
                { "OSCILLATOR 1", { { mOct, "Frequency (Oct)" }, { mO1Wave, "Shape" }, { mO1Lvl, "Level" } } },
                { "OSCILLATOR 2", { { mO2Fine, "Frequency" }, { mO2Wave, "Shape" }, { mO2Lvl, "Level" } } },
                { "OSCILLATOR 3", { { mLfoRate, "Frequency" }, { mLfoShape, "Shape" }, { mLfoPitch, "To Pitch" }, { mLfoCut, "To Filter" } } },
                { "RING MOD / NOISE", { { mRing, "Ring Modulator" }, { mNoise, "Noise" } } },
                { "FILTER", { { mCut, "Frequency" }, { mRes, "Response" }, { mEnv, "Envelope" } } },
                { "ENVELOPE SHAPER", { { mAA, "Attack" }, { mAD, "On" }, { mAS, "Trapezoid" }, { mAR, "Decay" }, { mFD, "Filter Decay" }, { mFS, "Filter Level" } } },
            };
            // ------------------------------------------------------------------ ARP
            const Panel arp = {
                { "VCO 1", { { mOct, "Coarse Tune" }, { mO1Wave, "Waveform" }, { mO1Pw, "Pulse Width" }, { mO1Lvl, "Level" } } },
                { "VCO 2", { { mO2Oct, "Coarse Tune" }, { mO2Fine, "Fine Tune" }, { mO2Wave, "Waveform" }, { mO2Lvl, "Level" }, { mSync, "Sync" } } },
                { "MIXER", { { mRing, "Ring Mod" }, { mSub, "Sub Osc" }, { mNoise, "Noise" } } },
                { "VCF", { { mCut, "Initial Frequency" }, { mRes, "Resonance" }, { mEnv, "ADSR Amount" }, { mKey, "Kybd CV" }, { mLfoCut, "LFO Amount" } } },
                { "ADSR", { { mFA, "Attack" }, { mFD, "Decay" }, { mFS, "Sustain" }, { mFR, "Release" } } },
                { "AR (VCA)", { { mAA, "Attack" }, { mAD, "Decay" }, { mAS, "Sustain" }, { mAR, "Release" } } },
                { "LFO", { { mLfoShape, "Waveform" }, { mLfoRate, "Speed" }, { mLfoPitch, "Pitch Mod" }, { mLfoDelay, "Delay" } } },
            };
            m["ARP 2500"] = arp; m["ARP 2600"] = arp; m["ARP Odyssey"] = arp;
            m["ARP Solina String Ensemble"] = {
                { "INSTRUMENTS", { { mOct, "Octave" }, { mO1Wave, "Violin / Viola" }, { mO2Wave, "Cello / Bass" }, { mO2Oct, "Cello Octave" }, { mO2Fine, "Ensemble Detune" }, { mO2Lvl, "Cello Level" } } },
                { "TONE", { { mCut, "Brightness" }, { mRes, "Resonance" } } },
                { "ENVELOPE", { { mAA, "Crescendo" }, { mAR, "Sustain" } } },
            };
            // ------------------------------------------------------------------ Yamaha CS-80
            m["Yamaha CS-80"] = {
                { "VCO I / II", { { mOct, "Feet" }, { mO1Wave, "Saw / Square" }, { mO1Pw, "Pulse Width" }, { mPwm, "PWM Speed" }, { mO2Oct, "Ch II Feet" }, { mO2Fine, "Ch II Detune" }, { mO2Wave, "Ch II Wave" }, { mO2Lvl, "Ch II Level" } } },
                { "NOISE / SINE", { { mSub, "Sine" }, { mNoise, "Noise" } } },
                { "VCF", { { mCut, "LPF Cutoff" }, { mRes, "Resonance" }, { mEnv, "EG Depth" }, { mKey, "Brilliance" } } },
                { "VCF EG", { { mFA, "Attack" }, { mFD, "Decay" }, { mFS, "Initial Level" }, { mFR, "Release" } } },
                { "VCA EG", { { mAA, "Attack" }, { mAD, "Decay" }, { mAS, "Sustain" }, { mAR, "Release" } } },
                { "SUB OSCILLATOR", { { mLfoShape, "Function" }, { mLfoRate, "Speed" }, { mLfoPitch, "VCO" }, { mLfoCut, "VCF" }, { mLfoTim, "PW" }, { mLfoDelay, "Delay" } } },
                { "UNISON / DETUNE", { { mUni, "Voices" }, { mDetune, "Detune" } } },
            };
            // ------------------------------------------------------------------ Sequential / Oberheim
            m["Sequential Prophet-5"] = {
                { "OSCILLATOR A", { { mOct, "Frequency" }, { mO1Wave, "Saw / Pulse" }, { mO1Pw, "Pulse Width" }, { mSync, "Sync" } } },
                { "OSCILLATOR B", { { mO2Semi, "Frequency" }, { mO2Fine, "Fine" }, { mO2Wave, "Saw / Tri / Pulse" }, { mO2Pw, "Pulse Width" } } },
                { "POLY-MOD", { { mXmod, "Osc B" }, { mPenv, "Filt Env" }, { mPdec, "Env Decay" } } },
                { "MIXER", { { mO1Lvl, "Osc A" }, { mO2Lvl, "Osc B" }, { mNoise, "Noise" } } },
                { "FILTER", { { mCut, "Cutoff" }, { mRes, "Resonance" }, { mEnv, "Env Amt" }, { mKey, "Keyboard" } } },
                { "FILTER ENVELOPE", { { mFA, "Attack" }, { mFD, "Decay" }, { mFS, "Sustain" }, { mFR, "Release" } } },
                { "AMPLIFIER ENVELOPE", { { mAA, "Attack" }, { mAD, "Decay" }, { mAS, "Sustain" }, { mAR, "Release" } } },
                { "LFO / WHEEL-MOD", { { mLfoShape, "Shape" }, { mLfoRate, "Frequency" }, { mLfoPitch, "Freq A+B" }, { mLfoTim, "PW A+B" }, { mPwm, "PWM" }, { mLfoCut, "Filter" } } },
            };
            const Panel obx = {
                { "OSCILLATOR 1", { { mOct, "Octave" }, { mO1Wave, "Saw / Pulse" }, { mO1Pw, "Pulse Width" } } },
                { "OSCILLATOR 2", { { mO2Fine, "Detune" }, { mO2Wave, "Saw / Pulse" }, { mO2Pw, "Pulse Width" }, { mO2Lvl, "Level" }, { mSync, "Sync" } } },
                { "FILTER", { { mCut, "Frequency" }, { mRes, "Resonance" }, { mEnv, "Modulation" }, { mKey, "Keyboard" } } },
                { "FILTER ENVELOPE", { { mFA, "Attack" }, { mFD, "Decay" }, { mFS, "Sustain" }, { mFR, "Release" } } },
                { "VOLUME ENVELOPE", { { mAA, "Attack" }, { mAD, "Decay" }, { mAS, "Sustain" }, { mAR, "Release" } } },
                { "MODULATION", { { mLfoShape, "Shape" }, { mLfoRate, "Rate" }, { mLfoPitch, "Freq" }, { mLfoCut, "Filter" } } },
                { "UNISON", { { mUni, "Voices" }, { mDetune, "Detune" } } },
            };
            m["Oberheim OB-X"] = obx; m["Oberheim OB-Xa"] = obx;
            // ------------------------------------------------------------------ Korg MS-20
            m["Korg MS-20"] = {
                { "VCO 1", { { mOct, "Scale" }, { mO1Wave, "Waveform" }, { mO1Pw, "Pulse Width" }, { mO1Lvl, "VCO 1 Level" } } },
                { "VCO 2", { { mO2Semi, "Pitch" }, { mO2Wave, "Waveform" }, { mO2Lvl, "VCO 2 Level" }, { mRing, "Ring Mod" } } },
                { "VCF", { { mCut, "LPF Cutoff" }, { mRes, "Peak" }, { mEnv, "EG Int" }, { mFDrive, "Drive" }, { mKey, "Kbd Track" } } },
                { "EG 1 (VCF)", { { mFA, "Attack" }, { mFD, "Release" }, { mFS, "Sustain" } } },
                { "EG 2 (VCA)", { { mAA, "Attack" }, { mAD, "Decay" }, { mAS, "Sustain" }, { mAR, "Release" } } },
                { "MG", { { mLfoShape, "Waveform" }, { mLfoRate, "Frequency" }, { mLfoPitch, "Pitch" }, { mLfoCut, "Cutoff" } } },
            };
            // ------------------------------------------------------------------ Roland analog
            m["Roland Jupiter-8"] = {
                { "VCO-1", { { mOct, "Range" }, { mO1Wave, "Waveform" }, { mXmod, "Cross Mod" } } },
                { "VCO-2", { { mO2Oct, "Range" }, { mO2Fine, "Fine Tune" }, { mO2Wave, "Waveform" }, { mO1Pw, "Pulse Width" }, { mSync, "Sync" } } },
                { "SOURCE MIX", { { mO1Lvl, "VCO-1" }, { mO2Lvl, "VCO-2" }, { mSub, "Sub" }, { mNoise, "Noise" } } },
                { "VCF", { { mCut, "Cutoff Freq" }, { mRes, "Resonance" }, { mEnv, "Env Mod" }, { mLfoCut, "LFO Mod" }, { mKey, "Key Follow" } } },
                { "ENV-1 (VCF)", { { mFA, "Attack" }, { mFD, "Decay" }, { mFS, "Sustain" }, { mFR, "Release" } } },
                { "ENV-2 (VCA)", { { mAA, "Attack" }, { mAD, "Decay" }, { mAS, "Sustain" }, { mAR, "Release" } } },
                { "LFO", { { mLfoShape, "Waveform" }, { mLfoRate, "Rate" }, { mLfoDelay, "Delay Time" }, { mLfoPitch, "VCO Mod" }, { mLfoTim, "PWM" }, { mPwm, "PWM Env" } } },
                { "UNISON", { { mUni, "Voices" }, { mDetune, "Detune" } } },
            };
            const Panel juno = {
                { "LFO", { { mLfoRate, "Rate" }, { mLfoDelay, "Delay Time" }, { mLfoShape, "Wave" } } },
                { "DCO", { { mOct, "Range" }, { mLfoPitch, "LFO" }, { mO1Wave, "Pulse / Saw" }, { mO1Pw, "PW" }, { mPwm, "PWM" }, { mLfoTim, "PWM (LFO)" }, { mSub, "Sub Osc" }, { mNoise, "Noise" } } },
                { "VCF", { { mCut, "Freq" }, { mRes, "Res" }, { mEnv, "Env" }, { mLfoCut, "LFO" }, { mKey, "Kybd" } } },
                { "ENV", { { mFA, "A" }, { mFD, "D" }, { mFS, "S" }, { mFR, "R" } } },
                { "VCA", { { mAA, "A" }, { mAD, "D" }, { mAS, "S" }, { mAR, "R" } } },
            };
            m["Roland Juno-60"] = juno; m["Roland Juno-106"] = juno;
            m["Roland SH-101"] = {
                { "VCO", { { mOct, "Range" }, { mLfoPitch, "Mod" }, { mO1Wave, "Waveform" }, { mO1Pw, "Pulse Width" }, { mPwm, "PWM" } } },
                { "SOURCE MIXER", { { mO1Lvl, "Pulse / Saw" }, { mSub, "Sub Osc" }, { mNoise, "Noise" } } },
                { "VCF", { { mCut, "Freq" }, { mRes, "Res" }, { mEnv, "Env" }, { mLfoCut, "Mod" }, { mKey, "Kybd" } } },
                { "ENV", { { mFA, "A" }, { mFD, "D" }, { mFS, "S" }, { mFR, "R" } } },
                { "VCA", { { mAA, "A" }, { mAD, "D" }, { mAS, "S" }, { mAR, "R" } } },
                { "MODULATOR", { { mLfoRate, "Rate" }, { mLfoShape, "Waveform" }, { mLfoDelay, "Delay" } } },
            };
            m["Roland TB-303"] = {
                { "TB-303", { { mFine, "Tuning" }, { mO1Wave, "Waveform" }, { mCut, "Cut Off Freq" }, { mRes, "Resonance" }, { mEnv, "Env Mod" }, { mFD, "Decay" }, { mAccent, "Accent" } } },
            };
            m["Roland JX-8P"] = {
                { "DCO-1", { { mOct, "Range" }, { mO1Wave, "Waveform" }, { mO1Lvl, "Level" } } },
                { "DCO-2", { { mO2Oct, "Range" }, { mO2Fine, "Fine Tune" }, { mO2Wave, "Waveform" }, { mO2Lvl, "Level" }, { mXmod, "Cross Mod" } } },
                { "VCF", { { mCut, "Cutoff Freq" }, { mRes, "Resonance" }, { mEnv, "Env Mod" }, { mKey, "Key Follow" } } },
                { "ENV-1", { { mFA, "Attack" }, { mFD, "Decay" }, { mFS, "Sustain" }, { mFR, "Release" } } },
                { "ENV-2", { { mAA, "Attack" }, { mAD, "Decay" }, { mAS, "Sustain" }, { mAR, "Release" } } },
                { "UNISON", { { mUni, "Voices" }, { mDetune, "Detune" } } },
            };
            m["Roland Alpha Juno-2"] = {
                { "DCO", { { mOct, "Range" }, { mO1Wave, "Pulse / Saw" }, { mO1Pw, "PW" }, { mPwm, "PWM Rate" }, { mSub, "Sub Osc" }, { mNoise, "Noise" }, { mLfoPitch, "LFO Mod" } } },
                { "DCO 2 (LAYER)", { { mO2Oct, "Range" }, { mO2Fine, "Detune" }, { mO2Wave, "Wave" }, { mO2Lvl, "Level" } } },
                { "VCF", { { mCut, "Cutoff Freq" }, { mRes, "Resonance" }, { mEnv, "Env Mod" }, { mKey, "Key Follow" } } },
                { "ENV", { { mAA, "T1" }, { mAD, "T2 / T3" }, { mAS, "L3" }, { mAR, "T4" } } },
                { "ENV (VCF)", { { mFA, "T1" }, { mFD, "T2 / T3" }, { mFS, "L3" }, { mFR, "T4" } } },
                { "LFO", { { mLfoRate, "Rate" }, { mLfoShape, "Waveform" }, { mLfoDelay, "Delay" } } },
                { "PITCH ENV / CHORUS", { { mPenv, "Pitch Env" }, { mPdec, "Env Time" }, { mUni, "Voices" }, { mDetune, "Detune" } } },
            };
            // ------------------------------------------------------------------ Yamaha FM
            m["Yamaha DX7"] = { fm ("OPERATORS / ALGORITHM"), { "PITCH EG", { { mPenv, "Pitch EG" }, { mPdec, "Rate" } } }, { "AMP", { { mAA, "Attack" }, { mAD, "Decay" }, { mAS, "Sustain" }, { mAR, "Release" } } } };
            m["Yamaha TX-816"] = m["Yamaha DX7"];
            m["Yamaha DX21"] = { fm ("OPERATORS / ALGORITHM", 4), { "AMP", { { mAA, "Attack" }, { mAD, "Decay" }, { mAS, "Sustain" }, { mAR, "Release" } } }, { "CHORUS", { { mUni, "Voices" }, { mDetune, "Detune" } } } };
            m["Yamaha TX81Z"] = { fm ("OPERATORS / ALGORITHM", 4), { "AMP", { { mAA, "Attack" }, { mAD, "Decay" }, { mAS, "Sustain" }, { mAR, "Release" } } } };
            m["Synclavier"] = { fm ("PARTIAL TIMBRE (FM)"), { "VOLUME ENVELOPE", { { mAA, "Attack" }, { mAD, "Decay" }, { mAS, "Sustain" }, { mAR, "Release" } } }, { "VIBRATO", { { mLfoRate, "Rate" }, { mLfoPitch, "Depth" }, { mLfoDelay, "Attack" } } } };
            m["Korg Wavestation"] = { fm ("WAVE (FM)"), { "VDF / VDA", { { mCut, "Cutoff" }, { mAA, "Attack" }, { mAD, "Decay" }, { mAS, "Sustain" }, { mAR, "Release" } } } };
            // ------------------------------------------------------------------ digital
            m["Fairlight CMI"] = {
                { "VOICE (PAGE 4 / 5)", { { mOct, "Octave" }, { mAddM, "Waveform" }, { mBright, "Harmonics" }, { mTilt, "Harmonic Decay" }, { mInh, "Inharmonicity" }, { mEven, "Even Harmonics" } }, 5 },
                { "SAMPLE RATE", { { mSrr, "Sample Rate" }, { mBits, "Bits" } } },
                { "FILTER", { { mFlt, "Type" }, { mCut, "Cutoff" } } },
                { "ENVELOPE", { { mAA, "Attack" }, { mAD, "Decay" }, { mAS, "Sustain" }, { mAR, "Release" } } },
                { "VIBRATO", { { mLfoRate, "Rate" }, { mLfoPitch, "Depth" } } },
            };
            m["PPG Wave 2.3"] = {
                { "WAVES", { { mWt, "Wavetable" }, { mWPos, "Wave" }, { mWEnv, "Env 2 > Wave" }, { mWLfo, "LFO > Wave" } }, 2 },
                { "OSCILLATORS", { { mOct, "Octave" }, { mUni, "Voices" }, { mDetune, "Detune" } } },
                { "VCF", { { mCut, "Cutoff" }, { mRes, "Emphasis" }, { mEnv, "Env 1" } } },
                { "ENV 1 (VCF)", { { mFA, "Attack" }, { mFD, "Decay" }, { mFS, "Sustain" }, { mFR, "Release" } } },
                { "ENV 3 (VCA)", { { mAA, "Attack" }, { mAD, "Decay" }, { mAS, "Sustain" }, { mAR, "Release" } } },
                { "DIGITAL", { { mBits, "Bits" } } },
            };
            m["Casio CZ-5000"] = {
                { "DCO", { { mOct, "Octave" }, { mPdW, "Waveform" }, { mDetune, "Detune" }, { mUni, "Line 1+2" } }, 4 },
                { "DCW", { { mPdAmt, "DCW" }, { mPdEnv, "DCW Env" } }, 4 },
                { "DCW ENV", { { mFA, "Rate 1" }, { mFD, "Rate 2" }, { mFS, "Sus Level" }, { mFR, "Rel Rate" } } },
                { "DCA ENV", { { mAA, "Rate 1" }, { mAD, "Rate 2" }, { mAS, "Sus Level" }, { mAR, "Rel Rate" } } },
                { "VIBRATO", { { mLfoShape, "Wave" }, { mLfoRate, "Rate" }, { mLfoTim, "Depth (DCW)" }, { mLfoPitch, "Depth" } } },
            };
            m["Roland D-50"] = {
                fm ("WG (FM PARTIAL)", 6, 1),
                { "WG (PCM)", { { mAddM, "PCM Wave" }, { mBright, "Brightness" }, { mTilt, "Decay" }, { mInh, "Inharmonic" }, { mEven, "Even" } }, 5 },
                { "WG (SYNTH WAVE)", { { mWt, "Waveform" }, { mWPos, "PW" }, { mWEnv, "PW Env" }, { mWLfo, "PW LFO" } }, 2 },
                { "TVF", { { mCut, "Cutoff" }, { mRes, "Resonance" }, { mEnv, "Env Depth" }, { mLfoCut, "LFO Depth" } } },
                { "TVF ENV", { { mFA, "T1" }, { mFD, "T2 / T3" }, { mFS, "Sus L" }, { mFR, "T5" } } },
                { "TVA ENV", { { mAA, "T1" }, { mAD, "T2 / T3" }, { mAS, "Sus L" }, { mAR, "T5" } } },
                { "LFO 1", { { mLfoShape, "Waveform" }, { mLfoRate, "Rate" }, { mLfoPitch, "Pitch Depth" } } },
                { "CHORUS", { { mUni, "Voices" }, { mDetune, "Detune" } } },
            };
            m["Korg M1"] = {
                { "OSC (MULTISOUND)", { { mOct, "Octave" }, { mAddM, "Multisound" }, { mBright, "Brightness" }, { mTilt, "Decay" }, { mInh, "Inharmonic" }, { mEven, "Even" } }, 5 },
                { "OSC (WAVE)", { { mOct, "Octave" }, { mWt, "Multisound" }, { mWPos, "Position" }, { mWEnv, "EG > Wave" }, { mWLfo, "LFO > Wave" } }, 2 },
                { "VDF", { { mCut, "Cutoff" }, { mRes, "Resonance" }, { mEnv, "EG Int" } } },
                { "VDF EG", { { mFA, "Attack" }, { mFD, "Decay" }, { mFS, "Sustain" }, { mFR, "Release" } } },
                { "VDA EG", { { mAA, "Attack" }, { mAD, "Decay" }, { mAS, "Sustain" }, { mAR, "Release" } } },
                { "PITCH MG", { { mLfoRate, "Frequency" }, { mLfoPitch, "Intensity" } } },
                { "DETUNE", { { mUni, "Voices" }, { mDetune, "Detune" } } },
            };
            m["Korg Triton"] = {
                { "OSC (MULTISAMPLE)", { { mOct, "Octave" }, { mAddM, "Multisample" }, { mBright, "Brightness" }, { mTilt, "Decay" }, { mInh, "Inharmonic" }, { mEven, "Even" } }, 5 },
                fm ("OSC (FM)", 6, 1),
                { "FILTER", { { mFlt, "Type" }, { mCut, "Frequency" }, { mRes, "Resonance" }, { mEnv, "EG Int" } } },
                { "FILTER EG", { { mFA, "Attack" }, { mFD, "Decay" }, { mFS, "Sustain" }, { mFR, "Release" } } },
                { "AMP EG", { { mAA, "Attack" }, { mAD, "Decay" }, { mAS, "Sustain" }, { mAR, "Release" } } },
                { "PITCH EG", { { mPenv, "Intensity" }, { mPdec, "Decay Time" } } },
            };
            const Panel jp = {
                { "OSC 1 (SUPER SAW)", { { mOct, "Octave" }, { mSsDet, "Ctrl 1 (Detune)" }, { mSsMix, "Ctrl 2 (Mix)" } }, 3 },
                { "OSC (FEEDBACK / WAVE)", { { mOct, "Octave" }, { mWt, "Waveform" }, { mWPos, "Ctrl 1" }, { mWEnv, "Env > Ctrl" }, { mUni, "Voices" }, { mDetune, "Detune" } }, 2 },
                { "FILTER", { { mFlt, "Type" }, { mCut, "Cutoff" }, { mRes, "Resonance" }, { mEnv, "Env Depth" }, { mKey, "Key Follow" } } },
                { "FILTER ENV", { { mFA, "Attack" }, { mFD, "Decay" }, { mFS, "Sustain" }, { mFR, "Release" } } },
                { "AMP ENV", { { mAA, "Attack" }, { mAD, "Decay" }, { mAS, "Sustain" }, { mAR, "Release" } } },
                { "LFO 1", { { mLfoShape, "Waveform" }, { mLfoRate, "Rate" }, { mLfoPitch, "Pitch" }, { mLfoCut, "Filter" } } },
            };
            m["Roland JP-8000"] = jp; m["Roland JP-8080"] = jp;
            m["Access Virus"] = {
                { "OSCILLATORS", { { mOct, "Semitone (Oct)" }, { mWt, "Wave" }, { mWPos, "Wave Select / PW" }, { mWEnv, "Filter Env > WS" }, { mWLfo, "LFO > WS" }, { mUni, "Unison Voices" }, { mDetune, "Detune" } }, 2 },
                { "FILTERS", { { mFlt, "Mode" }, { mCut, "Cutoff" }, { mRes, "Resonance" }, { mEnv, "Env Amount" }, { mKey, "Keyfollow" }, { mFDrive, "Saturation" } } },
                { "FILTER ENVELOPE", { { mFA, "Attack" }, { mFD, "Decay" }, { mFS, "Sustain" }, { mFR, "Release" } } },
                { "AMPLIFIER ENVELOPE", { { mAA, "Attack" }, { mAD, "Decay" }, { mAS, "Sustain" }, { mAR, "Release" } } },
                { "LFO 1", { { mLfoShape, "Shape" }, { mLfoRate, "Rate" }, { mLfoPitch, "Osc 1" }, { mLfoCut, "Cutoff" } } },
            };
            m["Clavia Nord Lead"] = {
                { "OSCILLATORS", { { mOct, "Octave" }, { mWt, "Waveform" }, { mWPos, "Pulse Width" }, { mWEnv, "Mod Env > Osc" }, { mUni, "Unison" }, { mDetune, "Detune" } }, 2 },
                { "FILTER", { { mFlt, "Type" }, { mCut, "Frequency" }, { mRes, "Resonance" }, { mEnv, "Env Amt" }, { mKey, "Kbd Track" } } },
                { "FILTER ENV", { { mFA, "Attack" }, { mFD, "Decay" }, { mFS, "Sustain" }, { mFR, "Release" } } },
                { "AMP ENV", { { mAA, "Attack" }, { mAD, "Decay" }, { mAS, "Sustain" }, { mAR, "Release" } } },
                { "LFO", { { mLfoShape, "Waveform" }, { mLfoRate, "Rate" }, { mLfoPitch, "Pitch" }, { mLfoCut, "Filter" } } },
            };
            m["NI Massive"] = {
                { "OSC 1", { { mOct, "Pitch (Oct)" }, { mWt, "Wavetable" }, { mWPos, "Wt-Position" }, { mWEnv, "Env > Wt-Pos" }, { mWLfo, "LFO > Wt-Pos" } }, 2 },
                { "OSC (SAW STACK)", { { mOct, "Pitch (Oct)" }, { mSsDet, "Detune" }, { mSsMix, "Amp" } }, 3 },
                { "VOICING", { { mUni, "Unisono" }, { mDetune, "Pitch Spread" } } },
                { "FILTER 1", { { mFlt, "Type" }, { mCut, "Cutoff" }, { mRes, "Resonance" }, { mEnv, "Env Mod" } } },
                { "ENV 1 (FILTER)", { { mFA, "Attack" }, { mFD, "Decay" }, { mFS, "Level" }, { mFR, "Release" } } },
                { "ENV 4 (AMP)", { { mAA, "Attack" }, { mAD, "Decay" }, { mAS, "Level" }, { mAR, "Release" } } },
                { "LFO 5", { { mLfoShape, "Wave" }, { mLfoRate, "Rate" }, { mLfoCut, "Cutoff" } } },
            };
            m["LennarDigital Sylenth1"] = {
                { "OSCILLATOR A1", { { mOct, "Octave" }, { mSsDet, "Detune" }, { mSsMix, "Volume" } }, 3 },
                { "OSCILLATOR A1 (WAVE)", { { mOct, "Octave" }, { mWt, "Wave" }, { mWPos, "Phase / Shape" }, { mWEnv, "Mod Env" }, { mUni, "Voices" }, { mDetune, "Detune" } }, 2 },
                { "FILTER A", { { mFlt, "Filter Type" }, { mCut, "Cutoff" }, { mRes, "Resonance" }, { mEnv, "Mod Env 1" } } },
                { "MOD ENV 1", { { mFA, "Attack" }, { mFD, "Decay" }, { mFS, "Sustain" }, { mFR, "Release" } } },
                { "AMP ENV A", { { mAA, "Attack" }, { mAD, "Decay" }, { mAS, "Sustain" }, { mAR, "Release" } } },
                { "LFO 1", { { mLfoShape, "Wave" }, { mLfoRate, "Rate" }, { mLfoCut, "Cutoff" }, { mLfoTim, "Shape" } } },
                { "PITCH ENV", { { mPenv, "Pitch" }, { mPdec, "Decay" } } },
            };
            m["Xfer Serum"] = {
                { "OSC A", { { mOct, "Oct" }, { mWt, "Wavetable" }, { mWPos, "WT Pos" }, { mWEnv, "Env 2 > WT Pos" }, { mWLfo, "LFO 1 > WT Pos" }, { mUni, "Unison" }, { mDetune, "Detune" } }, 2 },
                { "FILTER", { { mFlt, "Type" }, { mCut, "Cutoff" }, { mRes, "Res" }, { mEnv, "Env 2" }, { mFDrive, "Drive" } } },
                { "ENV 2", { { mFA, "Attack" }, { mFD, "Decay" }, { mFS, "Sustain" }, { mFR, "Release" } } },
                { "ENV 1", { { mAA, "Attack" }, { mAD, "Decay" }, { mAS, "Sustain" }, { mAR, "Release" } } },
                { "LFO 1", { { mLfoShape, "Shape" }, { mLfoRate, "Rate" }, { mLfoCut, "Cutoff" } } },
            };
            return m;
        }();
        auto it = all.find (model);
        return it == all.end() ? nullptr : &it->second;
    }
}
