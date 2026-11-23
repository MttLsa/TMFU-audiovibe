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

// Look-ahead window for per-track nudge (negative = earlier) without plug-in latency: the sequencer enumerates
// the grid up to `lookPpq` ahead of the block, events are queued at (onset + nudge) and fired when their time comes.
struct LookaheadWindow
{
    double lookEnd = 0.0, expectA = 0.0;
    bool valid = false;

    // Returns the window to enumerate (playing == false when stopped). reset = timeline jump / restart: drop the queue.
    BlockTime next (const BlockTime& bt, double lookPpq, bool& reset)
    {
        BlockTime w = bt;
        reset = false;
        if (! bt.playing || bt.ppqPerSample <= 0.0) { valid = false; w.playing = false; return w; }
        const double tol = 0.004 * bt.bpm / 60.0;                    // 4 ms of tolerance on the host position
        reset = bt.justStarted || ! valid || std::abs (bt.ppqStart - expectA) > tol;
        const double start = reset ? bt.ppqStart : lookEnd;
        const double end = bt.ppqEnd() + lookPpq;
        w.ppqStart = start;
        w.justStarted = reset;
        w.numSamples = end > start ? (int) std::ceil ((end - start) / bt.ppqPerSample) : 0;
        lookEnd = start + w.numSamples * bt.ppqPerSample;
        expectA = bt.ppqEnd();
        valid = true;
        return w;
    }
};

// Fixed-size queue of events waiting for their (nudged) time.
template <typename E, int N = 1024>
struct NudgeQueue
{
    struct Item { double ppq; E e; };
    Item items[N];
    int count = 0;
    void clear() noexcept { count = 0; }
    void add (double ppq, const E& e) noexcept { if (count < N) items[count++] = { ppq, e }; }
    // Moves every event due before `endPpq` to cb(ppq, e).
    template <typename F> void flush (double endPpq, F&& cb)
    {
        for (int i = 0; i < count;)
            if (items[i].ppq < endPpq) { cb (items[i].ppq, items[i].e); items[i] = items[--count]; }
            else ++i;
    }
};

constexpr double kNudgeMaxMs = 50.0;
inline double msToPpq (double ms, double bpm) noexcept { return ms * 0.001 * bpm / 60.0; }
