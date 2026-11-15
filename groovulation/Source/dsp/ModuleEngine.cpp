#include "ModuleEngine.h"
#include "../../third_party/signalsmith-stretch/signalsmith-stretch.h"

using namespace juce;

namespace
{
    inline double posMod (double a, double b) { const double r = std::fmod (a, b); return r < 0 ? r + b : r; }
}

ModuleEngine::ModuleEngine() : stretch (std::make_unique<Stretch>()) {}
ModuleEngine::~ModuleEngine() = default;

void ModuleEngine::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    stretch->presetDefault (2, (float) sr);
    const int maxIn = maxBlock * 16 + stretch->blockSamples() + stretch->intervalSamples() + 16;
    inL.assign ((size_t) maxIn, 0.0f); inR.assign ((size_t) maxIn, 0.0f);
    tmpL.assign ((size_t) maxBlock + 64, 0.0f); tmpR.assign ((size_t) maxBlock + 64, 0.0f);
    for (auto& s : svf) s.reset();
    dirt.prepare (sr);
    delay.prepare (sr, (int) (sr * 6.0));
    reverb.prepare (sr);
    needSeek = true; semisInit = false;
}

double ModuleEngine::regionBeats (const LoopData& l, const ModuleParams& p)
{
    return p.steps > 0 ? p.steps * lb::gridBeats (p.grid) : (double) jmax (1, l.beats);
}

double ModuleEngine::swingWarp (double ppq, float swing, int grid)
{
    if (swing <= 0.0005f) return ppq;
    // groove by time-warping: inside each pair of 1/8 (or 1/16) notes the off-beat is delayed by up to a
    // triplet position (swing 100 % = 2/3), the audio in between is stretched / compressed accordingly.
    const double unit = grid == 0 ? 1.0 : 0.5;
    const double mid = unit * 0.5, d = (double) swing * unit / 6.0;
    const double base = std::floor (ppq / unit) * unit, x = ppq - base;
    const double src = x < mid + d ? x * mid / (mid + d) : mid + (x - mid - d) * mid / (mid - d);
    return base + src;
}

double ModuleEngine::loopBeatAt (double ppq, const LoopData& l, const BlockContext& ctx, const ModuleParams& p)
{
    if (ctx.raw)   // original tempo: the loop runs at its own BPM (times the TEMPO factor)
        return ppq * (l.bpm / jmax (1.0, ctx.bpm)) * p.tempo;
    return swingWarp (ppq, ctx.swing, ctx.swingGrid) * p.tempo;
}

void ModuleEngine::readSource (double startBeat, int count, double bps, float* l, float* r) const
{
    const auto& L = *loop;
    const int N = L.numSamples();
    const double loopBeats = jmax (1, L.beats);
    const double region = regionBeats (L, lastParams);
    const float* a = L.audio.getReadPointer (0);
    const float* b = L.audio.getReadPointer (1);
    for (int k = 0; k < count; ++k)
    {
        const double U = startBeat + k * bps;
        const double srcBeat = posMod (posMod (U, region), loopBeats);
        const double idx = srcBeat / loopBeats * N;
        const int i0 = jlimit (0, N - 1, (int) idx);
        const int i1 = i0 + 1 < N ? i0 + 1 : 0;
        const float f = (float) (idx - i0);
        l[k] = a[i0] + f * (a[i1] - a[i0]);
        r[k] = b[i0] + f * (b[i1] - b[i0]);
    }
}

void ModuleEngine::doSeek (double targetBeat, double bps)
{
    const int seekLen = jmin ((int) inL.size(), stretch->blockSamples() + stretch->intervalSamples());
    readSource (targetBeat - seekLen * bps, seekLen, bps, inL.data(), inR.data());
    float* ins[2] { inL.data(), inR.data() };
    stretch->reset();
    stretch->seek (ins, seekLen, 1.0);
    feedBeat = targetBeat;
    needSeek = false;
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

void ModuleEngine::process (float* outL, float* outR, int n, const BlockContext& ctx, const ModuleParams& pIn)
{
    lastParams = pIn;
    const ModuleParams& p = pIn;
    std::fill (outL, outL + n, 0.0f);
    std::fill (outR, outR + n, 0.0f);
    const bool active = loop != nullptr && loop->numSamples() > 64 && ctx.playing && p.on;

    // ---------------- LFO modulation (control rate: once per block)
    float mod[lb::numTargets] {};
    for (int k = 0; k < 2; ++k)
        if (p.lfoTarget[k] != lb::tNone && std::abs (p.lfoDepth[k]) > 0.0005f)
            mod[p.lfoTarget[k]] += lfoValue (k, ctx.ppq, p) * p.lfoDepth[k];

    if (active)
    {
        const auto& L = *loop;
        const double loopBeats = jmax (1, L.beats);
        const double bps = loopBeats / L.numSamples();                     // loop beats per source sample
        const double region = regionBeats (L, p);
        const int outLat = stretch->outputLatency(), inLat = stretch->inputLatency();
        const float targetSemis = jlimit (-48.0f, 48.0f, ctx.semitones + 12.0f * mod[lb::tPitch]);
        if (! semisInit) { semisSm = targetSemis; semisInit = true; }
        if (ctx.jumped) needSeek = true;

        float* ins[2] { inL.data(), inR.data() };
        constexpr int sub = 64;
        for (int s0 = 0; s0 < n; s0 += sub)
        {
            const int m = jmin (sub, n - s0);
            const double ppqA = ctx.ppq + s0 * ctx.ppqPerSample;
            const double ppqB = ppqA + m * ctx.ppqPerSample;
            // future position: processing time = output time + output latency; input leads by input latency
            const double procA = loopBeatAt (ppqA + outLat * ctx.ppqPerSample, L, ctx, p) + inLat * bps;
            const double procB = loopBeatAt (ppqB + outLat * ctx.ppqPerSample, L, ctx, p) + inLat * bps;
            if (needSeek) doSeek (procA, bps);
            double want = (procB - feedBeat) / bps;
            if (want < -4.0 || want > m * 16.0) { doSeek (procA, bps); want = (procB - feedBeat) / bps; }
            const int count = jlimit (0, (int) inL.size(), (int) std::lround (want));
            readSource (feedBeat, count, bps, inL.data(), inR.data());
            feedBeat += count * bps;
            // smooth, continuous pitch (glide of ~10 ms): no audible steps when moving PITCH
            semisSm += (targetSemis - semisSm) * 0.35f;
            stretch->setTransposeSemitones (semisSm, 8000.0f / (float) sr);
            float* outs[2] { outL + s0, outR + s0 };
            stretch->process (ins, count, outs, m);
        }

        // ---------------- per-sample: slice gate, ADSR, clip gain, transient shaper
        const double beatsPerOut = ctx.raw ? ctx.ppqPerSample * L.bpm / jmax (1.0, ctx.bpm) * p.tempo : ctx.ppqPerSample * p.tempo;
        const double regionSamples = region / jmax (1e-12, beatsPerOut);
        const double regionSec = regionSamples / sr;
        float s0 = jlimit (0.0f, 1.0f, p.sStart + 0.25f * mod[lb::tSliceStart]);
        float s1 = jlimit (0.0f, 1.0f, p.sEnd + 0.25f * mod[lb::tSliceEnd]);
        if (s1 < s0 + 0.002f) s1 = jmin (1.0f, s0 + 0.002f);
        const double fadePh = (0.0005 * sr) / jmax (1.0, regionSamples);   // 0.5 ms
        const float shaper = jlimit (0.0f, 1.0f, p.shaper + 0.5f * mod[lb::tShaper]);
        const bool useAdsr = p.atkMs > 0.5f || p.sus < 0.999f || p.relMs > 0.5f;
        const VolPtr env = std::atomic_load (&vol);
        const bool useVol = p.volA && env != nullptr && ! env->empty();
        const auto& onsets = L.onsets;
        const double loopSamplesOut = loopBeats / jmax (1e-12, beatsPerOut);

        for (int i = 0; i < n; ++i)
        {
            const double U = loopBeatAt (ctx.ppq + i * ctx.ppqPerSample, L, ctx, p);
            const double ph = posMod (U, region) / region;
            if (i == 0) playPos = (float) ph;
            float g = 1.0f;
            double segA = 0.0, segB = 1.0;
            if (p.slice)
            {
                segA = s0; segB = s1;
                if (p.fade) g = (float) (jlimit (0.0, 1.0, (ph - s0) / fadePh) * jlimit (0.0, 1.0, (s1 - ph) / fadePh));
                else g = (ph >= s0 && ph < s1) ? 1.0f : 0.0f;
            }
            if (useAdsr && g > 0.0f)
            {
                const double tIn = (ph - segA) * regionSec, tOut = (segB - ph) * regionSec;
                const double a = p.atkMs * 0.001, d = jmax (0.001, (double) p.decMs * 0.001), r = p.relMs * 0.001;
                double e = tIn < a ? tIn / a : p.sus + (1.0 - p.sus) * std::exp (-(tIn - a) * 4.0 / d);
                if (r > 0 && tOut < r) e *= jmax (0.0, tOut / r);
                g *= (float) jlimit (0.0, 1.0, e);
            }
            if (useVol && g > 0.0f)
            {
                const auto& v = *env;
                float y = v.front().y;
                if (ph >= v.back().x) y = v.back().y;
                else
                    for (size_t k = 1; k < v.size(); ++k)
                        if (ph < v[k].x)
                        {
                            const float t = (float) ((ph - v[k - 1].x) / jmax (1e-6f, v[k].x - v[k - 1].x));
                            y = v[k - 1].y + jlimit (0.0f, 1.0f, t) * (v[k].y - v[k - 1].y);
                            break;
                        }
                g *= volYToGain (y);
            }
            if (shaper < 0.999f && ! onsets.empty() && g > 0.0f)
            {
                // Ableton-style "transient envelope": each hit fades out before the next one
                const double srcF = posMod (ph * region, loopBeats) / loopBeats;
                auto it = std::upper_bound (onsets.begin(), onsets.end(), (float) srcF);
                const double o0 = it == onsets.begin() ? onsets.back() - 1.0 : *(it - 1);
                const double o1 = it == onsets.end() ? onsets.front() + 1.0 : *it;
                const double segLen = jmax (1e-6, o1 - o0);
                const double x = (srcF - o0) / segLen;
                const double a = 0.04 + 0.96 * shaper;
                const double fadeOut = std::pow (jlimit (0.0, 1.0, 1.0 - x / a), 1.5);
                const double ramp = jmin (1.0, (srcF - o0) * loopSamplesOut / (0.001 * sr));   // 1 ms attack on each hit
                g *= (float) (fadeOut * ramp);
            }
            outL[i] *= g; outR[i] *= g;
        }
    }
    else
    {
        if (! ctx.playing || loop == nullptr) { needSeek = true; playPos = -1.0f; }
        if (! p.on) needSeek = true;
    }

    // ---------------- filter, dirt, delay, reverb, gain, pan (also run on silence so tails ring out)
    const float cutT = jlimit (20.0f, 20000.0f, p.cut * std::pow (2.0f, 4.0f * mod[lb::tCutoff]));
    const float resT = jlimit (0.0f, 1.0f, p.res + 0.5f * mod[lb::tRes]);
    cutSm += (cutT - cutSm) * 0.5f; resSm += (resT - resSm) * 0.5f;
    const bool bypassFilter = (p.ftype == 0 && cutSm >= 19800.0f) || (p.ftype == 2 && cutSm <= 21.0f);
    if (! bypassFilter)
    {
        const double q = 0.5 + resSm * resSm * 12.0;
        for (auto& f : svf) f.set (sr, cutSm, q);
        for (int i = 0; i < n; ++i) { outL[i] = svf[0].process (outL[i], p.ftype); outR[i] = svf[1].process (outR[i], p.ftype); }
    }
    dirt.process (outL, outR, n, jlimit (0.0f, 1.0f, p.dirt + 0.5f * mod[lb::tDirt]));
    const double dSamples = lb::syncBeats (p.dtime) * 60.0 / jmax (20.0, ctx.bpm) * sr;
    delay.process (outL, outR, n, dSamples, jlimit (0.0f, 0.95f, p.dfb + 0.5f * mod[lb::tDelayFb]),
                   jlimit (0.0f, 1.0f, p.dmix + 0.5f * mod[lb::tDelayMix]), p.dpp);
    reverb.process (outL, outR, n, p.rsize, p.rdecay, p.rdamp, p.rpre, jlimit (0.0f, 1.0f, p.rmix + 0.5f * mod[lb::tReverbMix]));

    const float gainT = (p.on ? fx::dbToGain (p.gainDb) : 0.0f) * jmax (0.0f, 1.0f + mod[lb::tGain]);
    const float panT = jlimit (-1.0f, 1.0f, p.pan + mod[lb::tPan]);
    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        gainSm += (gainT - gainSm) * 0.002f;
        panSm += (panT - panSm) * 0.002f;
        const float gl = gainSm * jmin (1.0f, 1.0f - panSm), gr = gainSm * jmin (1.0f, 1.0f + panSm);
        outL[i] *= gl; outR[i] *= gr;
        peak = jmax (peak, std::abs (outL[i]), std::abs (outR[i]));
    }
    level = jmax (peak, level.load() * 0.9f);
}
