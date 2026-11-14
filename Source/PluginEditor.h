#pragma once
#include "PluginProcessor.h"
#include "gui/LookAndFeel.h"
#include "gui/MicroSection.h"
#include "gui/LimeSection.h"
#include "gui/GlobalPanel.h"
#include "gui/PresetBrowser.h"

// Main view laid out at a fixed design size and scaled to the editor size.
class MainView : public juce::Component
{
public:
    explicit MainView (TwinboxProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    int designWidth() const;
    static constexpr int sectionWidth = 1400, heightA = 430, heightB = 740, margin = 8;
    static constexpr int titleH = 46;
    static constexpr int designHeight = titleH + heightA + margin + heightB + margin;
    GlobalPanel global;
private:
    void openBrowser (int section);
    TwinboxProcessor& proc;
    MicroSection sectionA;
    LimeSection  sectionB;
    std::array<std::unique_ptr<ExpandWindow>, 2> browsers;
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
