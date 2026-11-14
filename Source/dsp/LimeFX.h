#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include "MicroEngine.h"   // TptSvf

// Shared per-block context for the Section B effects
struct FxContext
{
    double sr = 44100.0, bpm = 120.0, ppq = 0.0, ppqPerSample = 0.0;
    bool playing = false;
};

// Circular stereo buffer with fractional read
struct DelayLine
{
    std::vector<float> l, r;
    int w = 0, size = 1;
    void prepare (int n) { size = juce::jmax (4, n); l.assign ((size_t) size, 0.0f); r.assign ((size_t) size, 0.0f); w = 0; }
    void clear() { std::fill (l.begin(), l.end(), 0.0f); std::fill (r.begin(), r.end(), 0.0f); }
    inline void push (float a, float b) { l[(size_t) w] = a; r[(size_t) w] = b; if (++w >= size) w = 0; }
    inline float read (const std::vector<float>& buf, float delay) const
    {
        float rp = (float) w - 1.0f - delay;
        while (rp < 0) rp += (float) size;
        const int i0 = (int) rp % size; const int i1 = (i0 + 1) % size; const float f = rp - std::floor (rp);
        return buf[(size_t) i0] + f * (buf[(size_t) i1] - buf[(size_t) i0]);
    }
    inline float readL (float d) const { return read (l, d); }
    inline float readR (float d) const { return read (r, d); }
};

// FILTER: low-pass with envelope follower (CUT, TRK, SM, MOD)
class EnvFilter
{
public:
    void prepare (double sr) { fs = sr; f[0].reset(); f[1].reset(); env = 0; }
    void process (float* L, float* R, int n, float cutHz, float trk, float sm, float modAmt, float modValue);
private:
    double fs = 44100.0; TptSvf f[2]; float env = 0.0f;
};

// EQ: 3-band split EQ + smooth saturator (HI, MID, LO, AMP, SPLIT)
class EqSat
{
public:
    void prepare (double sr) { fs = sr; for (auto& x : lo) x.reset(); for (auto& x : hi) x.reset(); }
    void process (float* L, float* R, int n, float hiDb, float midDb, float loDb, float ampDb, float split);
private:
    double fs = 44100.0; TptSvf lo[2], hi[2];
};

// MAGIC: granular pitch shifter with notch (PITCH, GR, MOD, RMX, MIX, NOTCH)
class Magic
{
public:
    void prepare (double sr);
    void process (float* L, float* R, int n, float semis, float grain, float mix, float notchHz);
private:
    double fs = 44100.0; DelayLine buf; double phase = 0.0; TptSvf notch[2];
};

// DELAY: stereo tape-like delay (ONE, TWO in 16ths, FB, GAIN, LFO, FM, DM, WIDE, CUT, RES, LO)
class TapeDelay
{
public:
    void prepare (double sr);
    // in: send signal, out: wet only
    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const FxContext& c,
                  int one, int two, float fb, float lfoHz, float fm, float dm, float wide, float cutHz, float res, bool lowcut);
private:
    double fs = 44100.0; DelayLine dl; double lfo = 0.0; float tL = 0, tR = 0; TptSvf lp[2], hp[2];
};

// XFX: 23-mode multi effect (mode, X/Y pad, SPD, LEN, LO, HI, play/rec, SM)
class XFx
{
public:
    void prepare (double sr);
    void process (float* L, float* R, int n, const FxContext& c, int mode, float x, float y, float spd, int len16,
                  float loHz, float hiHz, bool play, bool rec, float smooth);
private:
    void reset();
    double fs = 44100.0;
    int lastMode = -1;
    TptSvf svf[2], band[2][3], post[2][2];
    float ap[2][6] {};            // phaser allpass states
    float apFb[2] {};
    DelayLine dl, loop;            // general delay + loop buffer (freeze/chop/slice)
    std::array<DelayLine, 1> spare;
    double ph = 0.0, ph2 = 0.0;
    float hold[2] {}; int holdCount = 0;
    // reverb
    std::array<std::vector<float>, 8> comb; std::array<int, 8> combIdx {}; std::array<float, 8> combLp {};
    std::array<std::vector<float>, 4> allp; std::array<int, 4> apIdx {};
    // loop / slicer
    int loopLen = 1, loopPos = 0, writePos = 0; bool frozen = false; float xf = 1.0f;
    std::array<int, 16> sliceMap {}; long long lastSlice = -1;
    float spinRate = 1.0f; double spinPos = 0.0;
    uint32_t rng = 0x2468ace;
    float rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (float) rng / 4294967295.0f; }
};
