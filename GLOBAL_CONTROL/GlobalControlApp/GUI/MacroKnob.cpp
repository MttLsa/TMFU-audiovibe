#include "MacroKnob.h"

#include "LookAndFeel.h"

namespace gc::ui {

static constexpr float kStart = juce::MathConstants<float>::pi * -0.75f;
static constexpr float kEnd = juce::MathConstants<float>::pi * 0.75f;

MacroKnob::MacroKnob (AppController& a, int i) : app (a), index (i)
{
    shown = app.getMacro (index).knob;
    setRepaintsOnMouseActivity (false);
}

void MacroKnob::setSelected (bool s) { if (s != selected) { selected = s; repaint(); } }

void MacroKnob::refresh()
{
    const double target = app.getMacro (index).knob;
    if (dragging) shown = target;
    if (std::abs (shown - target) > 1e-4) { if (! isTimerRunning()) startTimerHz (60); }
    repaint();
}

void MacroKnob::timerCallback()
{
    const double target = app.getMacro (index).knob;
    shown += (target - shown) * 0.28;
    if (std::abs (target - shown) < 1e-4) { shown = target; stopTimer(); }
    repaint();
}

juce::Rectangle<float> MacroKnob::knobArea() const
{
    auto r = getLocalBounds().reduced (10).withTrimmedTop (30).withTrimmedBottom (70).toFloat();
    const float s = juce::jmin (r.getWidth(), r.getHeight());
    return r.withSizeKeepingCentre (s, s);
}

void MacroKnob::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto top = r.removeFromTop (24);
    gearArea = top.removeFromRight (22);
    nameArea = top;
    auto bottom = r.removeFromBottom (64);
    displayArea = bottom.removeFromTop (28);
}

void MacroKnob::paint (juce::Graphics& g)
{
    const auto& m = app.getMacro (index);
    const auto colour = juce::Colour (m.settings.colour);
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    // card
    g.setColour (col::panel);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (selected ? colour.withAlpha (0.9f) : (hover ? col::outlineHi : col::outline));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, selected ? 1.6f : 1.0f);
    if (selected) {
        g.setColour (colour.withAlpha (0.08f));
        g.fillRoundedRectangle (bounds.reduced (1.0f), 8.0f);
    }

    // header: number, name, mode badge, gear
    {
        auto h = nameArea;
        auto num = h.removeFromLeft (22).toFloat().withSizeKeepingCentre (20, 18);
        g.setColour (m.assigned ? colour : col::faint);
        g.fillRoundedRectangle (num, 4.0f);
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.setFont (uiFont (11.5f, true));
        g.drawText (juce::String (index + 1), num, juce::Justification::centred);
        h.removeFromLeft (6);
        if (m.assigned) {
            const bool rel = m.settings.mode == MacroMode::Relative;
            auto badge = h.removeFromRight (30).toFloat().withSizeKeepingCentre (30, 16);
            g.setColour (rel ? colour.withAlpha (0.18f) : col::raised);
            g.fillRoundedRectangle (badge, 3.0f);
            g.setColour (rel ? colour : col::dim);
            g.setFont (uiFont (9.5f, true));
            g.drawText (rel ? "REL" : "ABS", badge, juce::Justification::centred);
            h.removeFromRight (4);
        }
        g.setColour (m.assigned ? col::text : col::dim);
        g.setFont (uiFont (13.5f, true));
        g.drawFittedText (juce::String (m.settings.name), h, juce::Justification::centredLeft, 1, 0.8f);

        // gear
        auto ga = gearArea.toFloat().withSizeKeepingCentre (14, 14);
        g.setColour (hover ? col::text : col::faint);
        juce::Path gear;
        gear.addStar (ga.getCentre(), 8, 4.2f, 7.0f);
        g.fillPath (gear);
        g.setColour (col::panel);
        g.fillEllipse (ga.withSizeKeepingCentre (5, 5));
    }

    // knob
    const auto k = knobArea();
    const auto c = k.getCentre();
    const float radius = k.getWidth() * 0.5f;
    const float arcR = radius - 6.0f;
    const float pos = (float) shown;
    const bool rel = m.settings.mode == MacroMode::Relative;
    const float angle = kStart + pos * (kEnd - kStart);

    // ticks
    for (int t = 0; t <= 20; ++t) {
        const float a = kStart + (float) t / 20.0f * (kEnd - kStart);
        const float r0 = radius + 1.0f, r1 = radius + (t % 5 == 0 ? 5.0f : 3.0f);
        g.setColour (col::outlineHi.withAlpha (t % 5 == 0 ? 0.9f : 0.5f));
        g.drawLine (c.x + r0 * std::sin (a), c.y - r0 * std::cos (a), c.x + r1 * std::sin (a), c.y - r1 * std::cos (a), 1.0f);
    }
    // track
    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0, kStart, kEnd, true);
    g.setColour (col::lcd);
    g.strokePath (track, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // value arc (bipolar in relative mode)
    if (m.assigned) {
        juce::Path v;
        const float from = rel ? 0.0f : kStart;
        v.addCentredArc (c.x, c.y, arcR, arcR, 0, juce::jmin (from, angle), juce::jmax (from, angle), true);
        const auto ac = m.instances.empty() || m.controllableCount() == 0 ? col::faint : colour;
        g.setColour (ac.withAlpha (0.25f));
        g.strokePath (v, juce::PathStrokeType (11.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (ac);
        g.strokePath (v, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    // body
    const float bodyR = arcR - 9.0f;
    auto body = juce::Rectangle<float> (bodyR * 2, bodyR * 2).withCentre (c);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (body.translated (0, 3).expanded (1.5f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3c43), c.x, body.getY(), juce::Colour (0xff1a1b1f), c.x, body.getBottom(), false));
    g.fillEllipse (body);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2d2f35), c.x, body.getY(), juce::Colour (0xff17181b), c.x, body.getBottom(), false));
    g.fillEllipse (body.reduced (bodyR * 0.12f));
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawEllipse (body.reduced (0.5f), 1.0f);
    // pointer
    const float p0 = bodyR * 0.35f, p1 = bodyR * 0.86f;
    g.setColour (m.assigned ? juce::Colours::white.withAlpha (0.92f) : col::faint);
    g.drawLine (c.x + p0 * std::sin (angle), c.y - p0 * std::cos (angle), c.x + p1 * std::sin (angle), c.y - p1 * std::cos (angle), 3.0f);
    if (m.outOfSync || m.mixed) {
        g.setColour (col::warn);
        g.fillEllipse (juce::Rectangle<float> (7, 7).withCentre ({ k.getRight() - 6, k.getY() + 6 }));
    }

    // numeric display
    auto d = displayArea.toFloat();
    g.setColour (col::lcd);
    g.fillRoundedRectangle (d, 4.0f);
    g.setColour (col::outline);
    g.drawRoundedRectangle (d.reduced (0.5f), 4.0f, 1.0f);
    const auto text = dragging ? app.formatMacroValue (index, app.getMacro (index).knob) : m.valueText;
    g.setColour (m.assigned ? (m.mixed ? col::warn : colour.brighter (0.35f)) : col::faint);
    g.setFont (monoFont (16.0f, true));
    g.drawFittedText (text, d.toNearestInt().reduced (6, 0), juce::Justification::centred, 1);

    // assignment summary
    auto info = getLocalBounds().reduced (10).removeFromBottom (36);
    g.setFont (uiFont (11.5f));
    if (m.assigned) {
        g.setColour (col::text);
        g.drawFittedText (m.pluginName + " / " + m.paramName, info.removeFromTop (17), juce::Justification::centred, 1);
        const int ok = m.countStatus (InstanceStatus::Ok);
        const int bad = (int) m.instances.size() - ok - m.countStatus (InstanceStatus::Pending);
        const juce::String dot (juce::CharPointer_UTF8 ("  \xc2\xb7  "));
        juce::String s = juce::String (ok) + (ok == 1 ? " instance" : " instances");
        if (m.groupName.isNotEmpty()) s << dot << m.groupName;
        g.setColour (bad > 0 ? col::warn : col::dim);
        if (bad > 0) s << dot << bad << " unresolved";
        g.drawFittedText (s, info, juce::Justification::centred, 1);
    } else {
        g.setColour (col::faint);
        g.drawFittedText (selected ? "select parameters and press ASSIGN" : "not assigned", info.removeFromTop (17), juce::Justification::centred, 1);
    }
}

void MacroKnob::mouseMove (const juce::MouseEvent&) { if (! hover) { hover = true; repaint(); } }
void MacroKnob::mouseExit (const juce::MouseEvent&) { hover = false; repaint(); }

void MacroKnob::mouseDown (const juce::MouseEvent& e)
{
    if (onSelect) onSelect (index);
    if (e.mods.isPopupMenu()) { showMenu(); return; }
    if (gearArea.contains (e.getPosition())) { if (onOpenSettings) onOpenSettings (index); return; }
    if (! knobArea().expanded (8).contains (e.position) || ! app.getMacro (index).assigned) return;
    dragging = true;
    dragStartX = e.position.x; dragStartY = e.position.y;
    dragStartValue = app.getMacro (index).knob;
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void MacroKnob::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging) return;
    const auto& s = app.getMacro (index).settings;
    const float delta = ((dragStartY - e.position.y) + (e.position.x - dragStartX) * 0.5f) / 220.0f;
    const double v = juce::jlimit (0.0, 1.0, dragStartValue + delta * (e.mods.isShiftDown() ? s.fineFactor : 1.0));
    app.setMacroKnob (index, v);
    shown = v;
    repaint();
}

void MacroKnob::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
    setMouseCursor (juce::MouseCursor::NormalCursor);
    repaint();
}

void MacroKnob::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (knobArea().contains (e.position)) app.resetMacro (index);
}

void MacroKnob::moveBy (double delta, bool fine)
{
    if (! app.getMacro (index).assigned) return;
    app.setMacroKnob (index, MacroEngine::drag (app.getMacro (index).settings, app.getMacro (index).knob, delta, fine));
}

void MacroKnob::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    moveBy ((w.deltaY + w.deltaX) * 0.12 * (w.isReversed ? -1 : 1), e.mods.isShiftDown());
}

void MacroKnob::showMenu()
{
    const auto& m = app.getMacro (index);
    juce::PopupMenu menu;
    menu.addSectionHeader ("Macro " + juce::String (index + 1));
    menu.addItem (1, "Absolute mode", m.assigned, m.settings.mode == MacroMode::Absolute);
    menu.addItem (2, "Relative mode", m.assigned, m.settings.mode == MacroMode::Relative);
    menu.addSeparator();
    menu.addItem (3, "Reset", m.assigned);
    menu.addItem (4, "Capture current values as starting point", m.assigned && m.settings.mode == MacroMode::Relative);
    menu.addItem (5, "Settings...");
    menu.addSeparator();
    menu.addItem (6, "Clear assignment", m.assigned);
    juce::Component::SafePointer<MacroKnob> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r) {
        if (safe == nullptr) return;
        auto& a = safe->app; const int i = safe->index;
        switch (r) {
            case 1: a.setMacroMode (i, MacroMode::Absolute); break;
            case 2: a.setMacroMode (i, MacroMode::Relative); break;
            case 3: a.resetMacro (i); break;
            case 4: a.captureRelative (i); break;
            case 5: if (safe->onOpenSettings) safe->onOpenSettings (i); break;
            case 6: a.clearMacro (i); break;
            default: break;
        }
    });
}

} // namespace gc::ui
