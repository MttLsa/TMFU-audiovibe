#pragma once
#include "SampleLibrary.h"
#include <juce_core/juce_core.h>
#include <functional>
#include <cmath>

// -------------------------------------------------------------------------------------------------
//  Belt pitch engine (as in Groovulation): the transposition that brings a sample into the SCALE is done with
//  Signalsmith Stretch (high-quality polyphonic pitch shift, duration and attack position unchanged) instead of
//  re-sampling. A background thread renders, for every sample of the Belt folder, a copy shifted by the scale
//  correction; the audio thread picks it up when it matches (otherwise it plays the original, re-sampled).
//  Tune, melodies (VEL = tune), portamento and FM still act on top of it as a classic sampler.
// -------------------------------------------------------------------------------------------------
class BeltStretch : private juce::Thread
{
public:
    // returns the wanted correction in semitones for a sample, or NaN when no shift is needed (SCALE off, atonal)
    using CorrectionFn = std::function<float (const Sample&)>;

    explicit BeltStretch (SampleLibrary& lib);
    ~BeltStretch() override;

    void setCorrectionFunction (CorrectionFn fn);   // call before start()
    void start();

    // audio thread: rendered copy of sample 'index' of 'set' shifted by 'semis', or nullptr if not (yet) available
    SamplePtr lookup (const SampleSet* set, int index, float semis) const;

    bool idle() const { return busy.load() == 0; }   // tests: nothing left to render
    static SamplePtr render (const Sample& s, float semis);   // offline Signalsmith Stretch pitch shift

private:
    void run() override;
    struct Item { float semis; SamplePtr smp; };
    struct Table
    {
        const SampleSet* set = nullptr;
        SampleSetPtr keep;                                     // keeps 'set' alive while the table exists
        mutable std::vector<std::shared_ptr<const Item>> items;
    };
    SampleLibrary& library;
    CorrectionFn correction;
    std::shared_ptr<const Table> table;
    std::atomic<int> busy { 1 };
};
