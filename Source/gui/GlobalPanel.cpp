#include "GlobalPanel.h"

using namespace juce;
using namespace params;

//==============================================================================
void FilterCurve::timerCallback()
{
    const float c = proc.apvts.getRawParameterValue (gCutoff)->load();
    const float r = proc.apvts.getRawParameterValue (gRes)->load();
    const float m = proc.apvts.getRawParameterValue (gMode)->load();
    if (! exactlyEqual (c, lc) || ! exactlyEqual (r, lr) || ! exactlyEqual (m, lm)) { lc = c; lr = r; lm = m; repaint(); }
}

void FilterCurve::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (theme::outline);
    g.fillRoundedRectangle (b, 3.0f);
    const double fc = proc.apvts.getRawParameterValue (gCutoff)->load();
    const double q = 0.5 + std::pow (proc.apvts.getRawParameterValue (gRes)->load() * 0.01, 1.5) * 14.5;
    const int mode = (int) proc.apvts.getRawParameterValue (gMode)->load();   // 0 HP 1 BP 2 LP

    Path p;
    const int n = jmax (2, getWidth());
    for (int i = 0; i < n; ++i)
    {
        const double f = 20.0 * std::pow (1000.0, (double) i / (n - 1));
        const double w = f / fc;
        // analog 2-pole prototype magnitude
        const double den = std::sqrt ((1 - w * w) * (1 - w * w) + (w / q) * (w / q));
        double mag = mode == 2 ? 1.0 / den : (mode == 0 ? (w * w) / den : (w / q) / den);
        const double db = jlimit (-36.0, 24.0, 20.0 * std::log10 (jmax (1.0e-6, mag)));
        const float y = b.getY() + (float) ((24.0 - db) / 60.0) * b.getHeight();
        if (i == 0) p.startNewSubPath (b.getX() + (float) i, y); else p.lineTo (b.getX() + (float) i, y);
    }
    Path fill (p);
    fill.lineTo (b.getRight(), b.getBottom()); fill.lineTo (b.getX(), b.getBottom()); fill.closeSubPath();
    g.setColour (theme::accentG.withAlpha (0.15f));
    g.fillPath (fill);
    g.setColour (theme::accentG);
    g.strokePath (p, PathStrokeType (1.6f));
    g.setColour (theme::textDim.withAlpha (0.35f));
    const float y0 = b.getY() + (24.0f / 60.0f) * b.getHeight();
    g.drawHorizontalLine ((int) y0, b.getX(), b.getRight());
}

//==============================================================================
GlobalPanel::GlobalPanel (TwinboxProcessor& p) : proc (p), curve (p)
{
    auto& s = proc.apvts;
    xfade.attach (s, gXfade);
    cutoff.attach (s, gCutoff);
    mode.attach (s, gMode, { "HP", "BP", "LP" });
    res.attach (s, gRes);
    master.attach (s, gMaster);
    tempo.attach (s, gTempo);
    run.attach (s, gRun);
    run.setTooltip ("Run the internal clock (used when the host transport is stopped / in standalone)");
    xfade.slider.setTooltip ("A/B mix: 0% = only Section A, 50% = 50/50, 100% = only Section B");
    for (auto* c : std::initializer_list<Component*> { &xfade, &cutoff, &mode, &res, &master, &tempo, &run, &curve })
        addAndMakeVisible (c);
    setExpanded ((bool) proc.uiState.getProperty ("globalExpanded", true));
    startTimerHz (25);
}

void GlobalPanel::setExpanded (bool e)
{
    expanded = e;
    proc.uiState.setProperty ("globalExpanded", e, nullptr);
    for (auto* c : std::initializer_list<Component*> { &xfade, &cutoff, &mode, &res, &master, &tempo, &run, &curve })
        c->setVisible (e);
    if (onToggle) onToggle();
    resized();
    repaint();
}

void GlobalPanel::mouseDown (const MouseEvent& e)
{
    if (! expanded || headerArea.contains (e.getPosition())) setExpanded (! expanded);
}

void GlobalPanel::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (theme::panel.darker (0.25f));
    g.fillRoundedRectangle (r, 5.0f);

    if (! expanded)
    {
        g.setColour (theme::accentG);
        g.fillRoundedRectangle (r.removeFromTop (4.0f).reduced (4.0f, 0.0f), 2.0f);
        g.setColour (theme::text);
        g.setFont (theme::font (13.0f, true));
        GlyphArrangement ga;
        ga.addLineOfText (theme::font (13.0f, true), String::fromUTF8 ("\xe2\x97\x80  GLOBAL CONTROL"), 0, 0);
        Path p; ga.createPath (p);
        const auto pb = p.getBounds();
        p.applyTransform (AffineTransform::rotation (-MathConstants<float>::halfPi)
                            .translated ((float) getWidth() * 0.5f + pb.getHeight() * 0.5f - 2.0f, (float) getHeight() * 0.5f + pb.getWidth() * 0.5f));
        g.fillPath (p);
        drawMeter (g, Rectangle<float> ((float) getWidth() * 0.5f - 3.0f, (float) getHeight() - 70.0f, 6.0f, 60.0f), proc.meter (2), theme::accentG);
        return;
    }

    auto h = headerArea.toFloat();
    g.setColour (theme::header);
    g.fillRoundedRectangle (h, 4.0f);
    g.setColour (theme::accentG);
    g.fillRect (h.getX(), h.getY() + 6, 3.0f, h.getHeight() - 12);
    g.setColour (theme::text);
    g.setFont (theme::font (13.0f, true));
    g.drawText ("GLOBAL CONTROL", h.withTrimmedLeft (12).toNearestInt(), Justification::centredLeft);
    g.setColour (theme::textDim);
    g.drawText (String::fromUTF8 ("\xe2\x96\xb6"), h.withTrimmedRight (10).toNearestInt(), Justification::centredRight);

    g.setFont (theme::font (11.0f, true));
    g.setColour (theme::accentA);
    g.drawText ("A", xfade.getX() - 2, xfade.getY() + xfade.getHeight() / 2 - 6, 14, 12, Justification::centred);
    g.setColour (theme::accentB);
    g.drawText ("B", xfade.getRight() - 12, xfade.getY() + xfade.getHeight() / 2 - 6, 14, 12, Justification::centred);

    g.setColour (theme::textDim);
    g.setFont (theme::font (10.5f));
    g.drawText (proc.hostIsPlaying() ? "HOST SYNC" : (proc.isPlaying() ? "INTERNAL CLOCK" : "STOPPED"),
                run.getRight() + 10, run.getY(), 140, run.getHeight(), Justification::centredLeft);

    // section separators
    g.setColour (theme::header);
    for (int y : { xfade.getBottom() + 4, curve.getBottom() + 8 })
        g.drawHorizontalLine (y, 12.0f, (float) getWidth() - 12.0f);
    drawMeter (g, meterArea.toFloat(), proc.meter (2), theme::accentG);
}

void GlobalPanel::resized()
{
    auto r = getLocalBounds().reduced (8);
    if (! expanded) { headerArea = getLocalBounds(); meterArea = {}; return; }
    headerArea = r.removeFromTop (28);
    r.removeFromTop (8);

    xfade.setBounds (r.removeFromTop (140).withSizeKeepingCentre (150, 140));
    r.removeFromTop (12);
    auto filterTitle = r.removeFromTop (4);
    ignoreUnused (filterTitle);
    cutoff.setBounds (r.removeFromTop (240).withSizeKeepingCentre (230, 240));
    r.removeFromTop (4);
    mode.setBounds (r.removeFromTop (26).reduced (12, 0));
    r.removeFromTop (6);
    res.setBounds (r.removeFromTop (120).withSizeKeepingCentre (130, 120));
    r.removeFromTop (6);
    curve.setBounds (r.removeFromTop (130).reduced (6, 0));
    r.removeFromTop (16);
    auto row = r.removeFromTop (jmin (120, r.getHeight()));
    master.setBounds (row.removeFromLeft (row.getWidth() / 2 - 10));
    tempo.setBounds (row.removeFromLeft (row.getWidth() - 20));
    r.removeFromTop (10);
    run.setBounds (r.removeFromTop (26).withWidth (70).withX (r.getX() + 8));
    meterArea = row.reduced (4, 6);
}
