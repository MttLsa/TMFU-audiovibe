#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Params.h"
#include "Common.h"
#include "Tuning.h"
#include "Modulators.h"
#include "Lanes.h"
#include "ModalPerc.h"
#include "Texture.h"
#include "Shepard.h"
#include "Glitch.h"
#include "Fx.h"

// =====================================================================================================
//  MICROVIBE engine: sources -> glitch -> 8-slot FX chain, driven by the clock / lanes and by the
//  modulation system. Parameters are re-evaluated every control block of 32 samples:
//      effective = base + sum(matrix contributions)   (in normalised 0..1 space, clamped)
//  Matrix amounts and modulator parameters are destinations too (they use the previous block's values).
// =====================================================================================================
namespace mv
{
    struct Transport { bool hostRunning = false, hasHost = false, forceRun = false; double ppq = 0, bpm = 124; };

    // lane grids and custom LFO shapes, written by the editor, read by the audio thread
    struct SharedData
    {
        std::atomic<uint8_t> gate[prm::NPAT][prm::NLANE][kMaxSteps];
        std::atomic<float>   value[prm::NPAT][prm::NLANE][kMaxSteps];
        std::atomic<float>   lfoShape[prm::NLFO][16];
        std::atomic<uint64_t> snapMask[prm::NSNAP];      // texture drone chords: bit k = scale degree k (0 = automatic)
        SharedData()
        {
            for (int p = 0; p < prm::NPAT; ++p) for (int l = 0; l < prm::NLANE; ++l) for (int s = 0; s < kMaxSteps; ++s)
            { gate[p][l][s] = (uint8_t) ((s % 4) == 0); value[p][l][s] = (s % 4) == 0 ? 0.8f : 0.5f; }
            for (int k = 0; k < prm::NLFO; ++k) for (int i = 0; i < 16; ++i)
                lfoShape[k][i] = 0.5f + 0.5f * std::sin (kTwoPi * (float) i / 16.0f + (float) k);
            for (auto& m : snapMask) m = 0;
        }
    };

    // values shown by the editor
    struct Display
    {
        std::vector<std::atomic<float>> modNorm;          // per parameter: modulation offset (normalised)
        std::atomic<float> src[prm::NUM_SOURCES] {};
        std::atomic<int> laneStep[prm::NLANE] {};
        std::atomic<int> pattern { 0 }, glitchType { -1 }, snapshot { 0 };
        std::atomic<bool> running { true };
        std::atomic<float> peakL { 0 }, peakR { 0 }, inPeak { 0 };
        std::atomic<bool> limiting { false }, transient { false }, mts { false };
        std::atomic<double> beat { 0 };
    };

    class Engine
    {
    public:
        Engine();
        void prepare (double sampleRate, int maxBlock);
        void process (float* L, float* R, const float* scL, const float* scR, int n, const juce::MidiBuffer& midi,
                      const Transport& tr, const float* base);
        void resetState() { resetPending = true; }

        tuning::Tuning tuning;
        SharedData shared;
        Display display;

    private:
        static constexpr int kBlock = 32;
        void evalModulation (const float* base);
        void tickModulators (int n, double beat, bool running, double bpm);
        void handleMidi (const juce::MidiMessage& m);
        void laneFired (int lane, const LaneTrigger& t, double bpm);
        void onTransient();
        void triggerPerc (int v, float vel);
        void chord (int chordIdx, float rootSemis, float* out, int& n) const;
        float stepSeconds (int lane, double bpm) const;
        void processSample (float& l, float& r, float scl, float scr);
        void droneNotes (double bpm);
        void transportStopped();
        void setupBlockParams (double bpm);

        float sr = 48000;
        std::vector<juce::NormalisableRange<float>> ranges;
        std::vector<float> eff, prevEff, modAcc;
        std::vector<int> touched, prevTouched;
        std::vector<uint8_t> isTouched;
        float src[prm::NUM_SOURCES] {};
        bool resetPending = true;

        // modulators
        Lfo lfo[prm::NLFO]; Env env[prm::NENV]; RandomMod rnd[prm::NRND];
        Follower fol[prm::NFOL]; float folVal[prm::NFOL] {};
        float midiVel = 0, midiNote = 0.5f, midiMW = 0, midiAT = 0, midiPB = 0;
        int notesHeld = 0;
        bool pendMidiTrig = false, pendTransient = false, pendBar = false, pendGlitch = false;
        bool pendLane[prm::NLANE] {};
        float laneGateS[prm::NLANE] {};

        // clock & lanes
        double freeBeat = 0, lastBeat = 0, curBeat = 0, spb = 24000;
        bool running = false;
        Lane lanes[prm::NLANE];
        LaneConfig cfg[prm::NLANE];
        uint8_t gates[prm::NLANE][kMaxSteps] {};
        float values[prm::NLANE][kMaxSteps] {};
        int activePattern = 0;
        int64_t lastBar = -1;

        // transient detector
        Follower trFast, trSlow; int trHold = 0;

        // sources
        ModalVoice perc[prm::NPERC];
        int percCount[prm::NPERC] {};
        float inHist[2048] {}; int inHistPos = 0;
        Texture texture; TextureParams txp;
        ShepardGen shepard; ShepardFx shepFx;
        Glitch glitch; GlitchParams glp;
        int heldGlitch = -1;
        Rng rng { 0xD1CEu };

        // fx
        Tape tape; Vinyl vinyl; Wobble wobble; Delay delay; Reverb reverb; Filter filter; Resonator reso; RingShift ring;
        Crusher crusher; Chorus chorus; Phaser phaser; Comp comp; Stereo stereo;
        TapeParams tpp; VinylParams vnp; WobbleParams wbp; DelayParams dlp; ReverbParams rvp; FilterParams ftp; ResoParams rsp;
        RingParams rfp; CrushParams crp; ChorusParams chp; PhaserParams php; CompParams cpp; StereoParams stp;
        int order[prm::NFX] {}; int nOrder = 0;
        bool fxWasOn[prm::NUM_FXTYPES] {};
        int sfMode = 0, sfBands = 7; float sfSpeed = 0, sfRes = 0.5f, sfMix = 0;

        // drone progression
        float droneCur[8] {}, droneTarget[8] {}; int droneN = 0, curSnap = 0;
        // transport stop
        bool wasRunning = true; float stopGain = 1.0f, stopCoef = 0.001f;

        // shepard gen block params
        float shSpeed = 0, shInterval = 12; int shDirSign = 1; double shPendulumBeat = 0;

        // output
        float mixSm = 1, outSm = 1, widthSm = 1, normGain = 1, normRms = 0;
        Limiter limiter;
        float inGateEnv = 0, inGateGain = 1;
        float pkL = 0, pkR = 0, pkIn = 0; bool limited = false;
    };
}
