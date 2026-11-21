#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <array>
#include "DspCommon.h"
#include "Analog303.h"
#include "Circuits.h"
#include "Wavetables.h"
#include "../Params.h"

// =====================================================================================================
//  5ynth voice engine: up to 5 synth modules (A..E) per voice. Every module is a complete synth:
//   engine  Analog (poly-BLEP VCOs, sync, x-mod, ring, sub, unison + drift)
//           FM 6-op (32 DX7-style algorithms, per-operator ADSR, feedback)
//           Wavetable / Supersaw / Phase Distortion / Additive
//   filter  LP24 ladder, LP 303 diode (2x oversampled, accent sweep), LP12 / BP / HP / LP24 clean / notch
//   drive   Tube / RAT / DS-1 pedal models (2x oversampled), envelopes, LFO, pitch env, lo-fi, delay.
// =====================================================================================================
struct ModCtx
{
    const float* v = nullptr;   // snapshot + base of this module (v[suffix])
    const float* g = nullptr;   // whole snapshot (globals)
    float sr = 48000;
    float bend = 0, mw = 0;
    const float* lfo = nullptr; // per-sample module LFO
    const float* vib = nullptr;
    dsp::Circuit circuit = dsp::Circuit::Generic;   // filter circuit of the module's synth model
};

struct FmAlgorithms
{
    struct Alg { uint8_t mods[6]; uint8_t carriers; int fb; };
    Alg a[32];
    static const FmAlgorithms& get();
private:
    FmAlgorithms();
};

struct ModulePart
{
    static constexpr int maxUni = 7, numPartials = 40;
    // oscillators
    float ph1[maxUni] {}, ph2[maxUni] {}, o2prev[maxUni] {}, phSub = 0;
    float fmPh[maxUni][6] {}, fbHist[maxUni][2] {};
    dsp::ADSR opEnv[6];
    float ssPh[7] {}, ssLp = 0;
    float pRatio[numPartials] {}, pAmp[numPartials] {}, pSus[numPartials] {}, pDyn[numPartials] {}, pCoef[numPartials] {}, pPh[numPartials] {};
    int numP = 0;
    // anti-click glides at note changes of a sounding voice: partial gains (additive), FM index scale (key / velocity)
    float pSusSm[numPartials] {}, pAmpSm[numPartials] {}, fmIdx = -1.0f;
    // per-sample glides of parameters that act on the waveform itself (FM levels / feedback, wavetable position,
    // phase-distortion amount): the parameter snapshot changes every 32 samples, which made small steps audible
    float lvlSm[6] { -1, -1, -1, -1, -1, -1 }, fbSm = -1.0f, wposSm = -1.0f, pdSm = -1.0f;
    static float glide (float& state, float target, float k) { state = state < -0.5f ? target : state + (target - state) * k; return state; }
    // filters / drive
    dsp::Ladder lad[2];
    dsp::Diode303 d303[2];
    dsp::MoogLadder moog[2];
    dsp::Svf svf[2][2];
    dsp::PedalDist dist[2];
    float prevIn[2] {};
    dsp::DcBlock dc[2];
    dsp::ADSR fenv, aenv;
    dsp::Rng rng;
    float penv = 0, drift = 0, driftTarget = 0, lfoFade = 0, vel = 1, accent = 0, accSweep = 0;
    int driftCount = 0;
    bool active() const { return aenv.active(); }
    void start (float velocity, bool hard, const ModCtx& c, float note, uint32_t seed);
    static float releaseOf (const ModCtx& c, float decayMs, float releaseMs);
    void release() { fenv.gateOff(); aenv.gateOff(); for (auto& e : opEnv) e.gateOff(); }
    void kill() { fenv.kill(); aenv.kill(); for (auto& e : opEnv) e.kill(); }
    void render (float* L, float* R, int n, const float* notes, const ModCtx& c);
private:
    void setupAdditive (const ModCtx& c, float note);
};

struct Voice
{
    int note = -1;
    float vel = 0;
    bool gate = false, sustained = false;
    uint64_t age = 0;
    float curNote = 60, targetNote = 60;
    ModulePart part[params::numModules];
    bool use[params::numModules] {};
    bool active() const { for (int m = 0; m < params::numModules; ++m) if (use[m] && part[m].active()) return true; return false; }
};

class SynthEngine
{
public:
    static constexpr int numVoices = 16;
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void render (float* L, float* R, int n, const juce::MidiBuffer& midi, const float* v, double bpm);
    void allNotesOff();
    int activeVoices() const;
    float moduleLevel (int m) const { return modMeter[(size_t) m]; }
    // which circuit models each synth model (index of the Synth Model menu) uses
    void setModelCircuit (int modelIndex, dsp::Circuit c) { if (juce::isPositiveAndBelow (modelIndex, (int) modelCircuit.size())) modelCircuit[(size_t) modelIndex] = c; }

private:
    void handleMidi (const juce::MidiMessage& m, const float* v);
    void noteOn (int note, float vel, const float* v);
    void noteOff (int note, const float* v);
    void renderVoices (int start, int n, const float* v);
    void startVoice (Voice& vc, int note, float vel, bool hard, const float* v, bool glideFromLast);
    bool moduleUsed (int m, const float* v) const;

    float sr = 48000.0f;
    Voice voices[numVoices];
    std::array<dsp::Circuit, 128> modelCircuit {};
    uint64_t counter = 0;
    std::vector<int> held;
    bool sustainPedal = false;
    float bend = 0, mw = 0, lastNote = -1;
    dsp::LFO lfo[params::numModules];
    float vibPh = 0;
    std::vector<float> bus[params::numModules][2], noteBuf, lfoBuf[params::numModules], vibBuf, dly[params::numModules][2];
    int dlyW[params::numModules] {};
    float dlyTime[params::numModules] {}, dlyLp[params::numModules][2] {};
    dsp::DcBlock dcBus[params::numModules][2];
    float srrHold[params::numModules][2] {}, srrCount[params::numModules] {};
    std::array<float, params::numModules> modMeter {};
};
