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
            case 3: return sine (ph);
            // Minimoog Model D waveform switch: shark tooth (triangle / sawtooth cross), square and the two fixed
            // rectangles (no pulse-width control on the original), reverse sawtooth (oscillator 3)
            case 4: return (0.5f * (triangle (ph) + sawBlep (ph, inc)) + 0.25f) * (1.0f / 0.75f);
            case 5: return pulseBlep (ph, inc, 0.5f);
            case 6: return pulseBlep (ph, inc, 0.30f);
            case 7: return pulseBlep (ph, inc, 0.15f);
            case 8: return -sawBlep (ph, inc);
            default: return sine (ph);
        }
    }

    void unisonLayout (int n, float detune, float width, float* offs, float* pans)
    {
        if (n <= 1) { offs[0] = 0; pans[0] = 0; return; }
        for (int i = 0; i < n; ++i)
        {
            const float x = (float) i / (float) (n - 1) * 2.0f - 1.0f;
            offs[i] = detune * 0.55f * x * (0.6f + 0.4f * std::abs (x));
            pans[i] = width * x * (i % 2 ? -1.0f : 1.0f) * (n == 2 ? -1.0f : 1.0f);
        }
    }

    float supersawDetune (float x)
    {
        const double v = x;
        return (float) (10028.7312891634 * std::pow (v, 11) - 50818.8652045924 * std::pow (v, 10) + 111363.4808729368 * std::pow (v, 9)
            - 138150.6761080548 * std::pow (v, 8) + 106649.6679158292 * std::pow (v, 7) - 53046.9642751875 * std::pow (v, 6)
            + 17019.9518580080 * std::pow (v, 5) - 3425.0836591318 * std::pow (v, 4) + 404.2703938388 * std::pow (v, 3)
            - 24.1878824391 * v * v + 0.6717417634 * v + 0.0030115596);
    }
    constexpr float ssOffs[7] = { -0.11002313f, -0.06288439f, -0.01952356f, 0.0f, 0.01991221f, 0.06216538f, 0.10745242f };
    inline float fadeInc (float ms, float sr) { return ms < 1.0f ? 1.0f : 1000.0f / (ms * sr); }
    inline float opRatio (const float* v, int o) { const float c = v[opSuffix (o, opCrs)]; return (c < 0.5f ? 0.5f : c) * (1.0f + v[opSuffix (o, opFin)]); }
}

// =====================================================================================================
//  DX7-style algorithm table: "mod>op" pairs (operators 1..6) | carriers | feedback operator
// =====================================================================================================
FmAlgorithms::FmAlgorithms()
{
    static const char* t[32] = {
        "65 54 43 21|13|6", "65 54 43 21|13|2", "65 54 32 21|14|6", "65 54 32 21|14|6", "65 43 21|135|6", "65 43 21|135|6",
        "65 53 43 21|13|6", "65 53 43 21|13|4", "65 53 43 21|13|2", "32 21 64 54|14|3", "32 21 64 54|14|6", "21 63 53 43|13|2",
        "21 63 53 43|13|6", "21 64 54 43|13|6", "21 64 54 43|13|2", "21 43 31 65 51|1|6", "21 43 31 65 51|1|2", "21 31 65 54 41|1|3",
        "32 21 64 65|145|6", "31 32 64 54|124|3", "31 32 64 65|1245|3", "21 63 64 65|1345|6", "32 64 65|1245|6", "63 64 65|12345|6",
        "64 65|12345|6", "32 64 54|124|6", "32 64 54|124|3", "21 54 43|136|5", "43 65|1235|6", "54 43|1236|5", "65|12345|6", "|123456|6" };
    for (int i = 0; i < 32; ++i)
    {
        auto parts = juce::StringArray::fromTokens (t[i], "|", "");
        Alg al {};
        for (auto& pr : juce::StringArray::fromTokens (parts[0], " ", ""))
            if (pr.length() == 2) { const int from = pr[0] - '1', to = pr[1] - '1'; al.mods[to] |= (uint8_t) (1u << from); }
        for (auto ch : parts[1]) al.carriers |= (uint8_t) (1u << (ch - '1'));
        al.fb = parts[2].getIntValue() - 1;
        a[i] = al;
    }
}
const FmAlgorithms& FmAlgorithms::get() { static FmAlgorithms f; return f; }

// =====================================================================================================
//  Module part
// =====================================================================================================
void ModulePart::setupAdditive (const ModCtx& c, float note)
{
    const float* v = c.v;
    const int model = (int) v[mAddM];
    const float f0 = noteToHz (note + v[mOct] * 12.0f + v[mSemi]);
    const float bright = v[mBright], tilt = v[mTilt], inh = v[mInh], even = v[mEven];
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
        case 0: { static const int hs[] = { 1, 2, 3, 4, 6, 8, 10, 12, 16 }; static const float am[] = { 1.0f, 0.85f, 0.6f, 0.75f, 0.45f, 0.5f, 0.2f, 0.25f, 0.15f };
                  for (int i = 0; i < 9; ++i) add (stretch (hs[i]), am[i] * bt (hs[i]), i == 0 ? 1.0f : 0.45f, 0.7f * td (hs[i])); break; }
        case 1: { static const int hs[] = { 1, 2, 3, 4, 6, 8 };
                  for (int i = 0; i < 6; ++i) add (stretch (hs[i]), (i < 3 ? 1.0f : 0.7f) * bt (hs[i]), (hs[i] == 3 || hs[i] == 4) ? 0.55f : 1.0f, 0.45f); break; }
        case 2: add (1.0f, 1.0f, 0.0f, 3.5f * ks); add (2.0f, 0.28f * bt (2), 0.0f, 1.6f * ks * td (2)); add (3.0f, 0.10f * bt (3), 0.0f, 0.7f * ks * td (3));
                add (7.0f * (1.0f + B), (0.12f + 0.35f * vel) * bt (7), 0.0f, 0.09f * td (7)); add (14.0f, (0.04f + 0.18f * vel) * bt (14), 0.0f, 0.04f); break;
        case 3: { const float Bp = 0.0004f + inh * 0.003f;
                  for (int k = 1; k <= 32; ++k) add ((float) k * std::sqrt (1.0f + Bp * (float) (k * k)), std::pow ((float) k, -(1.5f - 0.6f * bright - 0.5f * vel)) * ((k % 2 == 0) ? even : 1.0f),
                                                     0.0f, 5.0f * ks / (1.0f + (0.25f + tilt) * 0.5f * (float) (k - 1)));
                  break; }
        case 4: for (int k = 1; k <= 40; ++k) add (stretch (k), (1.0f / (float) k) * bt (k), 1.0f, 100.0f); break;
        case 5: for (int k = 1; k <= 40; ++k)
                {
                    const float hz = f0 * (float) k;
                    auto pk = [] (float x, float cc, float bw) { return std::exp (-((x - cc) / bw) * ((x - cc) / bw)); };
                    add ((float) k, (0.03f + pk (hz, 750, 110) + 0.6f * pk (hz, 1150, 140) + 0.3f * pk (hz, 2600, 220) * bright * 2.0f) / std::sqrt ((float) k), 1.0f, 100.0f);
                }
                break;
        case 6: { static const float r[] = { 0.5f, 1.0f, 1.19f, 1.56f, 2.0f, 2.51f, 2.66f, 3.01f, 4.1f, 5.4f, 6.8f };
                  static const float a[] = { 0.5f, 1.0f, 0.6f, 0.45f, 0.55f, 0.35f, 0.3f, 0.25f, 0.2f, 0.15f, 0.1f };
                  for (int i = 0; i < 11; ++i) add (r[i] * (1.0f + inh * 0.02f * (float) i), a[i] * std::pow (r[i], (bright - 0.5f)), 0.0f, 6.0f / (1.0f + tilt * 2.0f * r[i])); break; }
        case 7: { static const float r[] = { 1.0f, 3.93f, 9.2f, 16.6f }; static const float a[] = { 1.0f, 0.35f, 0.15f, 0.06f }; static const float t[] = { 0.7f, 0.15f, 0.05f, 0.025f };
                  for (int i = 0; i < 4; ++i) add (r[i], a[i] * (0.5f + bright), 0.0f, t[i] * ks * (1.5f - tilt)); break; }
        case 8: { static const float r[] = { 1.0f, 2.32f, 4.25f, 6.63f, 9.38f, 12.9f }; static const float t[] = { 3.0f, 1.8f, 1.0f, 0.6f, 0.4f, 0.3f };
                  for (int i = 0; i < 6; ++i) add (r[i], std::pow (0.6f + bright * 0.3f, (float) i), 0.0f, t[i] * (1.5f - tilt)); break; }
        case 9: for (int k = 1; k <= 30; ++k) add (stretch (k), std::pow ((float) k, -(1.7f - 1.1f * bright)) * ((k % 2 == 0) ? even : 1.0f), 1.0f, 100.0f); break;
        default: for (int k = 1; k <= 8; ++k) add (stretch (k), (1.0f / (float) (k * k)) * bt (k) * 2.0f, 1.0f, 100.0f); break;
    }
    float sum = 0.0f;
    for (int i = 0; i < numP; ++i) sum += pAmp[i];
    const float norm = sum > 0 ? 1.0f / std::max (1.0f, sum * 0.7f) : 0.0f;
    for (int i = 0; i < numP; ++i) pAmp[i] *= norm;
}

void ModulePart::start (float velocity, bool hard, const ModCtx& c, float note, uint32_t seed)
{
    const float* v = c.v;
    vel = velocity;
    const bool fresh = hard && ! aenv.active();
    const int engine = (int) v[mEngine];
    if (fresh)
    {
        rng.seed (seed);
        for (int u = 0; u < maxUni; ++u)
        {
            ph1[u] = rng.uni(); ph2[u] = rng.uni(); o2prev[u] = 0;
            for (int o = 0; o < 6; ++o) fmPh[u][o] = 0.0f;
            fbHist[u][0] = fbHist[u][1] = 0;
        }
        for (int k = 0; k < 7; ++k) ssPh[k] = rng.uni();
        phSub = ph1[0];
        lfoFade = v[mLfoDelay] < 1.0f ? 1.0f : 0.0f; drift = rng.bip() * 0.5f; driftTarget = rng.bip();
        if (engine == 5) { setupAdditive (c, note); for (int k = 0; k < numPartials; ++k) { pAmpSm[k] = pAmp[k]; pSusSm[k] = pAmp[k] * pSus[k]; } }
        fmIdx = -1.0f; fbSm = wposSm = pdSm = -1.0f; for (auto& l : lvlSm) l = -1.0f;
    }
    else if (hard && engine == 5)
    {
        // retrigger of a voice that is still sounding (mono / note stealing): the oscillators keep running.
        // Resetting their phases (or the partial amplitudes) mid-waveform made an audible click on every note change.
        const int oldN = numP;
        float ph[numPartials], dyn[numPartials];
        std::copy (pPh, pPh + numPartials, ph); std::copy (pDyn, pDyn + numPartials, dyn);
        setupAdditive (c, note);
        for (int k = 0; k < std::min (oldN, numP); ++k) { pPh[k] = ph[k]; pDyn[k] = dyn[k]; }
        for (int k = oldN; k < numP; ++k) { pAmpSm[k] = 0.0f; pSusSm[k] = 0.0f; }   // new partials fade in
    }
    penv = v[mPenv];
    const bool acc = velocity > 0.75f;
    accent = v[mAccent] * std::max (0.0f, (velocity - 0.75f) / 0.25f);
    if (acc) accSweep = std::min (1.6f, accSweep * 0.6f + 1.0f);        // 303 accent sweep capacitor builds up
    const bool is303 = (int) v[mFlt] == 1;
    fenv.set (v[mFA], (is303 && acc) ? 200.0f : v[mFD], v[mFS], v[mFR], c.sr);
    aenv.set (std::max (2.0f, v[mAA]), v[mAD], v[mAS], v[mAR], c.sr);   // 2 ms minimum attack: no click at the start of a note
    fenv.gateOn (fresh);
    aenv.gateOn (fresh);
    for (int o = 0; o < 6; ++o)
    {
        opEnv[o].set (v[opSuffix (o, opAtk)], v[opSuffix (o, opDec)], v[opSuffix (o, opSus)], v[opSuffix (o, opRel)], c.sr);
        if (hard) opEnv[o].gateOn (fresh);
    }
    if (fresh) for (int ch = 0; ch < 2; ++ch) { lad[ch].reset(); moog[ch].reset(); d303[ch].reset(); svf[ch][0].reset(); svf[ch][1].reset(); dist[ch].reset(); prevIn[ch] = 0; }
}

void ModulePart::render (float* L, float* R, int n, const float* notes, const ModCtx& c)
{
    const float* v = c.v;
    const float* g = c.g;
    const auto& lay = layout();
    const float sr = c.sr;
    const int engine = (int) v[mEngine];
    const float off = v[mOct] * 12.0f + v[mSemi] + v[mFine] * 0.01f;
    const int w1 = (int) v[mO1Wave], w2 = (int) v[mO2Wave];
    const float o2 = v[mO2Oct] * 12.0f + v[mO2Semi] + v[mO2Fine] * 0.01f;
    const float lvl1 = v[mO1Lvl], lvl2 = v[mO2Lvl], ring = v[mRing], sub = v[mSub], noise = v[mNoise];
    const bool sync = v[mSync] > 0.5f;
    const float xmod = v[mXmod], pwm = v[mPwm];
    const int nu = (engine == 3 || engine == 5) ? 1 : std::clamp ((int) v[mUni], 1, maxUni);
    float offs[maxUni], pans[maxUni], gl[maxUni], gr[maxUni], det[maxUni];
    unisonLayout (nu, v[mDetune], g[lay.gWidth], offs, pans);
    for (int u = 0; u < nu; ++u) { det[u] = semisToRatio (offs[u]); gl[u] = std::sqrt (0.5f * (1.0f - pans[u])); gr[u] = std::sqrt (0.5f * (1.0f + pans[u])); }
    const float uniNorm = 1.0f / std::sqrt ((float) nu);
    const int ft = (int) v[mFlt];
    const float cut = v[mCut], res = v[mRes], fdrive = 1.0f + v[mFDrive] * 5.0f, key = v[mKey], envAmt = v[mEnv], velF = v[mVelF];
    const int distType = (int) v[mDist];
    const float dGain = v[mDGain], dTone = v[mDTone], dMix = v[mDMix];
    const float lfoPitch = v[mLfoPitch], lfoCut = v[mLfoCut], lfoTim = v[mLfoTim];
    const float pitchCoef = std::exp (-1.0f / (std::max (1.0f, v[mPdec]) * 0.001f * sr / 4.6f));
    const float fInc = fadeInc (v[mLfoDelay], sr);
    const float driftAmt = engine == 0 ? v[mDrift] * 0.12f : 0.0f;
    const float glide5 = 1.0f - std::exp (-1.0f / (0.005f * sr));   // one-pole, ~5 ms
    const float glide1 = 1.0f - std::exp (-1.0f / (0.0015f * sr));  // one-pole, ~1.5 ms (smooths the 32-sample parameter steps)
    const float velAmp = 1.0f - g[lay.gVel] + g[lay.gVel] * vel;
    const float accentAmp = 1.0f + accent * 0.45f, accentEnv = accent * 0.35f;
    const float mwCut = c.mw * g[lay.gMwCut] * 4.0f;
    const bool is303 = ft == 1;
    const float accDecay = std::exp (-1.0f / (0.2f * sr));
    aenv.set (std::max (2.0f, v[mAA]), v[mAD], v[mAS], v[mAR], sr);

    // FM setup
    const auto& alg = FmAlgorithms::get().a[std::clamp ((int) v[mAlg], 0, 31)];
    float ratio[6], lvl[6];
    for (int o = 0; o < 6; ++o) { ratio[o] = opRatio (v, o); lvl[o] = v[opSuffix (o, opLvl)]; }
    const float fmVel = 1.0f - v[mFmVel] + v[mFmVel] * vel * vel * 1.3f;
    const float fb = v[mFb] * 0.2f;
    int numCar = 0; for (int o = 0; o < 6; ++o) numCar += (alg.carriers >> o) & 1;
    const float carNorm = 1.0f / std::max (1.0f, std::sqrt ((float) numCar) * 1.15f);
    // supersaw
    const float ssMix = v[mSsMix];
    const float ssCenter = -0.55366f * ssMix + 0.99785f;
    const float ssSide = -0.73764f * ssMix * ssMix + 1.2841f * ssMix + 0.044372f;
    const WavetableBank& wt = WavetableBank::get();
    const int table = (int) v[mWt];
    const int pdw = (int) v[mPdW];

    for (int i = 0; i < n; ++i)
    {
        if (! aenv.active()) break;
        lfoFade = std::min (1.0f, lfoFade + fInc);
        const float lfo = c.lfo[i] * lfoFade;
        if (++driftCount >= 256) { driftCount = 0; if (rng.uni() < 0.02f) driftTarget = rng.bip(); }
        drift += (driftTarget - drift) * 0.00005f;
        const float pitch = notes[i] + off + c.bend + c.vib[i] + lfo * lfoPitch + penv + drift * driftAmt * 4.0f;
        penv *= pitchCoef;
        const float f0 = noteToHz (pitch);
        const float base = f0 / sr;
        const float fe = fenv.next();
        const float ae = aenv.next();
        accSweep *= accDecay;
        float sl = 0, sr2 = 0;

        switch (engine)
        {
            case 0:   // ------------------------------------------------ Analog
            {
                const float inc2b = base * semisToRatio (o2);
                const float pw1 = std::clamp (v[mO1Pw] + pwm * lfo * 0.4f, 0.03f, 0.97f);
                const float pw2 = std::clamp (v[mO2Pw] + pwm * lfo * 0.4f, 0.03f, 0.97f);
                for (int u = 0; u < nu; ++u)
                {
                    float inc1 = std::min (0.45f, base * det[u] * (1.0f + drift * driftAmt * 0.01f * (float) (u - 3)));
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
                    sl += s * gl[u]; sr2 += s * gr[u];
                }
                sl *= uniNorm; sr2 *= uniNorm;
                if (sub > 0)
                {
                    const float si = std::min (0.45f, base * 0.5f);
                    phSub += si; phSub -= std::floor (phSub);
                    const float s = pulseBlep (phSub, si, 0.5f) * sub * 0.8f;
                    sl += s * 0.7071f; sr2 += s * 0.7071f;
                }
                sl *= 0.5f; sr2 *= 0.5f;
                break;
            }
            case 1:   // ------------------------------------------------ FM, 6 operators
            {
                float env[6], mScale[6], cScale[6];
                const float ks = std::clamp (std::pow (261.6f / f0, 0.3f), 0.35f, 1.5f);
                // the index scale (key scaling, velocity) glides ~5 ms: a sudden change of the modulation index is a jump
                // of the carrier's phase, i.e. a click at every note change of a sounding voice
                const float idxTarget = fmVel * ks;
                fmIdx = fmIdx < 0.0f ? idxTarget : fmIdx + (idxTarget - fmIdx) * glide5;
                const float fbS = glide (fbSm, fb, glide1);
                for (int o = 0; o < 6; ++o)
                {
                    env[o] = opEnv[o].next();
                    const float lv = glide (lvlSm[o], lvl[o], glide1);
                    mScale[o] = lv * lv * (13.0f / twoPi) * fmIdx * env[o];
                    cScale[o] = lv * env[o];
                }
                for (int u = 0; u < nu; ++u)
                {
                    const float b = base * det[u];
                    float* p = fmPh[u];
                    float out[6] {};
                    for (int o = 0; o < 6; ++o) { p[o] += std::min (0.49f, b * ratio[o]); p[o] -= std::floor (p[o]); }
                    float mix = 0.0f;
                    for (int o = 5; o >= 0; --o)
                    {
                        float in = 0.0f;
                        for (int m = 0; m < 6; ++m) if ((alg.mods[o] >> m) & 1) in += out[m];
                        if (o == alg.fb) in += fbS * (fbHist[u][0] + fbHist[u][1]);
                        const float r = sine (p[o] + in);
                        if (o == alg.fb) { fbHist[u][1] = fbHist[u][0]; fbHist[u][0] = r * env[o]; }
                        out[o] = r * mScale[o];
                        if ((alg.carriers >> o) & 1) mix += r * cScale[o];
                    }
                    mix *= carNorm;
                    sl += mix * gl[u]; sr2 += mix * gr[u];
                }
                sl *= uniNorm; sr2 *= uniNorm;
                break;
            }
            case 2:   // ------------------------------------------------ Wavetable
            {
                const float pos = glide (wposSm, v[mWPos], glide1) + v[mWEnv] * fe + (v[mWLfo] + lfoTim) * lfo * 0.5f;
                for (int u = 0; u < nu; ++u)
                {
                    const float inc = std::min (0.45f, base * det[u]);
                    ph1[u] += inc; ph1[u] -= std::floor (ph1[u]);
                    const float s = wt.read (table, pos, ph1[u], inc);
                    sl += s * gl[u]; sr2 += s * gr[u];
                }
                sl *= uniNorm * 0.8f; sr2 *= uniNorm * 0.8f;
                break;
            }
            case 3:   // ------------------------------------------------ Supersaw
            {
                const float d = supersawDetune (std::clamp (v[mSsDet] + lfoTim * lfo * 0.3f, 0.0f, 1.0f));
                const float w = g[lay.gWidth];
                for (int k = 0; k < 7; ++k)
                {
                    const float inc = std::min (0.45f, base * (1.0f + d * ssOffs[k]));
                    ssPh[k] += inc; ssPh[k] -= std::floor (ssPh[k]);
                    const float s = sawBlep (ssPh[k], inc) * (k == 3 ? ssCenter : ssSide);
                    const float pan = k == 3 ? 0.0f : w * ((k % 2) ? -1.0f : 1.0f) * (0.4f + 0.2f * (float) std::abs (k - 3));
                    sl += s * std::sqrt (0.5f * (1.0f - pan)); sr2 += s * std::sqrt (0.5f * (1.0f + pan));
                }
                const float hg = 1.0f - std::exp (-twoPi * f0 / sr);
                ssLp += ((sl + sr2) * 0.5f - ssLp) * hg;
                sl = (sl - ssLp) * 0.38f; sr2 = (sr2 - ssLp) * 0.38f;
                break;
            }
            case 4:   // ------------------------------------------------ Phase distortion
            {
                const float amt = std::clamp (glide (pdSm, v[mPdAmt], glide1) + v[mPdEnv] * fe + lfoTim * lfo * 0.5f, 0.0f, 1.0f);
                for (int u = 0; u < nu; ++u)
                {
                    const float inc = std::min (0.45f, base * det[u]);
                    float& p = ph1[u];
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
            default:  // ------------------------------------------------ Additive
            {
                float s = 0.0f;
                for (int k = 0; k < numP; ++k)
                {
                    pPh[k] += std::min (0.49f, base * pRatio[k]); pPh[k] -= std::floor (pPh[k]);
                    // gains glide (~5 ms) to the new note's values instead of jumping (click on mono note changes)
                    pAmpSm[k] += (pAmp[k] - pAmpSm[k]) * glide5;
                    pSusSm[k] += (pAmp[k] * pSus[k] - pSusSm[k]) * glide5;
                    s += sine (pPh[k]) * (pSusSm[k] + pAmpSm[k] * pDyn[k]);
                    pDyn[k] *= pCoef[k];
                }
                sl = sr2 = s * 0.9f;
                break;
            }
        }
        if (noise > 0) { const float nz = rng.bip() * noise * 0.5f; sl += nz; sr2 += nz; }
        if (engine != 0) { sl = dc[0].process (sl); sr2 = dc[1].process (sr2); }

        // ------------------------------------------------------------ filter
        const float oct = envAmt * 7.0f * (fe + accentEnv * fe) + (is303 ? accSweep * accent * 1.2f : 0.0f)
                        + key * (notes[i] - 60.0f) / 12.0f + lfoCut * lfo * 3.0f + velF * (vel - 0.6f) * 3.0f + mwCut;
        const float fc = std::clamp (cut * std::exp2 (oct), 16.0f, 20000.0f);
        float yl = sl, yr = sr2;
        if (ft == 1)   // 303 diode ladder, 2x oversampled
        {
            const float sr2x = sr * 2.0f;
            float in[2] = { sl, sr2 }, out[2];
            for (int ch = 0; ch < 2; ++ch)
            {
                const float mid = 0.5f * (prevIn[ch] + in[ch]);
                const float a = d303[ch].process (mid, fc, sr2x, res, fdrive);
                const float b = d303[ch].process (in[ch], fc, sr2x, res, fdrive);
                out[ch] = 0.5f * (a + b);
                prevIn[ch] = in[ch];
            }
            yl = out[0]; yr = out[1];
        }
        else if (ft == 0 && c.circuit == dsp::Circuit::Moog)   // Moog transistor ladder (per-stage saturation)
        {
            yl = moog[0].process (sl, fc, sr, res, fdrive); yr = moog[1].process (sr2, fc, sr, res, fdrive);
        }
        else if (ft == 0)
        {
            const float gg = cutoffG (fc, sr);
            yl = lad[0].process (sl, gg, res, 4, fdrive); yr = lad[1].process (sr2, gg, res, 4, fdrive);
        }
        else if (ft != 7)
        {
            const float gg = cutoffG (fc, sr);
            const int mode = ft == 2 || ft == 5 ? 0 : ft == 3 ? 1 : ft == 4 ? 2 : 3;
            const bool sat = ft == 2 || ft == 3 || ft == 4;
            const float inL = sat ? fastTanh (sl * fdrive) / std::sqrt (fdrive) : sl, inR = sat ? fastTanh (sr2 * fdrive) / std::sqrt (fdrive) : sr2;
            yl = svf[0][0].process (inL, gg, res, mode, sat); yr = svf[1][0].process (inR, gg, res, mode, sat);
            if (ft == 5) { yl = svf[0][1].process (yl, gg, res * 0.5f, 0); yr = svf[1][1].process (yr, gg, res * 0.5f, 0); }
        }
        // ------------------------------------------------------------ pedal distortion (2x oversampled)
        if (distType > 0)
        {
            float xs[2] = { yl, yr };
            for (int ch = 0; ch < 2; ++ch)
            {
                const float a = dist[ch].process (distType, xs[ch], dGain, dTone, sr * 2.0f);
                const float b = dist[ch].process (distType, xs[ch], dGain, dTone, sr * 2.0f);
                xs[ch] = xs[ch] * (1.0f - dMix) + 0.5f * (a + b) * dMix;
            }
            yl = xs[0]; yr = xs[1];
        }
        const float amp = ae * velAmp * accentAmp;
        L[i] += yl * amp;
        R[i] += yr * amp;
    }
}

// =====================================================================================================
//  Engine
// =====================================================================================================
void SynthEngine::prepare (double sampleRate, int maxBlock)
{
    sr = (float) sampleRate;
    const size_t mb = (size_t) juce::jmax (64, maxBlock);
    for (int m = 0; m < numModules; ++m)
    {
        for (int c = 0; c < 2; ++c) { bus[m][c].assign (mb, 0.0f); dly[m][c].assign ((size_t) (sr * 2.6f) + 4, 0.0f); }
        lfoBuf[m].assign (mb, 0.0f);
        dlyW[m] = 0; dlyTime[m] = 0;
    }
    noteBuf.assign (mb, 0.0f); vibBuf.assign (mb, 0.0f);
    WavetableBank::get(); FmAlgorithms::get();
    reset();
}

void SynthEngine::reset()
{
    for (auto& vc : voices) { for (auto& p : vc.part) p.kill(); vc.note = -1; vc.gate = false; }
    held.clear(); sustainPedal = false; bend = 0; mw = 0;
}

void SynthEngine::allNotesOff()
{
    for (auto& vc : voices) { vc.gate = false; vc.sustained = false; for (auto& p : vc.part) p.release(); }
    held.clear();
}

int SynthEngine::activeVoices() const { int n = 0; for (auto& vc : voices) n += vc.active() ? 1 : 0; return n; }

bool SynthEngine::moduleUsed (int m, const float* v) const
{
    return m < (int) v[layout().gSlots] && v[mod (m, mOn)] > 0.5f;
}

void SynthEngine::startVoice (Voice& vc, int note, float vel, bool hard, const float* v, bool glideFromLast)
{
    vc.note = note; vc.vel = vel; vc.gate = true; vc.sustained = false; vc.age = ++counter;
    vc.targetNote = (float) note;
    if (hard && ! vc.active()) vc.curNote = (glideFromLast && lastNote >= 0 && v[layout().gGlide] > 0.5f) ? lastNote : (float) note;
    const uint32_t seed = (uint32_t) (counter * 7919u + (uint32_t) note * 31u + 1u);
    for (int m = 0; m < numModules; ++m)
    {
        vc.use[m] = moduleUsed (m, v);
        if (! vc.use[m]) { vc.part[m].kill(); continue; }
        ModCtx c; c.v = v + layout().moduleBase[m]; c.g = v; c.sr = sr;
        vc.part[m].start (vel, hard, c, (float) note, seed + (uint32_t) m * 977u);
    }
    lastNote = (float) note;
}

void SynthEngine::noteOn (int note, float vel, const float* v)
{
    const int mode = (int) v[layout().gMode];
    if (mode == 0)
    {
        Voice* best = nullptr;
        for (auto& vc : voices) if (vc.note == note && vc.active()) { best = &vc; break; }
        if (! best) for (auto& vc : voices) if (! vc.active()) { best = &vc; break; }
        if (! best)
        {
            uint64_t oldest = UINT64_MAX;
            for (auto& vc : voices) if (! vc.gate && vc.age < oldest) { oldest = vc.age; best = &vc; }
            if (! best) for (auto& vc : voices) if (vc.age < oldest) { oldest = vc.age; best = &vc; }
        }
        // a voice that is still sounding (same note again, or the oldest one stolen) is not cut to zero: it restarts
        // like a mono note change (oscillators keep running, envelopes rise from their current level) -> no click
        startVoice (*best, note, vel, true, v, true);
        return;
    }
    held.erase (std::remove (held.begin(), held.end(), note), held.end());
    held.push_back (note);
    Voice& vc = voices[0];
    for (int i = 1; i < numVoices; ++i) { for (auto& p : voices[i].part) p.release(); voices[i].gate = false; }
    const bool overlapping = vc.gate && vc.active();
    if (mode == 2 && overlapping)
    {
        vc.note = note; vc.targetNote = (float) note; vc.vel = vel; lastNote = (float) note;
        return;
    }
    const float prev = vc.active() ? vc.curNote : lastNote;
    startVoice (vc, note, vel, true, v, true);
    if (prev >= 0 && v[layout().gGlide] > 0.5f && mode == 1) vc.curNote = prev;
    if (! overlapping && mode == 2) vc.curNote = (float) note;
}

void SynthEngine::noteOff (int note, const float* v)
{
    const int mode = (int) v[layout().gMode];
    if (mode == 0)
    {
        for (auto& vc : voices)
            if (vc.note == note && vc.gate)
            {
                if (sustainPedal) { vc.sustained = true; continue; }
                vc.gate = false; for (auto& p : vc.part) p.release();
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
        vc.gate = false; for (auto& p : vc.part) p.release();
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
                for (auto& vc : voices) if (vc.sustained) { vc.sustained = false; vc.gate = false; for (auto& p : vc.part) p.release(); }
        }
        else if (cc == 123 || cc == 120) allNotesOff();
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff()) allNotesOff();
}

void SynthEngine::renderVoices (int start, int n, const float* v)
{
    const auto& lay = layout();
    for (int m = 0; m < numModules; ++m)
    {
        const float* mv = v + lay.moduleBase[m];
        const int sh = (int) mv[mLfoShape];
        const float inc = mv[mLfoRate] / sr;
        for (int i = 0; i < n; ++i) lfoBuf[m][(size_t) i] = lfo[m].next (sh, inc);
    }
    const float vibDepth = mw * v[lay.gMwVib] * 0.8f;
    for (int i = 0; i < n; ++i) { vibPh += 5.5f / sr; vibPh -= std::floor (vibPh); vibBuf[(size_t) i] = vibDepth * sine (vibPh); }
    const float glideMs = v[lay.gGlide];
    const float gCoef = glideMs < 0.5f ? 1.0f : 1.0f - std::exp (-4.6f / (glideMs * 0.001f * sr));
    for (auto& vc : voices)
    {
        if (! vc.active()) continue;
        for (int i = 0; i < n; ++i) { vc.curNote += (vc.targetNote - vc.curNote) * gCoef; noteBuf[(size_t) i] = vc.curNote; }
        for (int m = 0; m < numModules; ++m)
        {
            if (! vc.use[m] || ! vc.part[m].active()) continue;
            ModCtx c; c.v = v + lay.moduleBase[m]; c.g = v; c.sr = sr; c.bend = bend * v[lay.gBend]; c.mw = mw;
            c.circuit = modelCircuit[(size_t) std::clamp ((int) c.v[mModel], 0, (int) modelCircuit.size() - 1)];
            c.lfo = lfoBuf[m].data(); c.vib = vibBuf.data();
            vc.part[m].render (bus[m][0].data() + start, bus[m][1].data() + start, n, noteBuf.data(), c);
        }
        if (! vc.active()) vc.note = -1;
    }
}

void SynthEngine::render (float* L, float* R, int n, const juce::MidiBuffer& midi, const float* v, double bpm)
{
    for (int m = 0; m < numModules; ++m) for (int c = 0; c < 2; ++c) std::fill_n (bus[m][c].data(), n, 0.0f);
    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = std::clamp (meta.samplePosition, 0, n);
        if (at > pos) { renderVoices (pos, at - pos, v); pos = at; }
        handleMidi (meta.getMessage(), v);
    }
    if (pos < n) renderVoices (pos, n - pos, v);

    std::fill_n (L, n, 0.0f); std::fill_n (R, n, 0.0f);
    static const float divBeats[] = { 0.25f, 1.0f / 3.0f, 0.5f, 0.75f, 2.0f / 3.0f, 1.0f, 1.5f, 2.0f };
    for (int m = 0; m < numModules; ++m)
    {
        const float* mv = v + layout().moduleBase[m];
        float pk = modMeter[(size_t) m] * 0.92f;
        if (! moduleUsed (m, v)) { modMeter[(size_t) m] = pk; continue; }
        const float lv = mv[mLevel];
        const float pL = std::sqrt (0.5f * (1.0f - mv[mPan])) * 1.4142f, pR = std::sqrt (0.5f * (1.0f + mv[mPan])) * 1.4142f;
        const float bits = mv[mBits], srr = mv[mSrr];
        const float q = bits > 0.001f ? std::exp2 (15.0f - bits * 11.0f) : 0.0f;
        const float hold = 1.0f + srr * srr * 24.0f;
        const float dMix = mv[mDlyMix], dFb = mv[mDlyFb];
        const bool ping = mv[mDlyPing] > 0.5f;
        const int dsz = (int) dly[m][0].size();
        const float target = std::clamp ((float) (60.0 / juce::jmax (30.0, bpm)) * divBeats[std::clamp ((int) mv[mDlyDiv], 0, 7)], 0.01f, 2.5f) * sr;
        if (dlyTime[m] <= 0) dlyTime[m] = target;
        const float lpC = 1.0f - std::exp (-twoPi * 6000.0f / sr);
        for (int i = 0; i < n; ++i)
        {
            float l = dcBus[m][0].process (bus[m][0][(size_t) i]), r = dcBus[m][1].process (bus[m][1][(size_t) i]);
            if (srr > 0.001f)
            {
                srrCount[m] += 1.0f;
                if (srrCount[m] >= hold) { srrCount[m] -= hold; srrHold[m][0] = l; srrHold[m][1] = r; }
                l = srrHold[m][0]; r = srrHold[m][1];
            }
            if (q > 0) { l = std::round (l * q) / q; r = std::round (r * q) / q; }
            l *= lv * pL; r *= lv * pR;
            if (dMix > 0.0001f)
            {
                dlyTime[m] += (target - dlyTime[m]) * 0.0005f;
                auto rd = [&] (int ch) {
                    const float rp = (float) dlyW[m] - dlyTime[m];
                    const int i0 = (int) std::floor (rp); const float fr = rp - (float) i0;
                    const float a = dly[m][ch][(size_t) ((i0 % dsz + dsz) % dsz)], b = dly[m][ch][(size_t) (((i0 + 1) % dsz + dsz) % dsz)];
                    return a + fr * (b - a); };
                float a = rd (0), b = rd (1);
                dlyLp[m][0] += (a - dlyLp[m][0]) * lpC; dlyLp[m][1] += (b - dlyLp[m][1]) * lpC;
                a = dlyLp[m][0]; b = dlyLp[m][1];
                if (ping) { dly[m][0][(size_t) dlyW[m]] = 0.5f * (l + r) + b * dFb; dly[m][1][(size_t) dlyW[m]] = a * dFb; }
                else      { dly[m][0][(size_t) dlyW[m]] = l + a * dFb; dly[m][1][(size_t) dlyW[m]] = r + b * dFb; }
                dlyW[m] = (dlyW[m] + 1) % dsz;
                l += a * dMix; r += b * dMix;
            }
            pk = std::max (pk, std::max (std::abs (l), std::abs (r)));
            L[i] += l; R[i] += r;
        }
        modMeter[(size_t) m] = pk;
    }
}
