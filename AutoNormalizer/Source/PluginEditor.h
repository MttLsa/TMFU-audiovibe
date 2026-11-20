#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "BinaryData.h"

// Inter (SIL Open Font License; Regular + SemiBold) for every text in the plugin. It is installed as the
// default look-and-feel while an editor is open, so every Font without an explicit
// typeface (labels, sliders, buttons, menus, painted text) resolves to Inter.
// Only the plug-in name in the header keeps Comfortaa Bold (nameFont).
struct ComfortaaLookAndFeel : public juce::LookAndFeel_V4
{
    ComfortaaLookAndFeel()
    {
        juce::LookAndFeel::setDefaultLookAndFeel (this);
    }
    ~ComfortaaLookAndFeel() override
    {
        if (&juce::LookAndFeel::getDefaultLookAndFeel() == this)
            juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& f) override
    {
        return f.isBold() ? bold : regular;
    }

    juce::Font nameFont (float h) const { return juce::Font (juce::FontOptions (name).withHeight (h)); }

    juce::Typeface::Ptr regular = juce::Typeface::createSystemTypefaceFor (BinaryData::InterRegular_ttf,
                                                                          (size_t) BinaryData::InterRegular_ttfSize);
    juce::Typeface::Ptr bold = juce::Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_ttf,
                                                                       (size_t) BinaryData::InterSemiBold_ttfSize);
    juce::Typeface::Ptr name = juce::Typeface::createSystemTypefaceFor (BinaryData::ComfortaaBold_ttf,
                                                                       (size_t) BinaryData::ComfortaaBold_ttfSize);
};

class AutoNormalizerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AutoNormalizerEditor (AutoNormalizerProcessor&);
    ~AutoNormalizerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    void setupKnob (Knob&, const juce::String& paramId, const juce::String& name);
    void setupToggle (juce::ToggleButton&, std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>&,
                      const juce::String& paramId, const juce::String& text);

    AutoNormalizerProcessor& proc;
    juce::SharedResourcePointer<ComfortaaLookAndFeel> lnfShared;
    ComfortaaLookAndFeel& lnf = *lnfShared;

    Knob target, measure, gate, sens, maxBoost;
    juce::Label ceilingLabel;

    juce::ToggleButton autoBtn, midiBtn, safetyBtn, freezeBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoAtt, midiAtt, safetyAtt, freezeAtt;

    juce::TextButton normalizeBtn { "NORMALIZE NOW" }, resetBtn { "RESET 0 dB" };

    juce::Rectangle<int> statusArea;
    // cached display values
    an::State state = an::State::WaitingForSignal;
    float gainDb = 0, appliedDb = 0, measuredLufs = -100, measuredPeak = -100, score = 0, progress = 0, inputDb = -100,
          outputDb = -100, grDb = 0;
    an::TriggerSource lastTrig = an::TriggerSource::None;
    int count = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoNormalizerEditor)
};
