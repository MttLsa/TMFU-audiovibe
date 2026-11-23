#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

// =====================================================================================================
//  FamPat parameters (built programmatically):
//   * 5 synth modules A..E  -> ids "A_<suffix>" ... "E_<suffix>"  (same suffix table for every module)
//   * global                -> "g_*"   output filters -> "of_*"   graphic EQ -> "eq1_*".."eq5_*"
//   * 8 LFOs                -> "l1_*".."l8_*"  (can modulate any non-LFO parameter)
//  Every parameter has an index; the DSP reads a flat float array (real values) indexed by these numbers.
// =====================================================================================================
namespace params
{
    enum Kind { F, I, B, C };

    struct Def
    {
        juce::String id, name;
        Kind kind = F;
        float mn = 0, mx = 1, def = 0, centre = 0;
        juce::String unit;
        juce::StringArray choices;
        int module = -1;          // 0..4 for module parameters, -1 otherwise
        int suffix = -1;          // index in the module suffix table
        bool fader = false;       // drawn as a long fader on skins that have faders
        bool automatable = true;  // shown to the DAW (player: only master, envelopes and the effects)
    };

    constexpr int numModules = 5, numLfos = 8, numEqBands = 5, numOps = 6;
    inline const char* moduleLetter (int m) { static const char* L[] = { "A", "B", "C", "D", "E" }; return L[m]; }

    // ------------------------------------------------------------ module suffixes (index constants)
    enum ModSuffix : int
    {
        mOn, mModel, mLevel, mPan, mEngine, mOct, mSemi, mFine,
        mO1Wave, mO1Pw, mO1Lvl, mO2Wave, mO2Oct, mO2Semi, mO2Fine, mO2Pw, mO2Lvl, mSync, mXmod, mRing, mSub, mNoise, mPwm,
        mUni, mDetune, mDrift,
        mFmModern, mAlg, mFb, mFmVel,
        mOpBase,                                   // 6 ops x 7 params follow: crs, fin, lvl, atk, dec, sus, rel
        mWt = mOpBase + numOps * 7, mWPos, mWEnv, mWLfo,
        mSsDet, mSsMix, mPdW, mPdAmt, mPdEnv,
        mAddM, mBright, mTilt, mInh, mEven,
        mFlt, mCut, mRes, mFDrive, mKey, mEnv, mVelF,
        mDist, mDGain, mDTone, mDMix,
        mFA, mFD, mFS, mFR, mAA, mAD, mAS, mAR,
        mLfoShape, mLfoRate, mLfoDelay, mLfoPitch, mLfoCut, mLfoTim,
        mPenv, mPdec, mAccent, mBits, mSrr,
        mDlyDiv, mDlyFb, mDlyMix, mDlyPing,
        mRelSw,                                    // Minimoog DECAY switch: release = decay time (on) or ~8 ms (off)
        numModSuffix
    };
    enum OpParam { opCrs, opFin, opLvl, opAtk, opDec, opSus, opRel, numOpParams };
    inline int opSuffix (int op, int p) { return mOpBase + op * numOpParams + p; }

    // ------------------------------------------------------------ global / fx / eq / lfo parameter indices
    struct Layout
    {
        int moduleBase[numModules];     // index of "<M>_on"
        int gMaster, gTrim, gMode, gGlide, gBend, gVel, gWidth, gMwVib, gMwCut, gDrive, gChoMode, gChoMix,
            gRevSize, gRevDamp, gRevMix, gBpm, gSlots;
        int ofLpOn, ofLpCut, ofLpRes, ofBpOn, ofBpCut, ofBpRes, ofHpOn, ofHpCut, ofHpRes;
        int eqBase[numEqBands];         // on, type, freq, gain, q
        int lfoBase[numLfos];           // target, shape, rate, sync, div, depth, phase
        int numTargets;                 // LFO targets = every parameter before lfoBase[0]
        // preset player controls: one filter / amp envelope for the whole patch (scale every module's envelope),
        // the patch's delay and pedal drive
        int pFA, pFD, pFS, pFR, pAA, pAD, pAS, pAR, pDlyDiv, pDlyFb, pDlyMix, pPedal;
        int num;
    };
    enum EqP { eqOn, eqType, eqFreq, eqGain, eqQ, numEqP };
    enum LfoP { lTarget, lShape, lRate, lSync, lDiv, lDepth, lPhase, numLfoP };

    const Layout& layout();
    const std::vector<Def>& defs();
    inline const Def& def (int i) { return defs()[(size_t) i]; }
    inline int numParams() { return layout().num; }
    inline int mod (int module, int suffix) { return layout().moduleBase[module] + suffix; }
    int indexOf (const juce::String& id);         // -1 if unknown
    void setModelNames (const juce::StringArray& names, const std::vector<int>& defaults);   // call before defs()
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float lfoDivBeats (int div);
}
