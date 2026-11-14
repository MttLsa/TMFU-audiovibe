#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

// =====================================================================================
//  Visual language: flat, minimal, dark — in the spirit of Ableton Live's device view.
// =====================================================================================
namespace theme
{
    const juce::Colour bg        { 0xff1e1e1e };
    const juce::Colour panel     { 0xff2a2a2a };
    const juce::Colour panel2    { 0xff333333 };
    const juce::Colour header    { 0xff3a3a3a };
    const juce::Colour outline   { 0xff121212 };
    const juce::Colour track     { 0xff4a4a4a };
    const juce::Colour text      { 0xffd0d0d0 };
    const juce::Colour textDim   { 0xff8c8c8c };
    const juce::Colour accentA   { 0xffff9a3c };   // Section A — orange
    const juce::Colour accentB   { 0xffb4dc3c };   // Section B — lime
    const juce::Colour accentG   { 0xff5cc8e6 };   // Global   — cyan
    const juce::Colour stepOff   { 0xff3c3c3c };
    const juce::Colour stepBeat  { 0xff464646 };

    inline juce::Font font (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }
}

class TwinLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TwinLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return theme::font (12.0f); }
    juce::Font getPopupMenuFont() override { return theme::font (13.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return theme::font (juce::jmin (12.5f, h * 0.6f)); }
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawDocumentWindowTitleBar (juce::DocumentWindow&, juce::Graphics&, int w, int h, int, int, const juce::Image*, bool) override;
};

// -------------------------------------------------------------------------------------
//  Knob: rotary slider + caption + value readout. Re-attachable to another parameter.
// -------------------------------------------------------------------------------------
class Knob : public juce::Component
{
public:
    Knob (const juce::String& caption = {}, juce::Colour accent = theme::accentA);
    void attach (juce::AudioProcessorValueTreeState& s, const juce::String& paramId);
    void detach() { att.reset(); }
    void setCaption (const juce::String& c) { caption = c; repaint(); }
    void setAccent (juce::Colour c);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;

private:
    juce::String caption;
    juce::Colour accent;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// -------------------------------------------------------------------------------------
//  Segmented selector for a choice / bool / int parameter (e.g. LP | BP | HP)
// -------------------------------------------------------------------------------------
class Segmented : public juce::Component, public juce::SettableTooltipClient
{
public:
    Segmented (juce::Colour accentColour = theme::accentA, bool isVertical = false) : accent (accentColour), vertical (isVertical) {}
    void attach (juce::AudioProcessorValueTreeState& s, const juce::String& paramId, juce::StringArray labels = {});
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void setCaption (const juce::String& c) { caption = c; }
    void setVertical (bool v) { vertical = v; repaint(); }
    void setAccent (juce::Colour c) { accent = c; repaint(); }

private:
    juce::Colour accent;
    bool vertical;
    juce::String caption;
    juce::StringArray labels;
    int index = 0;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> att;
    juce::Rectangle<int> area() const;
};

// -------------------------------------------------------------------------------------
//  Toggle bound to a bool parameter
// -------------------------------------------------------------------------------------
class Toggle : public juce::TextButton
{
public:
    Toggle (const juce::String& text, juce::Colour accent = theme::accentA);
    void attach (juce::AudioProcessorValueTreeState& s, const juce::String& paramId);
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

// -------------------------------------------------------------------------------------
//  Titled panel with optional "expand" button (opens a large editor window)
// -------------------------------------------------------------------------------------
class Panel : public juce::Component
{
public:
    Panel (const juce::String& title, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Rectangle<int> content() const { return getLocalBounds().withTrimmedTop (headerH).reduced (6, 4); }
    // content area in the parent's coordinate space (for controls that are siblings of the panel)
    juce::Rectangle<int> contentInParent() const { return content() + getPosition(); }
    void setExpandable (std::function<void()> onExpand);
    void setTitle (const juce::String& t) { title = t; repaint(); }
    static constexpr int headerH = 20;
protected:
    juce::String title;
    juce::Colour accent;
    juce::TextButton expandBtn { juce::String::fromUTF8 ("\xe2\xa4\xa2") };
};

// -------------------------------------------------------------------------------------
//  Separate resizable window hosting a large editor component
// -------------------------------------------------------------------------------------
class ExpandWindow : public juce::DocumentWindow
{
public:
    ExpandWindow (const juce::String& name, juce::Component* content, int w, int h, std::function<void()> onClose);
    void closeButtonPressed() override { if (onCloseFn) onCloseFn(); }
private:
    std::function<void()> onCloseFn;
};

// small helper: draws a simple horizontal level meter
void drawMeter (juce::Graphics& g, juce::Rectangle<float> r, float level, juce::Colour c);

// Lays out components in rows of equal cells. Segmented/Toggle controls are sized sensibly.
inline void layoutRows (juce::Rectangle<int> r, const std::vector<std::vector<juce::Component*>>& rows, int columns = 0)
{
    if (rows.empty()) return;
    int cols = columns;
    for (auto& row : rows) cols = juce::jmax (cols, (int) row.size());
    const int rowH = r.getHeight() / (int) rows.size();
    for (size_t ri = 0; ri < rows.size(); ++ri)
    {
        auto rowR = r.removeFromTop (rowH);
        const float cw = (float) rowR.getWidth() / (float) cols;
        for (size_t ci = 0; ci < rows[ri].size(); ++ci)
        {
            auto* c = rows[ri][ci];
            if (c == nullptr) continue;
            auto cell = juce::Rectangle<int> (rowR.getX() + juce::roundToInt (cw * (float) ci), rowR.getY(), juce::roundToInt (cw), rowR.getHeight()).reduced (3, 2);
            if (dynamic_cast<juce::TextButton*> (c) != nullptr)
                cell = cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), 90), juce::jmin (cell.getHeight(), 24));
            else if (dynamic_cast<Segmented*> (c) != nullptr)
                cell = cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), 110), juce::jmin (cell.getHeight(), 140));
            c->setBounds (cell);
        }
    }
}
