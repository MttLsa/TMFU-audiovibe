#pragma once
#include "Common.h"

// =====================================================================================================
//  FX modules of the 8-slot chain. Each one: prepare(sr), reset(), process(l, r) with a parameter
//  struct filled by the engine once per control block (32 samples).
// =====================================================================================================
namespace mv
{
    // ============================================================================================ WOBBLE
    // Two sine LFOs (Wow 0.1-4 Hz, Flutter 6-20 Hz) blended by one slider, one Amount, Flux for irregularity,
    // applied to a fractional delay line (pitch/speed modulation like a tape deck).
    struct WobbleParams { float amt = 0.3f, bal = 0.3f, wowHz = 0.7f, flutHz = 11, flux = 0.2f, spread = 0.2f, mix = 1; };

    class Wobble
    {
    public:
        void prepare (float s) { sr = s; dl.prepare ((int) (sr * 0.06f)); dr.prepare ((int) (sr * 0.06f)); }
        void reset() { dl.clear(); dr.clear(); }
        // returns current delay modulation (samples) for the given channel phase offset
        void process (float& l, float& r, const WobbleParams& p)
        {
            dl.write (l); dr.write (r);
            // flux: slow random variation of rate and depth
            if (--fluxCount <= 0) { fluxCount = (int) (sr * 0.05f); fluxTarget = rng.bi(); fluxTarget2 = rng.bi(); }
            fluxA += (fluxTarget - fluxA) * 0.0005f; fluxB += (fluxTarget2 - fluxB) * 0.002f;
            const float rw = p.wowHz * (1.0f + 0.35f * p.flux * fluxA), rf = p.flutHz * (1.0f + 0.25f * p.flux * fluxB);
            wph += rw / sr; wph -= std::floor (wph);
            fph += rf / sr; fph -= std::floor (fph);
            const float wowD = p.amt * (1.0f - p.bal) * 0.0075f * sr * (1.0f + 0.4f * p.flux * fluxA);
            const float flD  = p.amt * p.bal * 0.0005f * sr * (1.0f + 0.5f * p.flux * fluxB);
            const float base = 0.0085f * sr;
            const float off = p.spread * 0.25f;
            const float mL = std::sin (kTwoPi * wph) * wowD + std::sin (kTwoPi * fph) * flD;
            const float mR = std::sin (kTwoPi * (wph + off)) * wowD + std::sin (kTwoPi * (fph + off * 1.7f)) * flD;
            const float yl = dl.tap (base + mL), yr = dr.tap (base + mR);
            l += (yl - l) * p.mix; r += (yr - r) * p.mix;
        }
    private:
        float sr = 48000; Ring dl, dr; double wph = 0, fph = 0;
        Rng rng { 0x1155u }; int fluxCount = 0; float fluxTarget = 0, fluxTarget2 = 0, fluxA = 0, fluxB = 0;
    };

    // ============================================================================================ TAPE
    // Jiles-Atherton magnetic hysteresis (RK2 solver, 2x oversampled) or an "Eco" rational saturator,
    // tape-speed dependent head bump and HF loss, hiss, compression, dropouts and its own wow/flutter.
    struct TapeParams { float drive = 0.4f, sat = 0.5f, bias = 0.6f, hiss = 0.15f, comp = 0.3f, drop = 0, flut = 0.15f, age = 0.2f, mix = 1; int speed = 1, mode = 0; };

    class JAHysteresis
    {
    public:
        void prepare (float fs) { T = 1.0 / fs; M = H1 = Hd1 = 0; }
        void set (float drive, float sat, float bias)
        {
            Ms = 1.0;
            a = Ms / (0.01 + 6.0 * drive);
            alpha = 1.6e-3;
            k = 0.47875 + 0.5 * (1.0 - bias);    // low bias: wider loop (dead zone)
            c = std::sqrt (1.0 - (0.2 + 0.79 * (1.0 - sat))) - 0.01;
            c = clampv (c, 0.01, 0.99);
        }
        double process (double H)
        {
            const double Hd = (H - H1) / T;
            const double k1 = T * dMdt (M, H1, Hd1);
            const double k2 = T * dMdt (M + 0.5 * k1, 0.5 * (H + H1), 0.5 * (Hd + Hd1));
            double Mn = M + k2;
            if (! std::isfinite (Mn) || std::abs (Mn) > 4.0) Mn = clampv (std::isfinite (Mn) ? Mn : 0.0, -4.0, 4.0);
            M = Mn; H1 = H; Hd1 = Hd;
            return M;
        }
    private:
        static double langevin (double x) { return std::abs (x) > 1.0e-4 ? 1.0 / std::tanh (x) - 1.0 / x : x / 3.0; }
        static double langevinD (double x)
        {
            if (std::abs (x) <= 1.0e-4) return 1.0 / 3.0;
            const double ct = 1.0 / std::tanh (x);
            return 1.0 / (x * x) - ct * ct + 1.0;
        }
        double dMdt (double m, double h, double hd) const
        {
            const double Q = (h + alpha * m) / a;
            const double Man = Ms * langevin (Q);
            const double Mdiff = Man - m;
            const double delta = hd >= 0 ? 1.0 : -1.0;
            const double deltaM = (delta > 0) == (Mdiff > 0) ? 1.0 : 0.0;
            const double Ld = langevinD (Q);
            double den = (1.0 - c) * delta * k - alpha * Mdiff;
            if (std::abs (den) < 1.0e-6) den = den < 0 ? -1.0e-6 : 1.0e-6;
            const double f1 = (1.0 - c) * deltaM * Mdiff / den;
            const double f2 = c * Ms / a * Ld;
            const double f3 = 1.0 - c * alpha * Ms / a * Ld;
            return hd * (f1 + f2) / (std::abs (f3) < 1.0e-6 ? 1.0e-6 : f3);
        }
        double T = 1.0 / 96000, M = 0, H1 = 0, Hd1 = 0, Ms = 1, a = 0.5, alpha = 1.6e-3, k = 0.48, c = 0.1;
    };

    class Tape
    {
    public:
        void prepare (float s)
        {
            sr = s;
            for (auto& h : hy) h.prepare (sr * 2.0f);
            wob.prepare (sr);
            reset();
            lastSpeed = -1;
        }
        void reset() { for (auto& f : hf) f.reset(); for (auto& b : bump) b.reset(); prevL = prevR = 0; compEnv = 0; dropGain = 1; }
        void process (float& l, float& r, const TapeParams& p)
        {
            if (p.speed != lastSpeed || std::abs (p.age - lastAge) > 0.01f)
            {
                lastSpeed = p.speed; lastAge = p.age;
                static const float bumpHz[] = { 45, 70, 110 }, lpHz[] = { 9000, 15000, 20000 };
                for (auto& b : bump) b.peak (bumpHz[p.speed], 1.2f, 3.0f, sr);
                for (auto& f : hf) f.set (lpHz[p.speed] * (1.0f - 0.6f * p.age), 0.6f, sr);
            }
            if (std::abs (p.drive - lastDrive) > 0.002f || std::abs (p.sat - lastSat) > 0.002f || std::abs (p.bias - lastBias) > 0.002f)
            {
                lastDrive = p.drive; lastSat = p.sat; lastBias = p.bias;
                for (auto& h : hy) h.set (p.drive, p.sat, p.bias);
            }
            const float inL = l, inR = r;
            // compression (slow, program dependent)
            const float pk = std::max (std::abs (l), std::abs (r));
            compEnv += (pk > compEnv ? 0.01f : 0.0003f) * (pk - compEnv);
            const float cg = 1.0f / (1.0f + p.comp * 3.0f * compEnv);
            float xl = l * cg, xr = r * cg;
            const float gIn = 0.5f + 3.0f * p.drive;
            if (p.mode == 0)
            {
                // 2x oversampled hysteresis (linear-interpolated input, averaged output)
                const double ml = 0.5 * (prevL + xl), mr = 0.5 * (prevR + xr);
                const double yl = 0.5 * (hy[0].process (clampv (ml * gIn, -6.0, 6.0)) + hy[0].process (clampv ((double) xl * gIn, -6.0, 6.0)));
                const double yr = 0.5 * (hy[1].process (clampv (mr * gIn, -6.0, 6.0)) + hy[1].process (clampv ((double) xr * gIn, -6.0, 6.0)));
                prevL = xl; prevR = xr;
                const float makeup = 1.0f / (0.35f + 0.6f * p.drive);
                xl = (float) yl * makeup; xr = (float) yr * makeup;
            }
            else
            {
                const float k = 1.0f + p.sat * 4.0f;
                xl = xl * gIn; xr = xr * gIn;
                xl = xl * k / (1.0f + std::abs (xl * k)) / std::max (0.3f, gIn * 0.8f);
                xr = xr * k / (1.0f + std::abs (xr * k)) / std::max (0.3f, gIn * 0.8f);
            }
            xl = bump[0].process (xl); xr = bump[1].process (xr);
            xl = hf[0].process (xl); xr = hf[1].process (xr);
            // dropouts
            if (p.drop > 0.001f)
            {
                if (rng.chance (p.drop * 3.0f / sr)) dropTarget = 0.15f + 0.5f * rng.uni();
                dropTarget += (1.0f - dropTarget) * 0.00015f;
                dropGain += (dropTarget - dropGain) * 0.003f;
                xl *= dropGain; xr *= dropGain;
            }
            // hiss
            if (p.hiss > 0.001f)
            {
                const float h = p.hiss * p.hiss * 0.03f;
                xl += hissHp[0].hp (pink[0].process (rng.bi())) * h;
                xr += hissHp[1].hp (pink[1].process (rng.bi())) * h;
            }
            if (p.flut > 0.001f) { WobbleParams wp; wp.amt = p.flut * 0.35f; wp.bal = 0.45f; wp.wowHz = 0.55f; wp.flutHz = 9.5f; wp.flux = 0.6f; wp.spread = 0.05f; wob.process (xl, xr, wp); }
            l = inL + (xl - inL) * p.mix; r = inR + (xr - inR) * p.mix;
        }
    private:
        float sr = 48000;
        JAHysteresis hy[2];
        Biquad bump[2]; Svf hf[2]; OnePole hissHp[2];
        Pink pink[2];
        Wobble wob;
        Rng rng { 0x7A9Eu };
        double prevL = 0, prevR = 0;
        float compEnv = 0, dropGain = 1, dropTarget = 1;
        int lastSpeed = -1; float lastAge = -1, lastDrive = -1, lastSat = -1, lastBias = -1;
    public:
        Tape() { hissHp[0].a = hissHp[1].a = 0.2f; }
    };

    // ============================================================================================ VINYL
    struct VinylParams { float dust = 0.3f, crackle = 0.3f, scratch = 0.05f, rumble = 0.2f, hum = 0, year = 1975, wear = 0.25f, warp = 0.1f,
                         brake = 0, mono = 0.5f, noise = 1, mix = 1; int humHz = 0, rpm = 0; };

    class Vinyl
    {
    public:
        void prepare (float s)
        {
            sr = s;
            bl.prepare ((int) (sr * 8)); br.prepare ((int) (sr * 8));
            lag = 0; lastYear = -1; lastWear = -1;
            monoLp[0].setHz (150, sr); monoLp[1].setHz (150, sr);
            dustHp.setHz (900, sr); rumbleLp.setHz (28, sr); scratchLp.setHz (2500, sr);
        }
        void reset() { bl.clear(); br.clear(); lag = 0; }
        void process (float& l, float& r, const VinylParams& p)
        {
            if (std::abs (p.year - lastYear) > 0.5f || std::abs (p.wear - lastWear) > 0.01f)
            {
                lastYear = p.year; lastWear = p.wear;
                const float y = clampv ((p.year - 1930.0f) / 70.0f, 0.0f, 1.0f);
                const float hp = 300.0f * std::pow (20.0f / 300.0f, y);
                const float lp = 3500.0f * std::pow (19000.0f / 3500.0f, y) * (1.0f - 0.6f * p.wear);
                for (int c = 0; c < 2; ++c) { hpF[c].set (hp, 0.6f, sr); lpF[c].set (lp, 0.75f, sr); lpF2[c].set (lp * 1.1f, 0.6f, sr); }
                yearMono = p.year < 1958 ? 1.0f : 0.0f;
            }
            bl.write (l); br.write (r);
            // brake (spin down) and catch-up (spin up)
            static const float rpms[] = { 33.333f, 45.0f, 78.0f };
            const float rotHz = rpms[clampv (p.rpm, 0, 2)] / 60.0f;
            float rate;
            if (p.brake > 0.001f) rate = 1.0f - p.brake;
            else rate = lag > 1.0 ? 1.0f + std::min (0.6f, (float) lag / (sr * 0.4f)) : 1.0f;
            lag += 1.0 - rate;
            lag = clampv (lag, 0.0, (double) bl.size() - sr * 0.1);
            // warp: wow at the rotation rate (+ 2nd harmonic)
            rph += rotHz * rate / sr; rph -= std::floor (rph);
            const float warpD = p.warp * 0.0025f * sr * (std::sin (kTwoPi * rph) + 0.3f * std::sin (2 * kTwoPi * rph + 1.3f));
            const double pos = (double) bl.w - 1 - (0.004 * sr + 4 + warpD) - lag;
            float xl = bl.read (pos), xr = br.read (pos);
            // year / wear filters, groove distortion
            xl = lpF2[0].process (lpF[0].process (xl)); xr = lpF2[1].process (lpF[1].process (xr));
            hpF[0].process (xl); hpF[1].process (xr); xl = hpF[0].hp; xr = hpF[1].hp;
            if (p.wear > 0.01f) { const float d = 1.0f + 3.0f * p.wear; xl = fastTanh (xl * d + 0.05f * p.wear) / d * 1.1f; xr = fastTanh (xr * d + 0.05f * p.wear) / d * 1.1f; }
            // mono: old records, and bass mono
            const float m = std::max (yearMono, 0.0f);
            if (m > 0) { const float c = 0.5f * (xl + xr); xl = c; xr = c; }
            if (p.mono > 0.01f)
            {
                const float lo = monoLp[0].lp (xl), ro = monoLp[1].lp (xr), c = 0.5f * (lo + ro);
                xl += (c - lo) * p.mono; xr += (c - ro) * p.mono;
            }
            // surface noise
            const float speedGain = clampv (rate, 0.0f, 1.0f);
            float n = 0;
            if (p.dust > 0.001f && rng.chance (p.dust * 25.0f * speedGain / sr)) dustAmp = rng.bi() * std::pow (rng.uni(), 2.0f) * 0.6f;
            n += dustAmp; dustAmp *= 0.86f;
            if (p.crackle > 0.001f && rng.chance (p.crackle * 900.0f * speedGain / sr)) crackAmp = rng.bi() * std::pow (rng.uni(), 3.0f) * 0.15f;
            n += crackAmp; crackAmp *= 0.6f;
            n = dustHp.hp (n);
            if (p.scratch > 0.001f && rph < lastRph && rng.chance (p.scratch)) scratchAmp = (rng.uni() * 0.6f + 0.4f) * 0.5f;
            lastRph = (float) rph;
            n += scratchLp.lp (rng.bi() * scratchAmp); scratchAmp *= 0.9985f;
            float rum = 0;
            if (p.rumble > 0.001f) rum = rumbleLp.lp (brown.process (rng.bi())) * p.rumble * 0.6f * (1 + 0.3f * std::sin (kTwoPi * rph));
            float hum = 0;
            if (p.hum > 0.001f)
            {
                hph += (p.humHz == 0 ? 50.0f : 60.0f) / sr; hph -= std::floor (hph);
                hum = (std::sin (kTwoPi * hph) + 0.4f * std::sin (3 * kTwoPi * hph) + 0.2f * std::sin (5 * kTwoPi * hph)) * p.hum * 0.015f;
            }
            const float ng = p.noise * speedGain;
            xl += (n * 0.9f + rum + hum) * ng; xr += (n * 0.8f + rum + hum) * ng;
            l += (xl - l) * p.mix; r += (xr - r) * p.mix;
        }
    private:
        float sr = 48000;
        Ring bl, br; double lag = 0, rph = 0, hph = 0;
        Svf hpF[2], lpF[2], lpF2[2]; OnePole monoLp[2]; OnePole dustHp, rumbleLp, scratchLp;
        Brown brown; Rng rng { 0x0A11u };
        float dustAmp = 0, crackAmp = 0, scratchAmp = 0, lastRph = 0, lastYear = -1, lastWear = -1, yearMono = 0;
    };

    // ============================================================================================ DELAY
    struct DelayParams { float timeS = 0.375f, fb = 0.45f, lp = 6000, hp = 150, sat = 0.2f, wob = 0.1f, pitch = 0, duck = 0, mix = 0.3f; int mode = 2, taps = 0; bool freeze = false; };

    class Delay
    {
    public:
        void prepare (float s)
        {
            sr = s;
            dl.prepare ((int) (sr * 4.2f)); dr.prepare ((int) (sr * 4.2f));
            psL.prepare (sr); psR.prepare (sr);
            duckF.set (5, 300, sr);
            timeSm = 0.3f * sr;
        }
        void reset() { dl.clear(); dr.clear(); psL.clear(); psR.clear(); fbL = fbR = 0; }
        void process (float& l, float& r, const DelayParams& p)
        {
            if (std::abs (p.lp - lastLp) > 1 || std::abs (p.hp - lastHp) > 1)
            { lastLp = p.lp; lastHp = p.hp; for (int c = 0; c < 2; ++c) { lpF[c].set (p.lp, 0.6f, sr); hpF[c].set (p.hp, 0.6f, sr); } }
            const float target = clampv (p.timeS * sr, 4.0f, sr * 4.0f);
            timeSm += (target - timeSm) * 0.0007f;
            wph += (0.6f + 4.0f * p.wob) / sr; wph -= std::floor (wph);
            const float wmod = (p.mode == 1 ? 0.002f : 0.0007f) * sr * p.wob * std::sin (kTwoPi * wph);
            const float t = timeSm + wmod;
            float inL = p.freeze ? 0.0f : l, inR = p.freeze ? 0.0f : r;
            float yl, yr;
            if (p.mode == 3)
            {
                static const int N[] = { 3, 4, 5, 7 };
                const int n = N[clampv (p.taps, 0, 3)];
                yl = yr = 0;
                for (int k = 1; k <= n; ++k)
                {
                    const float d = t * (float) k / n, g = 1.0f - 0.55f * (float) (k - 1) / n;
                    const float pan = (k & 1) ? 0.25f : 0.75f;
                    const float a = dl.tap (d), b = dr.tap (d);
                    yl += (a * (1 - pan) * 2) * g / n * 1.6f; yr += (b * pan * 2) * g / n * 1.6f;
                }
            }
            else { yl = dl.tap (t); yr = dr.tap (p.mode == 2 ? t : t * 1.0f); }
            // feedback path: filters, saturation, pitch
            float fl = yl, fr = yr;
            if (p.mode == 3) { fl = dl.tap (t); fr = dr.tap (t); }
            if (! p.freeze)
            {
                lpF[0].process (fl); lpF[1].process (fr); fl = lpF[0].lp; fr = lpF[1].lp;
                hpF[0].process (fl); hpF[1].process (fr); fl = hpF[0].hp; fr = hpF[1].hp;
            }
            const float satAmt = p.mode == 1 ? std::max (0.3f, p.sat) : p.sat;
            if (satAmt > 0.01f) { const float d = 1 + 3 * satAmt; fl = fastTanh (fl * d) / d * (1 + satAmt * 0.5f); fr = fastTanh (fr * d) / d * (1 + satAmt * 0.5f); }
            if (std::abs (p.pitch) > 0.01f) { const float ratio = semiToRatio (p.pitch); fl = psL.process (fl, ratio); fr = psR.process (fr, ratio); }
            const float fb = p.freeze ? 1.0f : p.fb;
            fl = softClip (fl * fb); fr = softClip (fr * fb);
            if (p.mode == 2) { dl.write (0.5f * (inL + inR) + fr); dr.write (fl); }
            else { dl.write (inL + fl); dr.write (inR + fr); }
            float wet = 1.0f;
            if (p.duck > 0.001f) { const float e = duckF.process (0.5f * (l + r)); wet = 1.0f / (1.0f + p.duck * 12.0f * e); }
            l += yl * p.mix * wet; r += yr * p.mix * wet;
        }
    private:
        float sr = 48000; Ring dl, dr; PitchShifter psL, psR; Svf lpF[2], hpF[2]; Follower duckF;
        float fbL = 0, fbR = 0, timeSm = 1000, wph = 0, lastLp = -1, lastHp = -1;
    };

    // ============================================================================================ REVERB
    // 8-line feedback delay network (Hadamard), input diffusion, modulation, damping, shimmer,
    // freeze, gated and reverse modes.
    struct ReverbParams { float size = 0.6f, decay = 2.5f, preMs = 20, damp = 0.5f, mod = 0.3f, diff = 0.7f, shim = 0, shimSt = 12, duck = 0, low = 120, mix = 0.25f, beatS = 0.5f; int mode = 0; bool freeze = false; };

    class Reverb
    {
    public:
        void prepare (float s)
        {
            sr = s;
            for (auto& d : lines) d.prepare ((int) (sr * 0.32f));
            for (auto& a : ap) a.prepare ((int) (sr * 0.03f));
            pre.prepare ((int) (sr * 0.55f)); preR.prepare ((int) (sr * 0.55f));
            ps.prepare (sr);
            revL.prepare ((int) (sr * 4.2f)); revR.prepare ((int) (sr * 4.2f));
            gateF.set (1, 80, sr); duckF.set (5, 300, sr);
            lowL.setHz (120, sr); lowR.setHz (120, sr);
            for (int k = 0; k < 8; ++k) mph[k] = 0.13f * k;
        }
        void reset() { for (auto& d : lines) d.clear(); for (auto& a : ap) a.clear(); pre.clear(); preR.clear(); ps.clear(); for (auto& z : damp) z.reset(); }
        void process (float& l, float& r, const ReverbParams& p)
        {
            static const float baseMs[8] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.7f, 67.1f, 73.9f };
            static const float apMs[4] = { 4.7f, 3.6f, 12.7f, 9.3f };
            if (std::abs (p.damp - lastDamp) > 0.005f) { lastDamp = p.damp; for (auto& z : damp) z.setHz (18000.0f * std::pow (0.04f, p.damp), sr); }
            if (std::abs (p.low - lastLow) > 1) { lastLow = p.low; lowL.setHz (p.low, sr); lowR.setHz (p.low, sr); }
            const float scale = 0.3f + 1.7f * p.size;
            pre.write (l); preR.write (r);
            const float pd = std::max (2.0f, p.preMs * 0.001f * sr);
            float in = p.freeze ? 0.0f : 0.5f * (pre.tap (pd) + preR.tap (pd));
            // input diffusion
            const float g = 0.45f + 0.3f * p.diff;
            for (int k = 0; k < 4; ++k)
            {
                const float d = ap[k].tap (apMs[k] * 0.001f * sr * (0.6f + 0.4f * scale));
                const float v = in + g * d;
                ap[k].write (v);
                in = d - g * v;
            }
            // read lines
            float x[8];
            for (int k = 0; k < 8; ++k)
            {
                mph[k] += (0.3f + 0.07f * k) / sr; mph[k] -= std::floor (mph[k]);
                const float len = baseMs[k] * 0.001f * sr * scale + p.mod * 0.0012f * sr * std::sin (kTwoPi * mph[k]);
                x[k] = lines[k].tap (len);
                lens[k] = baseMs[k] * 0.001f * scale;
            }
            // outputs
            float oL = (x[0] + x[2] + x[4] + x[6]) * 0.35f, oR = (x[1] + x[3] + x[5] + x[7]) * 0.35f;
            // Hadamard 8 (fast WHT)
            for (int h = 1; h < 8; h <<= 1)
                for (int i = 0; i < 8; i += h << 1)
                    for (int j = i; j < i + h; ++j) { const float a = x[j], b = x[j + h]; x[j] = a + b; x[j + h] = a - b; }
            const float norm = 0.35355339f;
            float sh = 0;
            if (p.shim > 0.001f) sh = ps.process (0.5f * (oL + oR), semiToRatio (p.shimSt)) * p.shim * 0.6f;
            for (int k = 0; k < 8; ++k)
            {
                const float gk = p.freeze ? 1.0f : std::pow (10.0f, -3.0f * lens[k] / std::max (0.05f, p.decay));
                float v = x[k] * norm * gk;
                if (! p.freeze) v = damp[k].lp (v);
                lines[k].write (flushDenorm (v + (in + sh) * ((k & 1) ? 0.5f : 0.5f)));
            }
            oL = lowL.hp (oL); oR = lowR.hp (oR);
            // modes
            if (p.mode == 1)
            {
                const float e = gateF.process (0.5f * (l + r));
                gateOpen = e > 0.02f ? std::min (1.0f, gateOpen + 0.01f) : (holdLeft > 0 ? gateOpen : gateOpen * 0.995f);
                if (e > 0.02f) holdLeft = (int) (p.decay * 0.15f * sr); else --holdLeft;
                oL *= gateOpen; oR *= gateOpen;
            }
            else if (p.mode == 2)
            {
                // reverse: record the wet signal in chunks, play the previous chunk backwards with a swell
                const int chunk = (int) clampv (p.beatS * 2.0f * sr, 0.1f * sr, 2.0f * sr);
                revL.write (oL); revR.write (oR);
                if (++chunkPos >= chunk) { chunkPos = 0; chunkStart = revL.w; chunkLen = chunk; }
                const float ph = (float) chunkPos / (float) std::max (1, chunkLen);
                const double pos = (double) chunkStart - 1 - chunkPos;
                const float env = ph * ph * (ph < 0.97f ? 1.0f : (1.0f - ph) / 0.03f);
                oL = revL.read (pos) * env * 1.6f; oR = revR.read (pos) * env * 1.6f;
            }
            float wet = 1.0f;
            if (p.duck > 0.001f) { const float e = duckF.process (0.5f * (l + r)); wet = 1.0f / (1.0f + p.duck * 12.0f * e); }
            l += oL * p.mix * wet; r += oR * p.mix * wet;
        }
    private:
        float sr = 48000;
        Ring lines[8], ap[4], pre, preR, revL, revR;
        OnePole damp[8], lowL, lowR;
        PitchShifter ps;
        Follower gateF, duckF;
        float mph[8] {}, lens[8] {}, lastDamp = -1, lastLow = -1, gateOpen = 0;
        int holdLeft = 0, chunkPos = 0, chunkLen = 1; int64_t chunkStart = 0;
    };

    // ============================================================================================ FILTER
    struct FilterParams { float cut = 2000, res = 0.2f, drive = 0, vowel = 0, mix = 1; int type = 0, slope = 0; };

    class Filter
    {
    public:
        void prepare (float s) { sr = s; cl.prepare ((int) (sr * 0.06f)); cr.prepare ((int) (sr * 0.06f)); }
        void reset() { for (auto& f : f1) f.reset(); for (auto& f : f2) f.reset(); for (auto& f : fm) f.reset(); cl.clear(); cr.clear(); }
        void process (float& l, float& r, const FilterParams& p)
        {
            const float inL = l, inR = r;
            float x[2] = { l, r };
            if (p.drive > 0.01f) { const float d = 1 + p.drive * 6; for (auto& v : x) v = fastTanh (v * d) / std::sqrt (d); }
            if (p.type <= 3)
            {
                if (std::abs (p.cut - lastCut) > 0.5f || std::abs (p.res - lastRes) > 0.002f || p.slope != lastSlope)
                {
                    lastCut = p.cut; lastRes = p.res; lastSlope = p.slope;
                    const float q = 0.5f + p.res * p.res * (p.slope ? 6.0f : 14.0f);
                    for (int c = 0; c < 2; ++c) { f1[c].set (p.cut, q, sr); f2[c].set (p.cut, p.slope ? 0.7f : q, sr); }
                }
                for (int c = 0; c < 2; ++c)
                {
                    auto pick = [&] (Svf& f, float in) { f.process (in); return p.type == 0 ? f.lp : p.type == 1 ? f.hp : p.type == 2 ? f.bpNorm() : f.notch(); };
                    float y = pick (f1[c], x[c]);
                    if (p.slope) y = pick (f2[c], y);
                    x[c] = y;
                }
            }
            else if (p.type == 4)
            {
                const float d = clampv (sr / std::max (20.0f, p.cut), 2.0f, sr * 0.05f);
                const float fb = 0.5f + 0.49f * p.res;
                const float yl = cl.tap (d), yr = cr.tap (d);
                cl.write (x[0] + yl * fb); cr.write (x[1] + yr * fb);
                x[0] = (x[0] + yl) * (1.0f - 0.5f * fb); x[1] = (x[1] + yr) * (1.0f - 0.5f * fb);
            }
            else
            {
                static const float F[5][3] = { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 270, 2140, 2950 }, { 450, 800, 2830 }, { 325, 700, 2530 } };
                const float vpos = clampv (p.vowel, 0.0f, 1.0f) * 4.0f; const int v0 = std::min (3, (int) vpos); const float t = vpos - v0;
                const float shift = std::pow (p.cut / 1000.0f, 0.3f);
                if (std::abs (vpos - lastV) > 0.002f || std::abs (shift - lastShift) > 0.002f || std::abs (p.res - lastRes) > 0.002f)
                {
                    lastV = vpos; lastShift = shift; lastRes = p.res;
                    for (int k = 0; k < 3; ++k)
                    {
                        const float hz = lerp (F[v0][k], F[v0 + 1][k], t) * shift;
                        fm[k].set (hz, 4 + p.res * 12, sr); fm[k + 3].set (hz, 4 + p.res * 12, sr);
                    }
                }
                float yl = 0, yr = 0;
                for (int k = 0; k < 3; ++k) { fm[k].process (x[0]); fm[k + 3].process (x[1]); const float g = k == 0 ? 1.0f : 0.6f; yl += fm[k].bpNorm() * g; yr += fm[k + 3].bpNorm() * g; }
                x[0] = yl * 1.2f; x[1] = yr * 1.2f;
            }
            l = inL + (x[0] - inL) * p.mix; r = inR + (x[1] - inR) * p.mix;
        }
    private:
        float sr = 48000; Svf f1[2], f2[2], fm[6]; Ring cl, cr;
        float lastCut = -1, lastRes = -1, lastV = -1, lastShift = -1; int lastSlope = -1;
    };

    // ============================================================================================ RESONATOR
    struct ResoParams { float noteSemis[8] {}; int nNotes = 3; float decay = 0.5f, bright = 0.5f, spread = 0.5f, mix = 0.5f; };

    class Resonator
    {
    public:
        void prepare (float s) { sr = s; }
        void reset() { for (auto& b : bl) b.reset(); for (auto& b : br) b.reset(); }
        void process (float& l, float& r, const ResoParams& p)
        {
            if (--cc <= 0)
            {
                cc = 64;
                n = std::min (8, std::max (1, p.nNotes) * 2);
                const float q = 15.0f + p.decay * p.decay * 400.0f;
                for (int k = 0; k < n; ++k)
                {
                    const float semis = p.noteSemis[k % std::max (1, p.nNotes)] + 12.0f * (float) (k / std::max (1, p.nNotes));
                    const float hz = 261.6256f * std::exp2 (semis / 12.0f);
                    bl[k].set (hz, q, sr); br[k].set (hz * 1.001f, q, sr);
                    gain[k] = std::pow (0.3f + 0.7f * p.bright, (float) k * 0.5f);
                    pan[k] = 0.5f + ((k & 1) ? 0.5f : -0.5f) * p.spread;
                }
                norm = 1.0f / std::sqrt ((float) n);
            }
            float yl = 0, yr = 0;
            const float x = 0.5f * (l + r);
            for (int k = 0; k < n; ++k)
            {
                bl[k].process (x); br[k].process (x);
                yl += bl[k].bpNorm() * gain[k] * (1 - pan[k]) * 2;
                yr += br[k].bpNorm() * gain[k] * pan[k] * 2;
            }
            yl = softClip (yl * norm * 1.5f); yr = softClip (yr * norm * 1.5f);
            l += (yl - l) * p.mix; r += (yr - r) * p.mix;
        }
    private:
        float sr = 48000; Svf bl[8], br[8]; float gain[8] {}, pan[8] {}, norm = 1; int n = 1, cc = 0;
    };

    // ============================================================================================ RING / FREQ SHIFT
    struct RingParams { float freq = 220, shift = 5, mix = 0.5f; int mode = 0; };
    class RingShift
    {
    public:
        void prepare (float s) { sr = s; }
        void process (float& l, float& r, const RingParams& p)
        {
            float yl, yr;
            if (p.mode == 0)
            {
                ph += p.freq / sr; ph -= std::floor (ph);
                const float m = std::sin (kTwoPi * ph);
                yl = l * m; yr = r * m;
            }
            else
            {
                ph += p.shift / sr; ph -= std::floor (ph);
                const float c = std::cos (kTwoPi * ph), s = std::sin (kTwoPi * ph);
                float re, im;
                hl.process (l, re, im); yl = re * c - im * s;
                hr.process (r, re, im); yr = re * c - im * s;
            }
            l += (yl - l) * p.mix; r += (yr - r) * p.mix;
        }
    private:
        float sr = 48000; double ph = 0; Hilbert hl, hr;
    };

    // ============================================================================================ CRUSHER
    struct CrushParams { float bits = 10, rate = 0.2f, jitter = 0, mix = 1; };
    class Crusher
    {
    public:
        void process (float& l, float& r, const CrushParams& p)
        {
            if (--count <= 0)
            {
                const float n = 1.0f + p.rate * p.rate * 48.0f;
                count = std::max (1, (int) (n * (1.0f + p.jitter * rng.bi() * 0.8f)));
                const float q = std::exp2 (clampv (p.bits, 1.0f, 16.0f)) * 0.5f;
                hl = std::round (l * q) / q; hr = std::round (r * q) / q;
            }
            l += (hl - l) * p.mix; r += (hr - r) * p.mix;
        }
    private:
        int count = 0; float hl = 0, hr = 0; Rng rng { 0xC2u };
    };

    // ============================================================================================ CHORUS
    struct ChorusParams { float rate = 0.6f, depth = 0.4f, mix = 0.5f; int voices = 2; };
    class Chorus
    {
    public:
        void prepare (float s) { sr = s; dl.prepare ((int) (sr * 0.05f)); dr.prepare ((int) (sr * 0.05f)); }
        void reset() { dl.clear(); dr.clear(); }
        void process (float& l, float& r, const ChorusParams& p)
        {
            dl.write (l); dr.write (r);
            ph += p.rate / sr; ph -= std::floor (ph);
            float yl = 0, yr = 0;
            const int n = clampv (p.voices, 1, 4);
            for (int v = 0; v < n; ++v)
            {
                const float o = (float) v / n;
                const float dL = (0.008f + 0.006f * p.depth * (1 + std::sin (kTwoPi * (ph + o)))) * sr;
                const float dR = (0.008f + 0.006f * p.depth * (1 + std::sin (kTwoPi * (ph + o + 0.25f)))) * sr;
                yl += dl.tap (dL); yr += dr.tap (dR);
            }
            yl /= (float) n; yr /= (float) n;
            l += (yl - l) * p.mix * 0.5f; r += (yr - r) * p.mix * 0.5f;
        }
    private:
        float sr = 48000; Ring dl, dr; double ph = 0;
    };

    // ============================================================================================ PHASER
    struct PhaserParams { float rate = 0.3f, depth = 0.6f, fb = 0.4f, centre = 800, mix = 0.5f; int stages = 1; };
    class Phaser
    {
    public:
        void prepare (float s) { sr = s; }
        void process (float& l, float& r, const PhaserParams& p)
        {
            static const int S[] = { 4, 6, 8, 12 };
            const int n = S[clampv (p.stages, 0, 3)];
            ph += p.rate / sr; ph -= std::floor (ph);
            float y[2] = { l + fb[0] * p.fb, r + fb[1] * p.fb };
            for (int c = 0; c < 2; ++c)
            {
                const float lfo = std::sin (kTwoPi * (ph + 0.25f * c));
                const float hz = clampv (p.centre * std::exp2 (lfo * 2.5f * p.depth), 30.0f, sr * 0.45f);
                const float t = std::tan (kPi * hz / sr), a = (t - 1) / (t + 1);
                float x = y[c];
                for (int k = 0; k < n; ++k) { const float o = a * x + z[c][k]; z[c][k] = x - a * o; x = o; }
                fb[c] = x; y[c] = x;
            }
            const float yl = 0.5f * (l + y[0]), yr = 0.5f * (r + y[1]);
            l += (yl - l) * p.mix; r += (yr - r) * p.mix;
        }
    private:
        float sr = 48000; double ph = 0; float z[2][12] {}, fb[2] {};
    };

    // ============================================================================================ COMP + TRANSIENT
    struct CompParams { float thr = -18, ratio = 4, attMs = 5, relMs = 120, gain = 0, tatk = 0, tsus = 0, mix = 1; };
    class Comp
    {
    public:
        void prepare (float s) { sr = s; fast.set (0.5f, 30, sr); slow.set (20, 250, sr); }
        void process (float& l, float& r, const CompParams& p)
        {
            const float inL = l, inR = r;
            if (p.attMs != lastA || p.relMs != lastR) { lastA = p.attMs; lastR = p.relMs; det.set (p.attMs, p.relMs, sr); }
            const float e = det.process (std::max (std::abs (l), std::abs (r)));
            const float db = gainToDb (e);
            float gr = 0;
            const float knee = 6.0f, over = db - p.thr;
            if (over > knee * 0.5f) gr = over * (1.0f - 1.0f / p.ratio);
            else if (over > -knee * 0.5f) { const float x = over + knee * 0.5f; gr = (1.0f - 1.0f / p.ratio) * x * x / (2 * knee); }
            float g = dbToGain (-gr + p.gain);
            if (std::abs (p.tatk) > 0.01f || std::abs (p.tsus) > 0.01f)
            {
                const float x = std::max (std::abs (l), std::abs (r));
                const float ef = fast.process (x), es = slow.process (x);
                const float diff = gainToDb (ef + 1e-6f) - gainToDb (es + 1e-6f);
                g *= dbToGain (clampv (diff, 0.0f, 24.0f) * p.tatk * 0.75f + clampv (-diff, 0.0f, 24.0f) * p.tsus * 0.75f);
            }
            l *= g; r *= g;
            l = inL + (l - inL) * p.mix; r = inR + (r - inR) * p.mix;
        }
    private:
        float sr = 48000; Follower det, fast, slow; float lastA = -1, lastR = -1;
    };

    // ============================================================================================ STEREO
    struct StereoParams { float width = 1.2f, haasMs = 0, panRate = 0.5f, panDepth = 0, monoHz = 20; };
    class Stereo
    {
    public:
        void prepare (float s) { sr = s; hd.prepare ((int) (sr * 0.04f)); }
        void reset() { hd.clear(); }
        void process (float& l, float& r, const StereoParams& p)
        {
            if (std::abs (p.monoHz - lastMono) > 0.5f) { lastMono = p.monoHz; ml.setHz (p.monoHz, sr); mr.setHz (p.monoHz, sr); }
            if (p.haasMs > 0.05f) { hd.write (r); r = hd.tap (p.haasMs * 0.001f * sr); }
            const float m = 0.5f * (l + r), s = 0.5f * (l - r) * p.width;
            l = m + s; r = m - s;
            if (p.monoHz > 21)
            {
                const float lo = ml.lp (l), ro = mr.lp (r), c = 0.5f * (lo + ro);
                l += c - lo; r += c - ro;
            }
            if (p.panDepth > 0.001f)
            {
                ph += p.panRate / sr; ph -= std::floor (ph);
                const float pan = std::sin (kTwoPi * ph) * p.panDepth;
                l *= std::sqrt (std::min (1.0f, 1.0f - pan)); r *= std::sqrt (std::min (1.0f, 1.0f + pan));
            }
        }
    private:
        float sr = 48000; Ring hd; OnePole ml, mr; double ph = 0; float lastMono = -1;
    };
}
