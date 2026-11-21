#include "LookAndFeel.h"
#include "BinaryData.h"

using namespace juce;

Typeface::Ptr theme::inter (bool bold)
{
    static const Typeface::Ptr regular = Typeface::createSystemTypefaceFor (BinaryData::InterRegular_ttf, (size_t) BinaryData::InterRegular_ttfSize);
    static const Typeface::Ptr semi    = Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_ttf, (size_t) BinaryData::InterSemiBold_ttfSize);
    return bold ? semi : regular;
}

Typeface::Ptr theme::urbanist()
{
    static const Typeface::Ptr light = Typeface::createSystemTypefaceFor (BinaryData::UrbanistLight_ttf, (size_t) BinaryData::UrbanistLight_ttfSize);
    return light;
}

GroovLookAndFeel::GroovLookAndFeel()
{
    setColour (ResizableWindow::backgroundColourId, theme::bg);
    setColour (DocumentWindow::backgroundColourId, theme::bg);
    setColour (Label::textColourId, theme::text);
    setColour (TextButton::buttonColourId, theme::panel2);
    setColour (TextButton::buttonOnColourId, theme::red);
    setColour (TextButton::textColourOffId, theme::text);
    setColour (TextButton::textColourOnId, theme::white);
    setColour (ComboBox::backgroundColourId, theme::panel2);
    setColour (ComboBox::textColourId, theme::text);
    setColour (ComboBox::outlineColourId, theme::outline);
    setColour (ComboBox::arrowColourId, theme::textDim);
    setColour (PopupMenu::backgroundColourId, theme::panel);
    setColour (PopupMenu::textColourId, theme::text);
    setColour (PopupMenu::headerTextColourId, theme::textDim);
    setColour (PopupMenu::highlightedBackgroundColourId, theme::header);
    setColour (PopupMenu::highlightedTextColourId, theme::headerText);
    setColour (TooltipWindow::backgroundColourId, theme::header);
    setColour (TooltipWindow::textColourId, theme::headerText);
    setColour (TooltipWindow::outlineColourId, theme::outline);
    setColour (TextEditor::backgroundColourId, theme::white);
    setColour (TextEditor::textColourId, theme::text);
    setColour (TextEditor::highlightColourId, theme::red.withAlpha (0.3f));
    setColour (TextEditor::outlineColourId, theme::outline);
    setColour (TextEditor::focusedOutlineColourId, theme::red);
    setColour (CaretComponent::caretColourId, theme::text);
    setColour (ScrollBar::thumbColourId, theme::track);
    setColour (AlertWindow::backgroundColourId, theme::panel);
    setColour (AlertWindow::textColourId, theme::text);
    setColour (AlertWindow::outlineColourId, theme::outline);
}

void GroovLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    const auto bounds = Rectangle<int> (x, y, w, h).toFloat().reduced (2.0f);
    const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float lineW = jmax (2.0f, radius * 0.17f);
    const float arcR = radius - lineW * 0.5f;
    const auto accent = s.findColour (Slider::rotarySliderFillColourId);
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;

    Path bgArc;
    bgArc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, start, end, true);
    g.setColour (theme::track);
    g.strokePath (bgArc, PathStrokeType (lineW, PathStrokeType::curved, PathStrokeType::butt));

    const float angle = start + pos * (end - start);
    const float from = bipolar ? (start + end) * 0.5f : start;
    Path valArc;
    valArc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, jmin (from, angle), jmax (from, angle), true);
    g.setColour (s.isEnabled() ? accent : theme::textDim);
    g.strokePath (valArc, PathStrokeType (lineW, PathStrokeType::curved, PathStrokeType::butt));

    // dark cap with a cream pointer (MPC-style)
    const float innerR = arcR - lineW * 1.15f;
    g.setColour (theme::knob);
    g.fillEllipse (c.x - innerR, c.y - innerR, innerR * 2, innerR * 2);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.fillEllipse (c.x - innerR * 0.8f, c.y - innerR * 0.9f, innerR * 1.6f, innerR * 1.1f);
    Path ptr;
    ptr.startNewSubPath (c.getPointOnCircumference (innerR * 0.2f, angle));
    ptr.lineTo (c.getPointOnCircumference (innerR * 0.92f, angle));
    g.setColour (theme::white);
    g.strokePath (ptr, PathStrokeType (jmax (1.5f, lineW * 0.55f), PathStrokeType::curved, PathStrokeType::rounded));
}

void GroovLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour& bgc, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    Colour c = b.getToggleState() ? b.findColour (TextButton::buttonOnColourId) : bgc;
    if (down) c = c.darker (0.12f);
    else if (over) c = c.brighter (0.06f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 3.0f);
    // MPC-style rubber key: light top edge, dark bottom edge
    g.setColour (Colours::white.withAlpha (0.25f));
    g.drawLine (r.getX() + 3, r.getY() + 1.0f, r.getRight() - 3, r.getY() + 1.0f, 1.0f);
    g.setColour (theme::outline);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
}

void GroovLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.findColour (b.getToggleState() ? TextButton::textColourOnId : TextButton::textColourOffId)
                  .withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.45f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (2, 1), Justification::centred, 1, 0.7f);
}

void GroovLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    g.setColour (box.findColour (ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (theme::outline);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    Path p;
    const float ax = (float) w - 8.0f, ay = (float) h * 0.5f;
    p.addTriangle (ax - 3, ay - 1.5f, ax + 3, ay - 1.5f, ax, ay + 2.5f);
    g.setColour (theme::textDim);
    g.fillPath (p);
}

void GroovLookAndFeel::drawDocumentWindowTitleBar (DocumentWindow& win, Graphics& g, int w, int h, int, int, const Image*, bool)
{
    g.fillAll (theme::header);
    g.setColour (theme::red);
    g.fillRect (10, h / 2 - 5, 4, 10);
    g.setColour (theme::headerText);
    g.setFont (theme::font (14.0f, true));
    g.drawText (win.getName(), 20, 0, w - 80, h, Justification::centredLeft);
}

//==============================================================================
Knob::Knob (const String& c, Colour a) : caption (c), accent (a)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setColour (Slider::rotarySliderFillColourId, accent);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
    slider.setMouseDragSensitivity (240);
    slider.setVelocityModeParameters (0.4, 1, 0.0, true, ModifierKeys::shiftModifier);   // shift = fine
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
    editor.setJustification (Justification::centred);
    editor.setFont (theme::font (12.0f));
    editor.onReturnKey = [this]
    {
        if (param != nullptr)
        {
            const float v = param->getValueForText (editor.getText().retainCharacters ("0123456789.-+"));
            param->beginChangeGesture();
            param->setValueNotifyingHost (jlimit (0.0f, 1.0f, v));
            param->endChangeGesture();
        }
        editor.setVisible (false);
    };
    editor.onEscapeKey = [this] { editor.setVisible (false); };
    editor.onFocusLost = [this] { editor.setVisible (false); };
    addChildComponent (editor);
}

void Knob::setAccent (Colour c)
{
    accent = c;
    slider.setColour (Slider::rotarySliderFillColourId, c);
    repaint();
}

void Knob::attach (AudioProcessorValueTreeState& s, const String& id)
{
    att.reset();
    param = s.getParameter (id);
    if (param == nullptr) return;
    att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    slider.setTooltip (param->getName (64) + "  (shift = fine, double-click = reset, double-click the value = type)");
    if (caption.isEmpty()) caption = param->getName (32);
    repaint();
}

void Knob::resized()
{
    auto r = getLocalBounds();
    const int capH = jlimit (10, 15, getHeight() / 6);
    r.removeFromTop (capH);
    r.removeFromBottom (capH);
    const int d = jmin (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (d, d));
    editor.setBounds (getLocalBounds().removeFromBottom (capH + 4));
}

void Knob::mouseDoubleClick (const MouseEvent& e)
{
    if (param == nullptr || e.y < getHeight() - jlimit (10, 15, getHeight() / 6) - 2) return;
    editor.setText (param->getCurrentValueAsText(), false);
    editor.setVisible (true);
    editor.grabKeyboardFocus();
    editor.selectAll();
}

void Knob::paint (Graphics& g)
{
    const int capH = jlimit (10, 15, getHeight() / 6);
    const float fh = jlimit (9.5f, 13.0f, (float) capH * 0.88f);
    g.setFont (theme::font (fh, true));
    g.setColour (theme::textDim);
    g.drawFittedText (caption.toUpperCase(), 0, 0, getWidth(), capH, Justification::centred, 1, 0.7f);
    if (param != nullptr)
    {
        g.setFont (theme::font (fh));
        g.setColour (slider.isMouseOverOrDragging() ? accent.darker (0.2f) : theme::text);
        g.drawFittedText (param->getCurrentValueAsText(), 0, getHeight() - capH, getWidth(), capH, Justification::centred, 1, 0.7f);
    }
}

//==============================================================================
void Segmented::attach (AudioProcessorValueTreeState& s, const String& id, StringArray lab)
{
    att.reset();
    param = s.getParameter (id);
    if (param == nullptr) return;
    labels = lab;
    if (labels.isEmpty())
    {
        if (auto* c = dynamic_cast<AudioParameterChoice*> (param)) labels = c->choices;
        else if (dynamic_cast<AudioParameterBool*> (param) != nullptr) labels = { "Off", "On" };
    }
    att = std::make_unique<ParameterAttachment> (*param, [this] (float v)
    {
        index = roundToInt (v - param->getNormalisableRange().start);
        repaint();
    });
    att->sendInitialUpdate();
    setTooltip (param->getName (64));
}

Rectangle<int> Segmented::area() const
{
    auto r = getLocalBounds();
    if (caption.isNotEmpty()) r.removeFromTop (14);
    return r;
}

void Segmented::paint (Graphics& g)
{
    if (caption.isNotEmpty())
    {
        g.setColour (theme::textDim);
        g.setFont (theme::font (11.0f, true));
        g.drawText (caption, getLocalBounds().removeFromTop (14), Justification::centred);
    }
    const auto r = area().toFloat();
    const int n = jmax (1, labels.size());
    g.setColour (theme::outline);
    g.fillRoundedRectangle (r, 3.0f);
    for (int i = 0; i < n; ++i)
    {
        auto cell = Rectangle<float> (r.getX() + r.getWidth() * i / n, r.getY(), r.getWidth() / n, r.getHeight()).reduced (0.75f);
        g.setColour (i == index ? accent : theme::panel2);
        g.fillRoundedRectangle (cell, 2.0f);
        g.setColour (i == index ? theme::white : theme::text);
        g.setFont (theme::font (jlimit (9.5f, 13.0f, cell.getHeight() * 0.55f), i == index));
        g.drawFittedText (labels[i], cell.toNearestInt().reduced (1, 0), Justification::centred, 1, 0.55f);
    }
}

void Segmented::mouseDown (const MouseEvent& e)
{
    if (att == nullptr || labels.isEmpty()) return;
    const auto r = area();
    const int n = labels.size();
    const int i = jlimit (0, n - 1, (e.x - r.getX()) * n / jmax (1, r.getWidth()));
    att->setValueAsCompleteGesture (param->getNormalisableRange().start + (float) i);
}

//==============================================================================
Toggle::Toggle (const String& buttonText, Colour accent) : TextButton (buttonText)
{
    setClickingTogglesState (true);
    setColour (TextButton::buttonOnColourId, accent);
}

void Toggle::attach (AudioProcessorValueTreeState& s, const String& id)
{
    att.reset();
    att = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (s, id, *this);
    if (auto* p = s.getParameter (id)) setTooltip (p->getName (64));
}

//==============================================================================
Panel::Panel (const String& t, Colour a) : title (t), accent (a) { setInterceptsMouseClicks (false, true); }

void Panel::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (theme::panel);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (theme::outline.withAlpha (0.5f));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    auto h = r.removeFromTop ((float) headerH);
    g.setColour (theme::header);
    g.fillRoundedRectangle (h, 4.0f);
    g.fillRect (h.withTrimmedTop (h.getHeight() * 0.5f));
    g.setColour (accent);
    g.fillRect (Rectangle<float> (h.getX() + 5, h.getY() + 5, 3.0f, h.getHeight() - 10));
    g.setColour (theme::headerText);
    g.setFont (theme::font (11.5f, true));
    g.drawText (title.toUpperCase(), h.withTrimmedLeft (13).withTrimmedRight (4), Justification::centredLeft);
}

//==============================================================================
ExpandWindow::ExpandWindow (const String& name, Component* content, int w, int h, std::function<void()> onClose)
    : DocumentWindow (name, theme::bg, DocumentWindow::closeButton, true), onCloseFn (std::move (onClose))
{
    setUsingNativeTitleBar (false);
    setTitleBarHeight (26);
    setContentOwned (content, false);
    setResizable (true, true);
    setResizeLimits (w / 2, h / 2, w * 3, h * 3);
    centreWithSize (w, h);
    setAlwaysOnTop (true);
    setVisible (true);
}

void drawMeter (Graphics& g, Rectangle<float> r, float level, Colour c)
{
    g.setColour (theme::header);
    g.fillRoundedRectangle (r, 2.0f);
    const float db = Decibels::gainToDecibels (level, -60.0f);
    const float p = jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
    g.setColour (level > 1.0f ? theme::red : c);
    if (r.getWidth() >= r.getHeight()) g.fillRoundedRectangle (r.withWidth (r.getWidth() * p), 2.0f);
    else g.fillRoundedRectangle (r.withTrimmedTop (r.getHeight() * (1.0f - p)), 2.0f);
}

void FloppyButton::paintButton (Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (over ? theme::panel2.brighter (0.06f) : theme::panel2);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (theme::outline); g.drawRoundedRectangle (r, 3.0f, 1.0f);
    const float s = jmin (r.getWidth(), r.getHeight()) * 0.72f;
    auto f = Rectangle<float> (s, s).withCentre (r.getCentre()).translated (0.0f, down ? 1.0f : 0.0f);
    Path body;   // disk with a clipped top-right corner
    body.startNewSubPath (f.getX(), f.getY());
    body.lineTo (f.getRight() - s * 0.18f, f.getY());
    body.lineTo (f.getRight(), f.getY() + s * 0.18f);
    body.lineTo (f.getRight(), f.getBottom());
    body.lineTo (f.getX(), f.getBottom());
    body.closeSubPath();
    g.setColour (theme::header); g.fillPath (body);
    g.setColour (Colour (0xffc7ccd2)); g.fillRect (f.getX() + s * 0.22f, f.getY(), s * 0.5f, s * 0.32f);          // metal shutter
    g.setColour (theme::header); g.fillRect (f.getX() + s * 0.52f, f.getY() + s * 0.05f, s * 0.1f, s * 0.22f);
    g.setColour (theme::white); g.fillRoundedRectangle (f.getX() + s * 0.14f, f.getY() + s * 0.48f, s * 0.72f, s * 0.42f, 1.5f);   // label
    g.setColour (theme::red); g.fillRect (f.getX() + s * 0.2f, f.getY() + s * 0.56f, s * 0.6f, s * 0.05f);
}

void drawLcd (Graphics& g, Rectangle<float> r)
{
    g.setColour (theme::header);
    g.fillRoundedRectangle (r, 4.0f);
    auto in = r.reduced (3.0f);
    ColourGradient grad (theme::lcdBg.brighter (0.06f), in.getX(), in.getY(), theme::lcdBg2, in.getX(), in.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (in, 2.5f);
    g.setColour (Colours::black.withAlpha (0.18f));
    g.drawRoundedRectangle (in, 2.5f, 1.0f);
}

void layoutRow (Rectangle<int> r, const std::vector<Component*>& items, int pad)
{
    if (items.empty()) return;
    const float w = (float) r.getWidth() / (float) items.size();
    for (size_t i = 0; i < items.size(); ++i)
        if (items[i] != nullptr)
            items[i]->setBounds (Rectangle<int> (r.getX() + roundToInt (w * (float) i), r.getY(), roundToInt (w), r.getHeight()).reduced (pad, 0));
}

// Groovulation logo: stylised woofer, two white drips running down from the dust cap (animated by 'phase')
void drawGroovLogo (Graphics& g, Rectangle<float> r, float level, double phase)
{
    const auto c = r.getCentre();
    const float R = jmin (r.getWidth(), r.getHeight()) * 0.5f;
    const float pulse = 1.0f + 0.05f * jlimit (0.0f, 1.0f, level);
    // frame + screws
    g.setColour (theme::header); g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);
    for (int i = 0; i < 4; ++i)
    {
        const float a = MathConstants<float>::pi * (0.25f + 0.5f * (float) i);
        const auto p = c + Point<float> (std::cos (a), std::sin (a)) * R * 0.9f;
        g.setColour (Colour (0xffb9b2a2)); g.fillEllipse (p.x - R * 0.06f, p.y - R * 0.06f, R * 0.12f, R * 0.12f);
    }
    // rubber surround
    const float rs = R * 0.8f * pulse;
    g.setColour (Colour (0xff5d5850)); g.fillEllipse (c.x - rs, c.y - rs, rs * 2, rs * 2);
    g.setColour (Colours::white.withAlpha (0.15f)); g.drawEllipse (c.x - rs * 0.93f, c.y - rs * 0.93f, rs * 1.86f, rs * 1.86f, R * 0.04f);
    // cone
    const float rc = R * 0.68f * pulse;
    ColourGradient cone (Colour (0xff4a463f), c.x - rc * 0.5f, c.y - rc * 0.6f, Colour (0xff22201d), c.x + rc * 0.6f, c.y + rc * 0.7f, true);
    g.setGradientFill (cone); g.fillEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2);
    g.setColour (Colours::black.withAlpha (0.25f));
    for (float k = 0.45f; k < 1.0f; k += 0.18f) g.drawEllipse (c.x - rc * k, c.y - rc * k, rc * 2 * k, rc * 2 * k, 0.8f);
    // dust cap
    const float rd = R * 0.3f * pulse;
    ColourGradient cap (Colour (0xff8a847a), c.x - rd * 0.4f, c.y - rd * 0.5f, Colour (0xff3b3833), c.x + rd * 0.6f, c.y + rd * 0.8f, true);
    g.setGradientFill (cap); g.fillEllipse (c.x - rd, c.y - rd, rd * 2, rd * 2);
    g.setColour (Colours::white.withAlpha (0.35f)); g.fillEllipse (c.x - rd * 0.55f, c.y - rd * 0.6f, rd * 0.5f, rd * 0.35f);

    // two white drips running down from the centre of the dust cap over the cone
    const Colour milk (0xfffbf9f3), milkShade (0xffcfcac0);
    for (int k = 0; k < 2; ++k)
    {
        const float side = k == 0 ? -1.0f : 1.0f;
        const double ph = std::fmod (phase * (k == 0 ? 0.22 : 0.17) + k * 0.4, 1.0);
        const float grow = (float) (ph < 0.85 ? ph / 0.85 : 1.0);           // slowly runs down ...
        const float fall = (float) (ph < 0.85 ? 0.0 : (ph - 0.85) / 0.15);  // ... then the drop falls off
        const float x0 = c.x + side * rd * 0.32f, y0 = c.y + rd * 0.15f;
        const float len = R * (0.22f + (k == 0 ? 0.42f : 0.32f) * grow);
        const float w0 = R * 0.055f;
        // centre line with a slight organic wiggle; width tapers towards the drop
        const int N = 24;
        Array<Point<float>> lft, rgt;
        for (int i = 0; i <= N; ++i)
        {
            const float t = (float) i / N;
            const float x = x0 + side * R * 0.035f * std::sin (t * 3.4f + (float) k) + side * R * 0.02f * t;
            const float y = y0 + len * t;
            const float w = w0 * (1.0f - 0.45f * t) * (1.0f + 0.12f * std::sin (t * 9.0f + (float) k * 2.0f));
            lft.add ({ x - w, y }); rgt.add ({ x + w, y });
        }
        Path stream;
        stream.startNewSubPath (lft[0]);
        for (int i = 1; i <= N; ++i) stream.lineTo (lft[i]);
        for (int i = N; i >= 0; --i) stream.lineTo (rgt[i]);
        stream.closeSubPath();
        const auto tip = (lft[N] + rgt[N]) * 0.5f;
        g.setColour (milk); g.fillPath (stream);
        g.fillEllipse (x0 - w0 * 1.5f, y0 - w0 * 1.3f, w0 * 3.0f, w0 * 2.2f);                  // where it comes out of the cap
        g.setColour (milkShade); g.strokePath (stream, PathStrokeType (0.6f));
        // hanging drop (teardrop), swelling while the stream runs, then falling
        const float dr = R * (0.05f + 0.035f * grow);
        const float dx = tip.x, dyy = tip.y + dr * 0.9f + fall * R * 0.5f;
        if (fall < 1.0f)
        {
            Path drop;
            drop.startNewSubPath (dx, dyy - dr * 2.1f);
            drop.cubicTo (dx + dr * 0.35f, dyy - dr * 1.3f, dx + dr, dyy - dr * 0.7f, dx + dr, dyy);
            drop.cubicTo (dx + dr, dyy + dr * 1.15f, dx - dr, dyy + dr * 1.15f, dx - dr, dyy);
            drop.cubicTo (dx - dr, dyy - dr * 0.7f, dx - dr * 0.35f, dyy - dr * 1.3f, dx, dyy - dr * 2.1f);
            g.setColour (milk.withAlpha (1.0f - fall)); g.fillPath (drop);
            g.setColour (milkShade.withAlpha (1.0f - fall)); g.strokePath (drop, PathStrokeType (0.6f));
            g.setColour (Colours::white.withAlpha (1.0f - fall)); g.fillEllipse (dx - dr * 0.5f, dyy - dr * 0.45f, dr * 0.4f, dr * 0.4f);
        }
        // glossy highlight along the stream
        g.setColour (Colours::white.withAlpha (0.8f));
        g.drawLine (lft[2].x + w0 * 0.5f, lft[2].y, lft[N / 2].x + w0 * 0.35f, lft[N / 2].y, 0.7f);
    }
}

