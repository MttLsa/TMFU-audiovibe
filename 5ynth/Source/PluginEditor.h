#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Skins.h"

namespace ui
{
    // Twinbox palette (flat, Ableton-like) + a little relief
    const juce::Colour bg     { 0xff1e1e1e };
    const juce::Colour panel  { 0xff2a2a2a };
    const juce::Colour panel2 { 0xff333333 };
    const juce::Colour header { 0xff3a3a3a };
    const juce::Colour line   { 0xff121212 };
    const juce::Colour track  { 0xff4a4a4a };
    const juce::Colour text   { 0xffd0d0d0 };
    const juce::Colour dim    { 0xff8c8c8c };
    const juce::Colour colA   { 0xffff8a3d };   // analog engine
    const juce::Colour colD   { 0xff35d0ff };   // digital engine
    const juce::Colour colG   { 0xffeeeeee };
    juce::Font font (float h, bool bold = false);
    juce::Typeface::Ptr typeface (bool bold);
    void relief (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour base, float corner, bool raised = true);
}

class FivesynthLook : public juce::LookAndFeel_V4
{
public:
    FivesynthLook();
    void setSkin (const skins::Skin& s);
    const skins::Skin* skin = nullptr;
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return ui::font (11.5f); }
    juce::Font getPopupMenuFont() override { return ui::font (13.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return ui::font (juce::jmin (12.0f, h * 0.55f), true); }
    juce::Font getLabelFont (juce::Label& l) override { return l.getFont(); }
    void positionComboBoxText (juce::ComboBox& b, juce::Label& label) override
    {
        label.setBounds (2, 1, juce::jmax (8, b.getWidth() - 16), b.getHeight() - 2);
        label.setFont (getComboBoxFont (b));
    }
};

// Slider with fine resolution: knobs snap to 0.01, faders to 0.001 (real units); arrows after a click
// move by 1 % (faders 0.1 %) of the range, Shift = 10x finer, long drag distance, double-click = default.
class FineSlider : public juce::Slider
{
public:
    double snapValue (double v, DragMode) override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent& e) override { grabKeyboardFocus(); Slider::mouseDown (e); }
    bool fader = false;
    std::function<juce::String (double)> formatter;
    juce::String getTextFromValue (double v) override { return formatter ? formatter (v) : Slider::getTextFromValue (v); }
};

class Control : public juce::Component
{
public:
    Control (juce::AudioProcessorValueTreeState& s, int pid, const juce::String& label);
    void setSkin (const skins::Skin& s);
    void setCaption (const juce::String& c) { caption = c; repaint(); }
    void setFormatter (std::function<juce::String (double)> f) { knob.formatter = std::move (f); repaint(); }
    // menu limited to the items the original instrument has (indices of the parameter's choices, in that order)
    void restrictChoices (juce::AudioProcessorValueTreeState& st, const std::vector<int>& allowed);
    void resized() override;
    void paint (juce::Graphics&) override;
    int pid;
    bool wide = false, isFader = false, isToggle = false;
    FineSlider knob;
    juce::ComboBox combo;
    juce::TextButton toggle;
private:
    juce::String caption;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sa;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ca;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ba;
    std::unique_ptr<juce::ParameterAttachment> pa;
};

struct GroupDef { juce::String title; std::vector<std::pair<int, juce::String>> items; int cols = 0; int page = -1; int id = 0; int ops = params::numOps; };

// FM algorithm diagram (operators as boxes, carriers on the bottom row)
class AlgDiagram : public juce::Component
{
public:
    void setAlg (int a, juce::Colour c, juce::Colour t) { alg = a; col = c; txt = t; repaint(); }
    void paint (juce::Graphics&) override;
private:
    int alg = 0; juce::Colour col, txt;
};

// One synth module editor (A..E). The look follows the chosen synth model.
class ModulePanel : public juce::Component, private juce::AudioProcessorValueTreeState::Listener, private juce::AsyncUpdater
{
public:
    ModulePanel (FivesynthProcessor&, int module);
    ~ModulePanel() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void refreshSkin();
    int preferredHeight (int width);
    const skins::Skin& currentSkin() const { return *skin; }
private:
    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { refreshSkin(); }
    void applyFmMode();
    void buildGroups();   // the panel of the chosen instrument (original sections / control names) + EXTRA
    int layoutGroups (int width, bool apply);
    int builtModel = -2, builtPreset = -2;
    FivesynthProcessor& proc;
    int module;
    FivesynthLook look;
    const skins::Skin* skin = nullptr;
    std::vector<GroupDef> groups;
    std::vector<std::vector<std::unique_ptr<Control>>> controls;
    std::vector<juce::Rectangle<int>> groupBounds;
    std::unique_ptr<Control> onBtn, engineBox;
    juce::TextButton extraBtn { "EXTRA" };
    AlgDiagram algView;
    juce::Rectangle<int> algArea;
    bool digitalLook = false;
};

// 5-band static EQ with draggable nodes (FabFilter-style display)
class EqView : public juce::Component, private juce::Timer
{
public:
    explicit EqView (FivesynthProcessor&);
    ~EqView() override { setLookAndFeel (nullptr); }
    void makeBig();
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    std::function<void()> onExpand;
    bool big = false;
private:
    void timerCallback() override { repaint(); }
    float xToF (float x) const; float fToX (float f) const; float gToY (float db) const; float yToG (float y) const;
    juce::Rectangle<float> plot() const;
    FivesynthProcessor& proc;
    int dragBand = -1;
    float range = 18.0f;
    FivesynthLook look;
    std::vector<std::unique_ptr<Control>> bandCtl;
};

class LfoPanel : public juce::Component, private juce::Timer
{
public:
    LfoPanel (FivesynthProcessor&, bool showAll);
    void resized() override;
    void paint (juce::Graphics&) override;
    std::function<void()> onExpand;
private:
    void timerCallback() override { repaint(); }
    void select (int l);
    FivesynthProcessor& proc;
    bool all;
    int sel = 0;
    juce::TextButton tabs[params::numLfos], expand { "+" };
    struct Row { std::unique_ptr<juce::ComboBox> target; std::unique_ptr<Control> shape, rate, sync, div, depth, phase;
                 std::unique_ptr<juce::ParameterAttachment> tAtt; };
    std::vector<Row> rows;
    FivesynthLook look;
};

class PresetBrowser : public juce::Component, private juce::ListBoxModel
{
public:
    explicit PresetBrowser (FivesynthProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    std::function<void()> onClose;
private:
    int getNumRows() override { return (int) rows.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int, const juce::MouseEvent&) override { if (onClose) onClose(); }
    void apply();
    FivesynthProcessor& proc;
    juce::ListBox list { "presets", this };
    juce::TextEditor search;
    juce::TextButton close { "CLOSE" };
    juce::OwnedArray<juce::TextButton> decadeBtns;
    std::vector<int> rows;
    juce::Label info;
};

// 5ynth logo, drawn as vectors (sharp at any size): "5YNTH" in brushed metal whose "5" carries a stylised keyboard
// and five glowing bars, inside a frame that follows the letters' outline
namespace logo
{
    void drawWordmark (juce::Graphics& g, juce::Rectangle<float> r, juce::RectanglePlacement place = juce::RectanglePlacement::centred);
}

// full-size logo shown over the editor (click on the header logo); any click closes it
class LogoSplash : public juce::Component
{
public:
    LogoSplash() { setInterceptsMouseClicks (true, false); setComponentID ("logoSplash"); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { setVisible (false); }
};

class FivesynthEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FivesynthEditor (FivesynthProcessor&);
    ~FivesynthEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    void timerCallback() override;
    void updateHeader();
    void drawLogo (juce::Graphics&);
    void selectModule (int m);
    void layoutSlots();
    void showOverlay (juce::Component* c);
    FivesynthProcessor& proc;
    FivesynthLook look;
    juce::Component content;
    std::unique_ptr<ModulePanel> panels[params::numModules];
    juce::Viewport moduleView;
    struct SlotCard : public juce::Component { int m = 0; std::function<void()> onSelect; void mouseDown (const juce::MouseEvent&) override { if (onSelect) onSelect(); } };
    SlotCard cards[params::numModules];
    std::unique_ptr<Control> cardOn[params::numModules], cardModel[params::numModules], cardLevel[params::numModules], cardPan[params::numModules];
    juce::TextButton cardRemove[params::numModules];
    std::unique_ptr<ModulePanel> dummy;
    std::vector<std::unique_ptr<Control>> globals;
    std::vector<juce::Rectangle<int>> globalGroups;
    juce::StringArray globalTitles;
    EqView eqSmall, eqBig;
    LfoPanel lfoSmall, lfoBig;
    juce::TextButton eqExpand { "+" }, overlayClose { "CLOSE" };
    juce::Component* overlay = nullptr;
    juce::MidiKeyboardComponent keys;
    PresetBrowser browser;
    juce::TextButton prev { "<" }, next { ">" }, browse { "BROWSER" }, custom { "+ CUSTOM" };
    juce::Label presetName, presetInfo;
    int shownPreset = -2, selected = 0, shownSlots = -1;
    juce::Rectangle<float> logoArea;
    LogoSplash splash;
    // BW x BH = reference layout. The window can take any size (full screen too): the UI is scaled by
    // uiScale = min (width / BW, height / BH) and the logical area W x H grows on the other side.
    static constexpr int BW = 1400, BH = 900;
    int W = BW, H = BH;
    float uiScale = 1.0f;
};
