#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace ui
{
const juce::Colour bg     { 0xff111316 };
const juce::Colour panel  { 0xff1b1e23 };
const juce::Colour panel2 { 0xff242830 };
const juce::Colour line   { 0xff333943 };
const juce::Colour text   { 0xffe3e6ea };
const juce::Colour dim    { 0xff8a929e };
const juce::Colour heat   { 0xffff7a3d };   // drive
const juce::Colour filt   { 0xff5ab8ff };   // filter
const juce::Colour eq     { 0xff7fd18b };   // EQ
const juce::Colour out    { 0xffe3e6ea };   // output
const juce::Colour lfo    { 0xffc792ff };   // LFO
const juce::Colour env    { 0xffffd166 };   // envelope
// house font: Urbanist (SIL OFL 1.1, embedded as binary data), Light + SemiBold
juce::Typeface::Ptr typeface (bool bold);
inline juce::Font font (float h, bool bold = false) { return juce::Font (juce::FontOptions (typeface (bold)).withHeight (h)); }
} // namespace ui

class HsLook : public juce::LookAndFeel_V4
{
public:
    HsLook();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return ui::font (13.0f); }
    juce::Font getPopupMenuFont() override { return ui::font (14.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return ui::font (juce::jmin (13.0f, h * 0.6f), true); }
    juce::Font getLabelFont (juce::Label&) override { return ui::font (12.5f); }
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& f) override { return ui::typeface (f.isBold()); }
};

class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const char* paramId, const juce::String& caption, juce::Colour accent,
          const juce::String& tip, int modTarget);
    void paint (juce::Graphics&) override;
    void resized() override;
    void setModFlags (bool lfoOn, bool envOn);

    juce::Slider slider;
    const int modTarget;

private:
    juce::String caption;
    juce::Colour accent;
    bool lfoMod = false, envMod = false;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class Choice : public juce::Component
{
public:
    Choice (juce::AudioProcessorValueTreeState&, const char* paramId, const juce::String& caption, const juce::StringArray& items,
            const juce::String& tip);
    void paint (juce::Graphics&) override;
    void resized() override;

    juce::ComboBox box;

private:
    juce::String caption;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

class Toggle : public juce::Component
{
public:
    Toggle (juce::AudioProcessorValueTreeState&, const char* paramId, const juce::String& text, juce::Colour accent,
            const juce::String& tip);
    void resized() override { button.setBounds (getLocalBounds()); }

    juce::TextButton button;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

class HeatsinkEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit HeatsinkEditor (HeatsinkProcessor&);
    ~HeatsinkEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintPanel (juce::Graphics&, juce::Rectangle<int>, const juce::String& title, juce::Colour accent) const;

    HeatsinkProcessor& proc;
    HsLook look;
    juce::TooltipWindow tooltips { this, 600 };

    Knob input, drive, cutoff, reso, lowFreq, lowGain, highFreq, highGain, mix, output;
    Knob lfoRate, lfoDepth, envAttack, envRelease, envThresh, envDepth;
    Choice circuit, filterType, lfoShape, lfoDiv, lfoTarget, envMode, envTarget;
    Toggle filterOn, lfoSync;
    juce::ComboBox presetBox;

    juce::Rectangle<int> rDrive, rFilter, rEq, rOut, rLfo, rEnv;
    float shownLfo = 0.0f, shownEnv = 0.0f;
    int shownProgram = -1, shownLfoT = -1, shownEnvT = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeatsinkEditor)
};
