#include "LfoPanel.h"

using namespace juce;
using namespace params;

//==============================================================================
//  LfoStrip
//==============================================================================
LfoStrip::LfoStrip (TwinboxProcessor& p, int s, int i, bool b)
    : proc (p), section (s), index (i), big (b), accent (s == 0 ? theme::accentA : theme::accentB),
      sync ("SYNC", s == 0 ? theme::accentA : theme::accentB),
      rate ("Rate", s == 0 ? theme::accentA : theme::accentB), depth ("Depth", s == 0 ? theme::accentA : theme::accentB),
      phase ("Phase", s == 0 ? theme::accentA : theme::accentB)
{
    targetParam = p.apvts.getParameter (lfoId (s, i, "target"));
    syncRaw = p.apvts.getRawParameterValue (lfoId (s, i, "sync"));

    targetBtn.onClick = [this] { chooseTarget(); };
    targetBtn.setTooltip ("Destination: any parameter of this section");
    addAndMakeVisible (targetBtn);

    shape.addItemList (lfoShapes(), 1);
    shapeAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, lfoId (s, i, "shape"), shape);
    shape.setTooltip ("Waveform");
    addAndMakeVisible (shape);

    StringArray divs; for (auto& d : lfoDivs()) divs.add (d.name);
    div.addItemList (divs, 1);
    divAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, lfoId (s, i, "div"), div);
    div.setTooltip ("SYNC rate: one LFO cycle per note value / bars, locked to the song position");
    addAndMakeVisible (div);

    sync.attach (p.apvts, lfoId (s, i, "sync"));
    sync.setTooltip ("SYNC on: rate in beats (1/16 ... 8 bars). Off: free rate in milliseconds (one cycle every N ms)");
    addAndMakeVisible (sync);

    rate.attach (p.apvts, lfoId (s, i, "ms"));
    rate.slider.setTooltip ("RATE: length of one LFO cycle in milliseconds (10 ms ... 30 s)");
    depth.attach (p.apvts, lfoId (s, i, "depth"));
    depth.slider.setTooltip ("Depth: -100 ... +100 % (100 % = half of the target range up and down; negative = inverted)");
    phase.attach (p.apvts, lfoId (s, i, "phase"));
    addAndMakeVisible (rate);
    addAndMakeVisible (depth);
    if (big) addAndMakeVisible (phase);

    lastSync = syncRaw->load() > 0.5f;
    rate.setVisible (! lastSync); div.setVisible (lastSync);
    targetBtn.setButtonText (targetLabel());
    startTimerHz (30);
}

LfoStrip::~LfoStrip() { stopTimer(); }

String LfoStrip::targetLabel() const
{
    const int t = targetParam != nullptr ? roundToInt (targetParam->convertFrom0to1 (targetParam->getValue())) - 1 : -1;
    const auto& list = lfoTargets (section);
    if (t < 0 || t >= (int) list.size()) return "-- target --";
    String n = list[(size_t) t].name;
    if (section == 0 && n.startsWith ("A") && n.length() > 2 && n[1] >= '1' && n[1] <= '8')     // "A3 Osc Freq" -> "CH: Osc Freq"
        n = proc.channelName (n[1] - '1') + ": " + n.fromFirstOccurrenceOf (" ", false, false);
    else if (n.startsWith ("A ") || n.startsWith ("B "))
        n = n.substring (2);
    return n;
}

void LfoStrip::chooseTarget()
{
    const auto& list = lfoTargets (section);
    PopupMenu menu;
    menu.addItem (1, "-- (off)", true, targetParam->getValue() < 1.0e-6f);
    StringArray groups;
    for (auto& t : list) if (! groups.contains (t.group)) groups.add (t.group);
    const int current = roundToInt (targetParam->convertFrom0to1 (targetParam->getValue())) - 1;
    for (auto& g : groups)
    {
        PopupMenu sub;
        bool hasCurrent = false;
        for (int k = 0; k < (int) list.size(); ++k)
            if (list[(size_t) k].group == g)
            {
                String n = list[(size_t) k].name;
                if (section == 0 && n.length() > 2 && n[1] >= '1' && n[1] <= '8') n = n.fromFirstOccurrenceOf (" ", false, false);
                else if (n.startsWith ("A ") || n.startsWith ("B ")) n = n.substring (2);
                sub.addItem (k + 2, n, true, k == current);
                hasCurrent = hasCurrent || k == current;
            }
        String title = g;
        if (section == 0 && g.startsWith ("Channel ")) title = g + "  (" + proc.channelName (g.getTrailingIntValue() - 1) + ")";
        menu.addSubMenu (title, sub, true, nullptr, hasCurrent);
    }
    SafePointer<LfoStrip> safe (this);
    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&targetBtn), [safe] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        auto* tp = safe->targetParam;
        tp->beginChangeGesture();
        tp->setValueNotifyingHost (tp->convertTo0to1 ((float) (r - 1)));
        tp->endChangeGesture();
        safe->targetBtn.setButtonText (safe->targetLabel());
    });
}

void LfoStrip::timerCallback()
{
    const bool s = syncRaw->load() > 0.5f;
    if (s != lastSync) { lastSync = s; rate.setVisible (! s); div.setVisible (s); }
    const String l = targetLabel();
    if (l != lastLabel) { lastLabel = l; targetBtn.setButtonText (l); }
    repaint (scopeArea());
}

Rectangle<int> LfoStrip::scopeArea() const
{
    if (big) return { 8, 26, getWidth() - 16, 96 };
    return { 2, 18, 30, jmax (8, getHeight() - 42) };
}

void LfoStrip::paint (Graphics& g)
{
    const float v = proc.mod.lfoValue (section, index);
    g.setColour (theme::text);
    g.setFont (theme::font (big ? 13.0f : 10.5f, true));
    g.drawText ("LFO " + String (index + 1), big ? 8 : 2, 0, big ? 80 : 34, big ? 22 : 16, Justification::centredLeft);
    auto sc = scopeArea().toFloat();
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (sc, 2.0f);
    const bool active = targetParam != nullptr && targetParam->getValue() > 1.0e-6f;
    const auto col = active ? accent : theme::textDim;
    if (big)
    {
        // one cycle of the waveform with the current position
        const int shp = roundToInt (proc.apvts.getRawParameterValue (lfoId (section, index, "shape"))->load());
        Path p;
        for (int x = 0; x <= (int) sc.getWidth(); ++x)
        {
            const double ph = x / jmax (1.0, (double) sc.getWidth());
            const float y = ModMatrix::shapeValue (shp, ph, 0.6f * (float) std::sin (ph * 17.0), -0.4f);
            const float py = sc.getCentreY() - y * (sc.getHeight() * 0.42f);
            if (x == 0) p.startNewSubPath (sc.getX() + (float) x, py); else p.lineTo (sc.getX() + (float) x, py);
        }
        g.setColour (col.withAlpha (0.8f));
        g.strokePath (p, PathStrokeType (1.6f));
        const float px = sc.getX() + proc.mod.lfoPhase (section, index) * sc.getWidth();
        g.setColour (Colours::white.withAlpha (0.7f));
        g.drawVerticalLine ((int) px, sc.getY() + 2, sc.getBottom() - 2);
        g.setColour (col);
        g.fillEllipse (px - 4.0f, sc.getCentreY() - v * sc.getHeight() * 0.42f - 4.0f, 8.0f, 8.0f);
    }
    else
    {
        // vertical meter of the current LFO value
        const float mid = sc.getCentreY(), h = sc.getHeight() * 0.5f - 2.0f;
        g.setColour (col);
        if (v >= 0) g.fillRect (sc.getX() + 6.0f, mid - v * h, sc.getWidth() - 12.0f, v * h);
        else        g.fillRect (sc.getX() + 6.0f, mid, sc.getWidth() - 12.0f, -v * h);
        g.setColour (theme::textDim.withAlpha (0.6f));
        g.drawHorizontalLine ((int) mid, sc.getX() + 3.0f, sc.getRight() - 3.0f);
    }
}

void LfoStrip::resized()
{
    if (big)
    {
        auto r = getLocalBounds().reduced (8);
        r.removeFromTop (18 + 96 + 8);
        targetBtn.setBounds (r.removeFromTop (26)); r.removeFromTop (6);
        auto row = r.removeFromTop (26);
        shape.setBounds (row.removeFromLeft (row.getWidth() / 2 - 3)); row.removeFromLeft (6);
        sync.setBounds (row);
        r.removeFromTop (10);
        auto knobs = r.removeFromTop (jmin (120, r.getHeight()));
        const int kw = knobs.getWidth() / 3;
        auto slot = knobs.removeFromLeft (kw);
        rate.setBounds (slot);                                                         // RATE (ms) when SYNC is off
        div.setBounds (slot.withSizeKeepingCentre (jmin (110, slot.getWidth() - 4), 26));   // beat division when SYNC is on
        depth.setBounds (knobs.removeFromLeft (kw));
        phase.setBounds (knobs);
        return;
    }
    // compact row: [label + meter + SYNC] [target / shape] [rate | div] [depth]
    auto r = getLocalBounds();
    auto c0 = r.removeFromLeft (34);
    sync.setBounds (c0.removeFromBottom (18).reduced (1, 0));
    r.removeFromLeft (3);
    auto c3 = r.removeFromRight (46);
    depth.setBounds (c3);
    auto c2 = r.removeFromRight (r.getWidth() >= 150 ? 52 : 46);
    rate.setBounds (c2);
    div.setBounds (c2.withSizeKeepingCentre (c2.getWidth(), 20));
    r.removeFromRight (3);
    auto top = r.removeFromTop (r.getHeight() / 2);
    targetBtn.setBounds (top.reduced (0, 2));
    shape.setBounds (r.reduced (0, 2));
}

//==============================================================================
//  LfoPanel / LfoWindowContent
//==============================================================================
LfoPanel::LfoPanel (TwinboxProcessor& p, int s) : Panel ("LFO", s == 0 ? theme::accentA : theme::accentB), proc (p), section (s)
{
    for (int i = 0; i < numLfos; ++i) addAndMakeVisible (strips.add (new LfoStrip (p, s, i, false)));
    setExpandable ([this]
    {
        if (window) { window->toFront (true); return; }
        window = std::make_unique<ExpandWindow> (String ("Section ") + (section == 0 ? "A" : "B") + " - LFO 1-3",
                                                 new LfoWindowContent (proc, section), 960, 360, [this] { window.reset(); });
    });
}

LfoPanel::~LfoPanel() { window.reset(); }

void LfoPanel::resized()
{
    Panel::resized();
    auto r = content();
    const int h = r.getHeight() / numLfos;
    for (int i = 0; i < strips.size(); ++i)
        strips[i]->setBounds ((i == strips.size() - 1 ? r : r.removeFromTop (h)).reduced (0, 1));
}

LfoWindowContent::LfoWindowContent (TwinboxProcessor& p, int s) : section (s)
{
    for (int i = 0; i < numLfos; ++i) addAndMakeVisible (strips.add (new LfoStrip (p, s, i, true)));
}

void LfoWindowContent::paint (Graphics& g)
{
    g.fillAll (theme::bg);
    for (int i = 0; i < strips.size(); ++i)
    {
        g.setColour (theme::panel);
        g.fillRoundedRectangle (strips[i]->getBounds().toFloat(), 4.0f);
    }
}

void LfoWindowContent::resized()
{
    auto r = getLocalBounds().reduced (8);
    const int w = r.getWidth() / numLfos;
    for (int i = 0; i < strips.size(); ++i)
        strips[i]->setBounds ((i == strips.size() - 1 ? r : r.removeFromLeft (w)).reduced (4, 0));
}
