#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace gc::ui {

// Palette: black / graphite, one warm accent, per-macro colours for the knobs.
namespace col {
    const juce::Colour bg        { 0xff0f1012 };
    const juce::Colour panel     { 0xff17181b };
    const juce::Colour panel2    { 0xff1e1f23 };
    const juce::Colour raised    { 0xff26282d };
    const juce::Colour outline   { 0xff2e3036 };
    const juce::Colour outlineHi { 0xff40434b };
    const juce::Colour text      { 0xffdcdee3 };
    const juce::Colour dim       { 0xff8b8f99 };
    const juce::Colour faint     { 0xff5a5e68 };
    const juce::Colour accent    { 0xffe8a33d };
    const juce::Colour ok        { 0xff5fcf80 };
    const juce::Colour warn      { 0xffe8c13d };
    const juce::Colour bad       { 0xffe5534b };
    const juce::Colour lcd       { 0xff0b0c0d };
}

juce::Font uiFont (float height, bool bold = false);
juce::Font monoFont (float height, bool bold = false);

class GCLookAndFeel : public juce::LookAndFeel_V4 {
public:
    GCLookAndFeel();

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float min, float max, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart, int thumbSize, bool over, bool down) override;
    void drawTableHeaderBackground (juce::Graphics&, juce::TableHeaderComponent&) override;
    void drawTableHeaderColumn (juce::Graphics&, juce::TableHeaderComponent&, const juce::String& name, int id, int w, int h, bool over, bool down, int flags) override;
    void drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics&, const juce::Path&, juce::Image&) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowTitleFont() override;
};

/** Rounded panel with a small caps title, used by every section of the main window. */
void drawSection (juce::Graphics&, juce::Rectangle<int> area, const juce::String& title, const juce::String& subtitle = {});

/** Small rounded status pill: coloured dot + text. */
void drawPill (juce::Graphics&, juce::Rectangle<float> r, juce::Colour dot, const juce::String& label, const juce::String& value);

} // namespace gc::ui
