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

class FamPatLook : public juce::LookAndFeel_V4
{
public:
    FamPatLook();
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

class PresetBrowser : public juce::Component, private juce::ListBoxModel
{
public:
    explicit PresetBrowser (FamPatProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    std::function<void()> onClose;
private:
    int getNumRows() override { return (int) rows.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int, const juce::MouseEvent&) override { if (onClose) onClose(); }
    void selectedRowsChanged (int lastRowSelected) override;   // arrow keys in the list load the preset too
    void returnKeyPressed (int) override { if (onClose) onClose(); }
    void loadRow (int row);
    bool syncing = false;   // refresh() selecting the current preset: do not reload it
    void apply();
    bool userMode() const { return userBtn.getToggleState(); }
    juce::TextButton userBtn { "USER" };
    juce::ComboBox sortBox;   // Year / Song / Preset / Artist
    juce::Array<juce::File> userFiles;
    FamPatProcessor& proc;
    juce::ListBox list { "presets", this };
    juce::TextEditor search;
    juce::TextButton close { "CLOSE" };
    juce::OwnedArray<juce::TextButton> decadeBtns;
    std::vector<int> rows;
    juce::Label info;
};

// "SAVE PRESET" window: name of the user preset + the existing ones (click one to overwrite it)
class SavePresetWindow : public juce::Component, private juce::ListBoxModel
{
public:
    explicit SavePresetWindow (FamPatProcessor&);
    void open();
    void paint (juce::Graphics&) override;
    void resized() override;
    std::function<void()> onDone;
private:
    int getNumRows() override { return files.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void save();
    FamPatProcessor& proc;
    juce::TextEditor name;
    juce::ListBox list { "user presets", this };
    juce::TextButton saveBtn { "SAVE" }, cancelBtn { "CANCEL" };
    juce::Label status;
    juce::Array<juce::File> files;
};

// FamPat logo: "FAMPAT" in Rostex Regular + an orange "play" triangle (vector, sharp at any size)
namespace logo
{
    void drawFamPat (juce::Graphics& g, juce::Rectangle<float> r, juce::RectanglePlacement place = juce::RectanglePlacement::centred);
}

// full-size logo shown over the editor (click on the header logo); any click closes it
class LogoSplash : public juce::Component
{
public:
    LogoSplash() { setInterceptsMouseClicks (true, false); setComponentID ("logoSplash"); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { setVisible (false); }
};
