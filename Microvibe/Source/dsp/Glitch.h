#pragma once
#include "Common.h"

// =====================================================================================================
//  GLITCH ENGINE: an 8 s stereo capture buffer is recorded continuously; rhythmic events (from a lane,
//  or held MIDI notes) replace the live signal with repeats, reversed / shuffled slices, tape stops,
//  gates, frozen grains, crushes, dropouts or pitched loops. Every cut is crossfaded.
// =====================================================================================================
namespace mv
{
    struct GlitchParams
    {
        float mix = 1, rptBeats = 0.25f, rptDecay = 0.2f, rptPitch = 0, rptShrink = 0, stopTime = 0.6f, gateBeats = 0.125f,
              bits = 6, srate = 0.5f, pitch = 7, xfadeMs = 2, freezeMs = 60, samplesPerBeat = 24000;
        int slices = 8, gateShape = 0;
    };

    class Glitch
    {
    public:
        void prepare (float sampleRate)
        {
            sr = sampleRate;
            bl.prepare ((int) (sr * 8)); br.prepare ((int) (sr * 8));
            active = false; env = 0;
        }

        int activeType() const { return active ? type : -1; }

        void start (int t, float lengthSamples, const GlitchParams& p)
        {
            type = t; active = true; t0 = 0; len = std::max (64.0f, lengthSamples); held = false;
            startW = bl.w;
            rep = 0; repStart = 0; repLen = std::max (32.0f, p.rptBeats * p.samplesPerBeat); repAmp = 1; repRatio = 1;
            readPos = 0; rate = 1;
            sliceLen = std::max (64.0f, p.rptBeats * p.samplesPerBeat); sliceIdx = -1; sliceOff = 0; sliceRev = false;
            loopLen = std::max (64.0f, p.freezeMs * 0.001f * sr);
            if (t == 8) loopLen = std::max (128.0f, std::min (sliceLen, sr * 0.25f));
            loopPh = 0; holdCount = 0; holdL = holdR = 0;
            dropOn = false; dropCount = 0;
        }
        void startHeld (int t, const GlitchParams& p) { start (t, 1.0e9f, p); held = true; }
        void release() { if (active && ! held) len = std::min (len, t0 + xfade); }
        void stopHeld (int t) { if (active && held && type == t) { len = t0 + xfade; held = false; } }

        void process (float& l, float& r, const GlitchParams& p)
        {
            bl.write (l); br.write (r);
            xfade = std::max (8.0f, p.xfadeMs * 0.001f * sr);
            if (! active) { env = std::max (0.0f, env - 1.0f / xfade); return; }
            float gL = l, gR = r;
            const double now = (double) bl.w - 1;
            switch (type)
            {
                case 0: // ---------------------------------------------------- repeat / stutter roll
                {
                    float local = t0 - repStart;
                    if (local >= repLen)
                    {
                        ++rep; repStart = t0; local = 0;
                        repLen = std::max (32.0f, repLen * (1.0f - 0.5f * p.rptShrink));
                        repAmp *= 1.0f - 0.5f * p.rptDecay;
                        repRatio = semiToRatio (p.rptPitch * std::min (1.0f, rep / 8.0f));
                        readPos = 0;
                    }
                    const double pos = (double) startW + (rep == 0 ? local : std::fmod (readPos, (double) repLen));
                    readPos += rep == 0 ? 1.0 : repRatio;
                    const float w = std::min (1.0f, std::min (local, repLen - local) / std::max (8.0f, xfade * 0.5f));
                    gL = bl.read (std::min (pos, now - 3)) * repAmp * w;
                    gR = br.read (std::min (pos, now - 3)) * repAmp * w;
                    break;
                }
                case 1: // ---------------------------------------------------- reverse
                {
                    const double pos = (double) startW - 4 - t0;
                    gL = bl.read (pos); gR = br.read (pos);
                    break;
                }
                case 2: // ---------------------------------------------------- slice shuffle
                {
                    const int idx = (int) (t0 / sliceLen);
                    if (idx != sliceIdx)
                    {
                        sliceIdx = idx;
                        sliceOff = 1 + rng.below (std::max (1, p.slices));
                        sliceRev = rng.chance (0.3f);
                    }
                    const float local = t0 - (float) idx * sliceLen;
                    const double base = (double) startW - (double) sliceOff * sliceLen;
                    const double pos = sliceRev ? base + sliceLen - 1 - local : base + local;
                    const float w = std::min (1.0f, std::min (local, sliceLen - local) / std::max (8.0f, xfade * 0.5f));
                    gL = bl.read (std::min (pos, now - 3)) * w; gR = br.read (std::min (pos, now - 3)) * w;
                    break;
                }
                case 3: // ---------------------------------------------------- tape stop
                {
                    const float stopLen = std::max (64.0f, p.stopTime * len);
                    const float x = std::min (1.0f, t0 / stopLen);
                    rate = (1.0f - x) * (1.0f - x);
                    readPos += rate;
                    const double pos = (double) startW + readPos;
                    const float g = std::min (1.0f, rate * 6.0f);
                    gL = bl.read (pos) * g; gR = br.read (pos) * g;
                    break;
                }
                case 4: // ---------------------------------------------------- rhythmic gate
                {
                    const float per = std::max (32.0f, p.gateBeats * p.samplesPerBeat);
                    const float ph = std::fmod (t0, per) / per;
                    float g = 1;
                    switch (p.gateShape)
                    {
                        case 0: g = ph < 0.5f ? 1.0f : 0.0f; break;
                        case 1: g = 1.0f - ph; break;
                        case 2: g = ph; break;
                        case 3: g = 1.0f - std::abs (2 * ph - 1); break;
                        default: g = ph < 0.15f ? 1.0f : 0.0f; break;
                    }
                    gateSm += (g - gateSm) * 0.05f;
                    gL = l * gateSm; gR = r * gateSm;
                    break;
                }
                case 5: case 8: // --------------------------------------------- freeze (5) / pitched loop (8)
                {
                    const float ratio = type == 8 ? semiToRatio (p.pitch) : 1.0f;
                    const double base = (double) startW - loopLen - 4;
                    loopPh += ratio / loopLen; loopPh -= std::floor (loopPh);
                    float yl = 0, yr = 0;
                    for (int h = 0; h < 2; ++h)
                    {
                        float q = loopPh + 0.5f * h; q -= std::floor (q);
                        const float g = std::sin (kPi * q);
                        const double pos = base + q * loopLen * 0.999;
                        yl += bl.read (pos) * g * g; yr += br.read (pos) * g * g;
                    }
                    gL = yl; gR = yr;
                    break;
                }
                case 6: // ---------------------------------------------------- crush
                {
                    const int hold = 1 + (int) (p.srate * p.srate * 40.0f);
                    if (holdCount <= 0) { holdL = l; holdR = r; holdCount = hold; }
                    --holdCount;
                    const float q = std::exp2 (clampv (p.bits, 1.0f, 16.0f)) * 0.5f;
                    gL = std::round (holdL * q) / q; gR = std::round (holdR * q) / q;
                    break;
                }
                case 7: // ---------------------------------------------------- dropout
                {
                    if (--dropCount <= 0) { dropOn = rng.chance (0.55f); dropCount = (int) std::max (32.0f, p.samplesPerBeat * 0.0625f * (1 + rng.below (3))); }
                    dropSm += ((dropOn ? 0.0f : 1.0f) - dropSm) * 0.02f;
                    gL = l * dropSm; gR = r * dropSm;
                    break;
                }
                default: break;
            }
            // event envelope
            const float fadeIn = std::min (1.0f, t0 / xfade);
            const float fadeOut = held ? 1.0f : std::min (1.0f, std::max (0.0f, (len - t0) / xfade));
            env = std::min (fadeIn, fadeOut);
            const float m = env * p.mix;
            l += (gL - l) * m; r += (gR - r) * m;
            t0 += 1;
            if (! held && t0 >= len) active = false;
        }

    private:
        float sr = 48000;
        Ring bl, br;
        Rng rng { 0x6117C4u };
        bool active = false, held = false;
        int type = 0;
        int64_t startW = 0;
        float t0 = 0, len = 1, xfade = 96, env = 0;
        int rep = 0; float repStart = 0, repLen = 1, repAmp = 1, repRatio = 1; double readPos = 0; float rate = 1;
        float sliceLen = 1; int sliceIdx = -1, sliceOff = 1; bool sliceRev = false;
        float loopLen = 1, loopPh = 0;
        int holdCount = 0; float holdL = 0, holdR = 0;
        bool dropOn = false; int dropCount = 0; float dropSm = 1, gateSm = 1;
    };
}
