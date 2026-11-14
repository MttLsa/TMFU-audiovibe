#include "PluginEditor.h"

namespace
{
const juce::Colour kBg (0xff16191f), kPanel (0xff20252d), kText (0xffe6e9ee), kDim (0xff8b93a1);
const juce::Colour kGreen (0xff3ddc84), kOrange (0xffffb020), kBlue (0xff4aa8ff), kRed (0xffff5a5a);

juce::String fmtDb (float v)
{
    if (v <= -99.0f)
        return "-inf";
    return (v > 0.0f ? "+" : "") + juce::String (v, 1);
}
} // namespace

AutoNormalizerEditor::AutoNormalizerEditor (AutoNormalizerProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    lnf.setColour (juce::Slider::rotarySliderFillColourId, kBlue);
    lnf.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff353c48));
    lnf.setColour (juce::Slider::thumbColourId, kText);
    lnf.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    lnf.setColour (juce::Slider::textBoxTextColourId, kText);
    lnf.setColour (juce::Label::textColourId, kText);
    lnf.setColour (juce::ToggleButton::textColourId, kText);
    lnf.setColour (juce::ToggleButton::tickColourId, kGreen);
    lnf.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2c3440));
    lnf.setColour (juce::TextButton::textColourOffId, kText);
    lnf.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2c3440));
    lnf.setColour (juce::ComboBox::textColourId, kText);
    lnf.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff3a4250));
    lnf.setColour (juce::PopupMenu::backgroundColourId, kPanel);
    setLookAndFeel (&lnf);

    setupKnob (target, "target", "Target");
    setupKnob (measure, "measure", "Measure time");
    setupKnob (gate, "gate", "Signal gate");
    setupKnob (sens, "sens", "Sensitivity");
    setupKnob (maxBoost, "maxboost", "Max boost");

    modeBox.addItemList (proc.apvts.getParameter ("mode")->getAllValueStrings(), 1);
    modeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "mode", modeBox);
    addAndMakeVisible (modeBox);
    modeLabel.setText ("Measure", juce::dontSendNotification);
    modeLabel.setColour (juce::Label::textColourId, kDim);
    addAndMakeVisible (modeLabel);

    setupToggle (autoBtn, autoAtt, "auto", "Auto-detect preset change");
    setupToggle (midiBtn, midiAtt, "midipc", "MIDI Program Change");
    setupToggle (safetyBtn, safetyAtt, "safety", "Safety ceiling (0 dBFS)");
    setupToggle (freezeBtn, freezeAtt, "freeze", "Freeze gain");

    normalizeBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2f6fb3));
    normalizeBtn.onClick = [this] { proc.engine.requestNormalize (an::TriggerSource::Manual); };
    resetBtn.onClick = [this] { proc.engine.requestResetGain(); };
    addAndMakeVisible (normalizeBtn);
    addAndMakeVisible (resetBtn);

    setSize (560, 420);
    startTimerHz (20);
}

AutoNormalizerEditor::~AutoNormalizerEditor()
{
    setLookAndFeel (nullptr);
}

void AutoNormalizerEditor::setupKnob (Knob& k, const juce::String& id, const juce::String& name)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    k.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setColour (juce::Label::textColourId, kDim);
    addAndMakeVisible (k.slider);
    addAndMakeVisible (k.label);
}

void AutoNormalizerEditor::setupToggle (juce::ToggleButton& b,
                                        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>& att,
                                        const juce::String& id, const juce::String& text)
{
    b.setButtonText (text);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, b);
    addAndMakeVisible (b);
}

void AutoNormalizerEditor::timerCallback()
{
    auto& e = proc.engine;
    state = e.getState();
    gainDb = e.getGainDb();
    appliedDb = e.getAppliedGainDb();
    measuredDb = e.getLastMeasuredDb();
    score = e.getChangeScore();
    progress = e.getMeasureProgress();
    inputDb = e.getInputLevelDb();
    lastTrig = e.getLastTrigger();
    count = e.getNormalizeCount();
    repaint (statusArea);
}

void AutoNormalizerEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    auto area = getLocalBounds().reduced (14);
    auto header = area.removeFromTop (30);
    g.setColour (kText);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("AutoNormalizer", header, juce::Justification::centredLeft);
    g.setColour (kDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("normalizes once per preset change, then holds", header, juce::Justification::centredRight);

    // ---- status panel
    auto s = statusArea;
    g.setColour (kPanel);
    g.fillRoundedRectangle (s.toFloat(), 8.0f);
    s = s.reduced (12, 8);

    juce::Colour stateCol = kGreen;
    juce::String stateText;
    switch (state)
    {
        case an::State::Locked:           stateCol = kGreen;  stateText = "LOCKED"; break;
        case an::State::WaitingForSignal: stateCol = kOrange; stateText = "WAITING FOR SIGNAL"; break;
        case an::State::Measuring:        stateCol = kBlue;   stateText = "MEASURING " + juce::String (juce::roundToInt (progress * 100)) + "%"; break;
    }

    auto row1 = s.removeFromTop (30);
    g.setColour (stateCol);
    g.fillEllipse (row1.removeFromLeft (14).withSizeKeepingCentre (12, 12).toFloat());
    row1.removeFromLeft (8);
    g.setFont (juce::FontOptions (19.0f, juce::Font::bold));
    g.drawText (stateText, row1.removeFromLeft (260), juce::Justification::centredLeft);

    g.setColour (kText);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("Gain " + fmtDb (gainDb) + " dB", row1, juce::Justification::centredRight);

    s.removeFromTop (4);
    auto row2 = s.removeFromTop (18);
    g.setFont (juce::FontOptions (13.0f));
    g.setColour (kDim);
    const char* trigNames[] = { "-", "first signal", "manual", "MIDI program change", "auto-detect" };
    const char* unit = proc.apvts.getRawParameterValue ("mode")->load() > 1.5f ? " LUFS" : " dBFS";
    g.drawText ("Last measured: " + fmtDb (measuredDb) + unit + "   |   by: " + trigNames[(int) lastTrig]
                    + "   |   count: " + juce::String (count),
                row2, juce::Justification::centredLeft);

    s.removeFromTop (8);
    auto drawBar = [&] (juce::Rectangle<int> r, const juce::String& name, float frac, juce::Colour c, const juce::String& val)
    {
        g.setColour (kDim);
        g.drawText (name, r.removeFromLeft (100), juce::Justification::centredLeft);
        auto valR = r.removeFromRight (110);
        r.removeFromRight (8);
        g.drawText (val, valR, juce::Justification::centredRight);
        auto bar = r.reduced (0, 4).toFloat();
        g.setColour (juce::Colour (0xff2c3440));
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (c);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, frac)), 3.0f);
    };

    drawBar (s.removeFromTop (18), "Input", juce::jmap (juce::jlimit (-60.0f, 0.0f, inputDb), -60.0f, 0.0f, 0.0f, 1.0f),
             kGreen.withAlpha (0.8f), fmtDb (inputDb) + " dB");
    drawBar (s.removeFromTop (18), "Preset change", score, score >= 1.0f ? kRed : kOrange,
             juce::String (juce::roundToInt (juce::jmin (score, 9.99f) * 100)) + "%");
    drawBar (s.removeFromTop (18), "Measure", state == an::State::Locked ? 0.0f : progress, kBlue,
             "applied " + fmtDb (appliedDb));
}

void AutoNormalizerEditor::resized()
{
    auto area = getLocalBounds().reduced (14);
    area.removeFromTop (34);
    statusArea = area.removeFromTop (132);
    area.removeFromTop (12);

    auto knobs = area.removeFromTop (120);
    const int w = knobs.getWidth() / 5;
    for (auto* k : { &target, &measure, &gate, &sens, &maxBoost })
    {
        auto r = knobs.removeFromLeft (w);
        k->label.setBounds (r.removeFromTop (18));
        k->slider.setBounds (r);
    }

    area.removeFromTop (10);
    auto row = area.removeFromTop (26);
    modeLabel.setBounds (row.removeFromLeft (64));
    modeBox.setBounds (row.removeFromLeft (170));
    row.removeFromLeft (16);
    resetBtn.setBounds (row.removeFromRight (100));
    row.removeFromRight (8);
    normalizeBtn.setBounds (row);

    area.removeFromTop (10);
    auto t1 = area.removeFromTop (24);
    autoBtn.setBounds (t1.removeFromLeft (t1.getWidth() / 2));
    midiBtn.setBounds (t1);
    auto t2 = area.removeFromTop (24);
    safetyBtn.setBounds (t2.removeFromLeft (t2.getWidth() / 2));
    freezeBtn.setBounds (t2);
}
