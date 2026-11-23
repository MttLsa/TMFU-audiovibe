#pragma once
#include "PluginEditor.h"

// =====================================================================================================
//  FamPat preset player: the 200 patches with their names and sounds (internal circuits unchanged), one
//  FILTER ENVELOPE and one AMP ENVELOPE for the whole patch, and a MASTER OUTPUT section with the patch's
//  effects (drive / pedal, chorus, delay, reverb: only the ones the patch uses) and the output level.
// =====================================================================================================

// ADSR shape drawn from four parameters (attack / decay / sustain / release)
class EnvCurve : public juce::Component, private juce::Timer
{
public:
    EnvCurve (FamPatProcessor& p, int a, int d, int s, int r, juce::Colour c) : proc (p), ia (a), id (d), is (s), ir (r), col (c) { startTimerHz (15); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override { repaint(); }
    FamPatProcessor& proc;
    int ia, id, is, ir;
    juce::Colour col;
};

class PlayerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PlayerEditor (FamPatProcessor&);
    ~PlayerEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    void timerCallback() override;
    void updatePatch();
    void layoutMaster();
    FamPatProcessor& proc;
    FamPatLook look;
    juce::Component content;
    PresetBrowser browser;
    SavePresetWindow saveWindow;
    LogoSplash splash;
    juce::MidiKeyboardComponent keys;
    juce::TextButton prev { "<" }, next { ">" }, browse { "BROWSER" }, savePreset { "SAVE" };
    juce::Label presetName, presetInfo;
    // patch card
    juce::String cardName, cardSong, cardSynth, cardNotes, cardFactory;
    juce::Colour cardAccent { 0xffe07b2a };
    // envelopes
    std::unique_ptr<Control> env[8];
    std::unique_ptr<EnvCurve> fCurve, aCurve;
    // master output: groups of controls, each shown only when the patch uses that effect (Level always)
    struct FxGroup { juce::String title; std::vector<std::unique_ptr<Control>> ctl; bool shown = true; juce::Rectangle<int> box; };
    std::vector<FxGroup> fx;
    juce::Rectangle<float> logoArea;
    juce::Rectangle<int> cardBox, fBox, aBox, masterBox;
    int shownSerial = -1;
    static constexpr int BW = 1400, BH = 900;
    int W = BW, H = BH;
    float uiScale = 1.0f;
};
