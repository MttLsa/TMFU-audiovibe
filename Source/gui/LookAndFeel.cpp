#include "LookAndFeel.h"

using namespace juce;

TwinLookAndFeel::TwinLookAndFeel()
{
    setColour (ResizableWindow::backgroundColourId, theme::bg);
    setColour (DocumentWindow::backgroundColourId, theme::bg);
    setColour (Label::textColourId, theme::text);
    setColour (TextButton::buttonColourId, theme::panel2);
    setColour (TextButton::buttonOnColourId, theme::accentA);
    setColour (TextButton::textColourOffId, theme::text);
    setColour (TextButton::textColourOnId, Colours::black);
    setColour (ComboBox::backgroundColourId, theme::panel2);
    setColour (ComboBox::textColourId, theme::text);
    setColour (ComboBox::outlineColourId, theme::outline);
    setColour (ComboBox::arrowColourId, theme::textDim);
    setColour (PopupMenu::backgroundColourId, theme::panel2);
    setColour (PopupMenu::textColourId, theme::text);
    setColour (PopupMenu::highlightedBackgroundColourId, theme::accentA);
    setColour (PopupMenu::highlightedTextColourId, Colours::black);
    setColour (Slider::textBoxTextColourId, theme::text);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (TooltipWindow::backgroundColourId, theme::header);
    setColour (TooltipWindow::textColourId, theme::text);
    setColour (TextEditor::backgroundColourId, theme::panel2);
    setColour (TextEditor::textColourId, theme::text);
    setColour (TextEditor::highlightColourId, theme::accentA.withAlpha (0.4f));
    setColour (TextEditor::outlineColourId, theme::outline);
    setColour (CaretComponent::caretColourId, theme::text);
}

void TwinLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    const auto bounds = Rectangle<int> (x, y, w, h).toFloat().reduced (2.0f);
    const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float lineW = jmax (2.0f, radius * 0.16f);
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

    // inner disc + pointer
    const float innerR = arcR - lineW * 1.2f;
    g.setColour (theme::panel2.brighter (0.05f));
    g.fillEllipse (c.x - innerR, c.y - innerR, innerR * 2, innerR * 2);
    Path ptr;
    ptr.startNewSubPath (c.getPointOnCircumference (innerR * 0.25f, angle));
    ptr.lineTo (c.getPointOnCircumference (innerR * 0.95f, angle));
    g.setColour (theme::text);
    g.strokePath (ptr, PathStrokeType (jmax (1.5f, lineW * 0.6f), PathStrokeType::curved, PathStrokeType::rounded));
}

void TwinLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                        Slider::SliderStyle style, Slider& s)
{
    const auto r = Rectangle<int> (x, y, w, h).toFloat();
    const auto accent = s.findColour (Slider::trackColourId);
    if (style == Slider::LinearHorizontal)
    {
        const auto track = r.withSizeKeepingCentre (r.getWidth(), 4.0f);
        g.setColour (theme::track);
        g.fillRoundedRectangle (track, 2.0f);
        const float mid = (float) s.valueToProportionOfLength (s.getMinimum() < 0 ? 0.0 : s.getMinimum());
        const float x0 = r.getX() + jlimit (0.0f, 1.0f, mid) * r.getWidth();
        g.setColour (accent);
        g.fillRoundedRectangle (Rectangle<float> (jmin (x0, pos), track.getY(), std::abs (pos - x0), 4.0f), 2.0f);
        g.setColour (theme::text);
        g.fillRoundedRectangle (Rectangle<float> (pos - 3.0f, r.getCentreY() - 8.0f, 6.0f, 16.0f), 2.0f);
    }
    else
    {
        const auto track = r.withSizeKeepingCentre (4.0f, r.getHeight());
        g.setColour (theme::track);
        g.fillRoundedRectangle (track, 2.0f);
        g.setColour (accent);
        g.fillRoundedRectangle (Rectangle<float> (track.getX(), pos, 4.0f, r.getBottom() - pos), 2.0f);
        g.setColour (theme::text);
        g.fillRoundedRectangle (Rectangle<float> (r.getCentreX() - 8.0f, pos - 3.0f, 16.0f, 6.0f), 2.0f);
    }
}

void TwinLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour& bgc, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    Colour c = b.getToggleState() ? b.findColour (TextButton::buttonOnColourId) : bgc;
    if (down) c = c.brighter (0.15f);
    else if (over) c = c.brighter (0.07f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 2.5f);
    g.setColour (theme::outline);
    g.drawRoundedRectangle (r, 2.5f, 1.0f);
}

void TwinLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.findColour (b.getToggleState() ? TextButton::textColourOnId : TextButton::textColourOffId)
                  .withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.5f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (2, 1), Justification::centred, 1);
}

void TwinLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    g.setColour (box.findColour (ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 2.5f);
    g.setColour (theme::outline);
    g.drawRoundedRectangle (r, 2.5f, 1.0f);
    Path p;
    const float ax = (float) w - 8.0f, ay = (float) h * 0.5f;
    p.addTriangle (ax - 3, ay - 1.5f, ax + 3, ay - 1.5f, ax, ay + 2.5f);
    g.setColour (theme::textDim);
    g.fillPath (p);
}

Label* TwinLookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (theme::font (11.0f));
    l->setColour (Label::textColourId, theme::text);
    l->setColour (Label::backgroundColourId, Colours::transparentBlack);
    l->setColour (Label::outlineColourId, Colours::transparentBlack);
    return l;
}

void TwinLookAndFeel::drawDocumentWindowTitleBar (DocumentWindow& win, Graphics& g, int w, int h, int, int, const Image*, bool)
{
    g.fillAll (theme::header);
    g.setColour (theme::text);
    g.setFont (theme::font (13.0f, true));
    g.drawText (win.getName(), 10, 0, w - 80, h, Justification::centredLeft);
}

//==============================================================================
FineSlider::FineSlider()
{
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (true);
}

void FineSlider::mouseDown (const MouseEvent& e)
{
    setMouseDragSensitivity (e.mods.isShiftDown() ? dragPixels * 4 : dragPixels);
    Slider::mouseDown (e);
}

bool FineSlider::keyPressed (const KeyPress& k)
{
    int dir = 0;
    if (k.isKeyCode (KeyPress::upKey) || k.isKeyCode (KeyPress::rightKey)) dir = 1;
    if (k.isKeyCode (KeyPress::downKey) || k.isKeyCode (KeyPress::leftKey)) dir = -1;
    if (dir == 0) return Slider::keyPressed (k);
    const double interval = getInterval();
    double v;
    if (interval > 0.0 && (getMaximum() - getMinimum()) / interval <= 400.0)       // stepped parameter: one step
        v = getValue() + dir * interval;
    else if (unitStep > 0.0)                                                         // unit parameter (Pan): 1 unit
        v = getValue() + dir * unitStep * (k.getModifiers().isShiftDown() ? 0.1 : 1.0);
    else                                                                             // continuous parameter: fine step
    {
        const double prop = valueToProportionOfLength (getValue()) + dir * (k.getModifiers().isShiftDown() ? 0.0002 : 0.002);
        v = proportionOfLengthToValue (jlimit (0.0, 1.0, prop));
    }
    setValue (jlimit (getMinimum(), getMaximum(), v), sendNotificationSync);
    return true;
}

Knob::Knob (const String& c, Colour a) : caption (c), accent (a)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setColour (Slider::rotarySliderFillColourId, accent);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
    slider.setVelocityBasedMode (false);
    slider.setMouseDragSensitivity (1000);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
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
    // double-click: back to the default value; Acc, V>A, Pan and Tune of the Section B modules go to the centre (12 o'clock)
    const bool centre = id.startsWith ("b_") && (id.endsWith ("_acc") || id.endsWith ("_vamt") || id.endsWith ("_pan") || id.endsWith ("_tune"))
                        && ! id.startsWith ("b_lfo");
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (centre ? 0.5f : param->getDefaultValue()));
    slider.unitStep = id.endsWith ("pan") ? 1.0 : 0.0;          // Pan: one step = 1 (L / R)
    slider.setTooltip (param->getName (64));
    if (caption.isEmpty()) caption = param->getName (32);
    repaint();
}

void Knob::resized()
{
    auto r = getLocalBounds();
    const int capH = jlimit (10, 16, getHeight() / 6);
    r.removeFromTop (capH);
    r.removeFromBottom (capH);
    slider.setBounds (r.withSizeKeepingCentre (jmin (r.getWidth(), r.getHeight()), jmin (r.getWidth(), r.getHeight())));
}

void Knob::paint (Graphics& g)
{
    const int capH = jlimit (10, 16, getHeight() / 6);
    const float fh = jlimit (9.5f, 14.0f, (float) capH * 0.85f);
    g.setFont (theme::font (fh));
    g.setColour (theme::textDim);
    g.drawFittedText (caption, 0, 0, getWidth(), capH, Justification::centred, 1, 0.7f);
    if (param != nullptr)
    {
        g.setColour (slider.isMouseOverOrDragging() || slider.hasKeyboardFocus (false) ? accent : theme::text);
        g.drawFittedText (param->getCurrentValueAsText(), 0, getHeight() - capH, getWidth(), capH, Justification::centred, 1, 0.7f);
    }
    if (slider.hasKeyboardFocus (false))                       // keyboard focus: the arrows move this knob
    {
        g.setColour (accent.withAlpha (0.7f));
        g.drawRoundedRectangle (slider.getBounds().toFloat().expanded (2.0f), 4.0f, 1.2f);
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
        else for (int i = (int) param->getNormalisableRange().start; i <= (int) param->getNormalisableRange().end; ++i) labels.add (String (i));
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
        g.setFont (theme::font (11.0f));
        g.drawText (caption, getLocalBounds().removeFromTop (14), Justification::centred);
    }
    const auto r = area().toFloat();
    const int n = jmax (1, labels.size());
    g.setColour (theme::outline);
    g.fillRoundedRectangle (r, 3.0f);
    for (int i = 0; i < n; ++i)
    {
        Rectangle<float> cell = vertical ? Rectangle<float> (r.getX(), r.getY() + r.getHeight() * i / n, r.getWidth(), r.getHeight() / n)
                                         : Rectangle<float> (r.getX() + r.getWidth() * i / n, r.getY(), r.getWidth() / n, r.getHeight());
        cell = cell.reduced (0.75f);
        g.setColour (i == index ? accent : theme::panel2);
        g.fillRoundedRectangle (cell, 2.0f);
        g.setColour (i == index ? Colours::black : theme::text);
        g.setFont (theme::font (jlimit (9.5f, 13.0f, cell.getHeight() * 0.55f), i == index));
        g.drawFittedText (labels[i], cell.toNearestInt().reduced (1, 0), Justification::centred, 1, 0.6f);
    }
}

void Segmented::mouseDown (const MouseEvent& e)
{
    if (att == nullptr || labels.isEmpty()) return;
    const auto r = area();
    const int n = labels.size();
    int i = vertical ? (e.y - r.getY()) * n / jmax (1, r.getHeight()) : (e.x - r.getX()) * n / jmax (1, r.getWidth());
    i = jlimit (0, n - 1, i);
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
Panel::Panel (const String& t, Colour a) : title (t), accent (a)
{
    expandBtn.setTooltip ("Open in a large editor window");
    addChildComponent (expandBtn);
}

void Panel::setExpandable (std::function<void()> fn)
{
    expandBtn.onClick = std::move (fn);
    expandBtn.setVisible (true);
}

void Panel::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (theme::panel);
    g.fillRoundedRectangle (r, 4.0f);
    auto h = r.removeFromTop ((float) headerH);
    g.setColour (theme::header);
    g.fillRoundedRectangle (h, 4.0f);
    g.fillRect (h.withTrimmedTop (h.getHeight() * 0.5f));
    g.setColour (accent);
    g.fillRect (Rectangle<float> (h.getX(), h.getY() + 4, 3.0f, h.getHeight() - 8));
    g.setColour (theme::text);
    g.setFont (theme::font (12.0f, true));
    g.drawText (title.toUpperCase(), h.withTrimmedLeft (10).withTrimmedRight (30), Justification::centredLeft);
}

void Panel::resized()
{
    expandBtn.setBounds (getWidth() - 24, 2, 20, headerH - 4);
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
    g.setColour (theme::outline);
    g.fillRoundedRectangle (r, 2.0f);
    const float db = Decibels::gainToDecibels (level, -60.0f);
    const float p = jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
    if (r.getWidth() >= r.getHeight())
        g.setColour (level > 1.0f ? Colours::red : c), g.fillRoundedRectangle (r.withWidth (r.getWidth() * p), 2.0f);
    else
        g.setColour (level > 1.0f ? Colours::red : c), g.fillRoundedRectangle (r.withTrimmedTop (r.getHeight() * (1.0f - p)), 2.0f);
}

//==============================================================================
//  NudgeBox
//==============================================================================
NudgeBox::NudgeBox (Colour a) : accent (a)
{
    for (auto* b : { &up, &down })
    {
        b->setRepeatSpeed (350, 60);
        b->setColour (TextButton::buttonColourId, theme::panel2);
        b->setColour (TextButton::textColourOffId, theme::textDim);
        addAndMakeVisible (*b);
    }
    up.onClick   = [this] { step (ModifierKeys::currentModifiers.isShiftDown() ? 0.1f : 1.0f); };
    down.onClick = [this] { step (ModifierKeys::currentModifiers.isShiftDown() ? -0.1f : -1.0f); };
    up.setTooltip ("Later (delay): +1 ms, Shift +0.1 ms");
    down.setTooltip ("Earlier (anticipation): -1 ms, Shift -0.1 ms");
    setTooltip ("Nudge: plays this track earlier (-) or later (+), -50..+50 ms. Drag or use the arrows; double-click = 0");
}

void NudgeBox::attach (AudioProcessorValueTreeState& s, const String& id)
{
    param = s.getParameter (id);
    jassert (param != nullptr);
    if (param == nullptr) return;
    att = std::make_unique<ParameterAttachment> (*param, [this] (float v) { value = v; repaint(); }, s.undoManager);
    att->sendInitialUpdate();
}

String NudgeBox::format (float ms)
{
    const float r = std::round (ms * 10.0f) / 10.0f;
    if (std::abs (r) < 0.05f) return "0 ms";
    const bool frac = std::abs (r - std::round (r)) > 0.05f;
    return (r > 0 ? "+" : "") + String (r, frac ? 1 : 0) + " ms";
}

void NudgeBox::setValue (float ms)
{
    ms = jlimit (-50.0f, 50.0f, std::round (ms * 10.0f) / 10.0f);
    if (att) att->setValueAsCompleteGesture (ms); else { value = ms; repaint(); }
}

void NudgeBox::step (float ms) { setValue (value + ms); }

void NudgeBox::resized()
{
    auto r = getLocalBounds();
    auto arrows = r.removeFromRight (jmin (14, getWidth() / 3));
    up.setBounds (arrows.removeFromTop (arrows.getHeight() / 2));
    down.setBounds (arrows);
}

void NudgeBox::paint (Graphics& g)
{
    auto r = getLocalBounds().withTrimmedRight (jmin (14, getWidth() / 3) + 1).toFloat();
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r, 2.0f);
    const bool zero = std::abs (value) < 0.05f;
    g.setColour (zero ? theme::textDim : accent);
    g.drawRoundedRectangle (r.reduced (0.5f), 2.0f, zero ? 0.6f : 1.0f);
    g.setFont (theme::font (jlimit (9.0f, 12.0f, r.getHeight() * 0.55f), ! zero));
    g.drawText (format (value), r.reduced (2.0f, 0.0f), Justification::centred);
}

void NudgeBox::mouseDown (const MouseEvent&) { dragStart = value; if (att) att->beginGesture(); }
void NudgeBox::mouseDrag (const MouseEvent& e)
{
    const float per = e.mods.isShiftDown() ? 0.02f : 0.2f;     // ms per pixel
    const float v = jlimit (-50.0f, 50.0f, std::round ((dragStart - (float) e.getDistanceFromDragStartY() * per) * 10.0f) / 10.0f);
    if (att) att->setValueAsPartOfGesture (v); else value = v;
    repaint();
}
void NudgeBox::mouseUp (const MouseEvent&) { if (att) att->endGesture(); }
void NudgeBox::mouseDoubleClick (const MouseEvent&) { setValue (0.0f); }
void NudgeBox::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    if (std::abs (w.deltaY) > 0.0f) step ((w.deltaY > 0 ? 1.0f : -1.0f) * (e.mods.isShiftDown() ? 0.1f : 1.0f));
}
