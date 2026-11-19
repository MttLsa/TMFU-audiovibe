#pragma once
#include "LfoPanel.h"
#include "SectionButtons.h"
#include "LookAndFeel.h"
#include "../PluginProcessor.h"

// Step grid for Section A.
//  - single-channel mode : 3 rows (Trigger / Accent / Fill) for the selected channel
//  - all-channel mode    : 8 channels x (Trigger / Accent / Fill) — used in the large window
class MicroStepGrid : public juce::Component, private juce::Timer
{
public:
    MicroStepGrid (TwinboxProcessor& p, bool allChannels);
    ~MicroStepGrid() override { stopTimer(); }
    void setChannel (int ch) { channel = ch; repaint(); }
    void setPage (int p) { page = p; repaint(); }
    int  getPage() const { return page; }
    int  visible() const { return all ? 32 : 16; }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    std::function<void (int)> onChannelClicked;

private:
    void timerCallback() override;
    bool cellAt (juce::Point<int> pt, int& ch, int& row, int& step) const;
    juce::Rectangle<float> cellRect (int rowIndex, int step) const;
    int editPattern() const;
    int numRows() const { return all ? params::numMicroChannels * 3 : 3; }
    int labelW() const { return all ? 110 : 56; }

    TwinboxProcessor& proc;
    bool all;
    int channel = 0, page = 0;
    int lastStep = -2;
    int dragRow = -1, dragCh = -1; bool dragSet = false; int lastDragStep = -1;
};

// The four drum-patch panels (Oscillator / Noise / Mixing / Velocity) for one channel.
class MicroPatchEditor : public juce::Component
{
public:
    explicit MicroPatchEditor (TwinboxProcessor& p);
    void setChannel (int ch);
    void resized() override;
    void setExpandable (std::function<void()> f) { oscPanel.setExpandable (std::move (f)); }
private:
    TwinboxProcessor& proc;
    Panel oscPanel { "Oscillator", theme::accentA }, noisePanel { "Noise", theme::accentA },
          mixPanel { "Mixing", theme::accentA }, velPanel { "Velocity Sensitivity", theme::accentA };
    Segmented oscWave, pmMode, nMode, nEnv;
    Knob oscFreq { "Osc Freq" }, oscAtk { "Attack" }, oscDcy { "Decay" }, pmAmt { "Mod Amount" }, pmRate { "Mod Rate" };
    Knob nFreq { "Filter Freq" }, nQ { "Filter Q" }, nAtk { "Attack" }, nDcy { "Decay" };
    Toggle nStereo { "Stereo" }, choke { "Choke" };
    Knob mix { "Osc / Noise" }, dist { "Distort" }, eqFreq { "EQ Freq" }, eqGain { "EQ Gain" }, lvl { "Level" }, pan { "Pan" };
    Knob velOsc { "Osc" }, velNoise { "Noise" }, velMod { "Mod" };
};

// Large window: channel selector + big drum patch editor
class MicroPatchWindowContent : public juce::Component
{
public:
    MicroPatchWindowContent (TwinboxProcessor& p, int ch, std::function<void (int)> onSelect);
    void setChannel (int ch);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    TwinboxProcessor& proc;
    juce::OwnedArray<juce::TextButton> chButtons;
    MicroPatchEditor editor;
    std::function<void (int)> onSelect;
};

// Large pattern window: pattern A-L, length, swing, fill controls + all-channel grid
class MicroPatternWindowContent : public juce::Component, private juce::Timer
{
public:
    MicroPatternWindowContent (TwinboxProcessor& p, std::function<void (int)> onChannel);
    ~MicroPatternWindowContent() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    TwinboxProcessor& proc;
    juce::OwnedArray<juce::TextButton> pats;
    juce::Slider lenBox;
    Knob swing { "Swing" };
    Segmented fillRate { theme::accentA };
    Toggle fill { "FILL" };
    MicroStepGrid grid;
    juce::OwnedArray<juce::TextButton> pages;
};

class MicroSection : public juce::Component, private juce::Timer
{
public:
    explicit MicroSection (TwinboxProcessor& p);
    ~MicroSection() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
public:
    void selectChannel (int ch);
private:
    void openPatternWindow();
    void openPatchWindow();
    void refreshPatternControls();

    TwinboxProcessor& proc;
    int selected = 0;

    // header
    juce::OwnedArray<juce::TextButton> patternButtons;
    juce::TextButton storeL { "Store L" }, storeR { "Store R" };
    Knob morph { "Morph", theme::accentA }, level { "Level A", theme::accentA };
    std::unique_ptr<NormButton> normBtn;
    std::unique_ptr<MidiTargetButton> midiBtn;
    Toggle seqBtn { "SEQ", theme::accentA };

    // channel list
    struct ChannelRow : public juce::Component
    {
        ChannelRow (MicroSection& o, int ch);
        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        MicroSection& owner; int ch;
        Toggle mute { "M", theme::accentA };
        juce::TextButton play { juce::String::fromUTF8 ("\xe2\x96\xb6") };
        NudgeBox nudge { theme::accentA };
        float flash = 0.0f; int lastHits = 0;
    };
    juce::OwnedArray<ChannelRow> rows;
    juce::Label nameEdit;
    juce::TextButton copyBtn { "Copy" }, pasteBtn { "Paste" };

    MicroPatchEditor patch;
    std::unique_ptr<LfoPanel> lfo;   // 3 LFOs for Section A
    std::unique_ptr<juce::Component> clickPanel;   // CLICK: transient layer of the Kick (channel 1)

    // pattern panel
    Panel patPanel { "Pattern", theme::accentA };
    MicroStepGrid grid;
    juce::Slider lenBox;
    Knob swing { "Swing" };
    Segmented fillRate { theme::accentA };
    Toggle fill { "FILL" };
    juce::TextButton clearBtn { "Clear" }, copyPat { "Copy Pat" }, pastePat { "Paste Pat" };
    int patClip = -1;
    juce::OwnedArray<juce::TextButton> pageBtns;
    juce::TextButton browserBtn { "BROWSER" };
public:
    std::function<void()> onOpenBrowser;
private:
    int lastPattern = -1;

    std::unique_ptr<ExpandWindow> patternWindow, patchWindow;
};
