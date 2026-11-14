#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
#include <vector>

// A sample: mono float data at a source sample rate.
struct Sample
{
    juce::String name;
    juce::String category;
    double sourceRate = 44100.0;
    std::vector<float> data;
};
using SamplePtr = std::shared_ptr<const Sample>;

// Factory sample bank for Section B.
// The original groovebox ships with a large proprietary sample library which cannot be
// redistributed, so the bank is rendered procedurally at start-up (kicks, snares, claps,
// hats, percussion, tonal stabs, FX). Users can additionally load their own WAV/AIFF
// files into any sampler slot.
class SampleBank
{
public:
    static const SampleBank& get();

    int size() const noexcept                 { return (int) samples.size(); }
    SamplePtr operator[] (int i) const        { return samples[(size_t) juce::jlimit (0, size() - 1, i)]; }
    juce::StringArray names() const;
    int firstOfCategory (const juce::String& cat, int offset = 0) const;

private:
    SampleBank();
    std::vector<SamplePtr> samples;
};
