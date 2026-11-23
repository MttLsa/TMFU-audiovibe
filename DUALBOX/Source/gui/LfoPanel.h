#pragma once
#include "LookAndFeel.h"
#include "../PluginProcessor.h"

// One LFO: target (any parameter of the section, grouped menu), shape, SYNC (beat divisions) or RATE (ms), depth, phase.
class LfoStrip : public juce::Component, private juce::Timer
{
public:
    LfoStrip (TwinboxProcessor& p, int section, int index, bool big);
    ~LfoStrip() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::String targetLabel() const;
    void chooseTarget();
private:
    void timerCallback() override;
    juce::Rectangle<int> scopeArea() const;
    TwinboxProcessor& proc;
    int section, index;
    bool big;
    juce::Colour accent;
    juce::TextButton targetBtn;
    juce::ComboBox shape, div;
    Toggle sync;
    Knob rate, depth, phase;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> shapeAtt, divAtt;
    juce::RangedAudioParameter* targetParam = nullptr;
    std::atomic<float>* syncRaw = nullptr;
    bool lastSync = true;
    juce::String lastLabel;
};

// The LFO module of a section: 3 LFOs (compact rows). The expand button opens the large LFO window.
class LfoPanel : public Panel, private juce::Timer
{
public:
    LfoPanel (TwinboxProcessor& p, int section);
    ~LfoPanel() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    static void setCount (TwinboxProcessor& p, int section, int n);
    static int  getCount (TwinboxProcessor& p, int section);
private:
    void timerCallback() override;
    void layoutStrips();
    TwinboxProcessor& proc;
    int section, page = 0, lastCount = -1;
    juce::OwnedArray<LfoStrip> strips;
    juce::TextButton addBtn { "+" }, removeBtn { "-" };
    juce::OwnedArray<juce::TextButton> pageBtns;
    std::unique_ptr<ExpandWindow> window;
};

// Large window: the 3 LFOs side by side with waveform scope, all controls
class LfoWindowContent : public juce::Component, private juce::Timer
{
public:
    LfoWindowContent (TwinboxProcessor& p, int section);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    TwinboxProcessor& proc;
    int section, lastCount = -1;
    juce::OwnedArray<LfoStrip> strips;
    juce::TextButton addBtn { "+ Add LFO" }, removeBtn { "- Remove last LFO" };
};
