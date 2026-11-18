#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include "DspCommon.h"
#include "Wavetables.h"
#include "../Params.h"

// =====================================================================================================
//  TWINSYNTH voice engine.
//   Section A = analog subtractive model (poly-BLEP VCOs, sync, cross-mod, sub, unison + drift, ZDF ladder /
//               SVF with saturation, analog-curve envelopes, accent).
//   Section D = digital engines (4-op FM, band-limited wavetables, JP-style supersaw, CZ-style phase
//               distortion, additive with per-partial envelopes), clean SVF, bit / rate reduction.
//   A preset can use A, D, or both in layer (each voice then runs both parts).
// =====================================================================================================
struct BlockCtx
{
    const float* v = nullptr;       // parameter snapshot (params::NUM real values)
    float sr = 48000.0f;
    float bend = 0.0f;              // semitones
    float mw = 0.0f;                // mod wheel 0..1
    const float* lfoA = nullptr;    // per-sample section LFOs (-1..1)
    const float* lfoD = nullptr;
    const float* vib = nullptr;     // per-sample vibrato (semitones)
};

struct AnalogPart
{
    static constexpr int maxUni = 7;
    float ph1[maxUni] {}, ph2[maxUni] {}, o2prev[maxUni] {}, phSub = 0;
    dsp::Ladder lad[2];
    dsp::Svf svf[2];
    dsp::ADSR fenv, aenv;
    dsp::Rng rng;
    float penv = 0, drift = 0, driftTarget = 0, lfoFade = 0, vel = 1, accent = 0;
    int driftCount = 0;

    void start (float velocity, bool hard, const BlockCtx& c, uint32_t seed);
    void release() { fenv.gateOff(); aenv.gateOff(); }
    bool active() const { return aenv.active(); }
    void render (float* L, float* R, int n, const float* notes, const BlockCtx& c);
};

struct DigitalPart
{
    static constexpr int maxUni = 7, numPartials = 40;
    float ph[maxUni][4] {};          // FM operators / wavetable / PD phases
    float fbHist[maxUni][2] {};
    float opLvl[4] {}; bool opAtk[4] {};
    float ssPh[7] {}; float ssLp = 0;
    // additive
    float pRatio[numPartials] {}, pAmp[numPartials] {}, pSus[numPartials] {}, pDyn[numPartials] {}, pCoef[numPartials] {}, pPh[numPartials] {};
    int numP = 0;
    dsp::Svf svf[2][2];
    dsp::ADSR fenv, aenv;
    dsp::Rng rng;
    dsp::DcBlock dc[2];
    float penv = 0, lfoFade = 0, vel = 1;

    void start (float velocity, bool hard, const BlockCtx& c, float note, uint32_t seed);
    void release() { fenv.gateOff(); aenv.gateOff(); }
    bool active() const { return aenv.active(); }
    void render (float* L, float* R, int n, const float* notes, const BlockCtx& c);
    void setupAdditive (const BlockCtx& c, float note);
};

struct Voice
{
    int note = -1;
    float vel = 0;
    bool gate = false, sustained = false;
    uint64_t age = 0;
    float curNote = 60, targetNote = 60;
    AnalogPart a;
    DigitalPart d;
    bool useA = true, useD = false;
    bool active() const { return (useA && a.active()) || (useD && d.active()); }
};

class SynthEngine
{
public:
    static constexpr int numVoices = 16;
    void prepare (double sampleRate, int maxBlock);
    void reset();
    // v = parameter snapshot; renders (replacing) into L/R
    void render (float* L, float* R, int n, const juce::MidiBuffer& midi, const float* v);
    int activeVoices() const;
    void allNotesOff();

private:
    void handleMidi (const juce::MidiMessage& m, const float* v);
    void noteOn (int note, float vel, const float* v);
    void noteOff (int note, const float* v);
    void renderVoices (int start, int n, const float* v);
    void startVoice (Voice& vc, int note, float vel, bool hard, const float* v, bool glideFromLast);

    float sr = 48000.0f;
    Voice voices[numVoices];
    uint64_t counter = 0;
    std::vector<int> held;          // mono note stack
    bool sustainPedal = false;
    float bend = 0, mw = 0, lastNote = -1;
    dsp::LFO lfoA, lfoD;
    float vibPh = 0;
    std::vector<float> busA[2], busD[2], noteBuf, lfoBufA, lfoBufD, vibBuf;
    dsp::DcBlock dcA[2], dcD[2];
    float srrHold[2] {}; float srrCount = 0;
    dsp::Rng rng;
};
