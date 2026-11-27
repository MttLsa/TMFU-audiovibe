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
void TriFilter::prepare (double sr)
{
    fs = sr;
    kSm = 1.0f - (float) std::exp (-1.0 / (0.003 * sr));
    wetStep = (float) (1.0 / (0.005 * sr));
    reset();
}

void TriFilter::reset()
{
    for (int k = 0; k < 3; ++k) { f[k][0].reset(); f[k][1].reset(); wet[k] = 0.0f; init[k] = false; curLogF[k] = curRes[k] = -1.0f; }
}

void TriFilter::process (float* L, float* R, int n, const TriFilterParams& p)
{
    static const int mode[3] { 2, 1, 0 };   // Svf modes: HP, BP, LP
    for (int k = 0; k < 3; ++k)
    {
        if (! p.on[k] && wet[k] <= 0.0f) { if (init[k]) { f[k][0].reset(); f[k][1].reset(); init[k] = false; } continue; }
        const float tF = std::log (jlimit (20.0f, 20000.0f, p.freq[k])), tR = jlimit (0.0f, 1.0f, p.res[k]);
        if (! init[k]) { logF[k] = tF; resS[k] = tR; init[k] = true; curLogF[k] = -1.0f; }
        const float wT = p.on[k] ? 1.0f : 0.0f;
        for (int i = 0; i < n; ++i)
        {
            if (logF[k] != tF) { logF[k] += (tF - logF[k]) * kSm; if (std::abs (tF - logF[k]) < 1.0e-5f) logF[k] = tF; }
            if (resS[k] != tR) { resS[k] += (tR - resS[k]) * kSm; if (std::abs (tR - resS[k]) < 1.0e-5f) resS[k] = tR; }
            if (logF[k] != curLogF[k] || resS[k] != curRes[k])
            {
                curLogF[k] = logF[k]; curRes[k] = resS[k];
                const double q = k == 1 ? 0.6 + resS[k] * resS[k] * 14.0 : 0.707 + resS[k] * resS[k] * 11.3;
                f[k][0].set (fs, std::exp ((double) logF[k]), q); f[k][1] = { f[k][0].g, f[k][0].k, f[k][0].a1, f[k][0].a2, f[k][0].a3, f[k][1].ic1, f[k][1].ic2 };
            }
            if (wet[k] != wT) wet[k] = jlimit (0.0f, 1.0f, wet[k] + (wT > wet[k] ? wetStep : -wetStep));
            const float a = f[k][0].process (L[i], mode[k]), b = f[k][1].process (R[i], mode[k]);
            L[i] += (a - L[i]) * wet[k];
            R[i] += (b - R[i]) * wet[k];
        }
    }
}

//==============================================================================
void Chorus::OnePole::set (float freq, float sr)
{
    const float t = std::tan ((float) kPi * jlimit (1.0f, 0.49f * sr, freq) / sr);
    g = t / (1.0f + t);
}

void Chorus::prepare (double sr)
{
    fs = (float) sr;
    int size = 4; while (size < (int) (0.06 * sr) + 8) size <<= 1;
    for (auto& b : buf) b.assign ((size_t) size, 0.0f);
    mask = size - 1;
    const float c = (float) std::exp (-1.0 / (0.05 * sr));
    for (auto* s : { &rate, &depthMs, &centreMs, &mixS, &phaseOff }) s->coeff = c;
    reset();
}

void Chorus::reset()
{
    for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f);
    for (auto& t : tone) t.s = 0.0f;
    w = 0; ph = 0.0; onGain = 0.0f; first = true;
}

float Chorus::read (int line, float d) const
{
    d = jlimit (1.0f, (float) (mask - 3), d);
    const int i = (int) d; const float fr = d - (float) i;
    const auto& b = buf[line];
    auto tap = [&] (int k) { return b[(size_t) ((w - 1 - k) & mask)]; };
    const float xm1 = tap (i - 1), x0 = tap (i), x1 = tap (i + 1), x2 = tap (i + 2);
    const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * fr + c2) * fr + c1) * fr + x0;
}

void Chorus::process (float* L, float* R, int n, bool on, int mode, float rateHz, float depth, float mix)
{
    if (! on && onGain <= 0.0f) { if (! first) reset(); return; }
    float r = rateHz, dep = depth * 5.0f, centre = 8.0f, off = 0.5f;
    switch (mode)
    {
        case chorusI:  r = 0.513f; dep = 1.6f; centre = 3.5f; break;
        case chorusII: r = 0.863f; dep = 1.9f; centre = 3.5f; break;
        case dim1: r = 0.25f; dep = 0.25f; centre = 6.0f; break;
        case dim2: r = 0.25f; dep = 0.45f; centre = 6.0f; break;
        case dim3: r = 0.25f; dep = 0.7f;  centre = 6.0f; break;
        case dim4: r = 0.5f;  dep = 0.9f;  centre = 6.0f; break;
        default: centre = jmax (centre, dep + 0.5f); break;
    }
    const bool bbd = mode != custom, dim = mode >= dim1 && mode <= dim4;
    rate.target = r; depthMs.target = dep; centreMs.target = centre; mixS.target = mix; phaseOff.target = off;
    for (auto& t : tone) t.set (bbd ? 9000.0f : 0.45f * fs, fs);
    if (first) { for (auto* s : { &rate, &depthMs, &centreMs, &mixS, &phaseOff }) s->value = s->target; first = false; }
    const float msToS = fs * 0.001f, onStep = 1.0f / (0.01f * fs);
    auto tri = [] (double p) { const float x = (float) (p - std::floor (p)); return 4.0f * std::fabs (x - 0.5f) - 1.0f; };
    auto sine = [] (double p) { return (float) std::sin (2.0 * kPi * (p - std::floor (p))); };
    for (int i = 0; i < n; ++i)
    {
        const float rt = rate.next(), dp = depthMs.next(), ce = centreMs.next(), m = mixS.next(), po = phaseOff.next();
        onGain = jlimit (0.0f, 1.0f, onGain + (on ? onStep : -onStep));
        ph += rt / fs; ph -= std::floor (ph);
        const float ma = bbd ? tri (ph) : sine (ph), mb = bbd ? tri (ph + po) : sine (ph + po);
        const float inL = dim ? 0.5f * (L[i] + R[i]) : L[i];
        const float inR = dim ? inL : R[i];
        buf[0][(size_t) w] = inL; buf[1][(size_t) w] = inR; w = (w + 1) & mask;
        float a = read (0, (ce + dp * ma) * msToS), b = read (1, (ce + dp * mb) * msToS);
        if (bbd)
        {
            a = std::tanh (tone[1].lp (tone[0].lp (a)) * 1.2f) / 1.2f;
            b = std::tanh (tone[3].lp (tone[2].lp (b)) * 1.2f) / 1.2f;
        }
        float wl = a, wr = b;
        if (dim) { wl = a - 0.5f * b; wr = b - 0.5f * a; }
        const float g = m * onGain;
        L[i] += g * (wl - L[i]);
        R[i] += g * (wr - R[i]);
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

void BeatRepeat::reset() { repeating = false; lastInterval = -1; w = 0; dryGain = 1.0f; repGain = 0.0f; repIndex = 0; }

void BeatRepeat::process (float* L, float* R, int n, double ppq0, double pps, bool playing, const BeatRepeatParams& p)
{
    if (! playing || pps <= 0) { repeating = false; lastInterval = -1; repGain = 0.0f; dryGain = 1.0f; repIndex = 0; return; }
    const double fadeS = 0.002 * fs;           // 2 ms: chunk edges and the switch back to the original
    const float step = (float) (1.0 / fadeS);
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
                readPos = 0; chunkPos = 0; chunkStartPpq = ppq; rate = 1;
            }
        }
        if (repeating && ppq >= gateEndPpq) repeating = false;   // the repeats fade out (2 ms), the original fades in

        if (repeating || repGain > 0.0f)
        {
            if (repeating && (ppq - chunkStartPpq) / pps >= chunkLen)
            {
                // next repetition (optionally with a new, shorter grid = variation)
                ++repIndex; chunkStartPpq = ppq; readPos = 0; chunkPos = 0;
                if (p.variation > 0 && rnd.nextFloat() < p.variation / 10.0f)
                {
                    int gi = 0; while (gi < 7 && grids[gi] < p.gridBeats - 1e-9) ++gi;
                    int ng = jmax (0, gi - 1 - rnd.nextInt (3));
                    chunkLen = jmax (32, (int) (grids[ng] / pps));
                }
                else chunkLen = jmax (32, (int) (p.gridBeats / pps));
                const float semis = jmax (-48.0f, p.pitch - p.pitchDecay * 12.0f * (float) (repIndex - 1));
                rate = std::pow (2.0, semis / 12.0);
            }
        }
        const bool repActive = repIndex > 0 && (repeating || repGain > 0.0f);
        repGain = jlimit (0.0f, 1.0f, repGain + ((repeating && repIndex > 0) ? step : -step));
        float repL = 0, repR = 0;
        if (repActive)
        {
            double rp = captureStart + readPos; while (rp >= size) rp -= size;
            const int i0 = (int) rp; const float fr = (float) (rp - i0); const int i1 = (i0 + 1) % size;
            const float env = (float) jlimit (0.0, 1.0, jmin (chunkPos / fadeS, (chunkLen - chunkPos) / fadeS));
            const float g = p.volume * std::pow (1.0f - p.decay, (float) jmax (0, repIndex - 1)) * env * repGain;
            repL = (bufL[(size_t) i0] + fr * (bufL[(size_t) i1] - bufL[(size_t) i0])) * g;
            repR = (bufR[(size_t) i0] + fr * (bufR[(size_t) i1] - bufR[(size_t) i0])) * g;
            readPos += rate; chunkPos += 1.0;
            if (chunkPos >= chunkLen) { readPos = 0; chunkPos = 0; }   // (only while fading out after the gate)
        }
        if (! repeating && repGain <= 0.0f) repIndex = 0;
        // Insert: the original is muted while the repeats play (crossfaded); Mix: always on
        const float dryTarget = (p.mode == 1 && repeating && repIndex > 0) ? 0.0f : 1.0f;
        dryGain = jlimit (0.0f, 1.0f, dryGain + (dryTarget > dryGain ? step : -step));
        L[i] = L[i] * dryGain + repL; R[i] = R[i] * dryGain + repR;
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
    w = 0; r = 0; speed = 1; engaged = false; action = none; xfade = 0; actionT = 0; fastRelease = false; motor = 1.0; lastMotor = true;
    engagedFlag = false; currentSpeed = 1.0f; currentAction = none; platterSpeed = 1.0f;
}

void Deck::engage()
{
    if (! engaged) { engaged = true; fastRelease = false; r = (double) w - 2.0; if (r < 0) r += size; speed = 1.0; }
}

void Deck::process (float* L, float* R, int n, int type, float ctl, double bpm, bool motorOn)
{
    if (type == 0)
    {
        if (engaged || xfade > 0) reset();
        pendingAction.store (none);
        return;
    }
    if (type != lastType) { lastType = type; action = none; motor = 1.0; lastMotor = true; }
    const double beat = 60.0 / jmax (30.0, bpm);   // seconds
    const int pa = pendingAction.exchange (none);
    if (pa != none)
    {
        action = pa;
        if (action == random)
        {
            static const int tapeActs[] { stop, start, wobble, pull, rewind };
            static const int vinylActs[] { spinback, scratch, wobble, spinback, scratch };
            action = type == 1 ? tapeActs[rnd.nextInt (5)] : vinylActs[rnd.nextInt (5)];
        }
        actionT = 0;
        switch (action)
        {
            case stop:     actionLen = beat * 2.0; break;
            case brake:    actionLen = beat * 1.5; break;
            case start:    actionLen = beat * 1.0; break;
            case wobble:   actionLen = beat * 4.0; break;
            case pull:     actionLen = 2.8; break;          // tape snatched off the reel: long, slowly falling rewind
            case rewind:   actionLen = 2.0; break;
            case spinback: actionLen = 2.0; break;
            case scratch:  actionLen = beat * 2.0; actionParam = 0.5f + rnd.nextFloat(); break;
            default:       actionLen = 0; break;
        }
        engage();
        if (action == start) speed = 0.0;
    }
    const bool held = handHeld.load();
    const float hs = handSpeed.load();
    const double tauHand = type == 2 ? 0.006 : 0.05;
    const double fadeEnd = 0.06;   // 60 ms fade-out at the end of rewind / spinback / pull

    // turntable motor (START / STOP): real decks brake in ~0.7 s and reach speed in ~0.35 s
    if (type == 2 && motorOn && ! lastMotor && motor < 0.999)
    {
        // land exactly on the live signal when the platter is back at speed
        engage();
        const double lag = (1.0 - motor) * (1.0 - motor) * 0.35 * 0.5 * fs;
        r = (double) w - jmax (2.0, lag); if (r < 0) r += size;
        speed = motor;
    }
    if (type == 2) lastMotor = motorOn; else motor = 1.0;

    for (int i = 0; i < n; ++i)
    {
        bufL[(size_t) w] = L[i]; bufR[(size_t) w] = R[i];
        if (type == 2) motor = motorOn ? jmin (1.0, motor + 1.0 / (0.35 * fs)) : jmax (0.0, motor - 1.0 / (0.7 * fs));
        double target = type == 2 ? motor : 1.0, tau = type == 2 ? 0.002 : 0.01;
        bool wantEngaged = type == 2 && motor < 0.999;
        float actGain = 1.0f;
        if (wantEngaged) engage();
        if (held) { engage(); target = hs; tau = tauHand; wantEngaged = true; }
        else if (std::abs (ctl) > 0.02f) { engage(); target = 1.0 + 2.0 * ctl; tau = tauHand; wantEngaged = true; }
        else if (action != none)
        {
            const double t = actionT / jmax (1e-3, actionLen);
            wantEngaged = true;
            switch (action)
            {
                case stop:     target = t < 0.5 ? jmax (0.0, 1.0 - t * 2.0) : 0.0; tau = 0.03; break;
                case brake:    target = t < 0.35 ? jmax (0.0, 1.0 - t / 0.35) : 0.0; tau = 0.01; break;
                case start:    target = jmin (1.0, t * 1.6); tau = 0.02; break;
                case wobble:   target = 1.0 + 0.05 * std::sin (2.0 * kPi * 5.0 * actionT) * std::sin (kPi * t); tau = 0.004; break;
                case pull:     target = -3.2 * std::exp (-actionT / 1.1); tau = 0.03; break;   // pitch falls slowly
                case rewind:   target = -3.0; tau = 0.05; break;
                case spinback: target = -4.5 * std::pow (1.0 - t, 1.4); tau = 0.008; break;
                case scratch:  target = 2.6 * std::sin (2.0 * kPi * actionT / (beat * 0.5 * actionParam)); tau = 0.004; break;
                default: break;
            }
            if (action == pull || action == rewind || action == spinback)
                actGain = (float) jlimit (0.0, 1.0, (actionLen - actionT) / fadeEnd);
            actionT += 1.0 / fs;
            if (actionT >= actionLen)
            {
                fastRelease = action == pull || action == rewind || action == spinback;   // deck is silent now: come back quickly
                action = none;
                actGain = fastRelease ? 0.0f : 1.0f;
            }
        }
        if (fastRelease) actGain = 0.0f;
        speed += (target - speed) * (1.0 - std::exp (-1.0 / (tau * fs)));
        float outL = L[i], outR = R[i];
        if (engaged)
        {
            double sp = speed;
            if (type == 1) { wowPh += 0.6 / fs; flutPh += 9.0 / fs; sp *= 1.0 + 0.0015 * std::sin (2 * kPi * wowPh) + 0.0005 * std::sin (2 * kPi * flutPh); }
            r += sp;
            // the head can never read the future, nor further back than the buffer
            double lag = std::fmod ((double) w - r, (double) size); if (lag < 0) lag += size;
            if (lag < 2.0) r = (double) w - 2.0;
            if (lag > size - 8.0) r = (double) w - (size - 8.0);
            r = std::fmod (r, (double) size); if (r < 0) r += size;
            const int i1 = (int) r; const float f = (float) (r - i1);
            const int i0 = (i1 - 1 + size) % size, i2 = (i1 + 1) % size, i3 = (i1 + 2) % size;
            auto cub = [f] (float a, float b, float c, float d)
            { return b + 0.5f * f * (c - a + f * (2.0f * a - 5.0f * b + 4.0f * c - d + f * (3.0f * (b - c) + d - a))); };
            const float lvl = (float) jmin (1.0, std::abs (sp) * 1.4) * actGain;   // silence when stopped
            const float dl = cub (bufL[(size_t) i0], bufL[(size_t) i1], bufL[(size_t) i2], bufL[(size_t) i3]) * lvl;
            const float dr = cub (bufR[(size_t) i0], bufR[(size_t) i1], bufR[(size_t) i2], bufR[(size_t) i3]) * lvl;
            if (fastRelease) xfade -= (float) (1.0 / (0.01 * fs));
            else if (! wantEngaged && std::abs (speed - 1.0) < 0.02) xfade -= (float) (1.0 / (0.04 * fs));
            else xfade += (float) (1.0 / (0.004 * fs));
            xfade = jlimit (0.0f, 1.0f, xfade);
            outL = L[i] + (dl - L[i]) * xfade;
            outR = R[i] + (dr - R[i]) * xfade;
            if (xfade <= 0.0f && ! wantEngaged) { engaged = false; fastRelease = false; speed = 1.0; }
        }
        L[i] = outL; R[i] = outR;
        if (++w >= size) w = 0;
    }
    engagedFlag = engaged;
    currentSpeed = engaged ? (float) speed : 1.0f;
    currentAction = action;
    platterSpeed = (float) motor;
}
} // namespace fx
