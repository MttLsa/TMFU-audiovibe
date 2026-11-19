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
    const juce::Colour src    { 0xff7fd18b };   // sources
    const juce::Colour glitch { 0xffff7a59 };   // lanes / glitch
    const juce::Colour fx     { 0xff5ab8ff };   // fx chain
    const juce::Colour mod    { 0xffc792ff };   // modulation
    const juce::Colour tune   { 0xffffd166 };   // tuning
    inline juce::Font font (float h, bool bold = false) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
    juce::Colour sourceColour (int src);
}

class MvLook : public juce::LookAndFeel_V4
{
public:
    MvLook();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return ui::font (11.0f); }
    juce::Font getPopupMenuFont() override { return ui::font (13.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return ui::font (juce::jmin (11.5f, h * 0.6f), true); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
};

// ----------------------------------------------------------------------------------------------- controls
class ParamControl : public juce::Component
{
public:
    ParamControl (MicrovibeProcessor& p, int pid, juce::Colour accent, const juce::String& caption = {});
    ~ParamControl() override;
    int pid;
    juce::Colour accent;
    MicrovibeProcessor& proc;
};

std::unique_ptr<ParamControl> makeControl (MicrovibeProcessor&, int pid, juce::Colour accent, const juce::String& caption = {});

// a page of controls laid out in a grid
class ParamGrid : public juce::Component
{
public:
    ParamGrid (MicrovibeProcessor& p, juce::Colour a) : proc (p), accent (a) {}
    void setParams (const std::vector<int>& pids, int columns, const juce::StringArray& captions = {});
    void resized() override;
    int columns = 4, rowH = 64;
private:
    MicrovibeProcessor& proc;
    juce::Colour accent;
    std::vector<std::unique_ptr<ParamControl>> controls;
};

// draggable modulation source
class SourceChip : public juce::Component, public juce::SettableTooltipClient
{
public:
    SourceChip (int src, const juce::String& label);
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    int src;
    float value = 0;
private:
    juce::String label;
};

// tab strip
class Tabs : public juce::Component
{
public:
    Tabs (const juce::StringArray& names, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    int selected = 0;
    std::function<void (int)> onChange;
    void select (int i) { selected = i; repaint(); if (onChange) onChange (i); }
private:
    juce::StringArray names; juce::Colour accent;
};

// ----------------------------------------------------------------------------------------------- lanes
class LaneView : public juce::Component
{
public:
    explicit LaneView (MicrovibeProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    int selectedLane = 0;
    std::function<void (int)> onSelectLane;
private:
    juce::Rectangle<int> rowArea (int l) const;
    juce::Rectangle<float> cell (int l, int s, int steps) const;
    bool gateOf (int l, int s) const;
    void toGrid (int l);
    MicrovibeProcessor& proc;
    int dragLane = -1, dragGate = -1;
};

// ----------------------------------------------------------------------------------------------- fx chain
class FxSlot : public juce::Component, public juce::DragAndDropTarget
{
public:
    FxSlot (MicrovibeProcessor&, int index);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;
    int index; bool selected = false;
    std::function<void (int)> onSelect;
private:
    MicrovibeProcessor& proc;
};

// ----------------------------------------------------------------------------------------------- modulation
class ShapeView : public juce::Component
{
public:
    explicit ShapeView (MicrovibeProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override { edit (e); }
    void mouseDrag (const juce::MouseEvent& e) override { edit (e); }
    int lfo = 0;
private:
    void edit (const juce::MouseEvent&);
    MicrovibeProcessor& proc;
};

class DestButton : public juce::Component
{
public:
    DestButton (MicrovibeProcessor& p, int id) : proc (p), pid (id) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    MicrovibeProcessor& proc; int pid;
};

class MatrixRow : public juce::Component
{
public:
    MatrixRow (MicrovibeProcessor&, int slot);
    void resized() override;
    void paint (juce::Graphics&) override;
    int slot;
private:
    MicrovibeProcessor& proc;
    std::unique_ptr<ParamControl> src, via, curve;
    juce::Slider amtSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amtAtt;
    DestButton dst;
    juce::TextButton clear { "x" };
};

// scale degrees of the current scale, toggled for one drone chord (TEXTURE chord = Scale)
class DegreeGrid : public juce::Component
{
public:
    explicit DegreeGrid (MicrovibeProcessor& p) : proc (p) {}
    void setScale (const tuning::Scale& s, int rootNote) { scale = s; root = rootNote; repaint(); }
    int preferredHeight (int width) const;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    int snap = 0;
private:
    int count() const { return juce::jmin (64, juce::jmax (1, scale.size)); }
    int columns() const { return count() <= 12 ? count() : (count() <= 24 ? 12 : 16); }
    juce::Rectangle<float> cell (int d) const;
    juce::String label (int d) const;
    MicrovibeProcessor& proc;
    tuning::Scale scale;
    int root = 0;
};

class ModPanel : public juce::Component
{
public:
    explicit ModPanel (MicrovibeProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void showPage (int page);
    void refresh();
    Tabs tabs;
private:
    void buildPage();
    MicrovibeProcessor& proc;
    int page = 0, sub = 0;
    std::vector<std::unique_ptr<juce::TextButton>> selectors;
    std::vector<std::unique_ptr<SourceChip>> chips;
    ParamGrid grid;
    ShapeView shape;
    juce::Viewport viewport;
    juce::Component matrixHolder;
    std::vector<std::unique_ptr<MatrixRow>> rows;
    juce::TextButton loadScl { "CARICA .SCL" }, loadKbm { "+ .KBM" };
    juce::Label scaleInfo;
    // drone chords (TUNING page, visible when TEXTURE chord = Scale)
    juce::Label droneTitle, droneHint;
    std::vector<std::unique_ptr<juce::TextButton>> snapButtons;
    DegreeGrid degrees;
    ParamGrid prGrid;
    bool droneShown = false; int shownScale = -1, shownRoot = -1, shownSize = -1;
    void updateDrone (bool force);
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File pendingScl;
};

// ----------------------------------------------------------------------------------------------- editor
class MicrovibeEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer
{
public:
    explicit MicrovibeEditor (MicrovibeProcessor&);
    ~MicrovibeEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void showTab (const juce::String& spec);          // "src:N", "ctr:N", "fx:N", "mod:N" (tests / snapshots)
    static constexpr int W = 1280, H = 790;
private:
    void timerCallback() override;
    void showSource (int i);
    void showCentre (int i);
    void showFx (int slot);
    void showLaneParams();
    void refreshPresetBox();

    MicrovibeProcessor& proc;
    MvLook look;
    juce::Component content;

    // header
    juce::ComboBox presetBox;
    juce::TextButton prev { "<" }, next { ">" };
    std::vector<std::unique_ptr<ParamControl>> headerControls;
    std::vector<std::unique_ptr<SourceChip>> macroChips;

    // sources
    Tabs srcTabs;
    ParamGrid srcGrid;
    std::vector<std::unique_ptr<juce::TextButton>> voiceButtons;
    int srcPage = 0, voice = 0;

    // centre
    LaneView lanes;
    Tabs ctrTabs;
    ParamGrid ctrGrid;
    juce::TextButton laneRandom { "RANDOM" }, laneClear { "CLEAR" }, patCopy { "COPIA PATTERN IN TUTTI" };
    int ctrPage = 0;

    // fx
    std::vector<std::unique_ptr<FxSlot>> slots;
    ParamGrid fxGrid;
    int fxSel = 0;
    juce::Label fxTitle;

    // modulation
    ModPanel modPanel;

    juce::Rectangle<int> rHeader, rSrc, rCtr, rFx, rMod;
    juce::ComponentBoundsConstrainer constrainer;
    juce::TooltipWindow tooltips { this, 600 };
    int shownPreset = -2;
    float meterL = 0, meterR = 0;
};
