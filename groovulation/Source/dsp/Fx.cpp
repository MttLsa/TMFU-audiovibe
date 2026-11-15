#include "Fx.h"

using namespace juce;

namespace fx
{
//==============================================================================
void GentleSat::process (float* L, float* R, int n, float amount)
{
    if (amount < 0.0005f && driveSm < 0.01f) return;
    // 0 % = 0 dB, 25 % ~ +3 dB, 50 % ~ +7 dB, 100 % = +18 dB into the curve, level compensated
    const float targetDb = std::pow (jlimit (0.0f, 1.0f, amount), 1.3f) * 18.0f;
    const float even = 0.04f * amount;
    const float dcA = 1.0f - (float) (2.0 * kPi * 5.0 / fs);
    const float wet = jmin (1.0f, amount * 3.0f);
    for (int i = 0; i < n; ++i)
    {
        driveSm += 0.0015f * (targetDb - driveSm);
        const float g = std::pow (10.0f, driveSm * 0.05f), comp = 1.0f / g;
        for (int c = 0; c < 2; ++c)
        {
            float& x = c == 0 ? L[i] : R[i];
            const float u = x * g, a2 = u * u;
            const float y = u / std::sqrt (std::sqrt (1.0f + a2 * a2)) + even * a2;   // soft knee + 2nd harmonic
            const float added = (y * comp - x) * wet;
            const float d = added - dcX[c] + dcA * dcY[c];   // DC blocker on the added part only
            dcX[c] = added; dcY[c] = d;
            x += d;
        }
    }
}

//==============================================================================
void SyncDelay::prepare (double sr, int maxSamples)
{
    fs = sr; size = maxSamples + 4;
    bl.assign ((size_t) size, 0.0f); br.assign ((size_t) size, 0.0f);
    reset();
}

void SyncDelay::reset()
{
    std::fill (bl.begin(), bl.end(), 0.0f); std::fill (br.begin(), br.end(), 0.0f);
    w = 0; dSm = -1; lpL = lpR = 0;
}

void SyncDelay::process (float* L, float* R, int n, double delaySamples, float fb, float mix, bool pingPong)
{
    if (mix < 0.0005f) { if (dSm >= 0) reset(); return; }
    delaySamples = jlimit (16.0, (double) size - 8, delaySamples);
    if (dSm < 0) dSm = delaySamples;
    const float dryG = jmin (1.0f, 2.0f * (1.0f - mix)), wetG = jmin (1.0f, 2.0f * mix);
    const float lpA = (float) std::exp (-2.0 * kPi * 7000.0 / fs);
    for (int i = 0; i < n; ++i)
    {
        dSm += 0.0005 * (delaySamples - dSm);
        double rp = (double) w - dSm; while (rp < 0) rp += size;
        const int i0 = (int) rp; const float fr = (float) (rp - i0); const int i1 = (i0 + 1) % size;
        const float dl = bl[(size_t) i0] + fr * (bl[(size_t) i1] - bl[(size_t) i0]);
        const float dr = br[(size_t) i0] + fr * (br[(size_t) i1] - br[(size_t) i0]);
        lpL = dl + lpA * (lpL - dl); lpR = dr + lpA * (lpR - dr);
        if (pingPong)
        {
            bl[(size_t) w] = 0.5f * (L[i] + R[i]) + fb * lpR;
            br[(size_t) w] = fb * lpL;
        }
        else
        {
            bl[(size_t) w] = L[i] + fb * lpL;
            br[(size_t) w] = R[i] + fb * lpR;
        }
        L[i] = L[i] * dryG + dl * wetG;
        R[i] = R[i] * dryG + dr * wetG;
        if (++w >= size) w = 0;
    }
}

//==============================================================================
void FdnReverb::prepare (double sr)
{
    fs = sr;
    static const double baseMs[N] { 29.7, 37.1, 41.1, 43.7, 53.0, 59.3, 67.1, 73.7 };
    for (int i = 0; i < N; ++i)
    {
        lines[(size_t) i].assign ((size_t) (baseMs[i] * 0.001 * sr * 2.3) + 64, 0.0f);
        len[(size_t) i] = (int) (baseMs[i] * 0.001 * sr);
    }
    static const double apMs[4] { 4.7, 3.6, 5.3, 3.1 };
    for (int i = 0; i < 4; ++i) { apLen[(size_t) i] = (int) (apMs[i] * 0.001 * sr); ap[(size_t) i].assign ((size_t) apLen[(size_t) i] + 1, 0.0f); }
    pre.assign ((size_t) (0.25 * sr) + 4, 0.0f);
    reset();
}

void FdnReverb::reset()
{
    for (auto& l : lines) std::fill (l.begin(), l.end(), 0.0f);
    for (auto& a : ap) std::fill (a.begin(), a.end(), 0.0f);
    std::fill (pre.begin(), pre.end(), 0.0f);
    pos.fill (0); apPos.fill (0); lp.fill (0.0f); prePos = 0; active = false; sizeSm = -1;
}

void FdnReverb::process (float* L, float* R, int n, float size, float decaySec, float damping, float predelayMs, float mix)
{
    if (mix < 0.0005f) { if (active) reset(); return; }
    active = true;
    static const double baseMs[N] { 29.7, 37.1, 41.1, 43.7, 53.0, 59.3, 67.1, 73.7 };
    const float target = 0.45f + 1.75f * jlimit (0.0f, 1.0f, size);
    if (sizeSm < 0) sizeSm = target;
    const float dryG = jmin (1.0f, 2.0f * (1.0f - mix)), wetG = jmin (1.0f, 2.0f * mix);
    const int preD = jlimit (1, (int) pre.size() - 2, (int) (predelayMs * 0.001f * (float) fs));
    const float damp = 0.05f + 0.85f * jlimit (0.0f, 1.0f, damping);
    for (int i = 0; i < n; ++i)
    {
        if ((i & 31) == 0)
        {
            sizeSm += 0.02f * (target - sizeSm);
            for (int k = 0; k < N; ++k) len[(size_t) k] = (int) (baseMs[k] * 0.001 * fs * sizeSm);
        }
        // predelay
        pre[(size_t) prePos] = 0.5f * (L[i] + R[i]);
        int rp = prePos - preD; if (rp < 0) rp += (int) pre.size();
        float inM = pre[(size_t) rp];
        if (++prePos >= (int) pre.size()) prePos = 0;
        const float inS = 0.5f * (L[i] - R[i]);
        // input diffusion (allpasses)
        float a = inM + 0.3f * inS, b = inM - 0.3f * inS;
        for (int k = 0; k < 4; ++k)
        {
            float& x = k < 2 ? a : b;
            auto& buf = ap[(size_t) k]; int& p = apPos[(size_t) k];
            const float dly = buf[(size_t) p];
            const float v = x + 0.6f * dly;
            buf[(size_t) p] = v;
            x = dly - 0.6f * v;
            if (++p >= apLen[(size_t) k]) p = 0;
        }
        lfo += 2.0 * kPi * 0.37 / fs; if (lfo > 2 * kPi) lfo -= 2 * kPi;
        float out[N];
        for (int k = 0; k < N; ++k)
        {
            auto& buf = lines[(size_t) k];
            const int sz = (int) buf.size();
            const double mod = (k % 3 == 0) ? (std::sin (lfo + k) * 0.0006 * fs) : 0.0;
            double r = pos[(size_t) k] - len[(size_t) k] - mod;
            while (r < 0) r += sz;
            const int i0 = (int) r % sz; const float fr = (float) (r - std::floor (r)); const int i1 = (i0 + 1) % sz;
            out[k] = buf[(size_t) i0] + fr * (buf[(size_t) i1] - buf[(size_t) i0]);
        }
        // Householder mixing
        float sum = 0; for (int k = 0; k < N; ++k) sum += out[k];
        sum *= 2.0f / N;
        for (int k = 0; k < N; ++k)
        {
            const float lenSec = (float) len[(size_t) k] / (float) fs;
            const float g = std::pow (10.0f, -3.0f * lenSec / jmax (0.1f, decaySec));
            float v = (out[k] - sum) * g;
            lp[(size_t) k] = v + damp * (lp[(size_t) k] - v);
            v = lp[(size_t) k] + ((k & 1) ? b : a) * 0.5f;
            auto& buf = lines[(size_t) k];
            buf[(size_t) pos[(size_t) k]] = v;
            if (++pos[(size_t) k] >= (int) buf.size()) pos[(size_t) k] = 0;
        }
        const float wl = 0.35f * (out[0] - out[1] + out[2] + out[4] - out[5] + out[6]);
        const float wr = 0.35f * (out[1] + out[3] - out[4] + out[5] + out[7] - out[2]);
        L[i] = L[i] * dryG + wl * wetG;
        R[i] = R[i] * dryG + wr * wetG;
    }
}

//==============================================================================
void BeatRepeat::prepare (double sr)
{
    fs = sr; size = (int) (sr * 10.0);
    bufL.assign ((size_t) size, 0.0f); bufR.assign ((size_t) size, 0.0f);
    reset();
}

void BeatRepeat::reset() { repeating = false; lastInterval = -1; w = 0; fadeGain = 0; repIndex = 0; }

void BeatRepeat::process (float* L, float* R, int n, double ppq0, double pps, bool playing, const BeatRepeatParams& p)
{
    if (! playing || pps <= 0) { repeating = false; lastInterval = -1; return; }
    const double fadeS = 0.002 * fs;
    static const double grids[] { 0.125, 1.0 / 6, 0.25, 1.0 / 3, 0.5, 1.0, 2.0, 4.0 };
    for (int i = 0; i < n; ++i)
    {
        const double ppq = ppq0 + pps * i;
        bufL[(size_t) w] = L[i]; bufR[(size_t) w] = R[i];
        // trigger points: k * interval + offset
        const double rel = ppq - p.offsetBeats;
        const long long k = (long long) std::floor (rel / p.intervalBeats);
        if (k != lastInterval && rel >= 0)
        {
            const bool justCrossed = (rel - (double) k * p.intervalBeats) < pps * 2.0 + 1e-9;
            lastInterval = k;
            if (justCrossed && rnd.nextFloat() < p.chance)
            {
                repeating = true;
                gateEndPpq = ppq + p.gateBeats;
                captureStart = w; repIndex = 0;
                chunkLen = jmax (32, (int) (p.gridBeats / pps));
                readPos = 0; chunkStartPpq = ppq; rate = 1;
            }
        }
        if (repeating && ppq >= gateEndPpq) repeating = false;

        float outL = L[i], outR = R[i];
        float repL = 0, repR = 0; bool repActive = false;
        if (repeating)
        {
            const double inChunk = (ppq - chunkStartPpq) / pps;
            if (inChunk >= chunkLen)
            {
                // next repetition (optionally with a new, shorter grid = variation)
                ++repIndex; chunkStartPpq = ppq; readPos = 0;
                if (p.variation > 0 && rnd.nextFloat() < p.variation / 10.0f)
                {
                    int gi = 0; while (gi < 7 && grids[gi] < p.gridBeats - 1e-9) ++gi;
                    int ng = jmax (0, gi - 1 - rnd.nextInt (3));
                    if (p.noTriplets && (ng == 1 || ng == 3)) ng = jmax (0, ng - 1);
                    chunkLen = jmax (32, (int) (grids[ng] / pps));
                }
                else chunkLen = jmax (32, (int) (p.gridBeats / pps));
                const float semis = jmax (-48.0f, p.pitch - p.pitchDecay * 12.0f * (float) (repIndex - 1));
                rate = std::pow (2.0, semis / 12.0);
            }
            if (repIndex > 0)
            {
                double rp = captureStart + readPos; while (rp >= size) rp -= size;
                const int i0 = (int) rp; const float fr = (float) (rp - i0); const int i1 = (i0 + 1) % size;
                const double posC = (ppq - chunkStartPpq) / pps;
                const float env = (float) jmin (1.0, posC / fadeS, (chunkLen - posC) / fadeS);
                const float g = p.volume * std::pow (1.0f - p.decay, (float) (repIndex - 1)) * jmax (0.0f, env);
                repL = (bufL[(size_t) i0] + fr * (bufL[(size_t) i1] - bufL[(size_t) i0])) * g;
                repR = (bufR[(size_t) i0] + fr * (bufR[(size_t) i1] - bufR[(size_t) i0])) * g;
                readPos += rate;
                repActive = true;
            }
        }
        // dry gain per mix mode (smoothed)
        const float dryTarget = p.mode == 2 ? 0.0f : ((p.mode == 1 && repActive) ? 0.0f : 1.0f);
        fadeGain += (dryTarget - fadeGain) * (float) (1.0 / fadeS);
        outL = outL * fadeGain + repL; outR = outR * fadeGain + repR;
        L[i] = outL; R[i] = outR;
        if (++w >= size) w = 0;
    }
}

//==============================================================================
void Deck::prepare (double sr)
{
    fs = sr; size = (int) (sr * 12.0);
    bufL.assign ((size_t) size, 0.0f); bufR.assign ((size_t) size, 0.0f);
    reset();
}

void Deck::reset()
{
    w = 0; r = 0; speed = 1; engaged = false; action = none; xfade = 0; actionT = 0;
    engagedFlag = false; currentSpeed = 1.0f; currentAction = none;
}

void Deck::process (float* L, float* R, int n, int type, float ctl, float noise, double bpm)
{
    if (type == 0)
    {
        if (engaged || xfade > 0) reset();
        pendingAction.store (none);
        return;
    }
    if (type != lastType) { lastType = type; action = none; }
    const double beat = 60.0 / jmax (30.0, bpm);   // seconds
    const int pa = pendingAction.exchange (none);
    auto engage = [this]
    {
        if (! engaged) { engaged = true; r = (double) w - 2.0; if (r < 0) r += size; speed = 1.0; }
    };
    if (pa != none)
    {
        action = pa;
        if (action == random)
        {
            static const int tapeActs[] { stop, start, wobble, pull, rewind };
            static const int vinylActs[] { brake, spinback, scratch, wobble, start };
            action = type == 1 ? tapeActs[rnd.nextInt (5)] : vinylActs[rnd.nextInt (5)];
        }
        actionT = 0;
        switch (action)
        {
            case stop:     actionLen = beat * 2.0; break;
            case brake:    actionLen = beat * 1.5; break;
            case start:    actionLen = beat * 1.0; break;
            case wobble:   actionLen = beat * 4.0; break;
            case pull:     actionLen = beat * 1.0; break;
            case rewind:   actionLen = beat * 1.0; break;
            case spinback: actionLen = beat * 1.0; break;
            case scratch:  actionLen = beat * 2.0; actionParam = 0.5f + rnd.nextFloat(); break;
            default:       actionLen = 0; break;
        }
        engage();
        if (action == start) speed = 0.0;
    }
    const bool held = handHeld.load();
    const float hs = handSpeed.load();
    const double tauHand = type == 2 ? 0.006 : 0.05, tauAct = 0.01;
    const float noiseAmt = noise * 0.015f;

    for (int i = 0; i < n; ++i)
    {
        bufL[(size_t) w] = L[i]; bufR[(size_t) w] = R[i];
        double target = 1.0, tau = tauAct;
        bool wantEngaged = false;
        if (held) { engage(); target = hs; tau = tauHand; wantEngaged = true; }
        else if (std::abs (ctl) > 0.02f) { engage(); target = 1.0 + 2.0 * ctl; tau = tauHand; wantEngaged = true; }
        else if (action != none)
        {
            const double t = actionT / jmax (1e-3, actionLen);
            wantEngaged = true;
            switch (action)
            {
                case stop:     target = t < 0.5 ? jmax (0.0, 1.0 - t * 2.0) : 0.0; tau = 0.03; break;   // slow down, then silence
                case brake:    target = t < 0.35 ? jmax (0.0, 1.0 - t / 0.35) : 0.0; tau = 0.01; break;
                case start:    target = jmin (1.0, t * 1.6); tau = 0.02; break;
                case wobble:   target = 1.0 + 0.05 * std::sin (2.0 * kPi * 5.0 * actionT) * std::sin (kPi * t); tau = 0.004; break;
                case pull:     target = 1.0 - 0.75 * std::sin (kPi * t); tau = 0.02; break;
                case rewind:   target = t < 0.75 ? -3.0 : 0.0; tau = 0.04; break;
                case spinback: target = t < 0.8 ? -4.0 * (1.0 - t) : 0.0; tau = 0.01; break;
                case scratch:  target = 2.6 * std::sin (2.0 * kPi * actionT / (beat * 0.5 * actionParam)); tau = 0.004; break;
                default: break;
            }
            actionT += 1.0 / fs;
            if (actionT >= actionLen) action = none;
        }
        speed += (target - speed) * (1.0 - std::exp (-1.0 / (tau * fs)));
        float outL = L[i], outR = R[i];
        if (engaged)
        {
            double sp = speed;
            if (type == 1) { wowPh += 0.6 / fs; flutPh += 9.0 / fs; sp *= 1.0 + 0.0015 * std::sin (2 * kPi * wowPh) + 0.0005 * std::sin (2 * kPi * flutPh); }
            r += sp;
            // the head can never read the future, nor further back than the buffer
            double lag = std::fmod ((double) w - r, (double) size); if (lag < 0) lag += size;
            if (lag < 2.0) { r = (double) w - 2.0; }
            if (lag > size - 8.0) { r = (double) w - (size - 8.0); }
            r = std::fmod (r, (double) size); if (r < 0) r += size;
            // cubic read
            const int i1 = (int) r; const float f = (float) (r - i1);
            const int i0 = (i1 - 1 + size) % size, i2 = (i1 + 1) % size, i3 = (i1 + 2) % size;
            auto cub = [f] (float a, float b, float c, float d)
            { return b + 0.5f * f * (c - a + f * (2.0f * a - 5.0f * b + 4.0f * c - d + f * (3.0f * (b - c) + d - a))); };
            float dl = cub (bufL[(size_t) i0], bufL[(size_t) i1], bufL[(size_t) i2], bufL[(size_t) i3]);
            float dr = cub (bufR[(size_t) i0], bufR[(size_t) i1], bufR[(size_t) i2], bufR[(size_t) i3]);
            // motor/tape "drag": level follows |speed| a little (silence when stopped)
            const float lvl = (float) jmin (1.0, std::abs (sp) * 1.4);
            dl *= lvl; dr *= lvl;
            if (! wantEngaged && std::abs (speed - 1.0) < 0.02) xfade -= (float) (1.0 / (0.04 * fs));   // back to live
            else xfade += (float) (1.0 / (0.004 * fs));
            xfade = jlimit (0.0f, 1.0f, xfade);
            outL = L[i] + (dl - L[i]) * xfade;
            outR = R[i] + (dr - R[i]) * xfade;
            if (xfade <= 0.0f && ! wantEngaged) { engaged = false; speed = 1.0; }
        }
        // character: tape hiss / vinyl crackle + rumble
        const float wn = rnd.nextFloat() * 2.0f - 1.0f;
        if (type == 1)
        {
            hissLp += 0.3f * (wn - hissLp);
            const float h = (wn - hissLp) * noiseAmt * 0.5f;
            outL += h; outR += h * 0.9f;
        }
        else
        {
            float cr = 0.0f;
            if (rnd.nextFloat() < 0.0004f * (0.3f + noise)) cr = (rnd.nextFloat() * 2.0f - 1.0f) * 0.6f;
            crackleLp += 0.5f * (cr - crackleLp);
            rumblePh += (float) (17.0 / fs); if (rumblePh > 1.0f) rumblePh -= 1.0f;
            const float c = (cr - crackleLp) * noise * 0.25f + std::sin (2.0f * (float) kPi * rumblePh) * noiseAmt * 0.3f;
            outL += c; outR += c;
        }
        L[i] = outL; R[i] = outR;
        if (++w >= size) w = 0;
    }
    engagedFlag = engaged;
    currentSpeed = engaged ? (float) speed : 1.0f;
    currentAction = action;
}
}
