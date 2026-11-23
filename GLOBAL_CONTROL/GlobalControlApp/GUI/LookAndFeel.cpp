#include "LookAndFeel.h"

namespace gc::ui {

juce::Font uiFont (float h, bool bold)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}

juce::Font monoFont (float h, bool bold)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, bold ? juce::Font::bold : juce::Font::plain));
}

GCLookAndFeel::GCLookAndFeel()
{
    using namespace juce;
    auto scheme = LookAndFeel_V4::getDarkColourScheme();
    scheme.setUIColour (ColourScheme::windowBackground, col::bg);
    scheme.setUIColour (ColourScheme::widgetBackground, col::panel2);
    scheme.setUIColour (ColourScheme::menuBackground, col::panel2);
    scheme.setUIColour (ColourScheme::outline, col::outline);
    scheme.setUIColour (ColourScheme::defaultText, col::text);
    scheme.setUIColour (ColourScheme::defaultFill, col::accent);
    scheme.setUIColour (ColourScheme::highlightedText, Colours::black);
    scheme.setUIColour (ColourScheme::highlightedFill, col::accent);
    scheme.setUIColour (ColourScheme::menuText, col::text);
    setColourScheme (scheme);

    setColour (ResizableWindow::backgroundColourId, col::bg);
    setColour (TextButton::buttonColourId, col::raised);
    setColour (TextButton::buttonOnColourId, col::accent);
    setColour (TextButton::textColourOffId, col::text);
    setColour (TextButton::textColourOnId, Colours::black);
    setColour (TextEditor::backgroundColourId, col::lcd);
    setColour (TextEditor::outlineColourId, col::outline);
    setColour (TextEditor::focusedOutlineColourId, col::accent.withAlpha (0.7f));
    setColour (TextEditor::textColourId, col::text);
    setColour (TextEditor::highlightColourId, col::accent.withAlpha (0.35f));
    setColour (CaretComponent::caretColourId, col::accent);
    setColour (ComboBox::backgroundColourId, col::raised);
    setColour (ComboBox::outlineColourId, col::outline);
    setColour (ComboBox::textColourId, col::text);
    setColour (ComboBox::arrowColourId, col::dim);
    setColour (PopupMenu::backgroundColourId, col::panel2);
    setColour (PopupMenu::highlightedBackgroundColourId, col::accent.withAlpha (0.85f));
    setColour (PopupMenu::highlightedTextColourId, Colours::black);
    setColour (ListBox::backgroundColourId, Colours::transparentBlack);
    setColour (ListBox::outlineColourId, Colours::transparentBlack);
    setColour (TableHeaderComponent::backgroundColourId, col::panel2);
    setColour (TableHeaderComponent::textColourId, col::dim);
    setColour (TableHeaderComponent::outlineColourId, col::outline);
    setColour (Label::textColourId, col::text);
    setColour (ToggleButton::textColourId, col::text);
    setColour (ToggleButton::tickColourId, col::accent);
    setColour (Slider::thumbColourId, col::accent);
    setColour (Slider::trackColourId, col::accent);
    setColour (Slider::backgroundColourId, col::lcd);
    setColour (Slider::textBoxTextColourId, col::text);
    setColour (Slider::textBoxBackgroundColourId, col::lcd);
    setColour (Slider::textBoxOutlineColourId, col::outline);
    setColour (AlertWindow::backgroundColourId, col::panel);
    setColour (AlertWindow::textColourId, col::text);
    setColour (AlertWindow::outlineColourId, col::outlineHi);
    setColour (ScrollBar::thumbColourId, col::outlineHi);
    setColour (TooltipWindow::backgroundColourId, col::raised);
    setColour (TooltipWindow::textColourId, col::text);
    setColour (TooltipWindow::outlineColourId, col::outlineHi);
}

void GCLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    auto c = on ? b.findColour (juce::TextButton::buttonOnColourId) : base;
    if (! b.isEnabled()) c = c.withMultipliedAlpha (0.45f);
    else if (down) c = c.brighter (0.15f);
    else if (over) c = c.brighter (0.07f);
    g.setGradientFill (juce::ColourGradient (c.brighter (0.05f), 0, r.getY(), c.darker (0.12f), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? c.brighter (0.3f) : col::outlineHi.withAlpha (over ? 1.0f : 0.7f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

juce::Font GCLookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return uiFont (juce::jmin (13.0f, h * 0.5f), true); }

void GCLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat();
    auto box = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.22f);
    g.setColour (col::lcd);
    g.fillRoundedRectangle (box, 3.0f);
    g.setColour (b.getToggleState() ? col::accent : (over ? col::outlineHi.brighter() : col::outlineHi));
    g.drawRoundedRectangle (box, 3.0f, 1.0f);
    if (b.getToggleState()) g.fillRoundedRectangle (box.reduced (3.5f), 1.5f);
    g.setColour (b.isEnabled() ? col::text : col::faint);
    g.setFont (uiFont (13.0f));
    g.drawFittedText (b.getButtonText(), r.toNearestInt().withTrimmedLeft (4), juce::Justification::centredLeft, 1);
}

void GCLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (juce::Rectangle<float> (0, 0, (float) w, (float) h), 4.0f);
}

void GCLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.hasKeyboardFocus (true) ? e.findColour (juce::TextEditor::focusedOutlineColourId) : e.findColour (juce::TextEditor::outlineColourId));
    g.drawRoundedRectangle (juce::Rectangle<float> (0.5f, 0.5f, (float) w - 1, (float) h - 1), 4.0f, 1.0f);
}

void GCLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.isMouseOver (true) ? col::outlineHi.brighter (0.2f) : col::outlineHi);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    juce::Path p;
    const float cx = (float) w - 12.0f, cy = (float) h * 0.5f;
    p.addTriangle (cx - 4, cy - 2, cx + 4, cy - 2, cx, cy + 3);
    g.setColour (col::dim);
    g.fillPath (p);
}

juce::Font GCLookAndFeel::getComboBoxFont (juce::ComboBox&) { return uiFont (13.0f); }
juce::Font GCLookAndFeel::getPopupMenuFont() { return uiFont (14.0f); }
juce::Font GCLookAndFeel::getLabelFont (juce::Label& l) { return l.getFont(); }
juce::Font GCLookAndFeel::getAlertWindowMessageFont() { return uiFont (14.0f); }
juce::Font GCLookAndFeel::getAlertWindowTitleFont() { return uiFont (17.0f, true); }

void GCLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& s)
{
    auto track = juce::Rectangle<float> ((float) x, (float) y + h * 0.5f - 2.0f, (float) w, 4.0f);
    g.setColour (col::lcd);
    g.fillRoundedRectangle (track, 2.0f);
    g.setColour (s.findColour (juce::Slider::trackColourId));
    g.fillRoundedRectangle (track.withRight (pos), 2.0f);
    g.setColour (col::text);
    g.fillEllipse (juce::Rectangle<float> (10, 10).withCentre ({ pos, track.getCentreY() }));
}

void GCLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int ts, int tsz, bool over, bool down)
{
    auto r = vertical ? juce::Rectangle<int> (x + w / 2 - 2, ts, 4, tsz) : juce::Rectangle<int> (ts, y + h / 2 - 2, tsz, 4);
    g.setColour (col::outlineHi.brighter (over || down ? 0.4f : 0.0f));
    g.fillRoundedRectangle (r.toFloat(), 2.0f);
}

void GCLookAndFeel::drawTableHeaderBackground (juce::Graphics& g, juce::TableHeaderComponent& h)
{
    g.setColour (col::panel2);
    g.fillRect (h.getLocalBounds());
    g.setColour (col::outline);
    g.fillRect (h.getLocalBounds().removeFromBottom (1));
}

void GCLookAndFeel::drawTableHeaderColumn (juce::Graphics& g, juce::TableHeaderComponent&, const juce::String& name, int, int w, int h, bool over, bool, int)
{
    g.setColour (over ? col::text : col::dim);
    g.setFont (uiFont (11.0f, true));
    g.drawFittedText (name.toUpperCase(), 6, 0, w - 8, h, juce::Justification::centredLeft, 1);
}

void GCLookAndFeel::drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics& g, const juce::Path& path, juce::Image&)
{
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillPath (path, juce::AffineTransform::translation (0, 3));
    g.setColour (col::panel);
    g.fillPath (path);
    g.setColour (col::outlineHi);
    g.strokePath (path, juce::PathStrokeType (1.0f));
}

void drawSection (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title, const juce::String& subtitle)
{
    auto r = area.toFloat();
    g.setColour (col::panel);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (col::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
    auto head = area.removeFromTop (30).reduced (12, 0);
    g.setColour (col::dim);
    g.setFont (uiFont (11.5f, true));
    const auto t = title.toUpperCase();
    g.drawText (t, head, juce::Justification::centredLeft);
    if (subtitle.isNotEmpty()) {
        const int tw = (int) juce::GlyphArrangement::getStringWidth (uiFont (11.5f, true), t) + 10;
        g.setColour (col::text);
        g.setFont (uiFont (12.5f, true));
        g.drawText (subtitle, head.withTrimmedLeft (tw), juce::Justification::centredLeft);
    }
}

void drawPill (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour dot, const juce::String& label, const juce::String& value)
{
    g.setColour (col::panel2);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (col::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), r.getHeight() * 0.5f, 1.0f);
    auto in = r.reduced (10.0f, 0);
    auto d = in.removeFromLeft (8.0f).withSizeKeepingCentre (8.0f, 8.0f);
    g.setColour (dot.withAlpha (0.25f));
    g.fillEllipse (d.expanded (3.0f));
    g.setColour (dot);
    g.fillEllipse (d);
    in.removeFromLeft (8.0f);
    g.setColour (col::dim);
    g.setFont (uiFont (10.5f, true));
    const auto lab = label.toUpperCase();
    const float lw = juce::GlyphArrangement::getStringWidth (uiFont (10.5f, true), lab) + 6.0f;
    g.drawText (lab, in.removeFromLeft (lw), juce::Justification::centredLeft);
    g.setColour (col::text);
    g.setFont (uiFont (12.5f, true));
    g.drawFittedText (value, in.toNearestInt(), juce::Justification::centredLeft, 1);
}

} // namespace gc::ui
