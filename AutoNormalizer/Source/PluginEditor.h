#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

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
    juce::LookAndFeel_V4 lnf;

    Knob target, measure, gate, sens, maxBoost;
    juce::ComboBox modeBox;
    juce::Label modeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAtt;

    juce::ToggleButton autoBtn, midiBtn, safetyBtn, freezeBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoAtt, midiAtt, safetyAtt, freezeAtt;

    juce::TextButton normalizeBtn { "NORMALIZE NOW" }, resetBtn { "RESET 0 dB" };

    juce::Rectangle<int> statusArea;
    // cached display values
    an::State state = an::State::WaitingForSignal;
    float gainDb = 0, appliedDb = 0, measuredDb = -100, score = 0, progress = 0, inputDb = -100;
    an::TriggerSource lastTrig = an::TriggerSource::None;
    int count = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoNormalizerEditor)
};
