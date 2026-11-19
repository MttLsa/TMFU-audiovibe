#pragma once
#include "LfoPanel.h"
#include "SectionButtons.h"
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
    bool keyPressed (const juce::KeyPress&) override;          // arrows edit the selected Tie / Pod / Mod column
    void focusLost (FocusChangeType) override { repaint(); }
    void setPage (int p) { page = p; repaint(); }
    int  getPage() const { return page; }
    int  visibleSteps() const { return big ? 32 : 16; }
    int  numPages() const { return params::numLimeSteps / visibleSteps(); }
    void setLowerLane (int l) { lowerLane = l; if (! big) selLane = -1; repaint(); }   // compact view: which value lane is shown
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
    int selLane = -1, selStep = -1;   // keyboard selection (value lanes 4..6)
    bool wavMode (int lane) const;
    void setValue (int lane, int step, float v, bool audition);
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
    juce::String wavParamId() const { return module == SampleLibrary::click ? juce::String (params::aClickWav) : params::b (module, params::lv::wav); }
    TwinboxProcessor& proc;
    int module; bool big;
    juce::TextButton twb { "DBX" }, leg { "LEG" }, usr { juce::String::fromUTF8 ("\xf0\x9f\x93\x81") }, play { juce::String::fromUTF8 ("\xe2\x96\xb6") };
    juce::ComboBox box;
    std::unique_ptr<juce::FileChooser> chooser;
    int shownIndex = -1;
};

// WAV knob: steps through the samples of the module's current folder (shows the sample name)
class WavKnob : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    WavKnob (TwinboxProcessor& p, int module, juce::Colour accent);
    ~WavKnob() override { stopTimer(); }
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    TwinboxProcessor& proc; int module;
    FineSlider slider;          // sample select: drag or arrows step through the samples
    juce::String name;
    int size = 0;
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
    std::function<void()> onOpenPitch;   // Tie / Pod: open the per-sample pitch window
private:
    int voice; bool big;
    std::unique_ptr<SampleSelector> selector;
    std::unique_ptr<WaveView> wave;
    std::unique_ptr<Toggle> power, scaleBtn, nameBtn;
    std::unique_ptr<NudgeBox> nudge;
    std::unique_ptr<juce::ParameterAttachment> ftWatch;   // Hat: filter type change -> sensible cutoff
    int lastFt = -1;
    juce::TextButton noteBtn { juce::String::fromUTF8 ("\xe2\x99\xaa") };
};

// Per-sample pitch window (Tie / Pod): every sample of the current folder with detected note, play button
// and a -32..+32 semitone knob (MANUAL mode). Works live while the sequencer is playing.
class PitchWindowContent : public juce::Component, private juce::ChangeListener, private juce::Timer
{
public:
    PitchWindowContent (TwinboxProcessor& p, int module);
    ~PitchWindowContent() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    struct Row : public juce::Component
    {
        Row (PitchWindowContent& o, int idx);
        void paint (juce::Graphics&) override;
        void resized() override;
        PitchWindowContent& owner; int index;
        juce::TextButton play { juce::String::fromUTF8 ("\xe2\x96\xb6") };
        FineSlider knob;
    };
    void changeListenerCallback (juce::ChangeBroadcaster*) override { rebuild(); }
    void timerCallback() override;
    void rebuild();
    TwinboxProcessor& proc;
    int module;
    juce::Viewport viewport;
    juce::Component list;
    juce::OwnedArray<Row> rows;
    juce::TextButton resetAll { "Reset all to 0" };
    Toggle nameKey { "KEY FROM FILE NAME", theme::accentB };
    bool scaleWasOn = false;
};

// XY pad bound to two parameters (xFx Parameter X / Additional) + XY motion recorder display
class XYPad : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    XYPad (TwinboxProcessor& p, const juce::String& xId, const juce::String& yId);
    ~XYPad() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override { set (e); }
    void mouseDrag (const juce::MouseEvent& e) override { set (e); }
    void mouseUp (const juce::MouseEvent&) override;
private:
    void timerCallback() override;
    void set (const juce::MouseEvent& e);
    TwinboxProcessor& proc;
    juce::RangedAudioParameter *px, *py;
    std::unique_ptr<juce::ParameterAttachment> ax, ay;
    float vx = 0.5f, vy = 0.5f;
    bool dragging = false;
    std::vector<juce::Point<float>> trail;
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
    juce::ComboBox scaleRoot, scaleType;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleRootAtt, scaleTypeAtt;
    Knob levelB { "Level B", theme::accentB };
    std::unique_ptr<NormButton> normBtn;
    std::unique_ptr<MidiTargetButton> midiBtn;
    Toggle seqBtn { "SEQ", theme::accentB };
    juce::String status;
    std::unique_ptr<juce::FileChooser> chooser;

    std::unique_ptr<LimeSeqPanel> seq;
    std::unique_ptr<ControlPanel> remix, out, global, xfx, delay, mix, eq, filter;
    juce::OwnedArray<LimeModulePanel> modules;
    std::map<juce::String, std::unique_ptr<ExpandWindow>> windows;
    std::unique_ptr<LfoPanel> lfo;   // 3 LFOs for Section B (FX row)
};

// helper used by the FX window and the main view
std::unique_ptr<ControlPanel> makeLimePanel (TwinboxProcessor& p, const juce::String& which, bool big);
