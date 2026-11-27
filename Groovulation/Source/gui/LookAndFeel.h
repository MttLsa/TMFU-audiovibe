#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

// =====================================================================================
//  Visual language: the flat Twinbox layout, coloured like a vintage MPC 60
//  (warm grey case, cream panels, charcoal strips, yellow-green backlit LCD, red / amber accents).
// =====================================================================================
namespace theme
{
    const juce::Colour bg        { 0xff8f8879 };   // case
    const juce::Colour panel     { 0xffd9d1be };   // cream face
    const juce::Colour panel2    { 0xffc5bca7 };   // insets, buttons
    const juce::Colour header    { 0xff45413a };   // charcoal strips
    const juce::Colour headerText{ 0xffece5d2 };
    const juce::Colour outline   { 0xff5d584e };
    const juce::Colour track     { 0xffa59d8b };
    const juce::Colour text      { 0xff2a2723 };
    const juce::Colour textDim   { 0xff6b6559 };
    const juce::Colour knob      { 0xff3d3a34 };   // knob cap (dark, like the MPC data wheel)
    const juce::Colour lcdBg     { 0xffa7b682 };
    const juce::Colour lcdBg2    { 0xff98a774 };
    const juce::Colour lcdInk    { 0xff25301b };
    const juce::Colour lcdDim    { 0xff5b6946 };
    const juce::Colour red       { 0xffc6402c };   // DRUMS (MPC red)
    const juce::Colour amber     { 0xffd6842a };   // BASS
    const juce::Colour teal      { 0xff4d7c8c };   // MELODIC
    const juce::Colour master    { 0xff6f6a61 };   // pad grey
    const juce::Colour customC   { 0xff7c7466 };   // CUSTOM (warm grey)
    const juce::Colour olive     { 0xff8a7f2e };   // VOCALS / FX
    const juce::Colour white     { 0xfff4f0e6 };

    inline juce::Colour sectionColour (int module)
    {
        return module < 5 ? red : (module == 5 ? customC : (module == 6 ? amber : (module <= 9 ? teal : olive)));
    }

    // Every text in the plugin uses Inter (SIL OFL, embedded via BinaryData: resources/fonts):
    // Regular for normal text, SemiBold for bold text.
    juce::Typeface::Ptr inter (bool bold);
    // The plug-in name ("GROOVULATION") keeps Urbanist Light, as before the switch to Inter.
    juce::Typeface::Ptr urbanist();

    inline juce::Font font (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (inter (bold)).withHeight (h));
    }
    inline juce::Font nameFont (float h)
    {
        return juce::Font (juce::FontOptions (urbanist()).withHeight (h));
    }
    inline juce::Font mono (float h, bool bold = false)   // numbers / LCD readouts: Inter too
    {
        return font (h, bold);
    }
}

class GroovLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GroovLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox& b) override { return theme::font (juce::jlimit (10.0f, 13.0f, b.getHeight() * 0.6f)); }
    void positionComboBoxText (juce::ComboBox& b, juce::Label& label) override
    {
        label.setBounds (2, 1, juce::jmax (8, b.getWidth() - 16), b.getHeight() - 2);
        label.setFont (getComboBoxFont (b));
        label.setBorderSize ({ 0, 3, 0, 0 });
    }
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& f) override { return theme::inter (f.isBold()); }
    juce::Font getPopupMenuFont() override { return theme::font (14.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return theme::font (juce::jmin (13.0f, h * 0.58f), true); }
    void drawDocumentWindowTitleBar (juce::DocumentWindow&, juce::Graphics&, int w, int h, int, int, const juce::Image*, bool) override;
};

// Knob: rotary slider + caption + value (double-click the value to type it, double-click the knob to reset)
class Knob : public juce::Component
{
public:
    Knob (const juce::String& caption = {}, juce::Colour accent = theme::red);
    void attach (juce::AudioProcessorValueTreeState& s, const juce::String& paramId);
    void setCaption (const juce::String& c) { caption = c; repaint(); }
    void setAccent (juce::Colour c);
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    juce::Slider slider;
private:
    juce::String caption;
    juce::Colour accent;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    juce::TextEditor editor;
};

// Segmented selector for a choice / bool parameter (LP | BP | HP)
class Segmented : public juce::Component, public juce::SettableTooltipClient
{
public:
    Segmented (juce::Colour accentColour = theme::red) : accent (accentColour) {}
    void attach (juce::AudioProcessorValueTreeState& s, const juce::String& paramId, juce::StringArray labels = {});
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void setCaption (const juce::String& c) { caption = c; }
    void setAccent (juce::Colour c) { accent = c; repaint(); }
    // inverted look: the selected cell is light, the others grey / dark
    void setInverted (bool i) { inverted = i; repaint(); }
private:
    juce::Colour accent;
    bool inverted = false;
    juce::String caption;
    juce::StringArray labels;
    int index = 0;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> att;
    juce::Rectangle<int> area() const;
};

// Toggle bound to a bool parameter
class Toggle : public juce::TextButton
{
public:
    Toggle (const juce::String& text, juce::Colour accent = theme::red);
    void attach (juce::AudioProcessorValueTreeState& s, const juce::String& paramId);
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

// Titled panel (charcoal header strip with an accent mark)
class Panel : public juce::Component
{
public:
    Panel (const juce::String& title, juce::Colour accent);
    void paint (juce::Graphics&) override;
    juce::Rectangle<int> content() const { return getLocalBounds().withTrimmedTop (headerH).reduced (5, 3); }
    void setTitle (const juce::String& t) { title = t; repaint(); }
    void setAccent (juce::Colour c) { accent = c; repaint(); }
    static constexpr int headerH = 18;
protected:
    juce::String title;
    juce::Colour accent;
};

// Separate resizable window hosting a component
class ExpandWindow : public juce::DocumentWindow
{
public:
    ExpandWindow (const juce::String& name, juce::Component* content, int w, int h, std::function<void()> onClose);
    void closeButtonPressed() override { if (onCloseFn) onCloseFn(); }
private:
    std::function<void()> onCloseFn;
};

void drawMeter (juce::Graphics& g, juce::Rectangle<float> r, float level, juce::Colour c);

// Floppy-disk button (CUSTOM module: load a file)
class FloppyButton : public juce::Button
{
public:
    FloppyButton() : juce::Button ("Load file") { setTooltip ("Load an audio file into CUSTOM (or drag & drop a file onto the module)"); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
};
void drawLcd (juce::Graphics& g, juce::Rectangle<float> r);
void drawGroovLogo (juce::Graphics& g, juce::Rectangle<float> r, float level, double phase);   // backlit LCD background with bezel

// Lays out components in rows of equal cells.
void layoutRow (juce::Rectangle<int> r, const std::vector<juce::Component*>& items, int pad = 2);
