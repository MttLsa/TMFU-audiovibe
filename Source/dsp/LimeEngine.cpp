#include "LimeEngine.h"
#include <algorithm>

using namespace params;
using juce::jlimit;
using juce::jmax;
using juce::jmin;
static constexpr double kPi = juce::MathConstants<double>::pi;

namespace
{
    enum K { kOn, kGain, kPan, kAcc, kVamt, kWav, kTune, kPort, kSt, kHold, kDcy, kSat, kShp, kSm, kAtt, kCd, kOd, kLink,
             kFm, kMp, kNoise, kZap, kMod, kVel, kLo, kFt, kFlam, kCut, kRes, kTrkLo, kTrkHi, kSrc, kIn, kOsc, kComb, kFb, kFreq, kScale, kName, kCount };
    const char* keyNames[kCount] { lv::on, lv::gain, lv::pan, lv::acc, lv::vamt, lv::wav, lv::tune, lv::port, lv::st, lv::hold, lv::dcy,
                                   lv::sat, lv::shp, lv::sm, lv::att, lv::cd, lv::od, lv::link, lv::fm, lv::mp, lv::noise, lv::zap,
                                   lv::mod, lv::vel, lv::lo, lv::ft, lv::flam, lv::cut, lv::res, lv::trkLo, lv::trkHi, lv::src, lv::inGain,
                                   lv::osc, lv::comb, lv::fb, lv::freq, lv::scale, lv::nameKey };
    inline float G (std::atomic<float>* a, float def) { return a != nullptr ? a->load() : def; }
    inline float decayTau (float pct) { return (float) (0.005 * std::pow (800.0, pct * 0.01)); }   // 5 ms … 4 s
    inline int   moduleOf (int voice) { return voice; }                                             // kick..pod map 1:1
}


LimeEngine::LimeEngine (SampleLibrary& lib) : library (lib)
{
    loadDefaultPatterns();
    // Remix table: 127 deterministic re-orderings of a 16-step bar (index 0 = straight)
    uint32_t s = 0x9e3779b9u;
    auto rnd = [&s] { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; };
    for (int r = 0; r < 128; ++r)
    {
        auto& m = remixTable[(size_t) r];
        for (int i = 0; i < 16; ++i) m[(size_t) i] = (int8_t) i;
        if (r == 0) continue;
        const int type = r % 8;
        const int seg = 1 << (1 + (r / 8) % 3);          // 2, 4 or 8 step segments
        for (int i = 0; i < 16; ++i)
        {
            const int segStart = (i / seg) * seg, inSeg = i % seg;
            switch (type)
            {
                case 0: m[(size_t) i] = (int8_t) (12 + (i % 4)); break;                                   // repeat last beat
                case 1: m[(size_t) i] = (int8_t) (segStart + (seg - 1 - inSeg)); break;                   // reverse segments
                case 2: m[(size_t) i] = (int8_t) (i < 8 ? i : (8 + (i % 2))); break;                      // stutter end
                case 3: m[(size_t) i] = (int8_t) (i / 2); break;                                          // half time
                case 4: m[(size_t) i] = (int8_t) ((i * 2) % 16); break;                                   // double time
                case 5: m[(size_t) i] = (int8_t) (segStart == 0 ? inSeg : ((rnd() % 4) * 4 + inSeg) % 16); break; // shuffle beats
                case 6: m[(size_t) i] = (int8_t) (i >= 12 ? 12 + (i % 2) : i); break;                     // roll fill
                default: m[(size_t) i] = (int8_t) ((rnd() % 3 == 0) ? (int) (rnd() % 16) : i); break;     // random swaps
            }
        }
    }
}

std::atomic<float>* LimeEngine::p (const juce::String& id) const { return state->getRawParameterValue (id); }

void LimeEngine::bind (juce::AudioProcessorValueTreeState& apvts)
{
    state = &apvts;
    for (int v = 0; v < numLimeVoices; ++v)
        for (int k = 0; k < kCount; ++k)
            vk[(size_t) v][(size_t) k] = apvts.getRawParameterValue (b (v, keyNames[k]));   // nullptr if the voice lacks it
}

void LimeEngine::prepare (double sampleRate, int blockSize)
{
    sr = sampleRate;
    for (auto& vv : voices) for (auto& v : vv) v.active = false;
    pulpV.active = false;
    pulpV.comb.prepare ((int) (sr * 0.1));
    envFilter.prepare (sr); eq.prepare (sr); magic.prepare (sr); delay.prepare (sr); xfx.prepare (sr); dirtSat.prepare (sr);
    const size_t n = (size_t) jmax (blockSize, 4096);
    for (auto* b : { &dryL, &dryR, &tieL, &tieR, &sendL, &sendR, &dlyL, &dlyR, &xL, &xR, &srcMono }) b->assign (n, 0.0f);
    lastLoop = -1;
}

void LimeEngine::audition (int target, float vel)
{
    if (target >= 0 && target < numTargets) auditionQ[(size_t) target].store (jlimit (1, 1000, (int) (vel * 1000.0f)));
}

void LimeEngine::readParams() {}

void LimeEngine::auditionSample (int module, int index)
{
    if (module >= 0 && module < 5) auditionIdx[(size_t) module].store (index);
}

int LimeEngine::remap (int pos, int len) const
{
    const int r = (int) p (bRemix)->load();
    if (! remixThisLoop || r <= 0) return pos;
    const int block = (pos / 16) * 16;
    const int src = block + remixTable[(size_t) r][(size_t) (pos % 16)];
    return src < len ? src : pos;
}

//==============================================================================
void LimeEngine::trigger (int target, float vel, float laneValue, int forcedIndex)
{
    const int voice = target <= tSnare ? target : (target <= tOHat ? hat : (target == tTie ? tie : (target == tPod ? pod : pulp)));
    const auto& k = vk[(size_t) voice];
    if (G (k[kOn], 1.0f) < 0.5f) return;
    hits[(size_t) target].fetch_add (1);

    if (voice == pulp)
    {
        pulpV.active = true; pulpV.t = 0; pulpV.vel = vel;
        return;
    }
    // Pulp is (re)triggered by the hits of its selected input source
    {
        const int srcSel = (int) G (vk[(size_t) pulp][kSrc], 3);
        if ((srcSel == 5 || srcSel == voice) && G (vk[(size_t) pulp][kOn], 0) > 0.5f)
        {
            float pv = vel;
            if (voice == tie || voice == pod) pv = jmax (0.05f, laneValue);
            pulpV.active = true; pulpV.t = 0; pulpV.vel = pv;
            hits[(size_t) tPulp].fetch_add (1);
        }
    }

    auto set = library.set (moduleOf (voice));
    if (set->size() == 0) return;

    // modulation sources: Mod lane (Kick/Snare/Hat) or step value (Tie/Pod)
    const float m = modHeld;
    int dest = 0;
    if (voice <= hat) dest = (int) G (k[kMod], 0);
    else dest = (int) G (k[kVel], 0);
    if (forcedIndex >= 0) dest = 0;                       // pitch-window audition: plain sample
    const float v = forcedIndex >= 0 ? 1.0f : laneValue;   // Tie/Pod step value 0..1

    int wav = (int) G (k[kWav], 0);
    float tune = G (k[kTune], 0) + p (bTune)->load();
    const float tuneBase = tune;            // module Tune + global B Tune (before any modulation)
    int velDegrees = 0; bool velTune = false;
    float start = G (k[kSt], 0) * 0.01f * 0.9f;
    float cut = G (k[kCut], 20000.0f);
    float dcyPct = voice == hat ? G (target == tOHat ? k[kOd] : k[kCd], 50) : G (k[kDcy], 50);
    float att = G (k[kAtt], 0) * 0.01f;
    float fm = G (k[kFm], 0), zap = G (k[kZap], 0);
    float flamMs = G (k[kFlam], 0) * 0.6f;

    if (voice <= hat)
    {
        switch (dest)
        {
            case 1: wav += (int) (m * 8.0f); break;
            case 2: tune += m * 12.0f; break;
            case 3: cut *= std::pow (2.0f, voice == hat ? m * 4.0f : -m * 5.0f); break;
            case 4: flamMs += m * 50.0f; break;
            default: break;
        }
    }
    else
    {
        // Tie: --, wav, tune, att, dcy, st, fm, cut   Pod: --, wav, tune, att, dcy, zap, cut
        const bool isTie = voice == tie;
        switch (dest)
        {
            case 1: wav += (int) (v * 8.0f); break;
            case 2: tune += std::round ((v - 0.5f) * 24.0f); velTune = true; velDegrees = (int) std::round ((v - 0.5f) * 14.0f); break;
            case 3: att = jlimit (0.0f, 1.0f, att + v); break;
            case 4: dcyPct = jlimit (0.0f, 100.0f, dcyPct * (0.25f + 1.5f * v)); break;
            case 5: if (isTie) start = jlimit (0.0f, 0.9f, start + v * 0.5f); else zap = jlimit (-100.0f, 100.0f, zap + (v - 0.5f) * 200.0f); break;
            case 6: if (isTie) fm = jlimit (0.0f, 100.0f, fm + v * 100.0f); else cut *= std::pow (2.0f, (v - 0.5f) * 6.0f); break;
            case 7: if (isTie) cut *= std::pow (2.0f, (v - 0.5f) * 6.0f); break;
            default: break;
        }
        const float vamt = G (k[kVamt], 70) * 0.01f;
        vel *= 1.0f - vamt * (1.0f - v);
    }
    if (forcedIndex >= 0) wav = forcedIndex;
    wav = ((wav % set->size()) + set->size()) % set->size();
    SamplePtr smp = set->samples[(size_t) wav];
    if (target == tOHat && (size_t) wav < set->openPartner.size() && set->openPartner[(size_t) wav] != nullptr)
        smp = set->openPartner[(size_t) wav];

    // Hat link: closed cuts open
    if (voice == hat && target == tCHat && G (k[kLink], 1) > 0.5f)
        for (auto& o : voices[(size_t) hat]) if (o.active) o.fading = true;

    const int slot = voice;
    for (auto& o : voices[(size_t) slot]) if (o.active && ! (voice == hat && target == tOHat)) o.fading = true;
    auto& nv = voices[(size_t) slot][(size_t) nextVoice[(size_t) slot]];
    nextVoice[(size_t) slot] ^= 1;

    nv.smp = smp;
    nv.active = true; nv.fading = false; nv.fade = 1.0f; nv.t = 0; nv.fmPhase = 0;
    nv.pos = start * (double) smp->length();
    // ---- pitch: SCALE mode (detected pitch quantized to the shared scale) or MANUAL (per-sample offset)
    nv.set = set; nv.smpIndex = wav; nv.manualPitch = false;
    if (voice == tie || voice == pod)
    {
        const bool scaleOn = G (k[kScale], 0) > 0.5f;
        // pitch source: key from the file name (if enabled and present), otherwise the audio analysis
        const bool useName = G (k[kName], 0) > 0.5f && smp->namePitch >= 0.0f;
        const float srcPitch = useName ? smp->namePitch : smp->pitch;
        if (scaleOn && srcPitch >= 0.0f)
        {
            const int root = (int) p (bScaleRoot)->load(), type = (int) p (bScaleType)->load();
            int q = quantizeToScale (srcPitch + tuneBase, root, type);
            if (velTune) q = stepScale (q, velDegrees, root, type);
            tune = (float) q - srcPitch;
        }
        else if (! scaleOn)
        {
            nv.manualPitch = true;
            nv.baseShift = tune;
            tune += set->offsets[(size_t) wav].load();
        }
    }
    nv.rateTarget = smp->sourceRate / sr * std::pow (2.0, tune / 12.0);
    if (voice == tie)
    {
        const float port = G (k[kPort], 0) * 0.01f;
        nv.rate = port > 0.001f ? tiePrevRate : nv.rateTarget;
        tiePrevRate = nv.rateTarget;
    }
    else nv.rate = nv.rateTarget;
    nv.gain = vel;
    nv.fullLen = dcyPct >= 99.5f;
    nv.tau = decayTau (dcyPct);
    nv.holdS = (voice <= snare) ? (float) (std::pow (G (k[kHold], 0) * 0.01, 2.0) * 0.8) : 0.0f;
    nv.attS = (float) (att * att * 0.5);
    nv.fmAmt = fm; nv.zap = zap; nv.cut = cut;
    nv.prevShape[0] = nv.prevShape[1] = 0;
    for (auto& f : nv.flt) f.reset();
    for (auto& f : nv.hp) f.reset();

    // snare flam: second hit a few ms later on the other declick voice
    if (voice == snare && flamMs > 1.0f)
    {
        auto& fv = voices[(size_t) snare][(size_t) nextVoice[(size_t) snare]];
        fv = nv;
        fv.pos -= flamMs * 0.001 * smp->sourceRate;   // negative position = pre-delay
        fv.gain *= 0.7f;
    }
}

//==============================================================================
void LimeEngine::renderSampleVoice (int voice, Voice& v, float* dl, float* dr, int n)
{
    if (! v.active || v.smp == nullptr) return;
    const auto& k = vk[(size_t) voice];
    const auto& S = *v.smp;
    const int len = S.length();
    const bool st = S.stereo();

    const float gain = juce::Decibels::decibelsToGain (G (k[kGain], 0), -60.0f) * v.gain;
    const float panN = (G (k[kPan], 0) * 0.01f + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
    const float gl = std::cos (panN) * 1.41421356f * gain, gr = std::sin (panN) * 1.41421356f * gain;
    const float res = G (k[kRes], 0);
    const double q = 0.55 + res * 0.01 * 9.0;
    int fmode = 0;
    if (voice == snare || voice == tie || voice == pod) fmode = (int) G (k[kFt], 0);
    if (voice == hat) fmode = 2;
    const bool useFilter = voice == hat ? v.cut > 25.0f : (v.cut < 19500.0f || res > 1.0f);
    if (useFilter) { v.flt[0].set (v.cut, sr, q); v.flt[1].set (v.cut, sr, q); }
    const bool lowcut = voice == kick && G (k[kLo], 0) > 0.5f;
    if (lowcut) { v.hp[0].set (80.0, sr, 0.8); v.hp[1].set (80.0, sr, 0.8); }

    const bool shaping = voice <= snare;
    const float shp = G (k[kShp], 0) * 0.01f, sm = G (k[kSm], 0) * 0.01f * 0.85f, sat = G (k[kSat], 0) * 0.01f;
    const float pw = std::pow (2.0f, -shp * 1.6f);
    const float drive = 1.0f + sat * 12.0f, satNorm = 1.0f / std::tanh (drive);
    const float noise = voice == tie ? G (k[kNoise], 0) * 0.01f : 0.0f;
    const float tieSat = voice == tie ? G (k[kSat], 0) * 0.01f : 0.0f;
    const double portCoef = voice == tie ? 1.0 - std::exp (-1.0 / (sr * (0.002 + std::pow (G (k[kPort], 0) * 0.01, 2.0) * 0.6))) : 1.0;
    const double fmRatio = voice == tie ? std::pow (2.0, G (k[kMp], 12) / 12.0) : 1.0;
    if (v.manualPitch && v.set != nullptr && v.smpIndex >= 0 && v.smpIndex < v.set->size())
        v.rateTarget = S.sourceRate / sr * std::pow (2.0, (v.baseShift + v.set->offsets[(size_t) v.smpIndex].load()) / 12.0);
    const double fmDepth = v.fmAmt * 0.01 * 1.5;
    const double zapTau = 0.004 + 0.06 * (1.0 - std::abs (v.zap) * 0.01);
    const double zapOct = v.zap * 0.01 * 4.0;
    uint32_t nrng = 0xabcdef ^ (uint32_t) (v.t * 1000.0);

    for (int i = 0; i < n; ++i)
    {
        v.rate += (v.rateTarget - v.rate) * portCoef;
        double rate = v.rate;
        if (fmDepth > 0.0)
        {
            v.fmPhase += 261.63 * rate * fmRatio / (S.sourceRate / sr) / sr;
            rate *= std::pow (2.0, fmDepth * std::sin (2.0 * kPi * v.fmPhase));
        }
        if (voice == pod && std::abs (zapOct) > 1.0e-9) rate *= std::pow (2.0, zapOct * std::exp (-v.t / zapTau));

        float xl = 0, xr = 0;
        if (v.pos >= 0.0)
        {
            const int i0 = (int) v.pos;
            if (i0 >= len - 1) { v.active = false; break; }
            const float fr = (float) (v.pos - i0);
            xl = S.L[(size_t) i0] + fr * (S.L[(size_t) i0 + 1] - S.L[(size_t) i0]);
            xr = st ? S.R[(size_t) i0] + fr * (S.R[(size_t) i0 + 1] - S.R[(size_t) i0]) : xl;
        }
        // envelope: attack, hold, decay
        float env = 1.0f;
        if (v.t < v.attS) env = (float) (v.t / jmax (1.0e-4f, v.attS));
        else if (! v.fullLen && v.t > v.attS + v.holdS) env = std::exp (-(float) (v.t - v.attS - v.holdS) / v.tau);
        if (noise > 0.0f)
        {
            nrng ^= nrng << 13; nrng ^= nrng >> 17; nrng ^= nrng << 5;
            const float nz = ((float) nrng / 4294967295.0f * 2.0f - 1.0f) * noise * 0.5f;
            xl += nz; xr += nz;
        }
        xl *= env; xr *= env;
        if (shaping && (std::abs (shp) > 0.01f || sm > 0.01f || sat > 0.01f))
        {
            for (int c = 0; c < 2; ++c)
            {
                float& x = c == 0 ? xl : xr;
                if (std::abs (shp) > 0.01f) x = std::copysign (std::pow (std::abs (x), pw), x);
                if (sm > 0.01f) { x = x * (1.0f - sm) + v.prevShape[c] * sm; v.prevShape[c] = x; }
                if (sat > 0.01f) x = std::tanh (x * drive) * satNorm;
            }
        }
        if (tieSat > 0.01f) { const float d = 1.0f + tieSat * 10.0f; xl = std::tanh (xl * d) / std::tanh (d); xr = std::tanh (xr * d) / std::tanh (d); }
        if (useFilter) { xl = v.flt[0].process (xl, fmode); xr = v.flt[1].process (xr, fmode); }
        if (lowcut)    { xl = v.hp[0].process (xl, 2);      xr = v.hp[1].process (xr, 2); }
        if (v.fading)
        {
            v.fade *= 0.993f;
            xl *= v.fade; xr *= v.fade;
            if (v.fade < 1.0e-3f) { v.active = false; break; }
        }
        dl[i] += xl * gl;
        dr[i] += xr * gr;
        v.pos += rate;
        v.t += 1.0 / sr;
        if (! v.fullLen && env < 1.0e-4f && v.t > v.attS + v.holdS) { v.active = false; break; }
    }
}

void LimeEngine::renderPulp (const float* src, float* dl, float* dr, int n)
{
    const auto& k = vk[(size_t) pulp];
    if (G (k[kOn], 1) < 0.5f) return;
    const float lo = G (k[kTrkLo], 40), hi = G (k[kTrkHi], 800);
    const float hpA = (float) std::exp (-2.0 * kPi * lo / sr), lpA = (float) (1.0 - std::exp (-2.0 * kPi * hi / sr));
    const float tune = G (k[kTune], 0), dcy = decayTau (G (k[kDcy], 40));
    const float portC = (float) (1.0 - std::exp (-1.0 / (sr * (0.002 + std::pow (G (k[kPort], 0) * 0.01, 2.0) * 0.5))));
    const float osc = G (k[kOsc], 0) * 0.01f;
    const float combMix = G (k[kComb], 0) * 0.01f, combFb = G (k[kFb], 50) * 0.01f * 0.97f;
    const float combFreq = G (k[kFreq], 220), combFm = G (k[kFm], 0) * 0.01f, combCut = G (k[kCut], 8000);
    const float gain = juce::Decibels::decibelsToGain (G (k[kGain], 0), -60.0f);
    const float panN = (G (k[kPan], 0) * 0.01f + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
    const float gl = std::cos (panN) * 1.41421356f * gain, gr = std::sin (panN) * 1.41421356f * gain;
    pulpV.combLp.set (combCut, sr, 0.7);

    for (int i = 0; i < n; ++i)
    {
        // ---- pitch tracking (band-limited zero crossings)
        const float in = src[i];
        pulpV.hpS = hpA * (pulpV.hpS + in - pulpV.hpPrev); pulpV.hpPrev = in;
        pulpV.lpS += lpA * (pulpV.hpS - pulpV.lpS);
        ++pulpV.sinceCross;
        if (pulpV.lastIn <= 0.0f && pulpV.lpS > 0.0f && std::abs (pulpV.lpS) > 1.0e-4f)
        {
            const float f = (float) sr / (float) jmax (1, pulpV.sinceCross);
            if (f >= lo && f <= hi) pulpV.tracked += 0.3f * (f - pulpV.tracked);
            pulpV.sinceCross = 0;
        }
        pulpV.lastIn = pulpV.lpS;
        pulpV.freqTarget = pulpV.tracked * std::pow (2.0f, tune / 12.0f);
        pulpV.freq += (pulpV.freqTarget - pulpV.freq) * portC;

        float y = 0.0f;
        if (pulpV.active)
        {
            pulpV.phase += pulpV.freq / sr; if (pulpV.phase >= 1.0) pulpV.phase -= 1.0;
            const float sine = (float) std::sin (2.0 * kPi * pulpV.phase);
            pulpV.rng ^= pulpV.rng << 13; pulpV.rng ^= pulpV.rng >> 17; pulpV.rng ^= pulpV.rng << 5;
            const float nz = (float) pulpV.rng / 4294967295.0f * 2.0f - 1.0f;
            const float s = osc < 0.5f ? sine * (1.0f - osc * 2.0f) + in * 3.0f * (osc * 2.0f)
                                       : in * 3.0f * (2.0f - osc * 2.0f) + nz * (osc * 2.0f - 1.0f);
            const float env = std::exp (-(float) pulpV.t / dcy) * jmin (1.0f, (float) pulpV.t * 2000.0f);
            y = s * env * pulpV.vel;
            pulpV.t += 1.0 / sr;
            if (env < 1.0e-4f && pulpV.t > 0.01) pulpV.active = false;
        }
        // ---- comb filter (frequency follows the tracked pitch by FM amount)
        if (combMix > 0.001f)
        {
            const float cf = combFreq * std::pow (2.0f, combFm * std::log2 (jmax (20.0f, pulpV.tracked) / 220.0f));
            const float d = jlimit (2.0f, (float) pulpV.comb.size - 4.0f, (float) sr / jmax (20.0f, cf));
            const float c = pulpV.comb.readL (d);
            pulpV.comb.push (y + pulpV.combLp.process (c, 0) * combFb, 0.0f);
            y = y * (1.0f - combMix) + c * combMix;
        }
        dl[i] += y * gl;
        dr[i] += y * gr;
    }
}

//==============================================================================
void LimeEngine::process (float* L, float* R, int n, const BlockTime& bt, const std::vector<TriggerEvent>& midi)
{
    if ((int) dryL.size() < n) for (auto* b : { &dryL, &dryR, &tieL, &tieR, &sendL, &sendR, &dlyL, &dlyR, &xL, &xR, &srcMono }) b->assign ((size_t) n, 0.0f);
    for (auto* b : { &dryL, &dryR, &tieL, &tieR, &srcMono }) std::fill (b->begin(), b->begin() + n, 0.0f);

    struct Ev { int offset; int target; float vel; float value; bool isMod; int forced = -1; };
    std::array<Ev, 1024> ev;
    int nev = 0;
    auto push = [&] (Ev e) { if (nev < (int) ev.size()) ev[(size_t) nev++] = e; };
    for (auto& e : midi) push ({ e.offset, e.target, e.velocity, 0.5f + 0.5f * e.velocity, false });
    for (int t = 0; t < numTargets; ++t)
        if (const int a = auditionQ[(size_t) t].exchange (0); a > 0) push ({ 0, t, (float) a / 1000.0f, 0.75f, false });
    {
        static const int targetOfModule[] { tKick, tSnare, tCHat, tTie, tPod };
        for (int m = 0; m < 5; ++m)
            if (const int idx = auditionIdx[(size_t) m].exchange (-1); idx >= 0) push ({ 0, targetOfModule[m], 1.0f, 0.75f, false, idx });
    }

    const int wanted = jlimit (0, NP - 1, (int) p (bPattern)->load());
    const int mainLen = jlimit (1, NS, (int) p (bLen)->load());
    const int startStep = jlimit (1, NS, (int) p (bStart)->load()) - 1;
    const bool half = p (bHalf)->load() > 0.5f;
    const double stepPpq = half ? 0.5 : 0.25;
    const double swing = p (bSwing)->load() * 0.01;
    const bool chainOn = p (bChainOn)->load() > 0.5f;
    const int chainLen = jlimit (1, 8, (int) p (bChainLen)->load());
    static const int rateBars[] { 0, 2, 4, 8, 12, 16 };
    const int remixRate = rateBars[jlimit (0, 5, (int) p (bRemixRate)->load())];
    const int shift = (int) p (bShift)->load();
    auto accVel = [this] (int voice, uint8_t f) { const float a = G (vk[(size_t) voice][kAcc], 50) * 0.01f; return f >= 2 ? 1.0f : 1.0f - 0.6f * a; };

    if (! bt.playing)
    {
        currentPattern.store (chainOn ? (int) p (bChainSlot (0))->load() : wanted);
        for (auto& s : laneSteps) s.store (-1);
        lastLoop = -1; remixing.store (false); remixThisLoop = false;
    }
    else
    {
        forEachOnset (bt, stepPpq, swing, [&] (long long k, double ppq)
        {
            const long long loop = k / mainLen;
            if (loop != lastLoop || bt.justStarted)
            {
                lastLoop = loop;
                if (chainOn)
                {
                    const int ci = (int) (loop % chainLen);
                    chainIndex.store (ci);
                    currentPattern.store ((int) p (bChainSlot (ci))->load());
                }
                else currentPattern.store (wanted);
                if (remixNowFlag.exchange (false)) nowPending = true;
                remixThisLoop = nowPending || (remixRate > 0 && ((loop + 1) % remixRate) == 0);
                nowPending = false;
                remixing.store (remixThisLoop);
            }
            int pos = (int) (k % mainLen);
            pos = ((pos + startStep + shift) % mainLen + mainLen) % mainLen;
            const int step = remap (pos, mainLen);
            remixPos = remixThisLoop ? (float) (step - pos) / 16.0f : 0.0f;
            laneSteps[0].store (step);
            auto& pat = patterns[(size_t) currentPattern.load()];
            const int off = bt.toSample (ppq);
            static const int voiceOf[] { kick, snare, hat, hat };
            static const int targetOf[] { tKick, tSnare, tCHat, tOHat };
            for (int l = 0; l < numTrigLanes; ++l)
            {
                const uint8_t f = pat.trig[(size_t) l][(size_t) step].load();
                if (f) push ({ off, targetOf[l], accVel (voiceOf[l], f), 0.0f, false });
            }
        });

        auto lane = [&] (int laneIdx, const char* lenId, const char* strId, int uiIdx)
        {
            const int len = jlimit (1, NS, (int) p (lenId)->load());
            const bool stretch = p (strId)->load() > 0.5f;
            const double sp = stretch ? stepPpq * (double) mainLen / (double) len : stepPpq;
            forEachOnset (bt, sp, stretch ? 0.0 : swing, [&] (long long k, double ppq)
            {
                const int step = (int) (k % len);
                laneSteps[(size_t) uiIdx].store (step);
                const float v = patterns[(size_t) currentPattern.load()].value[(size_t) laneIdx][(size_t) step].load();
                const int off = bt.toSample (ppq);
                if (laneIdx == laneMod) push ({ off, -1, 0.0f, v, true });
                else if (v > 0.001f)
                {
                    push ({ off, laneIdx == laneTie ? tTie : tPod, 1.0f, v, false });
                }
            });
        };
        lane (laneTie, bTieLen, bTieStr, 1);
        lane (lanePod, bPodLen, bPodStr, 2);
        lane (laneMod, bModLen, bModStr, 3);
    }

    std::stable_sort (ev.begin(), ev.begin() + nev, [] (const Ev& a, const Ev& b2)
    {
        return a.offset != b2.offset ? a.offset < b2.offset : (a.isMod && ! b2.isMod);
    });

    // ---------------- voices
    int pos = 0, ei = 0;
    while (pos < n)
    {
        while (ei < nev && ev[(size_t) ei].offset <= pos)
        {
            const auto& e = ev[(size_t) ei++];
            if (e.isMod) { modHeld = e.value; modNow.store (e.value); }
            else trigger (e.target, e.vel, e.value, e.forced);
        }
        const int next = ei < nev ? jmin (n, ev[(size_t) ei].offset) : n;
        const int cnt = jmax (1, next - pos);
        const int srcSel = (int) G (vk[(size_t) pulp][kSrc], 3);      // 0..4 = Kick..Pod, 5 = All
        for (int vIdx = 0; vIdx < 5; ++vIdx)
        {
            const bool isSrc = srcSel == 5 || srcSel == vIdx;
            if (isSrc)
            {
                std::fill (tieL.begin() + pos, tieL.begin() + pos + cnt, 0.0f);
                std::fill (tieR.begin() + pos, tieR.begin() + pos + cnt, 0.0f);
            }
            for (auto& v : voices[(size_t) vIdx])
            {
                if (isSrc) renderSampleVoice (vIdx, v, tieL.data() + pos, tieR.data() + pos, cnt);
                else       renderSampleVoice (vIdx, v, dryL.data() + pos, dryR.data() + pos, cnt);
            }
            if (isSrc)
                for (int i = pos; i < pos + cnt; ++i)
                {
                    dryL[(size_t) i] += tieL[(size_t) i]; dryR[(size_t) i] += tieR[(size_t) i];
                    srcMono[(size_t) i] += 0.5f * (tieL[(size_t) i] + tieR[(size_t) i]);
                }
        }
        pos += cnt;
    }
    // Pulp: tracks / processes the selected input source (Kick, Snare, Hat, Tie, Pod or All) scaled by Input Gain
    {
        const float inGain = juce::Decibels::decibelsToGain (G (vk[(size_t) pulp][kIn], 0), -60.0f);
        for (int i = 0; i < n; ++i) srcMono[(size_t) i] *= inGain;
        renderPulp (srcMono.data(), dryL.data(), dryR.data(), n);
    }

    // ---------------- FX chain
    FxContext c; c.sr = sr; c.bpm = bt.bpm; c.ppq = bt.ppqStart; c.ppqPerSample = bt.ppqPerSample; c.playing = bt.playing;
    float* dl = dryL.data(); float* dr = dryR.data();
    envFilter.process (dl, dr, n, p (bFltCut)->load(), p (bFltTrk)->load(), p (bFltSm)->load(), p (bFltMod)->load(), modHeld);
    eq.process (dl, dr, n, p (bEqHi)->load(), p (bEqMid)->load(), p (bEqLo)->load(), p (bEqAmp)->load(), p (bEqSplit)->load());
    const float magSemis = jlimit (-24.0f, 24.0f, p (bMagPitch)->load() + p (bMagMod)->load() * 0.01f * modHeld * 12.0f
                                                 + p (bMagRmx)->load() * 0.01f * remixPos * 24.0f);
    magic.process (dl, dr, n, magSemis, p (bMagGrain)->load(), p (bMagMix)->load() * 0.01f, p (bMagNotch)->load());

    const bool modIn = p (bModInput)->load() > 0.5f, modMixOn = p (bModMix)->load() > 0.5f;
    const float sendGain = modIn ? modHeld : 1.0f;
    for (int i = 0; i < n; ++i) { sendL[(size_t) i] = dl[i] * sendGain; sendR[(size_t) i] = dr[i] * sendGain; }
    delay.process (sendL.data(), sendR.data(), dlyL.data(), dlyR.data(), n, c,
                   (int) p (bDlyOne)->load(), (int) p (bDlyTwo)->load(), p (bDlyFb)->load(), p (bDlyLfo)->load(),
                   p (bDlyFM)->load(), p (bDlyDM)->load(), p (bDlyWide)->load(), p (bDlyCut)->load(), p (bDlyRes)->load(),
                   p (bDlyLo)->load() > 0.5f);
    const float xDly = p (bXDly)->load() * 0.01f;
    for (int i = 0; i < n; ++i)
    {
        xL[(size_t) i] = sendL[(size_t) i] * (1.0f - xDly) + dlyL[(size_t) i] * xDly;
        xR[(size_t) i] = sendR[(size_t) i] * (1.0f - xDly) + dlyR[(size_t) i] * xDly;
    }
    // ---- XY motion recorder: loop of LEN sixteenths, SPD = speed, SM = interpolation
    float xyX = p (bXParam)->load() * 0.01f, xyY = p (bXAdd)->load() * 0.01f;
    {
        const double speed = std::pow (2.0, (p (bXSpd)->load() - 50.0) / 25.0);
        const double loopBeats = jlimit (1.0, 64.0, (double) p (bXLen)->load()) / 4.0;
        const double ppqNow = bt.playing ? bt.ppqStart : xyFreePpq;
        if (! bt.playing) xyFreePpq += bt.ppqPerSample * n;
        double pos = std::fmod (ppqNow * speed, loopBeats) / loopBeats;
        if (pos < 0) pos += 1.0;
        xyPos.store ((float) pos);
        const int idx = jlimit (0, xyN - 1, (int) (pos * xyN));
        const bool rec = p (bXRec)->load() > 0.5f, play = p (bXPlay)->load() > 0.5f;
        const bool touching = xyTouching.load();
        if (xyClearReq.exchange (false)) xyHas.store (false);
        if (rec && touching)
        {
            const float tx = xyTouchX.load(), ty = xyTouchY.load();
            const int tc = xyTouchCount.load();
            if (tc != xyLastTouchCount)          // new gesture while REC is on: start a fresh recording
            {
                xyLastTouchCount = tc;
                xyBufX.fill (tx); xyBufY.fill (ty);
                xyLastIdx = idx;
            }
            // write every slot passed since the last block (no gaps, handles wrap-around)
            int i = xyLastIdx < 0 ? idx : xyLastIdx;
            for (int guard = 0; guard <= xyN; ++guard)
            {
                xyBufX[(size_t) i] = tx; xyBufY[(size_t) i] = ty;
                if (i == idx) break;
                i = (i + 1) % xyN;
            }
            xyLastIdx = idx;
            xyHas.store (true);
            xyX = tx; xyY = ty;
        }
        else xyLastIdx = -1;
        xyRecNow.store (rec && touching);
        const bool playing = play && xyHas.load() && ! touching;
        xyPlayback.store (playing);
        if (playing)
        {
            const double fpos = pos * xyN;
            const int i0 = (int) fpos % xyN, i1 = (i0 + 1) % xyN;
            const float fr = (float) (fpos - std::floor (fpos));
            const float tx = xyBufX[(size_t) i0] + fr * (xyBufX[(size_t) i1] - xyBufX[(size_t) i0]);
            const float ty = xyBufY[(size_t) i0] + fr * (xyBufY[(size_t) i1] - xyBufY[(size_t) i0]);
            const float smc = (float) std::exp (-(double) n / (sr * (0.002 + p (bXSm)->load() * 0.01 * 0.25)));
            xySmX = tx + (xySmX - tx) * smc;
            xySmY = ty + (xySmY - ty) * smc;
            xyX = xySmX; xyY = xySmY;
        }
        else { xySmX = xyX; xySmY = xyY; }
        xyCurX.store (xyX); xyCurY.store (xyY);
    }
    xfx.process (xL.data(), xR.data(), n, c, (int) p (bXMode)->load(), xyX * 100.0f, xyY * 100.0f, p (bXSpd)->load(),
                 (int) p (bXLen)->load(), p (bXLo)->load(), p (bXHi)->load(), true, true, p (bXSm)->load());

    const float dlyGain = juce::Decibels::decibelsToGain (p (bDlyGain)->load(), -60.0f);
    const float xGain = juce::Decibels::decibelsToGain (p (bXGain)->load(), -60.0f);
    float fxMix = p (bFxMix)->load() * 0.01f;
    if (modMixOn) fxMix *= modHeld;
    const float dryAmt = jmin (1.0f, 2.0f - 2.0f * fxMix), wetAmt = jmin (1.0f, 2.0f * fxMix);
    const bool norm = p (bNorm)->load() > 0.5f;
    const float nAmp = p (bNormAmp)->load() * 0.01f, nRel = p (bNormRel)->load() * 0.01f, nSm = p (bNormSm)->load() * 0.01f;
    const float relC = (float) std::exp (-1.0 / (sr * (0.02 + nRel * 1.5)));
    // fixed -8 dB of section headroom (6 voices + FX summing), then OUT Gain (= Level B)
    constexpr float headroom = 0.4f;   // fixed -8 dB section headroom, applied BEFORE Dirt so Dirt sees output-level signals
    const float gain = juce::Decibels::decibelsToGain (p (bGain)->load(), -60.0f);
    const float target = 0.25f + 0.6f * nAmp;
    for (int i = 0; i < n; ++i)
    {
        dl[i] = dl[i] * dryAmt + (dlyL[(size_t) i] * dlyGain + xL[(size_t) i] * xGain) * wetAmt;
        dr[i] = dr[i] * dryAmt + (dlyR[(size_t) i] * dlyGain + xR[(size_t) i] * xGain) * wetAmt;
    }
    for (int i = 0; i < n; ++i) { dl[i] *= headroom; dr[i] *= headroom; }
    dirtSat.process (dl, dr, n, p (bDirt)->load() * 0.01f);     // DIRT: gentle saturation
    for (int i = 0; i < n; ++i)
    {
        float l = dl[i], r = dr[i];
        if (norm)
        {
            l /= headroom; r /= headroom;   // Norm keeps working on the pre-headroom scale
            const float pk = jmax (std::abs (l), std::abs (r));
            normEnv = pk > normEnv ? normEnv + (0.5f - 0.45f * nSm) * (pk - normEnv) : normEnv * relC;
            const float g = normEnv > target ? target / normEnv : 1.0f;
            l = std::tanh (l * g * (1.0f + nAmp * 1.5f)) * 0.95f * headroom;
            r = std::tanh (r * g * (1.0f + nAmp * 1.5f)) * 0.95f * headroom;
        }
        L[i] += l * gain;
        R[i] += r * gain;
    }
}

//==============================================================================
void LimeEngine::loadDefaultPatterns()
{
    for (auto& pt : patterns) pt.clear();
    auto set = [this] (int pat, int lane, const char* s)
    {
        for (int i = 0; i < NS && s[i]; ++i)
            patterns[(size_t) pat].trig[(size_t) lane][(size_t) i].store ((uint8_t) (s[i] == 'X' ? 2 : (s[i] == 'x' ? 1 : 0)));
    };
    auto vals = [this] (int pat, int lane, std::initializer_list<float> v)
    {
        int i = 0; for (float x : v) if (i < NS) patterns[(size_t) pat].value[(size_t) lane][(size_t) i++].store (x);
    };
    set (0, laneKick,  "X---x---X---x---");
    set (0, laneSnare, "----X-------X--x");
    set (0, laneCHat,  "x-xxx-x-x-xxx-x-");
    set (0, laneOHat,  "--x---x---x---x-");
    vals (0, laneTie, { 0, 0, .5f, 0, 0, 0, 0, .7f, 0, 0, .5f, 0, 0, .62f, 0, 0 });
    vals (0, lanePod, { 0, 0, 0, 0, 0, 0, .8f, 0, 0, 0, 0, 0, 0, 0, .5f, 0 });
    vals (0, laneMod, { .1f, .3f, .5f, .7f, .9f, .7f, .5f, .3f, .1f, .3f, .5f, .7f, .9f, .7f, .5f, .3f });
    for (int p2 = 1; p2 < NP; ++p2) patterns[(size_t) p2].copyFrom (patterns[0]);
    set (1, laneKick, "X--x--x---X--x--");
    set (2, laneCHat, "xxxxxxxxxxxxxxxx");
    set (3, laneSnare, "----X--x-x--X-xX");
}

void LimeEngine::xyTouch (float x, float y, bool down)
{
    xyTouchX.store (jlimit (0.0f, 1.0f, x));
    xyTouchY.store (jlimit (0.0f, 1.0f, y));
    if (down && ! xyTouching.load()) xyTouchCount.fetch_add (1);
    xyTouching.store (down);
}

juce::ValueTree LimeEngine::saveState() const
{
    juce::ValueTree vt ("LIME_PATTERNS");
    vt.setProperty ("version", 2, nullptr);
    if (xyHas.load())
    {
        juce::MemoryBlock mb;
        mb.append (xyBufX.data(), sizeof (float) * xyN);
        mb.append (xyBufY.data(), sizeof (float) * xyN);
        vt.setProperty ("xymotion", mb.toBase64Encoding(), nullptr);
    }
    for (int pi = 0; pi < NP; ++pi)
    {
        juce::ValueTree pt ("P");
        juce::String s;
        for (int l = 0; l < numTrigLanes; ++l)
            for (int st = 0; st < NS; ++st) s << juce::String::charToString ((juce::juce_wchar) ('0' + patterns[(size_t) pi].trig[(size_t) l][(size_t) st].load()));
        pt.setProperty ("trig", s, nullptr);
        for (int l = 0; l < numValueLanes; ++l)
        {
            juce::StringArray v;
            for (int st = 0; st < NS; ++st) v.add (juce::String (patterns[(size_t) pi].value[(size_t) l][(size_t) st].load(), 3));
            pt.setProperty ("v" + juce::String (l), v.joinIntoString (","), nullptr);
        }
        vt.appendChild (pt, nullptr);
    }
    return vt;
}

void LimeEngine::loadState (const juce::ValueTree& vt)
{
    if (! vt.isValid() || (int) vt.getProperty ("version", 1) < 2) return;
    {
        juce::MemoryBlock mb;
        if (mb.fromBase64Encoding (vt.getProperty ("xymotion").toString()) && mb.getSize() == sizeof (float) * xyN * 2)
        {
            memcpy (xyBufX.data(), mb.getData(), sizeof (float) * xyN);
            memcpy (xyBufY.data(), static_cast<const char*> (mb.getData()) + sizeof (float) * xyN, sizeof (float) * xyN);
            xyHas.store (true);
        }
        else xyHas.store (false);
    }
    for (int pi = 0; pi < NP && pi < vt.getNumChildren(); ++pi)
    {
        auto pt = vt.getChild (pi);
        const juce::String s = pt.getProperty ("trig").toString();
        for (int l = 0; l < numTrigLanes; ++l)
            for (int st = 0; st < NS; ++st)
            {
                const int idx = l * NS + st;
                patterns[(size_t) pi].trig[(size_t) l][(size_t) st].store ((uint8_t) (idx < s.length() ? jlimit (0, 2, (int) (s[idx] - '0')) : 0));
            }
        for (int l = 0; l < numValueLanes; ++l)
        {
            juce::StringArray v; v.addTokens (pt.getProperty ("v" + juce::String (l)).toString(), ",", {});
            for (int st = 0; st < NS; ++st)
                patterns[(size_t) pi].value[(size_t) l][(size_t) st].store (st < v.size() ? jlimit (0.0f, 1.0f, v[st].getFloatValue()) : 0.0f);
        }
    }
}
