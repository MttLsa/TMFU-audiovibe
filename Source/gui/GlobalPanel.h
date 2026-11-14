#pragma once
#include "LookAndFeel.h"
#include "../PluginProcessor.h"

// Small display of the master cutoff filter response
class FilterCurve : public juce::Component, private juce::Timer
{
public:
    explicit FilterCurve (TwinboxProcessor& p) : proc (p) { startTimerHz (20); }
    ~FilterCurve() override { stopTimer(); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    TwinboxProcessor& proc;
    float lc = -1, lr = -1, lm = -1;
};

// "GLOBAL CONTROL" — right-hand panel, collapsible by clicking its header
class GlobalPanel : public juce::Component, private juce::Timer
{
public:
    explicit GlobalPanel (TwinboxProcessor& p);
    ~GlobalPanel() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    bool isExpanded() const { return expanded; }
    void setExpanded (bool e);
    std::function<void()> onToggle;

    static constexpr int collapsedWidth = 34;
    static constexpr int expandedWidth  = 250;

private:
    void timerCallback() override { repaint (meterArea); }
    TwinboxProcessor& proc;
    bool expanded = true;
    Knob xfade { "A / B Mix", theme::accentG };
    Knob cutoff { "CUTOFF", theme::accentG };
    Segmented mode { theme::accentG, false };
    Knob res { "Resonance", theme::accentG };
    Knob master { "Master", theme::accentG };
    Knob tempo { "Tempo", theme::accentG };
    Toggle run { "RUN", theme::accentG };
    FilterCurve curve;
    juce::Rectangle<int> meterArea, headerArea;
};
