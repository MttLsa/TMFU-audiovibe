#pragma once
#include "LookAndFeel.h"
#include "../PluginProcessor.h"

// NORM: output normaliser + limiter of a section. The small "LIM" lamp lights while the limiter is working.
class NormButton : public Toggle, private juce::Timer
{
public:
    NormButton (TwinboxProcessor& p, int s, juce::Colour accent) : Toggle ("NORM", accent), proc (p), section (s)
    {
        attach (p.apvts, s == 0 ? params::aNorm : params::bNorm);
        setTooltip ("NORM: normalises the output of this section (automatic gain to a steady level) and turns on a "
                    "limiter at -1 dBFS, so the section never clips. The LIM lamp lights while the limiter is working.");
        startTimerHz (20);
    }
    ~NormButton() override { stopTimer(); }
    void paintOverChildren (juce::Graphics& g) override
    {
        if (! getToggleState()) return;
        const bool lim = proc.normLimiting (section);
        auto r = getLocalBounds().toFloat().removeFromRight (7.0f).reduced (1.0f).withSizeKeepingCentre (5.0f, 5.0f);
        g.setColour (lim ? juce::Colours::red : juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (r);
    }
private:
    void timerCallback() override { repaint(); }
    TwinboxProcessor& proc;
    int section;
};

// MIDI A / MIDI B: which section the MIDI keyboard / pads play (only one at a time).
class MidiTargetButton : public juce::TextButton, private juce::Timer
{
public:
    MidiTargetButton (TwinboxProcessor& p, int s, juce::Colour accent) : juce::TextButton ("MIDI"), proc (p), section (s)
    {
        setColour (juce::TextButton::buttonOnColourId, accent);
        setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        setTooltip (juce::String ("MIDI: the MIDI keyboard / pads (Push, MPC...) play Section ") + (s == 0 ? "A" : "B")
                    + " (one section at a time). From C1 (note 36) upwards: "
                    + (s == 0 ? "the 8 channels" : "Kick, Snare, Closed Hat, Open Hat, Belt, Capsule") + ", repeating along the keyboard.");
        onClick = [this]
        {
            if (auto* prm = proc.apvts.getParameter (params::gMidiTarget))
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (prm->convertTo0to1 ((float) section));
                prm->endChangeGesture();
            }
        };
        startTimerHz (10);
        timerCallback();
    }
    ~MidiTargetButton() override { stopTimer(); }
private:
    void timerCallback() override
    {
        const bool on = juce::roundToInt (proc.apvts.getRawParameterValue (params::gMidiTarget)->load()) == section;
        if (on != getToggleState()) setToggleState (on, juce::dontSendNotification);
    }
    TwinboxProcessor& proc;
    int section;
};
