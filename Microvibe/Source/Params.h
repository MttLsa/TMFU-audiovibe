#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

// =====================================================================================================
//  MICROVIBE parameter registry.
//  The table is built in code (loops for LFOs, lanes, perc voices, matrix slots) and every index is
//  recorded in prm::ids(), so the DSP reads  v[ids().lfoRate[k]]  etc.
//  Every float / int parameter is also a modulation destination (prm::dests()), including the
//  parameters of the modulators and the matrix amounts themselves (intermodulation).
//  New parameters must be APPENDED at the end of build() so saved projects keep their mapping.
// =====================================================================================================
namespace prm
{
    enum Kind { F, I, B, C };

    struct Def
    {
        juce::String id, name;
        Kind kind;
        float mn, mx, def, centre;     // centre: skew centre (0 = linear)
        juce::String unit;
        juce::StringArray choices;
        bool dest = false;             // modulation destination
        juce::String group;            // UI / browser group
    };

    constexpr int NSNAP = 4, NPERC_MODELS = 13, FIRST_808 = 7;
    constexpr int NLFO = 8, NENV = 4, NRND = 4, NFOL = 2, NLANE = 4, NMAC = 4, NPERC = 6, NSLOT = 32, NFX = 8, NPAT = 8;

    // FX module types (order of the slot choice list)
    enum FxType { fxOff = 0, fxTape, fxVinyl, fxWobble, fxDelay, fxReverb, fxFilter, fxReso, fxRing, fxCrush,
                  fxChorus, fxPhaser, fxComp, fxStereo, fxShepard, NUM_FXTYPES };

    // glitch event types
    enum GlitchType { glRepeat = 0, glReverse, glSlice, glStop, glGate, glFreeze, glCrush, glDrop, glPitch, NUM_GLITCH };

    // modulation sources (order of the source choice list; 0 = None)
    enum Source { srcNone = 0, srcLfo1, srcEnv1 = srcLfo1 + NLFO, srcRnd1 = srcEnv1 + NENV, srcFol1 = srcRnd1 + NRND,
                  srcLane1 = srcFol1 + NFOL, srcMac1 = srcLane1 + NLANE, srcVel = srcMac1 + NMAC, srcNote, srcMW, srcAT, srcPB,
                  NUM_SOURCES };

    struct Ids
    {
        // global
        int mix, out, norm, density, bpm, width, pattern, chain, chainEnd;
        int macro[NMAC];
        // input
        int inLevel, inGate, trigThr, trigSrc;
        // texture
        int txOn, txLevel, txBed, txNoise, txResQ, txDrone, txDrift, txOct, txChord, txGrain, txGSrc, txDens, txSize,
            txSpray, txPitch, txPRand, txSpread, txFreeze, txTilt, txWin, txRev;
        // perc
        int pcOn, pcLevel;
        int pcLane[NPERC], pcModel[NPERC], pcExc[NPERC], pcTune[NPERC], pcDecay[NPERC], pcMat[NPERC], pcInh[NPERC],
            pcPos[NPERC], pcLvl[NPERC], pcPan[NPERC], pcHum[NPERC], pcProb[NPERC], pcEvery[NPERC], pcOffs[NPERC];
        // shepard generator
        int shOn, shLevel, shPartials, shInterval, shCents, shSync, shRate, shBars, shDir, shBase, shWidth, shWave;
        // glitch
        int glOn, glMix, glLane, glProb, glLen, glW[NUM_GLITCH], glRptDiv, glRptDecay, glRptPitch, glRptShrink, glSlices,
            glStopTime, glGateShape, glGateDiv, glBits, glSRate, glPitch, glXfade, glFreezeSize;
        // lanes
        int lnOn[NLANE], lnMode[NLANE], lnSteps[NLANE], lnPulses[NLANE], lnRot[NLANE], lnDiv[NLANE], lnRatio[NLANE],
            lnSync[NLANE], lnHz[NLANE], lnSwing[NLANE], lnProb[NLANE], lnRatch[NLANE], lnRProb[NLANE], lnSrc[NLANE],
            lnCond[NLANE], lnHum[NLANE];
        // fx chain
        int fxSlot[NFX];
        int tpOn, tpMix, tpDrive, tpSat, tpBias, tpSpeed, tpHiss, tpComp, tpDrop, tpFlut, tpAge, tpMode;
        int vnOn, vnMix, vnDust, vnCrackle, vnScratch, vnRumble, vnHum, vnHumHz, vnYear, vnWear, vnWarp, vnRpm, vnBrake, vnMono, vnNoise;
        int wbOn, wbMix, wbAmt, wbBal, wbWow, wbFlut, wbFlux, wbSpread;
        int dlOn, dlMix, dlMode, dlSync, dlDiv, dlMs, dlFb, dlLp, dlHp, dlSat, dlWob, dlPitch, dlTaps, dlFreeze, dlDuck;
        int rvOn, rvMix, rvMode, rvSize, rvDecay, rvPre, rvDamp, rvMod, rvDiff, rvShim, rvShInt, rvFreeze, rvDuck, rvLow;
        int ftOn, ftMix, ftType, ftSlope, ftCut, ftRes, ftDrive, ftVowel;
        int rsOn, rsMix, rsChord, rsOct, rsDecay, rsBright, rsSpread;
        int rfOn, rfMix, rfMode, rfFreq, rfShift;
        int crOn, crMix, crBits, crRate, crJitter;
        int chOn, chMix, chRate, chDepth, chVoices;
        int phOn, phMix, phRate, phDepth, phFb, phStages, phCentre;
        int cpOn, cpMix, cpThr, cpRatio, cpAtt, cpRel, cpGain, cpTAtk, cpTSus;
        int stOn, stWidth, stHaas, stPanRate, stPanDepth, stMonoHz;
        int sfOn, sfMix, sfMode, sfSync, sfRate, sfBars, sfDir, sfRes, sfBands;
        // tuning
        int tuScale, tuRoot, tuSnap, tuMts;
        // modulators
        int lfShape[NLFO], lfSync[NLFO], lfRate[NLFO], lfDiv[NLFO], lfPhase[NLFO], lfSkew[NLFO], lfSmooth[NLFO],
            lfMode[NLFO], lfRetrig[NLFO], lfAmp[NLFO], lfUni[NLFO];
        int enTrig[NENV], enA[NENV], enH[NENV], enD[NENV], enS[NENV], enR[NENV], enCurve[NENV];
        int rnMode[NRND], rnSync[NRND], rnRate[NRND], rnDiv[NRND], rnSmooth[NRND];
        int foSrc[NFOL], foAtt[NFOL], foRel[NFOL], foGain[NFOL];
        int mxSrc[NSLOT], mxVia[NSLOT], mxDst[NSLOT], mxAmt[NSLOT], mxCurve[NSLOT];
        int trim;     // appended in 1.0
        // 1.1: chord progression of the texture drone (TEXTURE chord = Scale)
        int prOn, prCount, prSync, prDiv, prSecs, prLen[4], prMode, prGlide, prVoicing;
        // 1.1: per-voice percussion filter
        int pcFType[NPERC], pcCut[NPERC], pcRes[NPERC];
    };

    const Ids& ids();
    const std::vector<Def>& defs();
    inline int count() { return (int) defs().size(); }
    inline const Def& def (int i) { return defs()[(size_t) i]; }
    int indexOf (const juce::String& id);                 // -1 if unknown
    const std::vector<int>& dests();                       // destination list (dst choice k -> dests()[k-1])
    int destIndexOf (int paramIndex);                      // choice index (1-based) of a param, 0 if not a destination
    const juce::StringArray& sourceNames();
    bool sourceIsBipolar (int src);
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // shared choice lists
    const juce::StringArray& fxNames();
    const juce::StringArray& glitchNames();
    const juce::StringArray& syncDivNames();               // LFO / random sync divisions
    float syncDivBeats (int i);
    const juce::StringArray& noteDivNames();               // delay / repeat divisions
    float noteDivBeats (int i);
    const juce::StringArray& laneDivNames();
    float laneDivBeats (int i);
    const juce::StringArray& ratioNames();
    float ratioFactor (int i);                             // step length multiplier
    const juce::StringArray& barNames();
    float barBeats (int i);
}
