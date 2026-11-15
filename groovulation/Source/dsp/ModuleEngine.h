#pragma once
#include "LoopLibrary.h"
#include "Fx.h"
#include <memory>

namespace signalsmith { namespace stretch { template<typename Sample, class RandomEngine> struct SignalsmithStretch; } }

// Volume automation ("clip gain") breakpoints: x = position in the loop region (0..1), y = 0..1 (0.889 = 0 dB)
struct VolPoint { float x, y; };
using VolEnvelope = std::vector<VolPoint>;
using VolPtr = std::shared_ptr<const VolEnvelope>;
inline float volYToGain (float y) { return y <= 0.001f ? 0.0f : fx::dbToGain (-48.0f + 54.0f * y); }
constexpr float volUnityY = 48.0f / 54.0f;

using LoopPool = std::vector<LoopPtr>;
using PoolPtr = std::shared_ptr<const LoopPool>;

// Parameter snapshot for one module (read once per block)
struct ModuleParams
{
    bool on = true;
    float gainDb = -6, pan = 0, atkMs = 0, decMs = 500, sus = 1, relMs = 0;
    float pitch = 0, tempo = 1;
    int ftype = 0; float cut = 20000, res = 0.1f, dirt = 0;
    int dtime = 8; float dfb = 0.35f, dmix = 0; bool dpp = false;
    float rsize = 0.5f, rdecay = 2, rdamp = 0.4f, rpre = 10, rmix = 0;
    int lfoRate[lb::numLfos] { 10, 10, 10 }, lfoShape[lb::numLfos] {}, lfoTarget[lb::numLfos] {}; float lfoDepth[lb::numLfos] {};
    int grid = 5, steps = 0;
    bool slice = false; float sStart = 0, sEnd = 1; bool fade = true; bool volA = false;
    bool usesLoopLfo() const { for (int k = 0; k < lb::numLfos; ++k) if (lfoTarget[k] == lb::tLoop && std::abs (lfoDepth[k]) > 0.0005f) return true; return false; }
};

// Context for one block
struct BlockContext
{
    bool playing = false;
    bool jumped = false;          // transport started / relocated / resync
    double ppq = 0;               // ppq at sample 0
    double ppqPerSample = 0;
    double bpm = 120;
    bool raw = false;
    float semitones = 0;          // final transposition (manual pitch or SCALE result) incl. tuning correction
    float swing = 0;              // 0..1 (0 = off)
    int swingGrid = 1;            // 0 = 1/8, 1 = 1/16
    double rateMul = 1.0;         // turntable speed (pitch fader, 33/45): changes tempo AND pitch of every module
};

// -------------------------------------------------------------------------------------------------
//  One loop module, locked to the host position. Two playback engines:
//   BEATS   (drums)  - the loop is cut at its transients (plus a 1/16 safety grid); every slice is
//                      replayed untouched at its (groove-shifted) time, like Ableton's "Beats" warp
//                      mode + Groove: swing moves slices instead of stretching audio.
//   COMPLEX (others) - polyphonic phase-vocoder time-stretch / pitch-shift (Signalsmith Stretch,
//                      "Complex Pro" class); swing is a smooth (C1-continuous) time warp, no steps.
//  Loop changes (LOOP SELECT, SIMILAR, LFO -> Loop) crossfade between two voices.
//  Plus slice gate, ADSR, clip-gain automation, filter, dirt, delay, reverb and 3 LFOs.
// -------------------------------------------------------------------------------------------------
class ModuleEngine
{
public:
    ModuleEngine();
    ~ModuleEngine();
    void prepare (double sampleRate, int maxBlock);
    void setLoop (LoopPtr l);                                                           // audio thread
    const LoopPtr& getLoop() const;
    void setVolEnvelope (VolPtr v) { std::atomic_store (&vol, std::move (v)); }         // any thread
    void setPool (PoolPtr p) { std::atomic_store (&pool, std::move (p)); }              // any thread (LFO -> Loop)
    // 1 = BEATS (drums), 0 = COMPLEX, -1 = decided per loop (CUSTOM: unpitched material -> BEATS)
    void setMode (int m) { mode = m; }

    // renders into outL/outR (replaces their content)
    void process (float* outL, float* outR, int n, const BlockContext& ctx, const ModuleParams& p);

    static double regionBeats (const LoopData& l, const ModuleParams& p);
    static double swingSmooth (double outBeat, float swing, int grid);   // COMPLEX: output time -> source time
    static double swingShift (double srcBeat, float swing, int grid);    // BEATS: slice start -> played time

    // UI feedback
    std::atomic<float> playPos { -1.0f };   // position in region 0..1
    std::atomic<float> level { 0.0f };
    std::atomic<int> sameSliceRestarts { 0 };   // diagnostics (tests): a slice restarted over itself = glitch

private:
    using Stretch = signalsmith::stretch::SignalsmithStretch<float, void>;
    struct Voice
    {
        LoopPtr loop;
        std::unique_ptr<Stretch> st;
        bool isBeats = false;
        double feedBeat = 0.0; bool needSeek = true; float semisSm = 0.0f; bool semisInit = false;
        // beats
        std::vector<double> segBeat, segSched; std::vector<int> segStart, segEnd;
        float schedSwing = -1.0f; int schedGrid = -1;
        long long curId = 0; int curSeg = -1; double curSrc = 0; float curGain = 1.0f;
        bool prevOn = false; double prevSrc = 0; float prevGain = 0;
        void buildSegments();
    };
    void renderVoice (Voice& v, float* l, float* r, int s0, int m, const ModuleParams& p, const BlockContext& ctx, float semis, double region);
    void renderBeats (Voice& v, float* l, float* r, int s0, int m, float semis, double region, const BlockContext& ctx);
    void renderComplex (Voice& v, float* l, float* r, int s0, int m, float semis, double region, const BlockContext& ctx);
    void readSource (const LoopData& L, double startBeat, int count, double bps, double region, float* l, float* r) const;
    double loopBeat (double q, const LoopData& L, const BlockContext& ctx) const;
    float lfoValue (int idx, double ppq, const ModuleParams& p);
    void switchTo (LoopPtr l);

    double sr = 44100.0;
    int mode = 0;
    void assign (Voice& v, LoopPtr l);
    Voice voices[2];
    int active = 0;
    float xfade = 1.0f;            // 1 = only the active voice
    LoopPtr pendingSwitch;
    VolPtr vol;
    PoolPtr pool;
    // musical position: q = ppq + offset (offset grows when TEMPO / LFO / turntable speed differ from 1)
    double offset = 0.0;
    std::vector<double> qArr;
    std::vector<float> tmpL, tmpR, inL, inR;
    double lastLoopStep = -1.0;
    // fx
    fx::Svf svf[2];
    float logCutSm = std::log (20000.0f), resSm = 0.1f, gainSm = 0.5f, panSm = 0.0f;
    bool filterActive = false;
    fx::GentleSat dirt;
    fx::SyncDelay delay;
    fx::FdnReverb reverb;
    long long shCycle[lb::numLfos] { -1, -1, -1 }; float shVal[lb::numLfos] {};
};
