#pragma once
#include "LookAndFeel.h"
#include "../PluginProcessor.h"

// -------------------------------------------------------------------------------------
//  Sequencer grid for Section B
//  Main window : Kick / Snare / Hat (16 steps, shared main length)
//  Lower window: Tie / Pod lanes (own length, optional stretch) + Mod value lane
// -------------------------------------------------------------------------------------
class LimeSeqGrid : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    LimeSeqGrid (TwinboxProcessor& p, bool big);
    ~LimeSeqGrid() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragLane = -1; }
    juce::Rectangle<int> laneBounds (int lane) const;    // lane rows: 0 kick,1 snare,2 hat,3 tie,4 pod,5 mod

private:
    void timerCallback() override;
    int  editPattern() const;
    int  laneLength (int lane) const;
    bool laneStretched (int lane) const;
    juce::Rectangle<float> cell (int lane, int step) const;
    int  stepAt (int lane, int x) const;
    int  laneAt (int y) const;
    void applyCell (int lane, int step, const juce::MouseEvent& e, bool first);

    TwinboxProcessor& proc;
    bool big;
    int dragLane = -1, lastDragStep = -1;
    uint8_t dragValue = 0;
    std::array<int, 4> lastSteps { -2, -2, -2, -2 };
};

// Large sequencer window: pattern slots, big grid, per-lane tools, remix controls
class LimeSeqWindowContent : public juce::Component, private juce::Timer
{
public:
    explicit LimeSeqWindowContent (TwinboxProcessor& p);
    ~LimeSeqWindowContent() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void laneOp (int lane, int op);    // 0 clear, 1 shift left, 2 shift right, 3 random, 4 fill every 4
    TwinboxProcessor& proc;
    juce::OwnedArray<juce::TextButton> slots;
    LimeSeqGrid grid;
    struct LaneTools { juce::TextButton clear { "Clr" }, left { "<" }, right { ">" }, rnd { "Rnd" }, four { "1/4" }; };
    std::array<LaneTools, 6> tools;
    Knob len { "Main Len", theme::accentB }, tieLen { "Tie Len", theme::accentB }, podLen { "Pod Len", theme::accentB },
         modLen { "Mod Len", theme::accentB }, shuffle { "Shuffle", theme::accentB }, accent { "Accent", theme::accentB };
    Toggle tieStr { "Stretch", theme::accentB }, podStr { "Stretch", theme::accentB }, modStr { "Stretch", theme::accentB };
    juce::ComboBox modDest;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modDestAtt;
    Knob modAmt { "Mod Amt", theme::accentB };
};

// -------------------------------------------------------------------------------------
//  Waveform display of a sampler slot
// -------------------------------------------------------------------------------------
class SampleView : public juce::Component, private juce::Timer
{
public:
    SampleView (TwinboxProcessor& p, int s) : proc (p), slot (s) { startTimerHz (15); }
    ~SampleView() override { stopTimer(); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    TwinboxProcessor& proc;
    int slot;
    const Sample* last = nullptr;
    float flash = 0; int lastHits = 0;
};

// -------------------------------------------------------------------------------------
//  One sampler (Kick / Snare / Hat / Tie / Pod). 'big' = large editor window version.
// -------------------------------------------------------------------------------------
class LimeSamplerPanel : public Panel, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    LimeSamplerPanel (TwinboxProcessor& p, int part, bool big);
    ~LimeSamplerPanel() override { stopTimer(); }
    void resized() override;
    void paint (juce::Graphics&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    struct SampleSlot
    {
        juce::ComboBox box;
        juce::TextButton load { "Load" };
        juce::TextButton play { juce::String::fromUTF8 ("\xe2\x96\xb6") };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
        int slot = 0;
    };
    void timerCallback() override;
    void setupSlot (SampleSlot& s, int slot, const juce::String& paramId);
    void chooseFile (int slot);

    TwinboxProcessor& proc;
    int part;
    bool big;
    SampleSlot s1, s2;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<SampleView> view1, view2;
    juce::OwnedArray<Knob> knobs;
    juce::OwnedArray<Toggle> toggles;
    std::unique_ptr<Segmented> fmode;
};

// -------------------------------------------------------------------------------------
//  Large mixer / FX window: 5 channel strips + Delay, Filter, LFO, Mod, Output
// -------------------------------------------------------------------------------------
class LimeMixerView : public juce::Component, private juce::Timer
{
public:
    explicit LimeMixerView (TwinboxProcessor& p);
    ~LimeMixerView() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override { repaint(); }
    TwinboxProcessor& proc;
    struct Strip
    {
        juce::Slider fader;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
        Knob pan { "Pan", theme::accentB }, send { "Delay", theme::accentB };
        Toggle mute { "Mute", theme::accentB };
        juce::TextButton play { juce::String::fromUTF8 ("\xe2\x96\xb6") };
    };
    std::array<Strip, params::numLimeParts> strips;
    Panel dlyP { "Delay", theme::accentB }, fltP { "Filter", theme::accentB }, lfoP { "LFO", theme::accentB },
          modP { "Mod Lane", theme::accentB }, outP { "Output", theme::accentB };
    Segmented dlyTime { theme::accentB };
    Knob dlyFb { "Feedback", theme::accentB }, dlyDamp { "Damp", theme::accentB }, dlyLvl { "Level", theme::accentB };
    Knob fltCut { "Cutoff", theme::accentB }, fltRes { "Reso", theme::accentB };
    Knob lfoRate { "Rate", theme::accentB }, lfoFM { "FM", theme::accentB }, lfoDM { "DM", theme::accentB };
    juce::ComboBox modDest;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modDestAtt;
    Knob modAmt { "Amount", theme::accentB };
    Knob gain { "Gain", theme::accentB }, pitch { "Pitch", theme::accentB }, dirt { "Dirt", theme::accentB };
    Toggle norm { "Norm", theme::accentB };
};

// -------------------------------------------------------------------------------------
//  Section B main view
// -------------------------------------------------------------------------------------
class LimeSection : public juce::Component, private juce::Timer
{
public:
    explicit LimeSection (TwinboxProcessor& p);
    ~LimeSection() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void openSeqWindow();
    void openSamplerWindow (int part);
    void openMixerWindow();

    TwinboxProcessor& proc;

    // top row
    Panel seqPanel { "Sequencer", theme::accentB };
    juce::OwnedArray<juce::TextButton> slots;
    juce::TextButton copyPat { "Copy" }, pastePat { "Paste" };
    int patClip = -1;
    LimeSeqGrid grid;

    Panel remixPanel { "Remix", theme::accentB };
    Knob len { "Length", theme::accentB }, tieLen { "Tie", theme::accentB }, podLen { "Pod", theme::accentB }, modLen { "Mod", theme::accentB };
    Toggle tieStr { "Str", theme::accentB }, podStr { "Str", theme::accentB }, modStr { "Str", theme::accentB };
    Knob shuffle { "Shuffle", theme::accentB }, accent { "Accent", theme::accentB };

    Panel outPanel { "Output", theme::accentB };
    Knob gain { "Gain", theme::accentB }, pitch { "Pitch", theme::accentB }, dirt { "Dirt", theme::accentB };
    Toggle norm { "Norm", theme::accentB };
    Knob lfoRate { "LFO Rate", theme::accentB }, lfoFM { "FM", theme::accentB }, lfoDM { "DM", theme::accentB };

    // sampler row
    juce::OwnedArray<LimeSamplerPanel> samplers;

    // FX row
    Panel dlyPanel { "Delay", theme::accentB }, fltPanel { "Filter", theme::accentB }, modPanel { "Mod", theme::accentB };
    juce::ComboBox dlyTime;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> dlyTimeAtt;
    Knob dlyFb { "Feedback", theme::accentB }, dlyDamp { "Damp", theme::accentB }, dlyLvl { "Level", theme::accentB };
    Knob fltCut { "Cutoff", theme::accentB }, fltRes { "Reso", theme::accentB };
    juce::ComboBox modDest;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modDestAtt;
    Knob modAmt { "Amount", theme::accentB };

    std::unique_ptr<ExpandWindow> seqWindow, mixerWindow;
    std::array<std::unique_ptr<ExpandWindow>, params::numLimeParts> samplerWindows;
};
