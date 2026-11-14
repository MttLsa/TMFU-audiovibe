#include "LimeFX.h"
#include <cmath>

using juce::jlimit;
using juce::jmax;
using juce::jmin;
static constexpr double kPi = juce::MathConstants<double>::pi;

static inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }

//==============================================================================
void EnvFilter::process (float* L, float* R, int n, float cutHz, float trk, float sm, float modAmt, float modValue)
{
    const float rel = (float) std::exp (-1.0 / (fs * (0.01 + sm * 0.01 * 0.6)));
    const float atk = (float) std::exp (-1.0 / (fs * 0.002));
    const bool active = cutHz < 19500.0f || std::abs (trk) > 0.5f || std::abs (modAmt) > 0.5f;
    for (int i = 0; i < n; ++i)
    {
        const float in = jmax (std::abs (L[i]), std::abs (R[i]));
        env = in > env ? atk * env + (1 - atk) * in : rel * env + (1 - rel) * in;
        if (! active) continue;
        if ((i & 15) == 0)
        {
            const double oct = trk * 0.01 * 5.0 * jmin (1.0f, env * 2.0f) + modAmt * 0.01 * modValue * 4.0;
            const double c = jlimit (30.0, 20000.0, (double) cutHz * std::pow (2.0, oct));
            f[0].set (c, fs, 1.1); f[1].set (c, fs, 1.1);
        }
        L[i] = f[0].process (L[i], 0);
        R[i] = f[1].process (R[i], 0);
    }
}

//==============================================================================
void EqSat::process (float* L, float* R, int n, float hiDb, float midDb, float loDb, float ampDb, float split)
{
    const bool eq = std::abs (hiDb) > 0.05f || std::abs (midDb) > 0.05f || std::abs (loDb) > 0.05f;
    const bool sat = ampDb > 0.05f;
    if (! eq && ! sat) return;
    for (int c = 0; c < 2; ++c) { lo[c].set (split, fs, 0.707); hi[c].set (jmin (16000.0f, split * 8.0f), fs, 0.707); }
    const float gl = dbToGain (loDb), gm = dbToGain (midDb), gh = dbToGain (hiDb);
    const float drive = dbToGain (ampDb), comp = 1.0f / std::pow (drive, 0.75f);
    for (int i = 0; i < n; ++i)
        for (int c = 0; c < 2; ++c)
        {
            float& x = c == 0 ? L[i] : R[i];
            if (eq)
            {
                const float l = lo[c].process (x, 0), h = hi[c].process (x, 2), m = x - l - h;
                x = l * gl + m * gm + h * gh;
            }
            if (sat) x = std::tanh (x * drive) * comp;
        }
}

//==============================================================================
void Magic::prepare (double sr) { fs = sr; buf.prepare ((int) (sr * 0.5)); phase = 0.0; notch[0].reset(); notch[1].reset(); }

void Magic::process (float* L, float* R, int n, float semis, float grain, float mix, float notchHz)
{
    if (mix < 0.001f) { for (int i = 0; i < n; ++i) buf.push (L[i], R[i]); return; }
    const double G = fs * (0.012 + grain * 0.01 * 0.14);
    const double ratio = std::pow (2.0, semis / 12.0);
    const double inc = (1.0 - ratio) / G;
    notch[0].set (notchHz, fs, 1.5); notch[1].set (notchHz, fs, 1.5);
    for (int i = 0; i < n; ++i)
    {
        buf.push (L[i], R[i]);
        phase += inc;
        phase -= std::floor (phase);
        const double p2 = std::fmod (phase + 0.5, 1.0);
        const float w1 = (float) std::sin (kPi * phase), w2 = (float) std::sin (kPi * p2);
        const float d1 = (float) (phase * G) + 2.0f, d2 = (float) (p2 * G) + 2.0f;
        float wl = buf.readL (d1) * w1 + buf.readL (d2) * w2;
        float wr = buf.readR (d1) * w1 + buf.readR (d2) * w2;
        wl -= notch[0].process (wl, 1);
        wr -= notch[1].process (wr, 1);
        L[i] = L[i] * (1.0f - mix) + wl * mix;
        R[i] = R[i] * (1.0f - mix) + wr * mix;
    }
}

//==============================================================================
void TapeDelay::prepare (double sr)
{
    fs = sr; dl.prepare ((int) (sr * 6.0)); lfo = 0; tL = tR = 0;
    for (auto& f : lp) f.reset();
    for (auto& f : hp) f.reset();
}

void TapeDelay::process (const float* inL, const float* inR, float* outL, float* outR, int n, const FxContext& c,
                         int one, int two, float fb, float lfoHz, float fm, float dm, float wide, float cutHz, float res, bool lowcut)
{
    const double six = 60.0 / jmax (20.0, c.bpm) / 4.0 * fs;
    const float targetL = (float) jlimit (8.0, (double) dl.size - 8.0, one * six);
    const float targetR = (float) jlimit (8.0, (double) dl.size - 8.0, two * six);
    if (tL <= 0) { tL = targetL; tR = targetR; }
    const double q = 0.6 + res * 0.01 * 6.0;
    const float w = wide * 0.01f;
    for (int i = 0; i < n; ++i)
    {
        lfo += lfoHz / fs; if (lfo >= 1.0) lfo -= 1.0;
        const float s = (float) std::sin (2.0 * kPi * lfo);
        tL += 0.0004f * (targetL * (1.0f + 0.03f * dm * 0.01f * s) - tL);
        tR += 0.0004f * (targetR * (1.0f - 0.03f * dm * 0.01f * s) - tR);
        if ((i & 15) == 0)
        {
            const double cc = jlimit (60.0, 19000.0, (double) cutHz * std::pow (2.0, fm * 0.01 * 2.0 * s));
            lp[0].set (cc, fs, q); lp[1].set (cc, fs, q);
            if (lowcut) { hp[0].set (150.0, fs, 0.707); hp[1].set (150.0, fs, 0.707); }
        }
        float yl = dl.readL (tL), yr = dl.readR (tR);
        float fl = lp[0].process (yl, 0), fr = lp[1].process (yr, 0);
        if (lowcut) { fl = hp[0].process (fl, 2); fr = hp[1].process (fr, 2); }
        // wide: 0 = mono feedback, 1 = full ping-pong
        const float xl = fl * (1.0f - 0.5f * w) + fr * 0.5f * w;
        const float xr = fr * (1.0f - 0.5f * w) + fl * 0.5f * w;
        dl.push (inL[i] + std::tanh (xl * fb * 0.01f * 1.05f), inR[i] + std::tanh (xr * fb * 0.01f * 1.05f));
        const float m = (yl + yr) * 0.5f;
        outL[i] = m + (yl - m) * (0.3f + 0.7f * w);
        outR[i] = m + (yr - m) * (0.3f + 0.7f * w);
    }
}

//==============================================================================
void XFx::prepare (double sr)
{
    fs = sr;
    dl.prepare ((int) (sr * 2.0));
    loop.prepare ((int) (sr * 8.0));
    static const int combLen[8] { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
    static const int apLen[4] { 556, 441, 341, 225 };
    for (int i = 0; i < 8; ++i) { comb[(size_t) i].assign ((size_t) (combLen[i] * sr / 44100.0) + 23 * (i & 1), 0.0f); combIdx[(size_t) i] = 0; combLp[(size_t) i] = 0; }
    for (int i = 0; i < 4; ++i) { allp[(size_t) i].assign ((size_t) (apLen[i] * sr / 44100.0), 0.0f); apIdx[(size_t) i] = 0; }
    reset();
}

void XFx::reset()
{
    for (auto& f : svf) f.reset();
    for (auto& c : band) for (auto& f : c) f.reset();
    for (auto& c : post) for (auto& f : c) f.reset();
    for (auto& c : ap) for (auto& v : c) v = 0;
    apFb[0] = apFb[1] = 0; dl.clear(); ph = ph2 = 0; holdCount = 0; frozen = false; lastSlice = -1; spinRate = 1; spinPos = 0;
    for (auto& c : comb) std::fill (c.begin(), c.end(), 0.0f);
    for (auto& c : allp) std::fill (c.begin(), c.end(), 0.0f);
}

void XFx::process (float* L, float* R, int n, const FxContext& c, int mode, float x, float y, float spd, int len16,
                   float loHz, float hiHz, bool play, bool rec, float smooth)
{
    if (mode != lastMode) { reset(); lastMode = mode; }
    if (mode == 0) { std::fill (L, L + n, 0.0f); std::fill (R, R + n, 0.0f); return; }
    x *= 0.01f; y *= 0.01f; spd *= 0.01f;
    const double six = 60.0 / jmax (20.0, c.bpm) / 4.0 * fs;           // samples per 16th
    const float sm = 0.02f + smooth * 0.01f * 0.3f;

    switch (mode)
    {
        case 1: case 2: case 3:   // highpass / bandpass / lowpass
        {
            const double fc = 30.0 * std::pow (600.0, (double) x);
            svf[0].set (fc, fs, 0.6 + y * 12.0); svf[1].set (fc, fs, 0.6 + y * 12.0);
            const int m = mode == 1 ? 2 : (mode == 2 ? 1 : 0);
            for (int i = 0; i < n; ++i) { L[i] = svf[0].process (L[i], m); R[i] = svf[1].process (R[i], m); }
            break;
        }
        case 4:   // phaser
        {
            for (int i = 0; i < n; ++i)
            {
                ph += (0.05 + spd * 4.0) / fs; if (ph >= 1) ph -= 1;
                const float sweep = (float) (0.5 + 0.5 * std::sin (2 * kPi * ph));
                const float fcN = (float) (200.0 * std::pow (20.0, 0.2 + 0.8 * x * sweep) / fs);
                const float a = (1.0f - std::tan ((float) kPi * fcN)) / (1.0f + std::tan ((float) kPi * fcN));
                for (int ch = 0; ch < 2; ++ch)
                {
                    float& s = ch == 0 ? L[i] : R[i];
                    float v = s + apFb[ch] * y * 0.9f;
                    for (int k = 0; k < 6; ++k) { const float o = -a * v + ap[ch][k]; ap[ch][k] = v + a * o; v = o; }
                    apFb[ch] = v;
                    s = 0.5f * (s + v);
                }
            }
            break;
        }
        case 5:   // vowel (formant morph a-e-i-o-u)
        {
            static const float F[5][3] { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 300, 2300, 3000 }, { 450, 800, 2830 }, { 325, 700, 2530 } };
            const float pos = x * 3.999f; const int v0 = (int) pos; const float fr = pos - (float) v0;
            for (int ch = 0; ch < 2; ++ch)
                for (int k = 0; k < 3; ++k)
                    band[ch][k].set (F[v0][k] + fr * (F[v0 + 1][k] - F[v0][k]), fs, 3.0 + y * 15.0);
            for (int i = 0; i < n; ++i)
                for (int ch = 0; ch < 2; ++ch)
                {
                    float& s = ch == 0 ? L[i] : R[i];
                    const float in = s;
                    s = band[ch][0].process (in, 1) + 0.7f * band[ch][1].process (in, 1) + 0.4f * band[ch][2].process (in, 1);
                    s *= 1.6f;
                }
            break;
        }
        case 6: case 12:   // flanger / ufo (deeper pitch-modulated delay + resonant band)
        {
            const bool ufo = mode == 12;
            if (ufo) { band[0][0].set (400 + 3000 * x, fs, 2.0 + 10.0 * y); band[1][0].set (400 + 3000 * x, fs, 2.0 + 10.0 * y); }
            for (int i = 0; i < n; ++i)
            {
                ph += (0.03 + spd * (ufo ? 8.0 : 2.0)) / fs; if (ph >= 1) ph -= 1;
                const float s = (float) std::sin (2 * kPi * ph);
                const float base = (float) (fs * (ufo ? 0.004 : 0.0007)), depth = (float) (fs * (ufo ? 0.006 : 0.004)) * (ufo ? 1.0f : x);
                const float dL = base + depth * (0.5f + 0.5f * s), dR = base + depth * (0.5f - 0.5f * s);
                const float yl = dl.readL (dL), yr = dl.readR (dR);
                const float fbk = ufo ? 0.6f : y * 0.9f;
                dl.push (L[i] + yl * fbk, R[i] + yr * fbk);
                if (ufo) { L[i] = band[0][0].process (yl, 1) * 2.0f; R[i] = band[1][0].process (yr, 1) * 2.0f; }
                else     { L[i] = 0.5f * (L[i] + yl); R[i] = 0.5f * (R[i] + yr); }
            }
            break;
        }
        case 7:   // fm (audio-rate delay modulation)
        {
            const double f = 20.0 * std::pow (250.0, (double) x);
            for (int i = 0; i < n; ++i)
            {
                ph += f / fs; if (ph >= 1) ph -= 1;
                const float d = (float) (2.0 + y * 0.004 * fs * (0.5 + 0.5 * std::sin (2 * kPi * ph)));
                dl.push (L[i], R[i]);
                L[i] = dl.readL (d); R[i] = dl.readR (d);
            }
            break;
        }
        case 8: case 9:   // reson (tuned comb) / feedback (saturated short delay)
        {
            const float d = mode == 8 ? (float) (fs / (40.0 * std::pow (50.0, (double) x)))
                                      : (float) (fs * (0.001 + 0.1 * x));
            const float g = mode == 8 ? 0.7f + 0.29f * y : 0.5f + 0.6f * y;
            for (int i = 0; i < n; ++i)
            {
                const float yl = dl.readL (d), yr = dl.readR (d * (mode == 9 ? 1.07f : 1.0f));
                dl.push (L[i] + std::tanh (yl * g), R[i] + std::tanh (yr * g));
                L[i] = yl; R[i] = yr;
            }
            break;
        }
        case 10:   // reflex: tempo multi-tap echoes
        {
            const float sp = (float) (six * (0.25 + x * 1.75));
            for (int i = 0; i < n; ++i)
            {
                dl.push (L[i], R[i]);
                float ol = 0, orr = 0, g = 1.0f;
                for (int k = 1; k <= 5; ++k)
                {
                    const float dd = jmin ((float) dl.size - 4.0f, sp * (float) k);
                    if (k & 1) ol += dl.readL (dd) * g; else orr += dl.readR (dd) * g;
                    g *= 0.35f + 0.55f * y;
                }
                L[i] = ol; R[i] = orr;
            }
            break;
        }
        case 11:   // reverb (Freeverb style)
        {
            const float room = 0.7f + 0.28f * x, damp = 0.1f + 0.8f * y;
            for (int i = 0; i < n; ++i)
            {
                const float in = (L[i] + R[i]) * 0.15f;
                float o[2] { 0, 0 };
                for (int k = 0; k < 8; ++k)
                {
                    auto& b = comb[(size_t) k]; int& idx = combIdx[(size_t) k];
                    const float out = b[(size_t) idx];
                    combLp[(size_t) k] = out * (1 - damp) + combLp[(size_t) k] * damp;
                    b[(size_t) idx] = in + combLp[(size_t) k] * room;
                    if (++idx >= (int) b.size()) idx = 0;
                    o[k & 1] += out;
                }
                for (int k = 0; k < 4; ++k)
                {
                    auto& b = allp[(size_t) k]; int& idx = apIdx[(size_t) k];
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        if (ch == 1 && k < 2) continue;
                        const float bo = b[(size_t) idx];
                        b[(size_t) idx] = o[ch] + bo * 0.5f;
                        o[ch] = bo - o[ch];
                    }
                    if (++idx >= (int) b.size()) idx = 0;
                }
                L[i] = o[0]; R[i] = o[1];
            }
            break;
        }
        case 13:   // rainman: random reversed grains
        {
            const int grain = (int) (fs * (0.03 + 0.15 * (1.0f - x)));
            for (int i = 0; i < n; ++i)
            {
                dl.push (L[i], R[i]);
                if (holdCount <= 0) { holdCount = grain; ph = rnd() * fs * 0.5; ph2 = 1.0 + (rnd() - 0.5) * y * 1.5; }
                const double pos = ph + (double) (grain - holdCount) * ph2;
                const float w = (float) std::sin (kPi * (double) (grain - holdCount) / grain);
                L[i] = dl.readL ((float) pos) * w; R[i] = dl.readR ((float) (pos * 1.01)) * w;
                --holdCount;
            }
            break;
        }
        case 14:   // distort
        {
            const float drive = 1.0f + x * 40.0f;
            svf[0].set (800 + 12000 * y, fs, 0.7); svf[1].set (800 + 12000 * y, fs, 0.7);
            for (int i = 0; i < n; ++i)
            {
                L[i] = svf[0].process (std::tanh (L[i] * drive) * 0.7f, 0);
                R[i] = svf[1].process (std::tanh (R[i] * drive) * 0.7f, 0);
            }
            break;
        }
        case 15:   // ringmod
        {
            const double f = 20.0 * std::pow (200.0, (double) x);
            for (int i = 0; i < n; ++i)
            {
                ph += f / fs; if (ph >= 1) ph -= 1;
                const float m = (float) std::sin (2 * kPi * ph);
                L[i] = L[i] * (1 - y) + L[i] * m * y; R[i] = R[i] * (1 - y) + R[i] * m * y;
            }
            break;
        }
        case 16:   // lofi
        {
            const int holdLen = 1 + (int) (x * 40.0f);
            const float steps = std::pow (2.0f, 16.0f - y * 13.0f);
            for (int i = 0; i < n; ++i)
            {
                if (--holdCount <= 0) { holdCount = holdLen; hold[0] = L[i]; hold[1] = R[i]; }
                L[i] = std::round (hold[0] * steps) / steps; R[i] = std::round (hold[1] * steps) / steps;
            }
            break;
        }
        case 17:   // pitchshift
        {
            const double semis = (x * 2.0 - 1.0) * 12.0;
            const double G = fs * (0.02 + y * 0.12);
            const double inc = (1.0 - std::pow (2.0, semis / 12.0)) / G;
            for (int i = 0; i < n; ++i)
            {
                dl.push (L[i], R[i]);
                ph += inc; ph -= std::floor (ph);
                const double p2 = std::fmod (ph + 0.5, 1.0);
                const float w1 = (float) std::sin (kPi * ph), w2 = (float) std::sin (kPi * p2);
                L[i] = dl.readL ((float) (ph * G) + 2) * w1 + dl.readL ((float) (p2 * G) + 2) * w2;
                R[i] = dl.readR ((float) (ph * G) + 2) * w1 + dl.readR ((float) (p2 * G) + 2) * w2;
            }
            break;
        }
        case 18:   // gate: 16th gate patterns
        {
            static const uint16_t pats[8] { 0xFFFF, 0xAAAA, 0xEEEE, 0xB6DB, 0x9249, 0xDDDD, 0xF0F0, 0xCCCC };
            const uint16_t pat = pats[jlimit (0, 7, (int) (x * 7.99f))];
            const double rate = spd < 0.33f ? 0.5 : (spd < 0.66f ? 0.25 : 0.125);
            for (int i = 0; i < n; ++i)
            {
                const double p = c.ppq + c.ppqPerSample * i;
                const int stepIdx = (int) std::floor (p / rate) & 15;
                const bool on = ((pat >> (15 - stepIdx)) & 1) != 0;
                xf += ((on ? 1.0f : 1.0f - y) - xf) * sm;
                L[i] *= xf; R[i] *= xf;
            }
            break;
        }
        case 19: case 20: case 21:   // chop / slice / freeze (tempo-synced loop buffer)
        {
            const int segLen = jmax (64, (int) (six * jlimit (1, 16, len16)));
            const int bar = jmin (loop.size - 1, (int) (six * 16));
            for (int i = 0; i < n; ++i)
            {
                const double p = c.ppq + c.ppqPerSample * i;
                const long long sixIdx = (long long) std::floor (p * 4.0);
                if (rec) loop.l[(size_t) writePos] = L[i], loop.r[(size_t) writePos] = R[i];
                writePos = (writePos + 1) % loop.size;
                float ol = L[i], orr = R[i];
                if (mode == 19)        // chop: repeat the last LEN slice at rate X
                {
                    const int rep = jmax (16, (int) (segLen / (1 + (int) (x * 7.0f))));
                    if (sixIdx != lastSlice && (sixIdx % jmax (1, len16)) == 0) { lastSlice = sixIdx; loopPos = 0; loopLen = rep; frozen = play; }
                    if (frozen && y > 0.05f)
                    {
                        const int rp = (writePos - 1 - loopLen + (loopPos % loopLen) + loop.size * 2) % loop.size;
                        ol = ol * (1 - y) + loop.l[(size_t) rp] * y; orr = orr * (1 - y) + loop.r[(size_t) rp] * y;
                        ++loopPos;
                    }
                }
                else if (mode == 20)   // slice: re-order 16ths of the last bar with probability X
                {
                    const long long s16 = sixIdx & 15;
                    if (sixIdx != lastSlice)
                    {
                        lastSlice = sixIdx;
                        sliceMap[(size_t) s16] = rnd() < x ? (int) (rnd() * 16.0f) : (int) s16;
                        loopPos = 0;
                    }
                    if (play && sliceMap[(size_t) s16] != (int) s16)
                    {
                        const int src = (writePos - bar + sliceMap[(size_t) s16] * (int) six + loopPos + loop.size * 2) % loop.size;
                        ol = loop.l[(size_t) src]; orr = loop.r[(size_t) src];
                    }
                    ++loopPos;
                }
                else                   // freeze: hold a LEN loop while X > 50 % (or Play)
                {
                    const bool wantFreeze = x > 0.5f && play;
                    if (wantFreeze && ! frozen) { frozen = true; loopLen = segLen; loopPos = 0; spinPos = writePos; }
                    if (! wantFreeze) frozen = false;
                    if (frozen)
                    {
                        const double speed = 0.25 + spd * 1.75;
                        loopPos = (loopPos + 1) % loopLen;
                        const int rp = (int) (spinPos - loopLen + std::fmod (loopPos * speed, (double) loopLen) + loop.size * 2) % loop.size;
                        ol = loop.l[(size_t) rp] * (1 - y * 0.5f) + ol * y * 0.5f; orr = loop.r[(size_t) rp] * (1 - y * 0.5f) + orr * y * 0.5f;
                    }
                }
                xf += ((frozen || mode != 21 ? 1.0f : 1.0f) - xf) * sm;
                L[i] = ol; R[i] = orr;
            }
            break;
        }
        case 22:   // spin: tape stop / speed-up
        {
            const float target = x;   // 1 = normal speed, 0 = stopped
            for (int i = 0; i < n; ++i)
            {
                dl.push (L[i], R[i]);
                spinRate += (target - spinRate) * (0.00005f + spd * 0.0005f);
                spinPos += 1.0 - spinRate;
                spinPos = jlimit (0.0, (double) dl.size - 8.0, spinPos);
                if (target > 0.98f) spinPos *= 0.999;
                L[i] = dl.readL ((float) spinPos); R[i] = dl.readR ((float) spinPos);
            }
            break;
        }
        default: break;
    }

    // LO / HI band limiting on the fx output
    const bool useLo = loHz > 25.0f, useHi = hiHz < 19000.0f;
    if (useLo || useHi)
    {
        for (int ch = 0; ch < 2; ++ch) { post[ch][0].set (loHz, fs, 0.707); post[ch][1].set (hiHz, fs, 0.707); }
        for (int i = 0; i < n; ++i)
            for (int ch = 0; ch < 2; ++ch)
            {
                float& s = ch == 0 ? L[i] : R[i];
                if (useLo) s = post[ch][0].process (s, 2);
                if (useHi) s = post[ch][1].process (s, 0);
            }
    }
}
