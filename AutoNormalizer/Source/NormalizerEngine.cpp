#include "NormalizerEngine.h"

#include <algorithm>

namespace an
{

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr std::array<double, NormalizerEngine::kNumBands> kBandFreqs { 60, 150, 400, 1000, 2500, 5000, 9000, 14000 };

void makeBandpass (Biquad& f, double fc, double q, double fs)
{
    fc = std::min (fc, fs * 0.45);
    const double w0 = 2.0 * kPi * fc / fs;
    const double alpha = std::sin (w0) / (2.0 * q);
    const double a0 = 1.0 + alpha;
    f.b0 = alpha / a0;
    f.b1 = 0.0;
    f.b2 = -alpha / a0;
    f.a1 = -2.0 * std::cos (w0) / a0;
    f.a2 = (1.0 - alpha) / a0;
    f.reset();
}

// ITU-R BS.1770 K-weighting, valid at any sample rate.
void makeKWeighting (Biquad& pre, Biquad& rlb, double fs)
{
    {
        const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        const double K = std::tan (kPi * f0 / fs);
        const double Vh = std::pow (10.0, G / 20.0);
        const double Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        pre.b0 = (Vh + Vb * K / Q + K * K) / a0;
        pre.b1 = 2.0 * (K * K - Vh) / a0;
        pre.b2 = (Vh - Vb * K / Q + K * K) / a0;
        pre.a1 = 2.0 * (K * K - 1.0) / a0;
        pre.a2 = (1.0 - K / Q + K * K) / a0;
        pre.reset();
    }
    {
        const double f0 = 38.13547087602444, Q = 0.5003270373238773;
        const double K = std::tan (kPi * f0 / fs);
        const double a0 = 1.0 + K / Q + K * K;
        rlb.b0 = 1.0;
        rlb.b1 = -2.0;
        rlb.b2 = 1.0;
        rlb.a1 = 2.0 * (K * K - 1.0) / a0;
        rlb.a2 = (1.0 - K / Q + K * K) / a0;
        rlb.reset();
    }
}
} // namespace

void NormalizerEngine::prepare (double sampleRate, int numChannels)
{
    sr = sampleRate > 0 ? sampleRate : 48000.0;
    numCh = std::clamp (numChannels, 1, kMaxChannels);

    for (int b = 0; b < kNumBands; ++b)
        makeBandpass (bands[(size_t) b], kBandFreqs[(size_t) b], 1.2, sr);
    for (int c = 0; c < kMaxChannels; ++c)
        makeKWeighting (kPre[(size_t) c], kRlb[(size_t) c], sr);

    hop = std::max (64, (int) std::lround (sr * 0.05));
    frameSec = hop / sr;
    frameN = 0;
    frameBand.fill (0);
    frameMonoSq = frameSq = frameKSq = 0;
    framePeak = 0;
    emaAlpha = 1.0 - std::exp (-frameSec / 0.4); // ~400 ms detector memory
    smoothCoef = (float) (1.0 - std::exp (-1.0 / (0.02 * sr))); // 20 ms gain ramp

    gainTargetLin = gainSmoothed = lastApplied = dbToLin (gainDb);

    // limiter
    ceilingLin = dbToLin (kCeilingDb);
    limLen = std::max (2, (int) std::lround (sr * kLookaheadSec));
    limDelay.assign ((size_t) (kMaxChannels * (limLen - 1)), 0.0f);
    limReq.assign ((size_t) limLen, 1.0f);
    limBox.assign ((size_t) limLen, 1.0);
    limBoxSum = (double) limLen;
    limPos = 0;
    limHold = 1.0f;
    limRelease = (float) (1.0 - std::exp (-1.0 / (0.08 * sr))); // 80 ms release

    if (snapshotPending.exchange (false))
        applySnapshot (pendingSnapshot);
}

void NormalizerEngine::requestNormalize (TriggerSource src) noexcept
{
    triggerRequest.store ((int) src);
}

void NormalizerEngine::startArming (TriggerSource src) noexcept
{
    state = State::WaitingForSignal;
    pendingSource = src;
    measActive = measSq = measKSq = measSamples = measMonoSq = 0;
    measPeak = 0;
    peakSinceArm = 0;
    measBand.fill (0);
    overTime = 0;
    // Drop the partially filled analysis frame: it still contains audio of the previous preset.
    frameN = 0;
    frameBand.fill (0);
    frameMonoSq = frameSq = frameKSq = 0;
    framePeak = 0;
    // After an external trigger (MIDI / manual) give the previous plugin 100 ms to switch preset.
    settle = (src == TriggerSource::Midi || src == TriggerSource::Manual) ? 0.1 : 0.0;
    uiProgress.store (0);
    uiState.store ((int) state);
}

void NormalizerEngine::applySnapshot (const Snapshot& s) noexcept
{
    normalizedOnce = s.normalizedOnce;
    gainDb = s.gainDb;
    gainTargetLin = gainSmoothed = lastApplied = dbToLin (gainDb);
    hasReference = s.hasReference;
    refShape = s.refShape;
    refLevelDb = s.refLevelDb;
    emaValid = false;
    overTime = 0;
    cooldown = 0.5;
    state = normalizedOnce ? State::Locked : State::WaitingForSignal;
    if (! normalizedOnce)
        startArming (TriggerSource::Initial);

    uiState.store ((int) state);
    uiGainDb.store (gainDb);
    pubNormalizedOnce.store (normalizedOnce);
    pubHasRef.store (hasReference);
    pubRefLevel.store (refLevelDb);
    for (int b = 0; b < kNumBands; ++b)
        pubRefShape[(size_t) b].store (refShape[(size_t) b]);
}

NormalizerEngine::Snapshot NormalizerEngine::getSnapshot() const noexcept
{
    Snapshot s;
    s.normalizedOnce = pubNormalizedOnce.load();
    s.gainDb = uiGainDb.load();
    s.hasReference = pubHasRef.load();
    s.refLevelDb = pubRefLevel.load();
    for (int b = 0; b < kNumBands; ++b)
        s.refShape[(size_t) b] = pubRefShape[(size_t) b].load();
    return s;
}

void NormalizerEngine::restoreSnapshot (const Snapshot& s) noexcept
{
    pendingSnapshot = s;
    snapshotPending.store (true);
}

void NormalizerEngine::computeShape (const double* e, float* shape) const noexcept
{
    std::array<float, kNumBands> db {};
    float mx = -300.0f;
    for (int b = 0; b < kNumBands; ++b)
    {
        db[(size_t) b] = powToDb (e[b] + 1.0e-14);
        mx = std::max (mx, db[(size_t) b]);
    }
    float mean = 0;
    for (int b = 0; b < kNumBands; ++b)
    {
        db[(size_t) b] = std::max (db[(size_t) b], mx - 30.0f); // ignore near-empty bands
        mean += db[(size_t) b];
    }
    mean /= (float) kNumBands;
    for (int b = 0; b < kNumBands; ++b)
        shape[b] = db[(size_t) b] - mean;
}

void NormalizerEngine::finishMeasurement() noexcept
{
    // Loudness (ITU-R BS.1770): K-weighted mean square, summed over channels.
    const float measured = -0.691f + powToDb (measKSq / std::max (1.0, measSamples));

    const float newGain = std::clamp (settings.targetLufs - measured, -settings.maxCutDb, settings.maxBoostDb);

    // Continue smoothly from whatever gain was actually being applied (it may have been
    // reduced by the safety ceiling) so that locking never causes a jump.
    gainSmoothed = lastApplied;
    gainDb = newGain;
    gainTargetLin = dbToLin (newGain);
    normalizedOnce = true;
    state = State::Locked;

    // Capture the fingerprint of the new preset for automatic change detection.
    if (measSamples > 0)
    {
        computeShape (measBand.data(), refShape.data());
        refLevelDb = powToDb (measMonoSq / measSamples);
        hasReference = true;
        for (int b = 0; b < kNumBands; ++b)
            bandEma[(size_t) b] = measBand[(size_t) b] / measSamples;
        levelEma = measMonoSq / measSamples;
        emaValid = true;
    }
    overTime = 0;
    cooldown = 1.0; // seconds of active signal before auto-detect is allowed again

    uiMeasuredDb.store (measured);
    uiMeasuredPeak.store (linToDb (measPeak));
    uiGainDb.store (gainDb);
    uiState.store ((int) state);
    uiProgress.store (1.0f);
    uiLastTrigger.store ((int) pendingSource);
    uiCount.fetch_add (1);
    pubNormalizedOnce.store (true);
    pubHasRef.store (hasReference);
    pubRefLevel.store (refLevelDb);
    for (int b = 0; b < kNumBands; ++b)
        pubRefShape[(size_t) b].store (refShape[(size_t) b]);
}

void NormalizerEngine::endFrame() noexcept
{
    const float gateLin = dbToLin (settings.gateDb);
    const bool active = framePeak > gateLin;
    const double n = (double) frameN;

    // input meter (peak hold with decay)
    inputPeakHold = std::max (framePeak, inputPeakHold * 0.85f);
    uiInputDb.store (linToDb (inputPeakHold));
    uiOutDb.store (linToDb (outputPeakHold));
    outputPeakHold *= 0.85f;
    uiGrDb.store (std::min (0.0f, linToDb (grHold)));
    grHold = 1.0f;

    const bool settling = settle > 0 && state != State::Locked;
    if (settling)
    {
        settle -= frameSec;
    }
    else if (state == State::WaitingForSignal && active)
    {
        state = State::Measuring;
        uiState.store ((int) state);
    }

    if (settling)
    {
        // still settling: nothing is measured
    }
    else if (state == State::Measuring && active)
    {
        measActive += frameSec;
        measSq += frameSq;
        measKSq += frameKSq;
        measMonoSq += frameMonoSq;
        measSamples += n;
        measPeak = std::max (measPeak, framePeak);
        for (int b = 0; b < kNumBands; ++b)
            measBand[(size_t) b] += frameBand[(size_t) b];

        uiProgress.store ((float) std::min (1.0, measActive / std::max (0.05f, settings.measureSeconds)));
        if (measActive >= settings.measureSeconds)
            finishMeasurement();
    }
    else if (state == State::Locked && active)
    {
        // ---- automatic preset-change detection: spectral fingerprint + level
        for (int b = 0; b < kNumBands; ++b)
        {
            const double e = frameBand[(size_t) b] / n;
            bandEma[(size_t) b] = emaValid ? bandEma[(size_t) b] + emaAlpha * (e - bandEma[(size_t) b]) : e;
        }
        levelEma = emaValid ? levelEma + emaAlpha * (frameMonoSq / n - levelEma) : frameMonoSq / n;
        emaValid = true;

        if (cooldown > 0)
            cooldown -= frameSec;

        float score = 0;
        if (hasReference)
        {
            std::array<float, kNumBands> cur {};
            computeShape (bandEma.data(), cur.data());
            float dist = 0;
            for (int b = 0; b < kNumBands; ++b)
                dist += std::abs (cur[(size_t) b] - refShape[(size_t) b]);
            dist /= (float) kNumBands;

            const float levelDiff = std::abs (powToDb (levelEma) - refLevelDb);
            const float s = std::clamp (settings.sensitivity, 0.0f, 1.0f);
            const float shapeThr = 10.0f - 7.0f * s;  // 10 dB (low sens) .. 3 dB (high sens)
            const float levelThr = 20.0f - 12.0f * s; // 20 dB .. 8 dB
            score = std::max (dist / shapeThr, levelDiff / levelThr);
        }
        uiScore.store (score);

        if (settings.autoDetect && ! settings.frozen && hasReference && cooldown <= 0)
        {
            if (score >= 1.0f)
                overTime += frameSec;
            else
                overTime = std::max (0.0, overTime - frameSec);

            if (overTime >= 0.3) // change must persist for 300 ms of audio
            {
                startArming (TriggerSource::Auto);
                // the signal is already there: start measuring right away
                state = State::Measuring;
                uiState.store ((int) state);
            }
        }
    }
    else if (! active)
    {
        uiScore.store (0);
    }

    frameN = 0;
    frameBand.fill (0);
    frameMonoSq = frameSq = frameKSq = 0;
    framePeak = 0;
}

void NormalizerEngine::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    if (snapshotPending.exchange (false))
        applySnapshot (pendingSnapshot);

    if (resetRequested.exchange (false))
    {
        gainDb = 0.0f;
        gainTargetLin = 1.0f;
        gainSmoothed = lastApplied;
        state = State::Locked;
        normalizedOnce = true;
        uiGainDb.store (0.0f);
        uiState.store ((int) state);
        pubNormalizedOnce.store (true);
    }

    const int req = triggerRequest.exchange (0);
    if (req != 0 && ! settings.frozen)
        startArming ((TriggerSource) req);

    const int nc = std::clamp (numChannels, 1, kMaxChannels);
    const float invNc = 1.0f / (float) nc;
    const bool measuringPhase = state != State::Locked;

    for (int i = 0; i < numSamples; ++i)
    {
        float mono = 0, pk = 0;
        double sq = 0, ksq = 0;
        for (int c = 0; c < nc; ++c)
        {
            const float x = channels[c][i];
            mono += x;
            pk = std::max (pk, std::abs (x));
            sq += (double) x * x;
            const float k = kRlb[(size_t) c].process (kPre[(size_t) c].process (x));
            ksq += (double) k * k;
        }
        mono *= invNc;
        for (int b = 0; b < kNumBands; ++b)
        {
            const float y = bands[(size_t) b].process (mono);
            frameBand[(size_t) b] += (double) y * y;
        }
        framePeak = std::max (framePeak, pk);
        frameSq += sq;
        frameKSq += ksq;
        frameMonoSq += (double) mono * mono;

        gainSmoothed += (gainTargetLin - gainSmoothed) * smoothCoef;
        float g = gainSmoothed;

        if (measuringPhase && state != State::Locked)
        {
            peakSinceArm = std::max (peakSinceArm, pk);
            if (settings.safetyCeiling && peakSinceArm > 1.0e-6f)
                g = std::min (g, ceilingLin / peakSinceArm); // keep the new preset below the ceiling while (re)measuring
        }
        lastApplied = g;

        for (int c = 0; c < numChannels; ++c)
            channels[c][i] *= g;

        const float gr = limiterProcess (channels, numChannels, i);
        grHold = std::min (grHold, gr);

        if (++frameN >= hop)
            endFrame();
    }

    uiAppliedGainDb.store (linToDb (lastApplied));
}

// Look-ahead brick-wall limiter. Gain path: required gain -> running minimum over the
// look-ahead window -> release smoothing (only upwards) -> box average over the window.
// With the audio delayed by (limLen - 1) samples the applied gain at every sample is <=
// the gain that sample requires, so the output can never exceed the ceiling.
float NormalizerEngine::limiterProcess (float* const* channels, int numChannels, int i) noexcept
{
    const int nc = std::min (numChannels, kMaxChannels);
    const int d = limLen - 1;
    const int dp = limPos % d;

    float mx = 0;
    for (int c = 0; c < nc; ++c)
        mx = std::max (mx, std::abs (channels[c][i]));
    const float req = mx > ceilingLin ? ceilingLin / mx : 1.0f;

    const int rp = limPos % limLen;
    limReq[(size_t) rp] = req;
    float h = 1.0f;
    for (float v : limReq)
        h = std::min (h, v);

    limHold = h < limHold ? h : limHold + limRelease * (h - limHold);

    limBoxSum += (double) limHold - limBox[(size_t) rp];
    limBox[(size_t) rp] = limHold;
    if (rp == 0) // recompute occasionally to avoid floating point drift
    {
        limBoxSum = 0;
        for (double v : limBox)
            limBoxSum += v;
    }
    const float g = std::min (1.0f, (float) (limBoxSum / limLen));

    float outPk = 0;
    for (int c = 0; c < nc; ++c)
    {
        float& slot = limDelay[(size_t) (c * d + dp)];
        const float delayed = slot;
        slot = channels[c][i];
        const float y = std::clamp (delayed * g, -ceilingLin, ceilingLin); // final safety
        channels[c][i] = y;
        outPk = std::max (outPk, std::abs (y));
    }
    outputPeakHold = std::max (outputPeakHold, outPk);
    ++limPos;
    if (limPos >= limLen * d)
        limPos = 0;
    return g;
}

} // namespace an
