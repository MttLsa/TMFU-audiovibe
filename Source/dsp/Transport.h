#pragma once
#include <cmath>
#include <functional>

// Musical time for one audio block. Positions are in quarter notes (PPQ).
struct BlockTime
{
    bool   playing = false;
    bool   justStarted = false;   // first block after a (re)start
    double ppqStart = 0.0;        // ppq at sample 0 of the block
    double ppqPerSample = 0.0;
    double bpm = 120.0;
    int    numSamples = 0;

    double ppqEnd() const noexcept { return ppqStart + ppqPerSample * numSamples; }

    // ppq -> sample offset inside the block (clamped)
    int toSample (double ppq) const noexcept
    {
        if (ppqPerSample <= 0.0) return 0;
        const int s = (int) std::floor ((ppq - ppqStart) / ppqPerSample);
        return s < 0 ? 0 : (s >= numSamples ? numSamples - 1 : s);
    }
};

// Enumerates grid onsets of a lane inside the block.
//   stepPpq : length of one lane step in ppq (0.25 = sixteenth)
//   swing   : 0..1, delays every odd step by swing * 0.5 * stepPpq
//   cb(globalStepIndex, onsetPpq)
inline void forEachOnset (const BlockTime& bt, double stepPpq, double swing,
                          const std::function<void (long long, double)>& cb)
{
    if (! bt.playing || stepPpq <= 0.0) return;
    const double maxShift = swing * 0.5 * stepPpq;
    const double a = bt.ppqStart, e = bt.ppqEnd();
    long long first = (long long) std::floor ((a - maxShift) / stepPpq) - 1;
    for (long long k = first < 0 ? 0 : first; ; ++k)
    {
        double t = (double) k * stepPpq;
        if (t > e) break;
        if (k & 1) t += maxShift;
        const bool include = bt.justStarted ? (t >= a - 1.0e-9 && t < e) : (t >= a && t < e);
        if (include) cb (k, t);
    }
}
