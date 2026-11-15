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

// Parameter snapshot for one module (read once per block)
struct ModuleParams
{
    bool on = true;
    float gainDb = 0, pan = 0, shaper = 1, atkMs = 0, decMs = 500, sus = 1, relMs = 0;
    float pitch = 0, tempo = 1;
    int ftype = 0; float cut = 20000, res = 0.1f, dirt = 0;
    int dtime = 8; float dfb = 0.35f, dmix = 0; bool dpp = false;
    float rsize = 0.5f, rdecay = 2, rdamp = 0.4f, rpre = 10, rmix = 0;
    int lfoRate[2] { 10, 10 }, lfoShape[2] {}, lfoTarget[2] {}; float lfoDepth[2] {};
    int grid = 5, steps = 0;
    bool slice = false; float sStart = 0, sEnd = 1; bool fade = true; bool volA = false;
};

// Context for one block
struct BlockContext
{
    bool playing = false;
    bool jumped = false;          // transport started / relocated
    double ppq = 0;               // ppq at sample 0
    double ppqPerSample = 0;
    double bpm = 120;
    bool raw = false;
    float semitones = 0;          // final transposition (manual pitch or SCALE result) incl. tuning correction
    float swing = 0;              // 0..1
    int swingGrid = 1;            // 0 = 1/8, 1 = 1/16
};

// -------------------------------------------------------------------------------------------------
//  One loop module: tempo/pitch independent playback (Signalsmith Stretch, phase-vocoder with
//  transient handling), locked to the host's musical position (ppq), plus groove warping, slice,
//  ADSR, clip-gain automation, transient shaper, filter, dirt, delay, reverb and 2 LFOs.
// -------------------------------------------------------------------------------------------------
class ModuleEngine
{
public:
    ModuleEngine();
    ~ModuleEngine();
    void prepare (double sampleRate, int maxBlock);
    void setLoop (LoopPtr l) { loop = std::move (l); needSeek = true; }
    const LoopPtr& getLoop() const { return loop; }
    void setVolEnvelope (VolPtr v) { std::atomic_store (&vol, std::move (v)); }   // any thread

    // renders into outL/outR (replaces their content)
    void process (float* outL, float* outR, int n, const BlockContext& ctx, const ModuleParams& p);

    // beats of the loop region (grid steps x grid, or the loop's own length)
    static double regionBeats (const LoopData& l, const ModuleParams& p);
    // unwarped "loop beat" position for a ppq (incl. RAW tempo, tempo factor and groove)
    static double loopBeatAt (double ppq, const LoopData& l, const BlockContext& ctx, const ModuleParams& p);
    static double swingWarp (double ppq, float swing, int grid);

    // UI feedback
    std::atomic<float> playPos { -1.0f };   // position in region 0..1
    std::atomic<float> level { 0.0f };

private:
    using Stretch = signalsmith::stretch::SignalsmithStretch<float, void>;
    void readSource (double startBeat, int count, double beatsPerSample, float* l, float* r) const;
    void doSeek (double targetBeat, double beatsPerSample);
    float lfoValue (int idx, double ppq, const ModuleParams& p);

    double sr = 44100.0;
    LoopPtr loop;
    VolPtr vol;
    ModuleParams lastParams;
    std::unique_ptr<Stretch> stretch;
    std::vector<float> inL, inR, tmpL, tmpR;
    double feedBeat = 0.0;        // loop-beat position of the next input sample
    bool needSeek = true;
    float semisSm = 0.0f;
    bool semisInit = false;
    fx::Svf svf[2];
    float cutSm = 20000.0f, resSm = 0.1f, gainSm = 1.0f, panSm = 0.0f;
    fx::GentleSat dirt;
    fx::SyncDelay delay;
    fx::FdnReverb reverb;
    long long shCycle[2] { -1, -1 }; float shVal[2] {};
    juce::Random rnd;
};
