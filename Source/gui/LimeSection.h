#pragma once
#include "LookAndFeel.h"
#include "../PluginProcessor.h"

// -------------------------------------------------------------------------------------------------
//  Generic panel that owns its controls (so the same panel can be shown small or in a big window)
// -------------------------------------------------------------------------------------------------
class ControlPanel : public Panel
{
public:
    ControlPanel (TwinboxProcessor& p, const juce::String& title, juce::Colour accent = theme::accentB);
    Knob*      knob (const juce::String& id, const juce::String& caption);
    Toggle*    toggle (const juce::String& id, const juce::String& caption);
    Segmented* seg (const juce::String& id, juce::StringArray labels = {}, const juce::String& caption = {});
    juce::ComboBox* combo (const juce::String& id);
    void row (std::vector<juce::Component*> r) { rows.push_back (std::move (r)); }
    void setTopArea (int h) { topH = h; }
    juce::Rectangle<int> topArea() const { return content().removeFromTop (topH); }
    void resized() override;
    void addOnTop (juce::Component* c) { tops.push_back (c); addAndMakeVisible (c); }
    juce::OwnedArray<juce::Component> owned;
    TwinboxProcessor& proc;

protected:
    std::vector<std::vector<juce::Component*>> rows;
    std::vector<juce::Component*> tops;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAtts;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAtts;
public:
    juce::Slider* incDec (const juce::String& id);
    std::function<void (ControlPanel&)> customLayout;
protected:
    int topH = 0;
};

// -------------------------------------------------------------------------------------------------
//  Sequencer grid (main grid Kick/Snare/C-Hat/O-Hat + value lanes Tie/Pod/Mod), 64 steps with paging
// -------------------------------------------------------------------------------------------------
class LimeSeqGrid : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    LimeSeqGrid (TwinboxProcessor& p, bool big);
    ~LimeSeqGrid() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragRow = -1; }
    void setPage (int p) { page = p; repaint(); }
    int  getPage() const { return page; }
    int  visibleSteps() const { return big ? 32 : 16; }
    int  numPages() const { return params::numLimeSteps / visibleSteps(); }
    void setLowerLane (int l) { lowerLane = l; repaint(); }   // compact view: which value lane is shown
    int  lowerLaneShown() const { return lowerLane; }
    juce::Rectangle<float> rowBounds (int row) const;        // rows: 0..3 trig, 4..6 value (big) or 4 (compact)

private:
    void timerCallback() override;
    int  editPattern() const;
    int  rowCount() const { return big ? 7 : 5; }
    int  rowToLane (int row) const { return row < 4 ? row : (big ? row - 4 : lowerLane) + 4; }   // 0..3 trig, 4..6 tie/pod/mod
    int  laneLen (int lane) const;
    int  stepAtX (float x) const;
    void edit (int row, int step, const juce::MouseEvent& e, bool first);
    TwinboxProcessor& proc;
    bool big;
    int page = 0, lowerLane = 0;
    int dragRow = -1, lastStep = -1; uint8_t dragVal = 0;
    std::array<int, 4> lastPlay { -2, -2, -2, -2 };
};

class LimeSeqPanel : public Panel, private juce::Timer
{
public:
    LimeSeqPanel (TwinboxProcessor& p, bool big);
    ~LimeSeqPanel() override { stopTimer(); }
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    TwinboxProcessor& proc;
    bool big;
    juce::OwnedArray<juce::TextButton> patBtns, pageBtns, laneBtns, chainBtns;
    Toggle chainOn { "ON", theme::accentB }, lock { "LOCK", theme::accentB };
    juce::Slider chainLen;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> chainLenAtt;
    juce::TextButton copyBtn { "COPY" }, pasteBtn { "PASTE" };
    LimeSeqGrid grid;
    int clip = -1;
};

// Sample source + selector row for a sampler module
class SampleSelector : public juce::Component, private juce::ChangeListener, private juce::Timer
{
public:
    SampleSelector (TwinboxProcessor& p, int module, bool big);
    ~SampleSelector() override;
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }
    void timerCallback() override;
    void refresh();
    void chooseFolder();
    TwinboxProcessor& proc;
    int module; bool big;
    juce::TextButton twb { "TWB" }, leg { "LEG" }, usr { juce::String::fromUTF8 ("\xf0\x9f\x93\x81") }, play { juce::String::fromUTF8 ("\xe2\x96\xb6") };
    juce::ComboBox box;
    std::unique_ptr<juce::FileChooser> chooser;
    int shownIndex = -1;
};

// Waveform display of the currently selected sample of a module
class WaveView : public juce::Component, private juce::Timer
{
public:
    WaveView (TwinboxProcessor& p, int m) : proc (p), module (m) { startTimerHz (10); }
    ~WaveView() override { stopTimer(); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override { repaint(); }
    TwinboxProcessor& proc; int module;
};

// One sampler module (Kick, Snare, Hat, Tie, Pod) — original control set
class LimeModulePanel : public ControlPanel
{
public:
    LimeModulePanel (TwinboxProcessor& p, int voice, bool big);
    void resized() override;
private:
    int voice; bool big;
    std::unique_ptr<SampleSelector> selector;
    std::unique_ptr<WaveView> wave;
    std::unique_ptr<Toggle> power;
};

// XY pad bound to two parameters (xFx Parameter X / Additional)
class XYPad : public juce::Component
{
public:
    XYPad (juce::AudioProcessorValueTreeState& s, const juce::String& xId, const juce::String& yId);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override { set (e); }
    void mouseDrag (const juce::MouseEvent& e) override { set (e); }
    void mouseUp (const juce::MouseEvent&) override;
private:
    void set (const juce::MouseEvent& e);
    juce::RangedAudioParameter *px, *py;
    std::unique_ptr<juce::ParameterAttachment> ax, ay;
    float vx = 0.5f, vy = 0.5f;
    bool dragging = false;
};

// -------------------------------------------------------------------------------------------------
//  Big windows
// -------------------------------------------------------------------------------------------------
class LimeFxWindowContent : public juce::Component
{
public:
    explicit LimeFxWindowContent (TwinboxProcessor& p);
    void paint (juce::Graphics& g) override { g.fillAll (theme::bg); }
    void resized() override;
private:
    juce::OwnedArray<Panel> panels;
};

// -------------------------------------------------------------------------------------------------
//  Section B main view
// -------------------------------------------------------------------------------------------------
class LimeSection : public juce::Component, private juce::Timer
{
public:
    explicit LimeSection (TwinboxProcessor& p);
    ~LimeSection() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    std::function<void()> onOpenBrowser;

private:
    void timerCallback() override;
    void openWindow (const juce::String& key, const juce::String& title, std::function<juce::Component*()> make, int w, int h);
    void importLegacy();

    TwinboxProcessor& proc;
    juce::TextButton browserBtn { "BROWSER" }, legacyBtn { "IMPORT LEGACY" };
    Knob levelB { "Level B", theme::accentB };
    juce::String status;
    std::unique_ptr<juce::FileChooser> chooser;

    std::unique_ptr<LimeSeqPanel> seq;
    std::unique_ptr<ControlPanel> remix, out, global, pulp, xfx, delay, mix, eq, filter, magic;
    juce::OwnedArray<LimeModulePanel> modules;
    std::map<juce::String, std::unique_ptr<ExpandWindow>> windows;
};

// helper used by the FX window and the main view
std::unique_ptr<ControlPanel> makeLimePanel (TwinboxProcessor& p, const juce::String& which, bool big);
