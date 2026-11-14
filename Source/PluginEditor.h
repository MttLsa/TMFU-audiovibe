#pragma once
#include "PluginProcessor.h"
#include "gui/LookAndFeel.h"
#include "gui/MicroSection.h"
#include "gui/LimeSection.h"
#include "gui/GlobalPanel.h"

// Main view laid out at a fixed design size and scaled to the editor size.
class MainView : public juce::Component
{
public:
    explicit MainView (TwinboxProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    int designWidth() const;
    static constexpr int sectionWidth = 1400, heightA = 430, heightB = 610, margin = 8;
    static constexpr int designHeight = margin + heightA + margin + heightB + margin;
    GlobalPanel global;
private:
    MicroSection sectionA;
    LimeSection  sectionB;
};

class TwinboxEditor : public juce::AudioProcessorEditor
{
public:
    explicit TwinboxEditor (TwinboxProcessor&);
    ~TwinboxEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void updateSizeForGlobal();
    TwinboxProcessor& proc;
    TwinLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };
    MainView view;
    float scale = 0.8f;
    bool adjusting = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TwinboxEditor)
};
