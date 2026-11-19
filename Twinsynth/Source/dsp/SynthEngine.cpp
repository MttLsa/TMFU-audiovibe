#include "SynthEngine.h"

using namespace params;
using namespace dsp;

namespace
{
    inline float semisToRatio (float s) { return std::exp2 (s / 12.0f); }

    inline float oscWave (int w, float ph, float inc, float pw)
    {
        switch (w)
        {
            case 0: return sawBlep (ph, inc);
            case 1: return pulseBlep (ph, inc, pw);
            case 2: return triangle (ph);
            default: return sine (ph);
        }
    }

    // unison detune offsets (semitones) and pan positions (-1..1)
    void unisonLayout (int n, float detune, float width, float* offs, float* pans)
    {
        if (n <= 1) { offs[0] = 0; pans[0] = 0; return; }
        for (int i = 0; i < n; ++i)
        {
            const float x = (float) i / (float) (n - 1) * 2.0f - 1.0f;          // -1..1
            offs[i] = detune * 0.55f * x * (0.6f + 0.4f * std::abs (x));
            pans[i] = width * x * (i % 2 ? -1.0f : 1.0f) * (n == 2 ? -1.0f : 1.0f);
        }
    }

    // JP-8000 style supersaw detune curve (A. Szabo's measurement fit)
    float supersawDetune (float x)
    {
        const double v = x;
        return (float) (10028.7312891634 * std::pow (v, 11) - 50818.8652045924 * std::pow (v, 10) + 111363.4808729368 * std::pow (v, 9)
            - 138150.6761080548 * std::pow (v, 8) + 106649.6679158292 * std::pow (v, 7) - 53046.9642751875 * std::pow (v, 6)
            + 17019.9518580080 * std::pow (v, 5) - 3425.0836591318 * std::pow (v, 4) + 404.2703938388 * std::pow (v, 3)
            - 24.1878824391 * v * v + 0.6717417634 * v + 0.0030115596);
    }
    constexpr float ssOffs[7] = { -0.11002313f, -0.06288439f, -0.01952356f, 0.0f, 0.01991221f, 0.06216538f, 0.10745242f };

    inline float lfoInc (float hz, float sr) { return hz / sr; }
    inline float fadeInc (float ms, float sr) { return ms < 1.0f ? 1.0f : 1000.0f / (ms * sr); }
}

// =====================================================================================================
//  Section A
// =====================================================================================================
void AnalogPart::start (float velocity, bool hard, const BlockCtx& c, uint32_t seed)
{
    const float* v = c.v;
    vel = velocity;
    if (hard)
    {
        rng.seed (seed);
        for (int i = 0; i < maxUni; ++i) { ph1[i] = rng.uni(); ph2[i] = rng.uni(); o2prev[i] = 0; }
        phSub = ph1[0];
        lfoFade = v[aLfoDelay] < 1.0f ? 1.0f : 0.0f;
        drift = rng.bip() * 0.5f; driftTarget = rng.bip();
    }
    penv = v[aPenv];
    accent = v[aAccent] * std::max (0.0f, (velocity - 0.75f) / 0.25f);
    fenv.set (v[aFA], v[aFD], v[aFS], v[aFR], c.sr);
    aenv.set (v[aAA], v[aAD], v[aAS], v[aAR], c.sr);
    fenv.gateOn (hard && ! aenv.active());
    aenv.gateOn (hard && ! aenv.active());
}

void AnalogPart::render (float* L, float* R, int n, const float* notes, const BlockCtx& c)
{
    const float* v = c.v;
    const float sr = c.sr;
    const int w1 = (int) v[aO1Wave], w2 = (int) v[aO2Wave];
    const float o1 = v[aO1Oct] * 12.0f;
    const float o2 = v[aO2Oct] * 12.0f + v[aO2Semi] + v[aO2Fine] * 0.01f;
    const float lvl1 = v[aO1Lvl], lvl2 = v[aO2Lvl], ring = v[aRing], sub = v[aSub], noise = v[aNoise];
    const bool sync = v[aSync] > 0.5f;
    const float xmod = v[aXmod], pwm = v[aPwm];
    const int nu = std::clamp ((int) v[aUni], 1, maxUni);
    float offs[maxUni], pans[maxUni], gl[maxUni], gr[maxUni], det[maxUni];
    unisonLayout (nu, v[aDetune], v[gWidth], offs, pans);
    for (int u = 0; u < nu; ++u)
    {
        det[u] = semisToRatio (offs[u]);
        gl[u] = std::sqrt (0.5f * (1.0f - pans[u])); gr[u] = std::sqrt (0.5f * (1.0f + pans[u]));
    }
    const float uniNorm = 1.0f / std::sqrt ((float) nu);
    const int ft = (int) v[aFltType];
    const float cut = v[aCut], res = v[aRes], drive = 1.0f + v[aDrive] * 5.0f, key = v[aKey], envAmt = v[aEnv], velF = v[aVelF];
    const float lfoPitch = v[aLfoPitch], lfoCut = v[aLfoCut];
    const float pCoef = std::exp (-1.0f / (std::max (1.0f, v[aPdec]) * 0.001f * sr / 4.6f));
    const float fInc = fadeInc (v[aLfoDelay], sr);
    const float driftAmt = v[aDrift] * 0.12f;
    const float velAmp = 1.0f - v[gVel] + v[gVel] * vel;
    const float accentAmp = 1.0f + accent * 0.45f, accentEnv = accent * 0.35f;
    const float mwCut = c.mw * v[gMwCut] * 4.0f;
    fenv.set (v[aFA], v[aFD], v[aFS], v[aFR], sr);
    aenv.set (v[aAA], v[aAD], v[aAS], v[aAR], sr);

    for (int i = 0; i < n; ++i)
    {
        if (! aenv.active()) break;
        lfoFade = std::min (1.0f, lfoFade + fInc);
        const float lfo = c.lfoA[i] * lfoFade;
        if (++driftCount >= 256) { driftCount = 0; if (rng.uni() < 0.02f) driftTarget = rng.bip(); }
        drift += (driftTarget - drift) * 0.00005f;
        const float pitch = notes[i] + c.bend + c.vib[i] + lfo * lfoPitch + penv + drift * driftAmt * 4.0f;
        penv *= pCoef;
        const float base = noteToHz (pitch) / sr;
        const float inc1b = base * semisToRatio (o1), inc2b = base * semisToRatio (o2);
        const float pw1 = std::clamp (v[aO1Pw] + pwm * lfo * 0.4f, 0.03f, 0.97f);
        const float pw2 = std::clamp (v[aO2Pw] + pwm * lfo * 0.4f, 0.03f, 0.97f);

        float sl = 0, srr = 0;
        for (int u = 0; u < nu; ++u)
        {
            float inc1 = std::min (0.45f, inc1b * det[u] * (1.0f + drift * driftAmt * 0.01f * (float) (u - 3)));
            const float inc2 = std::min (0.45f, inc2b * det[u]);
            if (xmod > 0) inc1 = std::clamp (inc1 * (1.0f + xmod * 2.0f * o2prev[u]), 0.0f, 0.45f);
            ph1[u] += inc1;
            bool wrapped = false;
            if (ph1[u] >= 1.0f) { ph1[u] -= std::floor (ph1[u]); wrapped = true; }
            ph2[u] += inc2;
            if (sync && wrapped) ph2[u] = ph1[u] * (inc2 / std::max (1.0e-6f, inc1));
            ph2[u] -= std::floor (ph2[u]);
            const float s1 = oscWave (w1, ph1[u], inc1, pw1);
            const float s2 = oscWave (w2, ph2[u], inc2, pw2);
            o2prev[u] = s2;
            const float s = s1 * lvl1 + s2 * lvl2 + ring * s1 * s2;
            sl += s * gl[u]; srr += s * gr[u];
        }
        sl *= uniNorm; srr *= uniNorm;
        if (sub > 0)
        {
            const float si = std::min (0.45f, inc1b * 0.5f);
            phSub += si; phSub -= std::floor (phSub);
            const float s = pulseBlep (phSub, si, 0.5f) * sub * 0.8f;
            sl += s * 0.7071f; srr += s * 0.7071f;
        }
        if (noise > 0) { const float nz = rng.bip() * noise * 0.6f; sl += nz * 0.7071f; srr += nz * 0.7071f; }
        sl *= 0.5f; srr *= 0.5f;

        const float fe = fenv.next();
        const float ae = aenv.next();
        const float oct = envAmt * 7.0f * (fe + accentEnv * fe) + key * (notes[i] - 60.0f) / 12.0f
                        + lfoCut * lfo * 3.0f + velF * (vel - 0.6f) * 3.0f + mwCut;
        const float g = cutoffG (cut * std::exp2 (oct), sr);
        float yl, yr;
        switch (ft)
        {
            case 0: yl = lad[0].process (sl, g, res, 4, drive); yr = lad[1].process (srr, g, res, 4, drive); break;
            case 1: yl = lad[0].process (sl, g, res, 3, drive); yr = lad[1].process (srr, g, res, 3, drive); break;
            default:
            {
                const int mode = ft == 2 ? 0 : ft == 3 ? 1 : 2;
                yl = svf[0].process (fastTanh (sl * drive) / std::sqrt (drive), g, res, mode, true);
                yr = svf[1].process (fastTanh (srr * drive) / std::sqrt (drive), g, res, mode, true);
                break;
            }
        }
        const float amp = ae * velAmp * accentAmp;
        L[i] += yl * amp;
        R[i] += yr * amp;
    }
}

// =====================================================================================================
//  Section D
// =====================================================================================================
void DigitalPart::setupAdditive (const BlockCtx& c, float note)
{
    const float* v = c.v;
    const int model = (int) v[dAddM];
    const float f0 = noteToHz (note + v[dOct] * 12.0f + v[dSemi]);
    const float bright = v[dBright], tilt = v[dTilt], inh = v[dInh], even = v[dEven];
    numP = 0;
    auto add = [&] (float ratio, float amp, float sus, float decaySec)
    {
        if (numP >= numPartials || amp <= 0.0f || f0 * ratio > c.sr * 0.45f) return;
        pRatio[numP] = ratio; pAmp[numP] = amp; pSus[numP] = sus; pDyn[numP] = 1.0f - sus;
        pCoef[numP] = decaySec > 50.0f ? 1.0f : std::exp (-1.0f / (std::max (0.005f, decaySec) * c.sr));
        pPh[numP] = rng.uni();
        ++numP;
    };
    auto bt = [&] (int k) { return std::pow ((float) k, (bright - 0.5f) * 1.4f) * ((k % 2 == 0) ? even : 1.0f); };
    auto td = [&] (int k) { return 1.0f / (1.0f + tilt * 0.6f * (float) (k - 1)); };
    const float B = inh * 0.0025f;
    auto stretch = [&] (int k) { return (float) k * std::sqrt (1.0f + B * (float) (k * k)); };
    const float ks = std::sqrt (261.6f / std::max (30.0f, f0));

    switch (model)
    {
        case 0: // House Organ: bright drawbar organ with a percussive, decaying upper register
        {
            static const int hs[] = { 1, 2, 3, 4, 6, 8, 10, 12, 16 };
            static const float am[] = { 1.0f, 0.85f, 0.6f, 0.75f, 0.45f, 0.5f, 0.2f, 0.25f, 0.15f };
            for (int i = 0; i < 9; ++i) add (stretch (hs[i]), am[i] * bt (hs[i]), i == 0 ? 1.0f : 0.45f, 0.35f * td (hs[i]) * 2.0f);
            break;
        }
        case 1: // Drawbar organ with 3rd-harmonic percussion
        {
            static const int hs[] = { 1, 2, 3, 4, 6, 8 };
            for (int i = 0; i < 6; ++i) add (stretch (hs[i]), (i < 3 ? 1.0f : 0.7f) * bt (hs[i]), (hs[i] == 3 || hs[i] == 4) ? 0.55f : 1.0f, 0.45f);
            break;
        }
        case 2: // E.Piano (tine): fundamental + bell-like tine partials that decay fast
        {
            add (1.0f, 1.0f, 0.0f, 3.5f * ks);
            add (2.0f, 0.28f * bt (2), 0.0f, 1.6f * ks * td (2));
            add (3.0f, 0.10f * bt (3), 0.0f, 0.7f * ks * td (3));
            add (7.0f * (1.0f + B), (0.12f + 0.35f * vel) * bt (7), 0.0f, 0.09f * td (7));
            add (14.0f, (0.04f + 0.18f * vel) * bt (14), 0.0f, 0.04f);
            break;
        }
        case 3: // Piano: stretched partials, velocity-dependent brightness, per-partial decay
        {
            const float Bp = 0.0004f + inh * 0.003f;
            for (int k = 1; k <= 32; ++k)
            {
                const float r = (float) k * std::sqrt (1.0f + Bp * (float) (k * k));
                const float a = std::pow ((float) k, -(1.5f - 0.6f * bright - 0.5f * vel)) * ((k % 2 == 0) ? even : 1.0f);
                add (r, a, 0.0f, 5.0f * ks / (1.0f + (0.25f + tilt) * 0.5f * (float) (k - 1)));
            }
            break;
        }
        case 4: // Strings
            for (int k = 1; k <= 40; ++k) add (stretch (k), (1.0f / (float) k) * bt (k), 1.0f, 100.0f);
            break;
        case 5: // Choir "aah": formants at absolute frequencies
            for (int k = 1; k <= 40; ++k)
            {
                const float hz = f0 * (float) k;
                auto pk = [] (float x, float cc, float bw) { return std::exp (-((x - cc) / bw) * ((x - cc) / bw)); };
                const float fm = 0.03f + pk (hz, 750, 110) + 0.6f * pk (hz, 1150, 140) + 0.3f * pk (hz, 2600, 220) * bright * 2.0f;
                add ((float) k, fm / std::sqrt ((float) k), 1.0f, 100.0f);
            }
            break;
        case 6: // Bell
        {
            static const float r[] = { 0.5f, 1.0f, 1.19f, 1.56f, 2.0f, 2.51f, 2.66f, 3.01f, 4.1f, 5.4f, 6.8f };
            static const float a[] = { 0.5f, 1.0f, 0.6f, 0.45f, 0.55f, 0.35f, 0.3f, 0.25f, 0.2f, 0.15f, 0.1f };
            for (int i = 0; i < 11; ++i) add (r[i] * (1.0f + inh * 0.02f * (float) i), a[i] * std::pow (r[i], (bright - 0.5f)), 0.0f, 6.0f / (1.0f + tilt * 2.0f * r[i]));
            break;
        }
        case 7: // Mallet / marimba
        {
            static const float r[] = { 1.0f, 3.93f, 9.2f, 16.6f };
            static const float a[] = { 1.0f, 0.35f, 0.15f, 0.06f };
            static const float t[] = { 0.7f, 0.15f, 0.05f, 0.025f };
            for (int i = 0; i < 4; ++i) add (r[i], a[i] * (0.5f + bright), 0.0f, t[i] * ks * (1.5f - tilt));
            break;
        }
        case 8: // Glass
        {
            static const float r[] = { 1.0f, 2.32f, 4.25f, 6.63f, 9.38f, 12.9f };
            static const float t[] = { 3.0f, 1.8f, 1.0f, 0.6f, 0.4f, 0.3f };
            for (int i = 0; i < 6; ++i) add (r[i], std::pow (0.6f + bright * 0.3f, (float) i), 0.0f, t[i] * (1.5f - tilt));
            break;
        }
        case 9: // Brass
            for (int k = 1; k <= 30; ++k) add (stretch (k), std::pow ((float) k, -(1.7f - 1.1f * bright)) * ((k % 2 == 0) ? even : 1.0f), 1.0f, 100.0f);
            break;
        default: // Soft pad
            for (int k = 1; k <= 8; ++k) add (stretch (k), (1.0f / (float) (k * k)) * bt (k) * 2.0f, 1.0f, 100.0f);
            break;
    }
    float sum = 0.0f;
    for (int i = 0; i < numP; ++i) sum += pAmp[i];
    const float norm = sum > 0 ? 1.0f / std::max (1.0f, sum * 0.7f) : 0.0f;
    for (int i = 0; i < numP; ++i) pAmp[i] *= norm;
}

void DigitalPart::start (float velocity, bool hard, const BlockCtx& c, float note, uint32_t seed)
{
    const float* v = c.v;
    vel = velocity;
    if (hard)
    {
        rng.seed (seed * 2654435761u + 7u);
        for (int u = 0; u < maxUni; ++u)
        {
            for (int o = 0; o < 4; ++o) ph[u][o] = (int) v[dEngine] == 0 ? 0.0f : rng.uni();
            fbHist[u][0] = fbHist[u][1] = 0;
        }
        for (int k = 0; k < 7; ++k) ssPh[k] = rng.uni();
        lfoFade = v[dLfoDelay] < 1.0f ? 1.0f : 0.0f;
    }
    const bool fresh = hard && ! aenv.active();
    if (hard)
        for (int o = 0; o < 4; ++o) { opLvl[o] = fresh ? 0.0f : opLvl[o]; opAtk[o] = true; }
    if ((int) v[dEngine] == 4 && hard) setupAdditive (c, note);
    penv = v[dPenv];
    fenv.set (v[dFA], v[dFD], v[dFS], v[dFR], c.sr);
    aenv.set (v[dAA], v[dAD], v[dAS], v[dAR], c.sr);
    fenv.gateOn (fresh);
    aenv.gateOn (fresh);
}

void DigitalPart::render (float* L, float* R, int n, const float* notes, const BlockCtx& c)
{
    const float* v = c.v;
    const float sr = c.sr;
    const int engine = (int) v[dEngine];
    const float off = v[dOct] * 12.0f + v[dSemi] + v[dFine] * 0.01f;
    const int nu = engine == 2 || engine == 4 ? 1 : std::clamp ((int) v[dUni], 1, maxUni);
    float offs[maxUni], pans[maxUni], gl[maxUni], gr[maxUni], det[maxUni];
    unisonLayout (nu, v[dDetune], v[gWidth], offs, pans);
    for (int u = 0; u < nu; ++u)
    {
        det[u] = semisToRatio (offs[u]);
        gl[u] = std::sqrt (0.5f * (1.0f - pans[u])); gr[u] = std::sqrt (0.5f * (1.0f + pans[u]));
    }
    const float uniNorm = 1.0f / std::sqrt ((float) nu);
    const int ft = (int) v[dFltType];
    const float cut = v[dCut], res = v[dRes], key = v[dKey], envAmt = v[dEnv], velF = v[dVelF];
    const float lfoPitch = v[dLfoPitch], lfoCut = v[dLfoCut], lfoTim = v[dLfoTim];
    const float pitchCoef = std::exp (-1.0f / (std::max (1.0f, v[dPdec]) * 0.001f * sr / 4.6f));
    const float fInc = fadeInc (v[dLfoDelay], sr);
    const float velAmp = 1.0f - v[gVel] + v[gVel] * vel;
    const float noise = v[dNoise];
    const float mwCut = c.mw * v[gMwCut] * 4.0f;
    fenv.set (v[dFA], v[dFD], v[dFS], v[dFR], sr);
    aenv.set (v[dAA], v[dAD], v[dAS], v[dAR], sr);

    // FM setup
    const int alg = (int) v[dAlg];
    const float ratio[4] = { v[dR1], v[dR2], v[dR3], v[dR4] };
    const float lvl[4] = { v[dL1], v[dL2], v[dL3], v[dL4] };
    const float sus[4] = { v[dS1], v[dS2], v[dS3], v[dS4] };
    float opDec[4], opAtkC[4];
    {
        const float dms[4] = { v[dE1], v[dE2], v[dE3], v[dE4] };
        for (int o = 0; o < 4; ++o)
        {
            opDec[o] = 1.0f - std::exp (-4.6f / (std::max (5.0f, dms[o]) * 0.001f * sr));
            opAtkC[o] = 1.0f - std::exp (-1.466f / (std::max (0.5f, v[dFmAtk]) * 0.001f * sr));
        }
    }
    const float fmVel = 1.0f - v[dFmVel] + v[dFmVel] * vel * vel * 1.3f;
    const float fb = v[dFb] * 0.18f;
    const float idxCycles = 13.0f / twoPi;

    // supersaw setup
    const float ssMix = v[dSsMix];
    const float ssCenter = -0.55366f * ssMix + 0.99785f;
    const float ssSide = -0.73764f * ssMix * ssMix + 1.2841f * ssMix + 0.044372f;
    const WavetableBank& wt = WavetableBank::get();
    const int table = (int) v[dWt];
    const int pdw = (int) v[dPdW];

    for (int i = 0; i < n; ++i)
    {
        if (! aenv.active()) break;
        lfoFade = std::min (1.0f, lfoFade + fInc);
        const float lfo = c.lfoD[i] * lfoFade;
        const float pitch = notes[i] + off + c.bend + c.vib[i] + lfo * lfoPitch + penv;
        penv *= pitchCoef;
        const float f0 = noteToHz (pitch);
        const float base = f0 / sr;
        const float fe = fenv.next();
        const float ae = aenv.next();
        float sl = 0, sr2 = 0;

        switch (engine)
        {
            case 0: // ---------------------------------------------------------------- FM (4 operators)
            {
                for (int o = 0; o < 4; ++o)
                {
                    if (opAtk[o]) { opLvl[o] += (1.25f - opLvl[o]) * (o == 0 ? 0.5f : opAtkC[o]); if (opLvl[o] >= 1.0f) { opLvl[o] = 1.0f; opAtk[o] = false; } }
                    else opLvl[o] += (sus[o] - opLvl[o]) * opDec[o];
                }
                const float ks = std::clamp (std::pow (261.6f / f0, 0.3f), 0.35f, 1.5f);
                float mScale[4], cScale[4];
                for (int o = 0; o < 4; ++o) { mScale[o] = lvl[o] * lvl[o] * idxCycles * fmVel * ks * opLvl[o]; cScale[o] = lvl[o] * opLvl[o]; }
                for (int u = 0; u < nu; ++u)
                {
                    const float b = base * det[u];
                    float* p = ph[u];
                    for (int o = 0; o < 4; ++o) { p[o] += std::min (0.49f, b * ratio[o]); p[o] -= std::floor (p[o]); }
                    const float fbIn = fb * (fbHist[u][0] + fbHist[u][1]);
                    const float r4 = sine (p[3] + fbIn);
                    fbHist[u][1] = fbHist[u][0]; fbHist[u][0] = r4;
                    const float m4 = r4 * mScale[3];
                    float out = 0.0f;
                    switch (alg)
                    {
                        case 0: { const float m3 = sine (p[2] + m4) * mScale[2]; const float m2 = sine (p[1] + m3) * mScale[1]; out = sine (p[0] + m2) * cScale[0]; break; }
                        case 1: { const float m3 = sine (p[2]) * mScale[2]; const float m2 = sine (p[1] + m3 + m4) * mScale[1]; out = sine (p[0] + m2) * cScale[0]; break; }
                        case 2: { const float m3 = sine (p[2] + m4) * mScale[2]; const float m2 = sine (p[1]) * mScale[1]; out = sine (p[0] + m2 + m3) * cScale[0]; break; }
                        case 3: { const float m3 = sine (p[2]) * mScale[2]; const float m2 = sine (p[1] + m3) * mScale[1]; out = sine (p[0] + m2 + m4) * cScale[0]; break; }
                        case 4: { const float m2 = sine (p[1]) * mScale[1]; out = (sine (p[0] + m2) * cScale[0] + sine (p[2] + m4) * cScale[2]) * 0.7f; break; }
                        case 5: out = (sine (p[0] + m4) * cScale[0] + sine (p[1] + m4) * cScale[1] + sine (p[2] + m4) * cScale[2]) * 0.55f; break;
                        case 6: out = (sine (p[0]) * cScale[0] + sine (p[1]) * cScale[1] + sine (p[2] + m4) * cScale[2]) * 0.55f; break;
                        default: out = (sine (p[0]) * cScale[0] + sine (p[1]) * cScale[1] + sine (p[2]) * cScale[2] + r4 * cScale[3]) * 0.45f; break;
                    }
                    sl += out * gl[u]; sr2 += out * gr[u];
                }
                sl *= uniNorm; sr2 *= uniNorm;
                break;
            }
            case 1: // ---------------------------------------------------------------- Wavetable
            {
                const float pos = v[dWPos] + v[dWEnv] * fe + (v[dWLfo] + lfoTim) * lfo * 0.5f;
                for (int u = 0; u < nu; ++u)
                {
                    const float inc = std::min (0.45f, base * det[u]);
                    ph[u][0] += inc; ph[u][0] -= std::floor (ph[u][0]);
                    const float s = wt.read (table, pos, ph[u][0], inc);
                    sl += s * gl[u]; sr2 += s * gr[u];
                }
                sl *= uniNorm * 0.8f; sr2 *= uniNorm * 0.8f;
                break;
            }
            case 2: // ---------------------------------------------------------------- Supersaw
            {
                const float d = supersawDetune (std::clamp (v[dSsDet] + lfoTim * lfo * 0.3f, 0.0f, 1.0f));
                const float w = v[gWidth];
                for (int k = 0; k < 7; ++k)
                {
                    const float inc = std::min (0.45f, base * (1.0f + d * ssOffs[k]));
                    ssPh[k] += inc; ssPh[k] -= std::floor (ssPh[k]);
                    const float s = sawBlep (ssPh[k], inc) * (k == 3 ? ssCenter : ssSide);
                    const float pan = k == 3 ? 0.0f : w * ((k % 2) ? -1.0f : 1.0f) * (0.4f + 0.2f * (float) std::abs (k - 3));
                    sl += s * std::sqrt (0.5f * (1.0f - pan)); sr2 += s * std::sqrt (0.5f * (1.0f + pan));
                }
                // high-pass at the fundamental, like the original
                const float g = 1.0f - std::exp (-twoPi * f0 / sr);
                ssLp += ((sl + sr2) * 0.5f - ssLp) * g;
                sl = (sl - ssLp) * 0.38f; sr2 = (sr2 - ssLp) * 0.38f;
                break;
            }
            case 3: // ---------------------------------------------------------------- Phase distortion (CZ)
            {
                const float amt = std::clamp (v[dPdAmt] + v[dPdEnv] * fe + lfoTim * lfo * 0.5f, 0.0f, 1.0f);
                for (int u = 0; u < nu; ++u)
                {
                    const float inc = std::min (0.45f, base * det[u]);
                    float& p = ph[u][0];
                    p += inc; p -= std::floor (p);
                    float s;
                    switch (pdw)
                    {
                        case 0: { const float d = 0.5f - 0.495f * amt; const float phi = p < d ? 0.5f * p / d : 0.5f + 0.5f * (p - d) / (1.0f - d); s = -std::cos (twoPi * phi); break; }
                        case 1: { const float half = p < 0.5f ? 0.0f : 0.5f; const float q = (p - half) * 2.0f; const float w = 1.0f - 0.97f * amt; s = -std::cos (twoPi * (half + 0.5f * std::min (1.0f, q / w))); break; }
                        case 2: { const float w = 1.0f - 0.97f * amt; s = -std::cos (twoPi * std::min (1.0f, p / w)); break; }
                        case 3: { const float q = p < 0.5f ? p * 2.0f : (p - 0.5f) * 2.0f; const float d = 0.5f - 0.45f * amt; const float phi = q < d ? 0.5f * q / d : 0.5f + 0.5f * (q - d) / (1.0f - d); s = -std::cos (twoPi * phi) * (p < 0.5f ? 1.0f : 0.6f); break; }
                        default:
                        {
                            const float m = 1.0f + amt * 14.0f;
                            const float win = pdw == 4 ? 1.0f - p : pdw == 5 ? 1.0f - std::abs (2.0f * p - 1.0f) : std::min (1.0f, 2.0f * (1.0f - p));
                            float cp = p * m; cp -= std::floor (cp);
                            s = win * (1.0f - std::cos (twoPi * cp)) - 0.5f;
                            break;
                        }
                    }
                    sl += s * gl[u]; sr2 += s * gr[u];
                }
                sl *= uniNorm * 0.8f; sr2 *= uniNorm * 0.8f;
                break;
            }
            default: // ---------------------------------------------------------------- Additive
            {
                float s = 0.0f;
                for (int k = 0; k < numP; ++k)
                {
                    pPh[k] += std::min (0.49f, base * pRatio[k]); pPh[k] -= std::floor (pPh[k]);
                    s += sine (pPh[k]) * pAmp[k] * (pSus[k] + pDyn[k]);
                    pDyn[k] *= pCoef[k];
                }
                sl = sr2 = s * 0.9f;
                break;
            }
        }
        if (noise > 0) { const float nz = rng.bip() * noise * 0.5f; sl += nz; sr2 += nz; }
        sl = dc[0].process (sl); sr2 = dc[1].process (sr2);

        float yl = sl, yr = sr2;
        if (ft > 0)
        {
            const float oct = envAmt * 7.0f * fe + key * (notes[i] - 60.0f) / 12.0f + lfoCut * lfo * 3.0f + velF * (vel - 0.6f) * 3.0f + mwCut;
            const float g = cutoffG (cut * std::exp2 (oct), sr);
            const int mode = ft == 1 || ft == 2 ? 0 : ft == 3 ? 1 : ft == 4 ? 2 : 3;
            yl = svf[0][0].process (sl, g, res, mode); yr = svf[1][0].process (sr2, g, res, mode);
            if (ft == 2) { yl = svf[0][1].process (yl, g, res * 0.5f, 0); yr = svf[1][1].process (yr, g, res * 0.5f, 0); }
        }
        const float amp = ae * velAmp;
        L[i] += yl * amp * 0.6f;
        R[i] += yr * amp * 0.6f;
    }
}

// =====================================================================================================
//  Engine
// =====================================================================================================
void SynthEngine::prepare (double sampleRate, int maxBlock)
{
    sr = (float) sampleRate;
    for (int c = 0; c < 2; ++c) { busA[c].assign ((size_t) maxBlock, 0.0f); busD[c].assign ((size_t) maxBlock, 0.0f); }
    noteBuf.assign ((size_t) maxBlock, 0.0f);
    lfoBufA.assign ((size_t) maxBlock, 0.0f); lfoBufD.assign ((size_t) maxBlock, 0.0f); vibBuf.assign ((size_t) maxBlock, 0.0f);
    WavetableBank::get();
    reset();
}

void SynthEngine::reset()
{
    for (auto& vc : voices)
    {
        vc.a.aenv.kill(); vc.a.fenv.kill(); vc.d.aenv.kill(); vc.d.fenv.kill(); vc.note = -1; vc.gate = false;
        for (auto& l : vc.a.lad) l.reset();
        for (auto& s : vc.a.svf) s.reset();
    }
    held.clear(); sustainPedal = false; bend = 0; mw = 0;
}

void SynthEngine::allNotesOff()
{
    for (auto& vc : voices) { vc.gate = false; vc.sustained = false; vc.a.release(); vc.d.release(); }
    held.clear();
}

int SynthEngine::activeVoices() const
{
    int n = 0;
    for (auto& vc : voices) n += vc.active() ? 1 : 0;
    return n;
}

void SynthEngine::startVoice (Voice& vc, int note, float vel, bool hard, const float* v, bool glideFromLast)
{
    BlockCtx c; c.v = v; c.sr = sr;
    vc.useA = v[aOn] > 0.5f; vc.useD = v[dOn] > 0.5f;
    vc.note = note; vc.vel = vel; vc.gate = true; vc.sustained = false; vc.age = ++counter;
    vc.targetNote = (float) note;
    if (hard && ! vc.active()) vc.curNote = (glideFromLast && lastNote >= 0 && v[gGlide] > 0.5f) ? lastNote : (float) note;
    const uint32_t seed = (uint32_t) (counter * 7919u + (uint32_t) note * 31u + 1u);
    if (vc.useA) vc.a.start (vel, hard, c, seed);
    if (vc.useD) vc.d.start (vel, hard, c, (float) note, seed);
    lastNote = (float) note;
}

void SynthEngine::noteOn (int note, float vel, const float* v)
{
    const int mode = (int) v[gMode];
    if (mode == 0)
    {
        Voice* best = nullptr;
        for (auto& vc : voices) if (vc.note == note && vc.active()) { best = &vc; break; }   // retrigger same key
        if (! best) for (auto& vc : voices) if (! vc.active()) { best = &vc; break; }
        if (! best)
        {
            uint64_t oldest = UINT64_MAX;
            for (auto& vc : voices) if (! vc.gate && vc.age < oldest) { oldest = vc.age; best = &vc; }
            if (! best) for (auto& vc : voices) if (vc.age < oldest) { oldest = vc.age; best = &vc; }
        }
        const bool wasActive = best->active();
        if (wasActive) { best->a.aenv.kill(); best->d.aenv.kill(); }   // fast steal
        startVoice (*best, note, vel, true, v, true);
        return;
    }
    // mono / legato
    held.erase (std::remove (held.begin(), held.end(), note), held.end());
    held.push_back (note);
    Voice& vc = voices[0];
    for (int i = 1; i < numVoices; ++i) { voices[i].a.release(); voices[i].d.release(); voices[i].gate = false; }
    const bool overlapping = vc.gate && vc.active();
    if (mode == 2 && overlapping)
    {
        vc.note = note; vc.targetNote = (float) note; vc.vel = vel; lastNote = (float) note;   // slide, no retrigger
        return;
    }
    const float prev = vc.active() ? vc.curNote : lastNote;
    startVoice (vc, note, vel, true, v, true);
    if (prev >= 0 && v[gGlide] > 0.5f && mode == 1) vc.curNote = prev;
    if (! overlapping && mode == 2) vc.curNote = (float) note;   // legato: glide only between overlapping notes
}

void SynthEngine::noteOff (int note, const float* v)
{
    const int mode = (int) v[gMode];
    if (mode == 0)
    {
        for (auto& vc : voices)
            if (vc.note == note && vc.gate)
            {
                if (sustainPedal) { vc.sustained = true; continue; }
                vc.gate = false; vc.a.release(); vc.d.release();
            }
        return;
    }
    held.erase (std::remove (held.begin(), held.end(), note), held.end());
    Voice& vc = voices[0];
    if (! held.empty())
    {
        if (vc.note == note) { vc.note = held.back(); vc.targetNote = (float) held.back(); lastNote = vc.targetNote; }
        return;
    }
    if (vc.note == note)
    {
        if (sustainPedal) { vc.sustained = true; return; }
        vc.gate = false; vc.a.release(); vc.d.release();
    }
}

void SynthEngine::handleMidi (const juce::MidiMessage& m, const float* v)
{
    if (m.isNoteOn()) noteOn (m.getNoteNumber(), m.getFloatVelocity(), v);
    else if (m.isNoteOff()) noteOff (m.getNoteNumber(), v);
    else if (m.isPitchWheel()) bend = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
    else if (m.isController())
    {
        const int cc = m.getControllerNumber();
        if (cc == 1) mw = (float) m.getControllerValue() / 127.0f;
        else if (cc == 64)
        {
            sustainPedal = m.getControllerValue() >= 64;
            if (! sustainPedal)
                for (auto& vc : voices) if (vc.sustained) { vc.sustained = false; vc.gate = false; vc.a.release(); vc.d.release(); }
        }
        else if (cc == 123 || cc == 120) allNotesOff();
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff()) allNotesOff();
}

void SynthEngine::renderVoices (int start, int n, const float* v)
{
    // shared per-sample modulators
    const int shA = (int) v[aLfoShape], shD = (int) v[dLfoShape];
    const float incA = lfoInc (v[aLfoRate], sr), incD = lfoInc (v[dLfoRate], sr);
    const float vibDepth = mw * v[gMwVib] * 0.8f;
    for (int i = 0; i < n; ++i)
    {
        lfoBufA[(size_t) i] = lfoA.next (shA, incA);
        lfoBufD[(size_t) i] = lfoD.next (shD, incD);
        vibPh += 5.5f / sr; vibPh -= std::floor (vibPh);
        vibBuf[(size_t) i] = vibDepth * sine (vibPh);
    }
    BlockCtx c; c.v = v; c.sr = sr; c.bend = bend * v[gBend]; c.mw = mw;
    c.lfoA = lfoBufA.data(); c.lfoD = lfoBufD.data(); c.vib = vibBuf.data();
    const float glideMs = v[gGlide];
    const float gCoef = glideMs < 0.5f ? 1.0f : 1.0f - std::exp (-4.6f / (glideMs * 0.001f * sr));

    for (auto& vc : voices)
    {
        if (! vc.active()) continue;
        for (int i = 0; i < n; ++i)
        {
            vc.curNote += (vc.targetNote - vc.curNote) * gCoef;
            noteBuf[(size_t) i] = vc.curNote;
        }
        if (vc.useA && vc.a.active()) vc.a.render (busA[0].data() + start, busA[1].data() + start, n, noteBuf.data(), c);
        if (vc.useD && vc.d.active()) vc.d.render (busD[0].data() + start, busD[1].data() + start, n, noteBuf.data(), c);
        if (! vc.active()) vc.note = -1;
    }
}

void SynthEngine::render (float* L, float* R, int n, const juce::MidiBuffer& midi, const float* v)
{
    for (int c = 0; c < 2; ++c) { std::fill_n (busA[c].data(), n, 0.0f); std::fill_n (busD[c].data(), n, 0.0f); }
    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = std::clamp (meta.samplePosition, 0, n);
        if (at > pos) { renderVoices (pos, at - pos, v); pos = at; }
        handleMidi (meta.getMessage(), v);
    }
    if (pos < n) renderVoices (pos, n - pos, v);

    // section buses: level / pan, D lo-fi
    const float la = v[aOn] > 0.5f ? v[aLevel] : 0.0f, ld = v[dOn] > 0.5f ? v[dLevel] : 0.0f;
    const float paL = std::sqrt (0.5f * (1.0f - v[aPan])) * 1.4142f, paR = std::sqrt (0.5f * (1.0f + v[aPan])) * 1.4142f;
    const float pdL = std::sqrt (0.5f * (1.0f - v[dPan])) * 1.4142f, pdR = std::sqrt (0.5f * (1.0f + v[dPan])) * 1.4142f;
    const float bits = v[dBits], srr = v[dSrr];
    const float q = bits > 0.001f ? std::exp2 (15.0f - bits * 11.0f) : 0.0f;
    const float hold = 1.0f + srr * srr * 24.0f;
    for (int i = 0; i < n; ++i)
    {
        float dl = busD[0][(size_t) i], dr = busD[1][(size_t) i];
        if (srr > 0.001f)
        {
            srrCount += 1.0f;
            if (srrCount >= hold) { srrCount -= hold; srrHold[0] = dl; srrHold[1] = dr; }
            dl = srrHold[0]; dr = srrHold[1];
        }
        if (q > 0) { dl = std::round (dl * q) / q; dr = std::round (dr * q) / q; }
        L[i] = dcA[0].process (busA[0][(size_t) i]) * la * paL + dl * ld * pdL;
        R[i] = dcA[1].process (busA[1][(size_t) i]) * la * paR + dr * ld * pdR;
    }
}
