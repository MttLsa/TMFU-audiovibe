#include "ModuleEngine.h"
#if defined (__clang__)
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Weverything"
#elif defined (__GNUC__)
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wsign-conversion"
 #pragma GCC diagnostic ignored "-Wconversion"
 #pragma GCC diagnostic ignored "-Wfloat-equal"
 #pragma GCC diagnostic ignored "-Wshadow"
 #pragma GCC diagnostic ignored "-Wunused-parameter"
 #pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
#include "../../third_party/signalsmith-stretch/signalsmith-stretch.h"
#if defined (__clang__)
 #pragma clang diagnostic pop
#elif defined (__GNUC__)
 #pragma GCC diagnostic pop
#endif

using namespace juce;

namespace
{
    inline double posMod (double a, double b) { const double r = std::fmod (a, b); return r < 0 ? r + b : r; }
    constexpr int kSub = 32;   // control-rate block (LFOs, tempo, loop switching): 0.7 ms at 48 kHz
    inline float readLin (const float* d, int N, double pos)
    {
        if (pos < 0.0 || pos >= (double) (N - 1)) return 0.0f;
        const int i = (int) pos; const float f = (float) (pos - i);
        return d[i] + f * (d[i + 1] - d[i]);
    }
}

ModuleEngine::ModuleEngine()
{
    for (auto& v : voices) v.st = std::make_unique<Stretch>();
}

ModuleEngine::~ModuleEngine() = default;

void ModuleEngine::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    int maxIn = 0;
    for (auto& v : voices)
    {
        if (percussive)
        {
            const int block = (int) (sr * 0.05) & ~1;   // ~50 ms window, 12.5 ms hop: attacks stay crisp
            v.st->configure (2, block, block / 4);
        }
        else v.st->presetDefault (2, (float) sr);
        v.needSeek = true; v.semisInit = false;
        maxIn = jmax (maxIn, kSub * 16 + v.st->blockSamples() + v.st->intervalSamples() + 16);
    }
    inL.assign ((size_t) maxIn, 0.0f); inR.assign ((size_t) maxIn, 0.0f);
    tmpL.assign ((size_t) maxBlock + 64, 0.0f); tmpR.assign ((size_t) maxBlock + 64, 0.0f);
    qArr.assign ((size_t) maxBlock + 64, 0.0);
    for (auto& s : svf) s.reset();
    filterActive = false;
    dirt.prepare (sr);
    delay.prepare (sr, (int) (sr * 6.0));
    reverb.prepare (sr);
}

const LoopPtr& ModuleEngine::getLoop() const { return voices[active].loop; }

void ModuleEngine::switchTo (LoopPtr l)
{
    if (xfade < 1.0f) { pendingSwitch = std::move (l); return; }   // finish the running crossfade first
    const int other = 1 - active;
    assign (voices[other], std::move (l));
    active = other;
    xfade = voices[1 - active].loop != nullptr ? 0.0f : 1.0f;
}

void ModuleEngine::assign (Voice& v, LoopPtr l)
{
    v.loop = std::move (l);
    v.needSeek = true; v.semisInit = false;
}

void ModuleEngine::setLoop (LoopPtr l)
{
    if (l == voices[active].loop) return;
    if (voices[active].loop == nullptr) { assign (voices[active], std::move (l)); return; }
    switchTo (std::move (l));
}

double ModuleEngine::regionBeats (const LoopData& l, const ModuleParams& p)
{
    return p.steps > 0 ? p.steps * lb::gridBeats (p.grid) : (double) jmax (1, l.beats);
}

// Swing: smooth warp out = s + d * sin^2(pi s / P) (rate changes continuously, no steps);
// returns the source time s for an output time (Newton iterations).
double ModuleEngine::swingSmooth (double x, float swing, int grid)
{
    if (swing <= 0.0005f) return x;
    const double P = grid == 0 ? 1.0 : 0.5, d = (double) swing * P / 6.0;
    const double base = std::floor (x / P) * P, xr = x - base;
    double s = xr;
    for (int it = 0; it < 5; ++it)
    {
        const double sn = std::sin (MathConstants<double>::pi * s / P);
        const double f = s + d * sn * sn - xr;
        const double df = 1.0 + d * MathConstants<double>::pi / P * std::sin (2.0 * MathConstants<double>::pi * s / P);
        s -= f / df;
    }
    return base + jlimit (0.0, P, s);
}

double ModuleEngine::loopBeat (double q, const LoopData& L, const BlockContext& ctx) const
{
    return ctx.raw ? q * L.bpm / jmax (1.0, ctx.bpm) : q;
}

void ModuleEngine::readSource (const LoopData& L, double startBeat, int count, double bps, double region, float* l, float* r) const
{
    const int N = L.numSamples();
    const double loopBeats = jmax (1, L.beats);
    const float* a = L.audio.getReadPointer (0);
    const float* b = L.audio.getReadPointer (1);
    auto at = [&] (double regionBeat, float& outL, float& outR)
    {
        const double srcBeat = posMod (posMod (regionBeat, region), loopBeats);
        const double idx = srcBeat / loopBeats * N;
        const int i0 = jlimit (0, N - 1, (int) idx);
        const int i1 = i0 + 1 < N ? i0 + 1 : 0;
        const float f = (float) (idx - i0);
        outL = a[i0] + f * (a[i1] - a[i0]);
        outR = b[i0] + f * (b[i1] - b[i0]);
    };
    // SLICE LOOP: only the part between the markers, repeated; 5 ms crossfade into the start at every repeat
    const bool looping = slOn && slL > 1.0e-6;
    const double xf = looping ? jmin (slL * 0.25, 0.005 * loopBeats * sr / jmax (1, N)) : 0.0;
    for (int k = 0; k < count; ++k)
    {
        const double U = startBeat + k * bps;
        if (! looping) { at (U, l[k], r[k]); continue; }
        const double rr = posMod (U, slL);
        at (slA + rr, l[k], r[k]);
        if (rr > slL - xf)
        {
            const float g = (float) ((rr - (slL - xf)) / xf);
            float l2, r2;
            at (slA + rr - slL, l2, r2);
            l[k] = l[k] * (1.0f - g) + l2 * g;
            r[k] = r[k] * (1.0f - g) + r2 * g;
        }
    }
}

float ModuleEngine::lfoValue (int k, double ppq, const ModuleParams& p)
{
    const double len = lb::syncBeats (p.lfoRate[k]);
    const double ph = posMod (ppq / len, 1.0);
    switch (p.lfoShape[k])
    {
        case 0: return (float) std::sin (2.0 * fx::kPi * ph);
        case 1: return (float) (1.0 - 4.0 * std::abs (ph - 0.5));
        case 2: return (float) (2.0 * ph - 1.0);
        case 3: return (float) (1.0 - 2.0 * ph);
        case 4: return ph < 0.5 ? 1.0f : -1.0f;
        default:
        {
            const long long cyc = (long long) std::floor (ppq / len);
            if (cyc != shCycle[k]) { shCycle[k] = cyc; Random r2 ((int64) cyc * 7919 + k * 104729 + 17); shVal[k] = r2.nextFloat() * 2.0f - 1.0f; }
            return shVal[k];
        }
    }
}

//==============================================================================
void ModuleEngine::renderComplex (Voice& v, float* outL, float* outR, int s0, int m, float semis, double region, const BlockContext& ctx)
{
    const auto& L = *v.loop;
    const double loopBeats = jmax (1, L.beats);
    const double bps = loopBeats / L.numSamples();
    const int outLat = v.st->outputLatency(), inLat = v.st->inputLatency();
    const double qA = qArr[(size_t) s0];
    const double dq = m > 1 ? (qArr[(size_t) (s0 + m - 1)] - qA) / (m - 1) : ctx.ppqPerSample;
    const bool warp = ! ctx.raw;
    auto proc = [&] (double q)   // processing time (future) in source beats + input lead
    {
        double u = loopBeat (q + outLat * dq, L, ctx);
        if (warp) u = swingSmooth (u, ctx.swing, ctx.swingGrid);
        return u + inLat * bps;
    };
    const double procA = proc (qA), procB = proc (qA + dq * m);
    float* ins[2] { inL.data(), inR.data() };
    auto seek = [&]
    {
        const int seekLen = jmin ((int) inL.size(), v.st->blockSamples() + v.st->intervalSamples());
        readSource (L, procA - seekLen * bps, seekLen, bps, region, inL.data(), inR.data());
        v.st->reset();
        v.st->seek (ins, seekLen, 1.0);
        v.feedBeat = procA;
        v.needSeek = false;
    };
    if (v.needSeek) seek();
    double want = (procB - v.feedBeat) / bps;
    // small position changes (groove / tempo moving) are absorbed by the stretcher (it simply gets less or more
    // input); only real jumps (> 250 ms) re-seek, which would be audible
    const double jump = 0.25 * sr;
    if (want < -jump || want > jump) { seek(); ++reseeks; want = (procB - v.feedBeat) / bps; }
    const int count = jlimit (0, jmin ((int) inL.size(), m * 8), (int) std::lround (want));
    readSource (L, v.feedBeat, count, bps, region, inL.data(), inR.data());
    v.feedBeat += count * bps;
    if (! v.semisInit) { v.semisSm = semis; v.semisInit = true; }
    v.semisSm += (semis - v.semisSm) * 0.2f;   // ~3 ms glide at control rate: no steps when moving PITCH
    v.st->setTransposeSemitones (v.semisSm, 8000.0f / (float) sr);
    float* outs[2] { outL + s0, outR + s0 };
    v.st->process (ins, count, outs, m);
}

void ModuleEngine::renderVoice (Voice& v, float* l, float* r, int s0, int m, const ModuleParams&, const BlockContext& ctx, float semis, double region)
{
    if (v.loop == nullptr || v.loop->numSamples() < 64) { std::fill (l + s0, l + s0 + m, 0.0f); std::fill (r + s0, r + s0 + m, 0.0f); return; }
    renderComplex (v, l, r, s0, m, semis, region, ctx);
}

//==============================================================================
void ModuleEngine::process (float* outL, float* outR, int n, const BlockContext& ctx, const ModuleParams& p)
{
    if ((int) qArr.size() < n) { qArr.resize ((size_t) n + 64); tmpL.resize ((size_t) n + 64); tmpR.resize ((size_t) n + 64); }
    std::fill (outL, outL + n, 0.0f);
    std::fill (outR, outR + n, 0.0f);
    const bool running = ctx.playing && p.on && voices[active].loop != nullptr;
    if (ctx.jumped) { offset = ctx.ppq * (p.tempo * ctx.rateMul - 1.0); for (auto& v : voices) v.needSeek = true; lastLoopStep = -1.0; }
    if (! running) { for (auto& v : voices) v.needSeek = true; if (! ctx.playing) playPos = -1.0f; }

    auto P = std::atomic_load (&pool);
    const VolPtr env = std::atomic_load (&vol);
    const bool useVol = p.volA && env != nullptr && ! env->empty();
    const bool useAdsr = p.atkMs > 0.5f || p.sus < 0.999f || p.relMs > 0.5f;
    const float smoothK = 1.0f - std::exp (-1.0f / (0.004f * (float) sr));   // ~4 ms parameter smoothing
    float peak = 0.0f;

    for (int s0 = 0; s0 < n; s0 += kSub)
    {
        const int m = jmin (kSub, n - s0);
        const double ppqS = ctx.ppq + s0 * ctx.ppqPerSample;
        // ---- LFOs (control rate)
        float mod[lb::numTargets] {};
        float loopMod = 0.0f;
        for (int k = 0; k < lb::numLfos; ++k)
            if (p.lfoTarget[k] != lb::tNone && std::abs (p.lfoDepth[k]) > 0.0005f)
            {
                const float v = lfoValue (k, ppqS, p) * p.lfoDepth[k];
                if (p.lfoTarget[k] == lb::tLoop) loopMod += v; else mod[p.lfoTarget[k]] += v;
            }
        // ---- groove amount glides (~150 ms) so a moving SWING knob never makes the timeline jump
        swingSm += (ctx.swing - swingSm) * (1.0f - std::exp (-(float) m / (0.15f * (float) sr)));
        if (std::abs (ctx.swing - swingSm) < 1.0e-4f) swingSm = ctx.swing;
        BlockContext cx = ctx;
        cx.swing = swingSm;
        // ---- musical position (TEMPO incl. LFO and turntable speed)
        const double rate = jlimit (0.125, 8.0, (double) p.tempo * std::pow (2.0, (double) mod[lb::tTempo]) * ctx.rateMul);
        for (int i = s0; i < s0 + m; ++i)
        {
            qArr[(size_t) i] = ctx.ppq + i * ctx.ppqPerSample + offset;
            offset += ctx.ppqPerSample * (rate - 1.0);
        }
        if (running)
        {
            // ---- LFO -> Loop: pick a loop of the pool on every grid step (quantised, crossfaded)
            if (P != nullptr && P->size() > 1 && p.usesLoopLfo())
            {
                const double step = jmax (0.125, lb::gridBeats (p.grid));
                const double st = std::floor (qArr[(size_t) s0] / step);
                if (std::abs (st - lastLoopStep) > 0.5)
                {
                    lastLoopStep = st;
                    const float v = jlimit (-1.0f, 1.0f, loopMod);
                    const int idx = jlimit (0, (int) P->size() - 1, (int) std::lround ((v + 1.0f) * 0.5f * (float) (P->size() - 1)));
                    if ((*P)[(size_t) idx] != nullptr && (*P)[(size_t) idx] != voices[active].loop) switchTo ((*P)[(size_t) idx]);
                }
            }
            if (pendingSwitch != nullptr && xfade >= 1.0f) { auto l = std::move (pendingSwitch); pendingSwitch = nullptr; switchTo (std::move (l)); }

            const auto& L = *voices[active].loop;
            const double region = regionBeats (L, p);
            float sl0 = jlimit (0.0f, 1.0f, p.sStart + 0.25f * mod[lb::tSliceStart]);
            float sl1 = jlimit (0.0f, 1.0f, p.sEnd + 0.25f * mod[lb::tSliceEnd]);
            if (sl1 < sl0 + 0.002f) sl1 = jmin (1.0f, sl0 + 0.002f);
            slOn = p.slice && p.slLoop;
            slA = sl0 * region; slL = (sl1 - sl0) * region;
            const float semis = jlimit (-48.0f, 48.0f, cx.semitones + 12.0f * mod[lb::tPitch] + 12.0f * (float) std::log2 (cx.rateMul));
            renderVoice (voices[active], outL, outR, s0, m, p, cx, semis, region);
            if (xfade < 1.0f)
            {
                auto& old = voices[1 - active];
                const double oregion = old.loop != nullptr ? regionBeats (*old.loop, p) : region;
                renderVoice (old, tmpL.data(), tmpR.data(), s0, m, p, cx, semis, oregion);
                const float inc = (float) (1.0 / (0.02 * sr));
                for (int i = s0; i < s0 + m; ++i)
                {
                    xfade = jmin (1.0f, xfade + inc);
                    const float gn = std::sin (xfade * MathConstants<float>::halfPi), go = std::cos (xfade * MathConstants<float>::halfPi);
                    outL[i] = outL[i] * gn + tmpL[(size_t) i] * go;
                    outR[i] = outR[i] * gn + tmpR[(size_t) i] * go;
                }
            }
            // ---- slice gate, ADSR, clip gain (output timeline)
            const double loopBeats = jmax (1, L.beats);
            const double bpo = (qArr[(size_t) (s0 + m - 1)] - qArr[(size_t) s0]) / jmax (1, m - 1);
            const double regionSamples = region / jmax (1.0e-12, cx.raw ? bpo * L.bpm / jmax (1.0, cx.bpm) : bpo);
            const double regionSec = regionSamples / sr;
            const double fadePh = (0.0005 * sr) / jmax (1.0, regionSamples);   // 0.5 ms slice fades
            ignoreUnused (loopBeats);
            for (int i = s0; i < s0 + m; ++i)
            {
                const double U = loopBeat (qArr[(size_t) i], L, cx);
                // in SLICE LOOP the timeline runs around the slice: ADSR, VOL.A and the playhead follow it
                const double ph = slOn ? (slA + posMod (U, jmax (1.0e-9, slL))) / region : posMod (U, region) / region;
                if (i == 0) playPos = (float) ph;
                float g = 1.0f;
                double segA = 0.0, segB = 1.0;
                if (slOn) { segA = sl0; segB = sl1; }
                else if (p.slice)
                {
                    segA = sl0; segB = sl1;
                    if (p.fade) g = (float) (jlimit (0.0, 1.0, (ph - sl0) / fadePh) * jlimit (0.0, 1.0, (sl1 - ph) / fadePh));
                    else g = (ph >= sl0 && ph < sl1) ? 1.0f : 0.0f;
                }
                if (useAdsr && g > 0.0f)
                {
                    const double tIn = (ph - segA) * regionSec, tOut = (segB - ph) * regionSec;
                    const double at = p.atkMs * 0.001, dc = jmax (0.001, (double) p.decMs * 0.001), rl = p.relMs * 0.001;
                    double e = tIn < at ? tIn / at : p.sus + (1.0 - p.sus) * std::exp (-(tIn - at) * 4.0 / dc);
                    if (rl > 0 && tOut < rl) e *= jmax (0.0, tOut / rl);
                    g *= (float) jlimit (0.0, 1.0, e);
                }
                if (useVol && g > 0.0f)
                {
                    const auto& vv = *env;
                    float y = vv.front().y;
                    if (ph >= vv.back().x) y = vv.back().y;
                    else
                        for (size_t k = 1; k < vv.size(); ++k)
                            if (ph < vv[k].x)
                            {
                                const float t = (float) ((ph - vv[k - 1].x) / jmax (1e-6f, vv[k].x - vv[k - 1].x));
                                y = vv[k - 1].y + jlimit (0.0f, 1.0f, t) * (vv[k].y - vv[k - 1].y);
                                break;
                            }
                    g *= volYToGain (y);
                }
                outL[i] *= g; outR[i] *= g;
            }
        }

        // ---- filter: per-sample smoothed (log) cutoff, coefficients every 4 samples -> no zipper / glitches
        {
            const float cutT = std::log (jlimit (20.0f, 20000.0f, p.cut * std::pow (2.0f, 4.0f * mod[lb::tCutoff])));
            const float resT = jlimit (0.0f, 1.0f, p.res + 0.5f * mod[lb::tRes]);
            const float lo = std::log (21.0f), hi = std::log (19800.0f);
            const bool wantBypass = (p.ftype == 0 && cutT >= hi && logCutSm >= hi - 0.01f) || (p.ftype == 2 && cutT <= lo && logCutSm <= lo + 0.01f);
            if (wantBypass) { if (filterActive) { svf[0].reset(); svf[1].reset(); filterActive = false; } logCutSm = cutT; resSm = resT; }
            else
            {
                filterActive = true;
                for (int i = s0; i < s0 + m; ++i)
                {
                    logCutSm += (cutT - logCutSm) * smoothK;
                    resSm += (resT - resSm) * smoothK;
                    if (((i - s0) & 3) == 0)
                    {
                        const double q = 0.5 + resSm * resSm * 12.0;
                        svf[0].set (sr, std::exp (logCutSm), q); svf[1].set (sr, std::exp (logCutSm), q);
                    }
                    outL[i] = svf[0].process (outL[i], p.ftype);
                    outR[i] = svf[1].process (outR[i], p.ftype);
                }
            }
        }
        // ---- dirt, delay, reverb
        dirt.process (outL + s0, outR + s0, m, jlimit (0.0f, 1.0f, p.dirt + 0.5f * mod[lb::tDirt]));
        const double dSamples = lb::syncBeats (p.dtime) * 60.0 / jmax (20.0, ctx.bpm) * sr;
        delay.process (outL + s0, outR + s0, m, dSamples, jlimit (0.0f, 0.95f, p.dfb + 0.5f * mod[lb::tDelayFb]),
                       jlimit (0.0f, 1.0f, p.dmix + 0.5f * mod[lb::tDelayMix]), p.dpp);
        reverb.process (outL + s0, outR + s0, m, p.rsize, p.rdecay, p.rdamp, p.rpre, jlimit (0.0f, 1.0f, p.rmix + 0.5f * mod[lb::tReverbMix]));
        // ---- gain / pan
        const float gainT = (p.on ? fx::dbToGain (p.gainDb) : 0.0f) * jmax (0.0f, 1.0f + mod[lb::tGain]);
        const float panT = jlimit (-1.0f, 1.0f, p.pan + mod[lb::tPan]);
        for (int i = s0; i < s0 + m; ++i)
        {
            gainSm += (gainT - gainSm) * smoothK;
            panSm += (panT - panSm) * smoothK;
            const float gl = gainSm * jmin (1.0f, 1.0f - panSm), gr = gainSm * jmin (1.0f, 1.0f + panSm);
            outL[i] *= gl; outR[i] *= gr;
            peak = jmax (peak, std::abs (outL[i]), std::abs (outR[i]));
        }
    }
    level = jmax (peak, level.load() * 0.9f);
}
