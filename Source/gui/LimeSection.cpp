#include "LimeSection.h"

using namespace juce;
using namespace params;

namespace
{
    const char* laneNames[] { "KICK", "SNARE", "HAT", "TIE", "POD", "MOD" };

    void setParam (AudioProcessorValueTreeState& s, const String& id, float value)
    {
        if (auto* p = s.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (value));
            p->endChangeGesture();
        }
    }

    int slotOfPart (int part, bool open = false)
    {
        switch (part)
        {
            case kick:  return LimeEngine::slotKick;
            case snare: return LimeEngine::slotSnare;
            case hat:   return open ? LimeEngine::slotHatOpen : LimeEngine::slotHatClosed;
            case tie:   return LimeEngine::slotTie;
            default:    return LimeEngine::slotPod;
        }
    }
}

//==============================================================================
//  LimeSeqGrid
//==============================================================================
LimeSeqGrid::LimeSeqGrid (TwinboxProcessor& p, bool b) : proc (p), big (b)
{
    startTimerHz (30);
    setTooltip (big ? String() : String ("Click: on/off (Hat: closed > open > off) | Right-click: accent | Mod lane: drag values"));
}

int LimeSeqGrid::editPattern() const { return (int) proc.apvts.getRawParameterValue (bPattern)->load(); }

int LimeSeqGrid::laneLength (int lane) const
{
    const char* id = lane <= 2 ? bLen : (lane == 3 ? bTieLen : (lane == 4 ? bPodLen : bModLen));
    return jlimit (1, numSteps, (int) proc.apvts.getRawParameterValue (id)->load());
}

bool LimeSeqGrid::laneStretched (int lane) const
{
    if (lane <= 2) return false;
    const char* id = lane == 3 ? bTieStr : (lane == 4 ? bPodStr : bModStr);
    return proc.apvts.getRawParameterValue (id)->load() > 0.5f;
}

Rectangle<int> LimeSeqGrid::laneBounds (int lane) const
{
    const float w[] { 1.0f, 1.0f, 1.0f, 0.85f, 0.85f, big ? 2.4f : 1.2f };
    const float gap = big ? 16.0f : 6.0f;
    float total = 0; for (float x : w) total += x;
    const float unit = ((float) getHeight() - gap) / total;
    float y = 0;
    for (int i = 0; i < lane; ++i) { y += w[i] * unit; if (i == 2) y += gap; }
    return Rectangle<float> (0.0f, y, (float) getWidth(), w[lane] * unit).toNearestInt();
}

Rectangle<float> LimeSeqGrid::cell (int lane, int step) const
{
    const float lw = big ? 70.0f : 44.0f;
    const float cw = ((float) getWidth() - lw) / (float) numSteps;
    auto lb = laneBounds (lane).toFloat();
    float w = cw;
    if (laneStretched (lane)) w = cw * (float) laneLength (0) / (float) laneLength (lane);
    return { lw + (float) step * w, lb.getY(), w, lb.getHeight() };
}

int LimeSeqGrid::laneAt (int y) const
{
    for (int l = 0; l < 6; ++l)
        if (laneBounds (l).contains (laneBounds (l).getX(), y)) return l;
    return -1;
}

int LimeSeqGrid::stepAt (int lane, int x) const
{
    for (int s = 0; s < numSteps; ++s)
    {
        auto c = cell (lane, s);
        if ((float) x >= c.getX() && (float) x < c.getRight()) return s;
    }
    return -1;
}

void LimeSeqGrid::paint (Graphics& g)
{
    const int pat = editPattern();
    auto& P = proc.lime.pattern (pat);
    const bool showPlay = proc.isPlaying() && proc.lime.playingPattern() == pat;
    const float lw = big ? 70.0f : 44.0f;

    for (int lane = 0; lane < 6; ++lane)
    {
        auto lb = laneBounds (lane);
        const int len = laneLength (lane);
        const bool stretched = laneStretched (lane);
        const int ui = lane <= 2 ? 0 : lane - 2;
        const int ps = showPlay ? proc.lime.laneStep (ui) : -1;

        g.setColour (theme::text);
        g.setFont (theme::font (big ? 14.0f : 10.5f, true));
        g.drawText (laneNames[lane], 2, lb.getY(), (int) lw - 4, lb.getHeight(), Justification::centredLeft);
        if (lane >= 3)
        {
            g.setColour (theme::textDim);
            g.setFont (theme::font (big ? 11.0f : 8.5f));
            g.drawText (String (len) + (stretched ? "~" : ""), 2, lb.getBottom() - (big ? 16 : 10), (int) lw - 6, big ? 14 : 9, Justification::bottomRight);
        }

        for (int s = 0; s < numSteps; ++s)
        {
            if (stretched && s >= len) break;
            auto c = cell (lane, s).reduced (big ? 2.0f : 1.0f, big ? 2.0f : 1.0f);
            const bool beyond = s >= len;
            Colour bgc = ((stretched ? s * laneLength (0) / jmax (1, len) : s) / 4) % 2 == 0 ? theme::stepBeat : theme::stepOff;
            g.setColour (beyond ? bgc.withMultipliedAlpha (0.3f) : bgc);
            g.fillRoundedRectangle (c, 2.0f);

            if (lane == 5)
            {
                const float v = P.mod[(size_t) s].load();
                auto mid = c.getCentreY();
                auto top = c.getY() + c.getHeight() * (1.0f - v);
                g.setColour (theme::accentB.withAlpha (beyond ? 0.25f : 0.85f));
                g.fillRect (Rectangle<float>::leftTopRightBottom (c.getX() + 1, jmin (mid, top), c.getRight() - 1, jmax (mid, top)).withHeight (jmax (1.5f, std::abs (top - mid))));
                g.setColour (theme::textDim.withAlpha (0.4f));
                g.drawHorizontalLine ((int) mid, c.getX(), c.getRight());
            }
            else
            {
                const uint8_t f = P.steps[(size_t) lane][(size_t) s].load();
                if (f & LimeEngine::trig)
                {
                    Colour on = theme::accentB;
                    if (lane == 2 && (f & LimeEngine::open)) on = theme::accentG;
                    if (! (f & LimeEngine::accent)) on = on.withMultipliedBrightness (0.72f);
                    g.setColour (beyond ? on.withMultipliedAlpha (0.3f) : on);
                    g.fillRoundedRectangle (c.reduced (1.0f), 2.0f);
                    if (f & LimeEngine::accent)
                    {
                        g.setColour (Colours::black.withAlpha (0.6f));
                        g.fillEllipse (c.getCentreX() - 2.5f, c.getY() + 3.0f, 5.0f, 5.0f);
                    }
                    if (lane == 2 && big)
                    {
                        g.setColour (Colours::black);
                        g.setFont (theme::font (11.0f, true));
                        g.drawText ((f & LimeEngine::open) ? "O" : "C", c, Justification::centred);
                    }
                }
            }
            if (s == ps)
            {
                g.setColour (Colours::white.withAlpha (0.6f));
                g.drawRoundedRectangle (c, 2.0f, 1.5f);
            }
        }
        if (lane == 2)
        {
            g.setColour (theme::outline);
            g.fillRect (0, lb.getBottom() + (big ? 7 : 2), getWidth(), 2);
        }
    }
}

void LimeSeqGrid::applyCell (int lane, int step, const MouseEvent& e, bool first)
{
    auto& P = proc.lime.pattern (editPattern());
    if (lane == 5)
    {
        auto lb = laneBounds (lane).toFloat();
        const float v = jlimit (0.0f, 1.0f, 1.0f - ((float) e.y - lb.getY()) / lb.getHeight());
        P.mod[(size_t) step].store (e.mods.isAltDown() || e.mods.isRightButtonDown() ? 0.5f : v);
        return;
    }
    auto& c = P.steps[(size_t) lane][(size_t) step];
    if (first)
    {
        const uint8_t v = c.load();
        if (e.mods.isRightButtonDown())
            dragValue = (v & LimeEngine::accent) ? (uint8_t) (v & ~LimeEngine::accent) : (uint8_t) (v | LimeEngine::accent | LimeEngine::trig);
        else if (lane == 2)
            dragValue = ! (v & LimeEngine::trig) ? LimeEngine::trig
                      : (! (v & LimeEngine::open) ? (uint8_t) (LimeEngine::trig | LimeEngine::open | (v & LimeEngine::accent)) : 0);
        else
            dragValue = (v & LimeEngine::trig) ? 0 : LimeEngine::trig;
    }
    c.store (dragValue);
}

void LimeSeqGrid::mouseDown (const MouseEvent& e)
{
    const int lane = laneAt (e.y);
    if (lane < 0) return;
    const int s = stepAt (lane, e.x);
    if (s < 0) return;
    dragLane = lane; lastDragStep = s;
    applyCell (lane, s, e, true);
    repaint();
}

void LimeSeqGrid::mouseDrag (const MouseEvent& e)
{
    if (dragLane < 0) return;
    const int s = stepAt (dragLane, e.x);
    if (s < 0 || (s == lastDragStep && dragLane != 5)) return;
    lastDragStep = s;
    applyCell (dragLane, s, e, false);
    repaint();
}

void LimeSeqGrid::timerCallback()
{
    bool changed = false;
    for (int i = 0; i < 4; ++i)
    {
        const int s = proc.isPlaying() ? proc.lime.laneStep (i) : -1;
        if (s != lastSteps[(size_t) i]) { lastSteps[(size_t) i] = s; changed = true; }
    }
    if (changed || big) repaint();
}

//==============================================================================
//  LimeSeqWindowContent
//==============================================================================
LimeSeqWindowContent::LimeSeqWindowContent (TwinboxProcessor& p) : proc (p), grid (p, true)
{
    for (int i = 0; i < numLimePatterns; ++i)
    {
        auto* b = slots.add (new TextButton ("Pattern " + String (i + 1)));
        b->setColour (TextButton::buttonOnColourId, theme::accentB);
        b->onClick = [this, i] { setParam (proc.apvts, bPattern, (float) i); };
        addAndMakeVisible (b);
    }
    addAndMakeVisible (grid);
    for (int l = 0; l < 6; ++l)
    {
        auto& t = tools[(size_t) l];
        t.clear.onClick = [this, l] { laneOp (l, 0); };
        t.left.onClick  = [this, l] { laneOp (l, 1); };
        t.right.onClick = [this, l] { laneOp (l, 2); };
        t.rnd.onClick   = [this, l] { laneOp (l, 3); };
        t.four.onClick  = [this, l] { laneOp (l, 4); };
        t.clear.setTooltip ("Clear lane"); t.left.setTooltip ("Shift left"); t.right.setTooltip ("Shift right");
        t.rnd.setTooltip ("Randomise lane"); t.four.setTooltip (l == 5 ? "Reset values to centre" : "Fill every 4th step");
        for (auto* b : { &t.clear, &t.left, &t.right, &t.rnd, &t.four }) addAndMakeVisible (b);
    }
    auto& s = proc.apvts;
    len.attach (s, bLen); tieLen.attach (s, bTieLen); podLen.attach (s, bPodLen); modLen.attach (s, bModLen);
    shuffle.attach (s, bShuffle); accent.attach (s, bAccent);
    tieStr.attach (s, bTieStr); podStr.attach (s, bPodStr); modStr.attach (s, bModStr);
    modDest.addItemList (modDestNames(), 1);
    modDestAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (s, bModDest, modDest);
    modAmt.attach (s, bModAmt);
    for (auto* c : std::initializer_list<Component*> { &len, &tieLen, &podLen, &modLen, &shuffle, &accent, &tieStr, &podStr, &modStr, &modDest, &modAmt })
        addAndMakeVisible (c);
    startTimerHz (10);
}

void LimeSeqWindowContent::laneOp (int lane, int op)
{
    auto& P = proc.lime.pattern ((int) proc.apvts.getRawParameterValue (bPattern)->load());
    Random rnd;
    if (lane == 5)
    {
        std::array<float, numSteps> v;
        for (int i = 0; i < numSteps; ++i) v[(size_t) i] = P.mod[(size_t) i].load();
        for (int i = 0; i < numSteps; ++i)
        {
            float nv = v[(size_t) i];
            if (op == 0 || op == 4) nv = 0.5f;
            else if (op == 1) nv = v[(size_t) ((i + 1) % numSteps)];
            else if (op == 2) nv = v[(size_t) ((i + numSteps - 1) % numSteps)];
            else if (op == 3) nv = rnd.nextFloat();
            P.mod[(size_t) i].store (nv);
        }
    }
    else
    {
        std::array<uint8_t, numSteps> v;
        for (int i = 0; i < numSteps; ++i) v[(size_t) i] = P.steps[(size_t) lane][(size_t) i].load();
        for (int i = 0; i < numSteps; ++i)
        {
            uint8_t nv = v[(size_t) i];
            if (op == 0) nv = 0;
            else if (op == 1) nv = v[(size_t) ((i + 1) % numSteps)];
            else if (op == 2) nv = v[(size_t) ((i + numSteps - 1) % numSteps)];
            else if (op == 3)
            {
                nv = rnd.nextFloat() < 0.3f ? LimeEngine::trig : 0;
                if (nv && rnd.nextFloat() < 0.25f) nv |= LimeEngine::accent;
                if (nv && lane == 2 && rnd.nextFloat() < 0.3f) nv |= LimeEngine::open;
            }
            else if (op == 4) nv = (i % 4 == 0) ? LimeEngine::trig : 0;
            P.steps[(size_t) lane][(size_t) i].store (nv);
        }
    }
    grid.repaint();
}

void LimeSeqWindowContent::timerCallback()
{
    const int pat = (int) proc.apvts.getRawParameterValue (bPattern)->load();
    for (int i = 0; i < slots.size(); ++i) slots[i]->setToggleState (i == pat, dontSendNotification);
}

void LimeSeqWindowContent::paint (Graphics& g)
{
    g.fillAll (theme::bg);
    g.setColour (theme::textDim);
    g.setFont (theme::font (12.0f));
    g.drawText ("Click: step on/off  |  Hat: closed > open > off  |  Right-click: accent  |  Drag: paint  |  Mod lane: drag bars (right-click = centre)",
                12, getHeight() - 24, getWidth() - 24, 20, Justification::centredLeft);
}

void LimeSeqWindowContent::resized()
{
    auto r = getLocalBounds().reduced (12);
    r.removeFromBottom (24);
    auto top = r.removeFromTop (32);
    const int bw = top.getWidth() / numLimePatterns;
    for (auto* b : slots) b->setBounds (top.removeFromLeft (bw).reduced (3, 2));
    r.removeFromTop (10);

    auto bottom = r.removeFromBottom (190);
    layoutRows (bottom, { { &len, &tieLen, &podLen, &modLen, &shuffle, &accent, &modDest, &modAmt },
                          { nullptr, &tieStr, &podStr, &modStr, nullptr, nullptr, nullptr, nullptr } });
    modDest.setBounds (modDest.getBounds().withSizeKeepingCentre (modDest.getWidth(), 26));
    r.removeFromBottom (10);

    auto toolsArea = r.removeFromRight (230);
    r.removeFromRight (8);
    grid.setBounds (r);
    for (int l = 0; l < 6; ++l)
    {
        auto lb = grid.laneBounds (l).translated (0, r.getY());
        auto row = Rectangle<int> (toolsArea.getX(), lb.getY(), toolsArea.getWidth(), lb.getHeight()).withSizeKeepingCentre (toolsArea.getWidth(), jmin (28, lb.getHeight() - 4));
        auto& t = tools[(size_t) l];
        const int w = row.getWidth() / 5;
        for (auto* b : { &t.clear, &t.left, &t.right, &t.rnd, &t.four }) b->setBounds (row.removeFromLeft (w).reduced (2, 0));
    }
}

//==============================================================================
//  SampleView
//==============================================================================
void SampleView::timerCallback()
{
    auto s = proc.lime.sampleFor (slot);
    const int h = proc.lime.hitCount (slot);
    if (s.get() != last || h != lastHits || flash > 0.01f)
    {
        if (h != lastHits) flash = 1.0f; else flash *= 0.7f;
        lastHits = h;
        last = s.get();
        repaint();
    }
}

void SampleView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (theme::outline);
    g.fillRoundedRectangle (r, 3.0f);
    auto s = proc.lime.sampleFor (slot);
    if (s == nullptr || s->data.empty()) return;
    const auto& d = s->data;
    const int w = getWidth();
    const float mid = r.getCentreY(), hh = r.getHeight() * 0.45f;
    g.setColour (theme::accentB.withAlpha (0.6f + 0.4f * flash));
    for (int x = 0; x < w; ++x)
    {
        const size_t a0 = (size_t) ((double) x / w * d.size()), a1 = jmin (d.size(), (size_t) ((double) (x + 1) / w * d.size()) + 1);
        float mn = 0, mx = 0;
        for (size_t i = a0; i < a1; ++i) { mn = jmin (mn, d[i]); mx = jmax (mx, d[i]); }
        g.drawVerticalLine (x, mid - mx * hh, mid - mn * hh + 1.0f);
    }
    g.setColour (theme::text);
    g.setFont (theme::font (12.0f));
    g.drawText (s->name + "   " + String ((double) d.size() / s->sourceRate, 2) + " s", r.reduced (6, 4).toNearestInt(), Justification::topLeft);
}

//==============================================================================
//  LimeSamplerPanel
//==============================================================================
LimeSamplerPanel::LimeSamplerPanel (TwinboxProcessor& p, int pt, bool b)
    : Panel (limePartName (pt), theme::accentB), proc (p), part (pt), big (b)
{
    auto& st = proc.apvts;
    setupSlot (s1, slotOfPart (part, false), params::b (part, lm::smp));
    if (part == hat) setupSlot (s2, slotOfPart (part, true), params::b (part, lm::smpOpen));

    auto addK = [&] (const char* suffix, const String& cap)
    {
        auto* k = knobs.add (new Knob (cap, theme::accentB));
        k->attach (st, params::b (part, suffix));
        addAndMakeVisible (k);
    };
    auto addT = [&] (const char* suffix, const String& cap)
    {
        auto* t = toggles.add (new Toggle (cap, theme::accentB));
        t->attach (st, params::b (part, suffix));
        addAndMakeVisible (t);
    };

    addK (lm::tune, "Tune");
    if (part == hat) { addK (lm::dcy, "Decay C"); addK (lm::dcyOpen, "Decay O"); addK (lm::cut, "Color"); }
    else             { addK (lm::dcy, "Decay"); addK (lm::cut, "Cutoff"); addK (lm::res, "Reso"); }
    if (part == tie) { addK (lm::fmFreq, "FM Freq"); addK (lm::fmAmt, "FM Amt"); }
    if (part == pod) { addK (lm::zap, "Zap"); addK (lm::zapDcy, "Zap Dcy"); }
    addK (lm::lvl, "Level");
    addK (lm::pan, "Pan");
    addK (lm::send, "Delay");
    if (part == kick) addT (lm::lowcut, "Low Cut");
    if (part == hat)  addT (lm::link, "Link");
    addT (lm::mute, "Mute");
    if (part == snare)
    {
        fmode = std::make_unique<Segmented> (theme::accentB, false);
        fmode->attach (st, params::b (part, lm::fmode));
        addAndMakeVisible (*fmode);
    }
    if (big)
    {
        view1 = std::make_unique<SampleView> (proc, s1.slot);
        addAndMakeVisible (*view1);
        if (part == hat) { view2 = std::make_unique<SampleView> (proc, s2.slot); addAndMakeVisible (*view2); }
    }
    startTimerHz (8);
}

void LimeSamplerPanel::setupSlot (SampleSlot& s, int slot, const String& paramId)
{
    s.slot = slot;
    s.box.addItemList (SampleBank::get().names(), 1);
    s.box.setTooltip ("Factory sample (drop a WAV/AIFF file on the panel or press Load for your own)");
    s.att = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, paramId, s.box);
    s.load.onClick = [this, slot, &s]
    {
        if (proc.lime.userSample (slot) != nullptr) { proc.lime.setUserSample (slot, nullptr); s.box.setEnabled (true); timerCallback(); }
        else chooseFile (slot);
    };
    s.play.onClick = [this, slot] { proc.lime.audition (slot, 1.0f); };
    s.play.setTooltip ("Audition");
    addAndMakeVisible (s.box); addAndMakeVisible (s.load); addAndMakeVisible (s.play);
}

void LimeSamplerPanel::chooseFile (int slot)
{
    chooser = std::make_unique<FileChooser> ("Load sample", File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this, slot] (const FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f.existsAsFile()) proc.loadUserSample (slot, f);
        timerCallback();
    });
}

void LimeSamplerPanel::filesDropped (const StringArray& files, int, int y)
{
    if (files.isEmpty()) return;
    int slot = s1.slot;
    if (part == hat && y > getHeight() / 2) slot = s2.slot;
    proc.loadUserSample (slot, File (files[0]));
    timerCallback();
}

void LimeSamplerPanel::timerCallback()
{
    for (auto* s : { &s1, &s2 })
    {
        if (s->att == nullptr) continue;
        auto u = proc.lime.userSample (s->slot);
        if (u != nullptr)
        {
            s->box.setEnabled (false);
            s->box.setText ("User: " + u->name, dontSendNotification);
            s->load.setButtonText ("Clear");
        }
        else
        {
            if (! s->box.isEnabled()) { s->box.setEnabled (true); s->att.reset(); s->att = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, part == hat && s == &s2 ? params::b (part, lm::smpOpen) : params::b (part, lm::smp), s->box); }
            s->load.setButtonText ("Load");
        }
    }
}

void LimeSamplerPanel::paint (Graphics& g)
{
    Panel::paint (g);
    if (big && part == hat)
    {
        g.setColour (theme::textDim);
        g.setFont (theme::font (12.0f));
        g.drawText ("Closed", s1.box.getX() - 58, s1.box.getY(), 54, s1.box.getHeight(), Justification::centredRight);
        g.drawText ("Open", s2.box.getX() - 58, s2.box.getY(), 54, s2.box.getHeight(), Justification::centredRight);
    }
}

void LimeSamplerPanel::resized()
{
    Panel::resized();
    auto r = content();
    auto sampleRow = [&] (SampleSlot& s, const String& label)
    {
        auto row = r.removeFromTop (big ? 28 : 22);
        if (label.isNotEmpty() && big)
        {
            row.removeFromLeft (60);
        }
        s.play.setBounds (row.removeFromRight (big ? 34 : 24));
        row.removeFromRight (2);
        s.load.setBounds (row.removeFromRight (big ? 60 : 40));
        row.removeFromRight (2);
        s.box.setBounds (row);
        r.removeFromTop (3);
    };
    sampleRow (s1, part == hat ? "Closed" : "");
    if (part == hat) sampleRow (s2, "Open");

    if (big)
    {
        auto views = r.removeFromTop (jmax (120, r.getHeight() / 3));
        if (view2 != nullptr)
        {
            view1->setBounds (views.removeFromLeft (views.getWidth() / 2).reduced (0, 2).withTrimmedRight (3));
            view2->setBounds (views.reduced (0, 2).withTrimmedLeft (3));
        }
        else view1->setBounds (views.reduced (0, 2));
        r.removeFromTop (8);
    }

    std::vector<Component*> items;
    for (auto* k : knobs) items.push_back (k);
    if (fmode) items.push_back (fmode.get());
    for (auto* t : toggles) items.push_back (t);
    const int cols = big ? (int) (items.size() + 1) / 2 : (int) (items.size() + 1) / 2;
    std::vector<std::vector<Component*>> rowsV (2);
    for (size_t i = 0; i < items.size(); ++i) rowsV[i < (size_t) cols ? 0 : 1].push_back (items[i]);
    layoutRows (r, rowsV, cols);
    if (fmode) fmode->setBounds (fmode->getBounds().withSizeKeepingCentre (jmin (fmode->getWidth(), 100), 24));
}

//==============================================================================
//  LimeMixerView
//==============================================================================
LimeMixerView::LimeMixerView (TwinboxProcessor& p) : proc (p)
{
    auto& s = proc.apvts;
    for (int pt = 0; pt < numLimeParts; ++pt)
    {
        auto& st = strips[(size_t) pt];
        st.fader.setSliderStyle (Slider::LinearVertical);
        st.fader.setTextBoxStyle (Slider::TextBoxBelow, false, 70, 18);
        st.fader.setColour (Slider::trackColourId, theme::accentB);
        st.att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (s, params::b (pt, lm::lvl), st.fader);
        st.pan.attach (s, params::b (pt, lm::pan));
        st.send.attach (s, params::b (pt, lm::send));
        st.mute.attach (s, params::b (pt, lm::mute));
        st.play.onClick = [this, pt] { proc.lime.audition (slotOfPart (pt), 1.0f); };
        for (auto* c : std::initializer_list<Component*> { &st.fader, &st.pan, &st.send, &st.mute, &st.play }) addAndMakeVisible (c);
    }
    for (auto* pn : { &dlyP, &fltP, &lfoP, &modP, &outP }) addAndMakeVisible (pn);
    dlyTime.attach (s, bDlyTime); dlyTime.setCaption ("Time (tempo synced)");
    dlyFb.attach (s, bDlyFb); dlyDamp.attach (s, bDlyDamp); dlyLvl.attach (s, bDlyLvl);
    fltCut.attach (s, bFltCut); fltRes.attach (s, bFltRes);
    lfoRate.attach (s, bLfoRate); lfoFM.attach (s, bLfoFM); lfoDM.attach (s, bLfoDM);
    modDest.addItemList (modDestNames(), 1);
    modDestAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (s, bModDest, modDest);
    modAmt.attach (s, bModAmt);
    gain.attach (s, bGain); pitch.attach (s, bPitch); dirt.attach (s, bDirt); norm.attach (s, bNorm);
    for (auto* c : std::initializer_list<Component*> { &dlyTime, &dlyFb, &dlyDamp, &dlyLvl, &fltCut, &fltRes, &lfoRate, &lfoFM, &lfoDM,
                                                         &modDest, &modAmt, &gain, &pitch, &dirt, &norm })
        addAndMakeVisible (c);
    startTimerHz (20);
}

void LimeMixerView::paint (Graphics& g)
{
    g.fillAll (theme::bg);
    auto r = getLocalBounds().reduced (12);
    auto mix = r.removeFromLeft (r.getWidth() * 5 / 11);
    const int sw = mix.getWidth() / numLimeParts;
    for (int pt = 0; pt < numLimeParts; ++pt)
    {
        auto col = mix.removeFromLeft (sw).reduced (4, 0).toFloat();
        g.setColour (theme::panel);
        g.fillRoundedRectangle (col, 4.0f);
        g.setColour (theme::text);
        g.setFont (theme::font (14.0f, true));
        g.drawText (limePartName (pt), col.removeFromTop (26), Justification::centred);
    }
    drawMeter (g, Rectangle<float> ((float) getWidth() - 22.0f, 40.0f, 8.0f, (float) getHeight() - 80.0f), proc.meter (1), theme::accentB);
}

void LimeMixerView::resized()
{
    auto r = getLocalBounds().reduced (12);
    r.removeFromRight (24);
    auto mix = r.removeFromLeft ((getWidth() - 24) * 5 / 11);
    const int sw = mix.getWidth() / numLimeParts;
    for (int pt = 0; pt < numLimeParts; ++pt)
    {
        auto col = mix.removeFromLeft (sw).reduced (8, 0);
        col.removeFromTop (30);
        auto& st = strips[(size_t) pt];
        st.play.setBounds (col.removeFromBottom (26).reduced (4, 2));
        st.mute.setBounds (col.removeFromBottom (26).reduced (4, 2));
        st.send.setBounds (col.removeFromBottom (80));
        st.pan.setBounds (col.removeFromBottom (80));
        st.fader.setBounds (col.reduced (0, 4));
    }
    r.removeFromLeft (12);
    const int h = r.getHeight() / 3;
    auto row1 = r.removeFromTop (h), row2 = r.removeFromTop (h), row3 = r;
    dlyP.setBounds (row1.reduced (3));
    auto rr = row2; fltP.setBounds (rr.removeFromLeft (rr.getWidth() / 2).reduced (3)); lfoP.setBounds (rr.reduced (3));
    auto r3 = row3; modP.setBounds (r3.removeFromLeft (r3.getWidth() / 2).reduced (3)); outP.setBounds (r3.reduced (3));

    auto d = dlyP.contentInParent();
    dlyTime.setBounds (d.removeFromTop (46).reduced (2));
    layoutRows (d, { { &dlyFb, &dlyDamp, &dlyLvl } });
    layoutRows (fltP.contentInParent(), { { &fltCut, &fltRes } });
    layoutRows (lfoP.contentInParent(), { { &lfoRate, &lfoFM, &lfoDM } });
    auto m = modP.contentInParent();
    modDest.setBounds (m.removeFromTop (28).reduced (2));
    layoutRows (m, { { &modAmt } });
    layoutRows (outP.contentInParent(), { { &gain, &pitch }, { &dirt, &norm } });
}

//==============================================================================
//  LimeSection
//==============================================================================
LimeSection::LimeSection (TwinboxProcessor& p) : proc (p), grid (p, false)
{
    auto& s = proc.apvts;

    addAndMakeVisible (seqPanel);
    seqPanel.setExpandable ([this] { openSeqWindow(); });
    for (int i = 0; i < numLimePatterns; ++i)
    {
        auto* b = slots.add (new TextButton (String (i + 1)));
        b->setColour (TextButton::buttonOnColourId, theme::accentB);
        b->setTooltip ("Pattern " + String (i + 1) + " (switches at the end of the main sequence)");
        b->onClick = [this, i] { setParam (proc.apvts, bPattern, (float) i); };
        addAndMakeVisible (b);
    }
    copyPat.onClick = [this] { patClip = (int) proc.apvts.getRawParameterValue (bPattern)->load(); };
    pastePat.onClick = [this]
    {
        if (patClip < 0) return;
        auto& src = proc.lime.pattern (patClip);
        auto& dst = proc.lime.pattern ((int) proc.apvts.getRawParameterValue (bPattern)->load());
        for (int l = 0; l < LimeEngine::numTrigLanes; ++l)
            for (int st = 0; st < numSteps; ++st) dst.steps[(size_t) l][(size_t) st].store (src.steps[(size_t) l][(size_t) st].load());
        for (int st = 0; st < numSteps; ++st) dst.mod[(size_t) st].store (src.mod[(size_t) st].load());
        grid.repaint();
    };
    addAndMakeVisible (copyPat); addAndMakeVisible (pastePat);
    addAndMakeVisible (grid);

    addAndMakeVisible (remixPanel);
    remixPanel.setExpandable ([this] { openSeqWindow(); });
    len.attach (s, bLen); tieLen.attach (s, bTieLen); podLen.attach (s, bPodLen); modLen.attach (s, bModLen);
    tieStr.attach (s, bTieStr); podStr.attach (s, bPodStr); modStr.attach (s, bModStr);
    tieStr.setTooltip ("Stretch the Tie sequence to the main length"); podStr.setTooltip ("Stretch the Pod sequence to the main length");
    modStr.setTooltip ("Stretch the Mod sequence to the main length");
    shuffle.attach (s, bShuffle); accent.attach (s, bAccent);
    for (auto* c : std::initializer_list<Component*> { &len, &tieLen, &podLen, &modLen, &tieStr, &podStr, &modStr, &shuffle, &accent })
        addAndMakeVisible (c);

    addAndMakeVisible (outPanel);
    outPanel.setExpandable ([this] { openMixerWindow(); });
    gain.attach (s, bGain); pitch.attach (s, bPitch); dirt.attach (s, bDirt); norm.attach (s, bNorm);
    lfoRate.attach (s, bLfoRate); lfoFM.attach (s, bLfoFM); lfoDM.attach (s, bLfoDM);
    lfoFM.slider.setTooltip ("LFO > filter cutoff modulation"); lfoDM.slider.setTooltip ("LFO > delay time modulation");
    for (auto* c : std::initializer_list<Component*> { &gain, &pitch, &dirt, &norm, &lfoRate, &lfoFM, &lfoDM }) addAndMakeVisible (c);

    for (int pt = 0; pt < numLimeParts; ++pt)
    {
        auto* sp = samplers.add (new LimeSamplerPanel (proc, pt, false));
        sp->setExpandable ([this, pt] { openSamplerWindow (pt); });
        addAndMakeVisible (sp);
    }

    for (auto* pn : { &dlyPanel, &fltPanel, &modPanel }) { addAndMakeVisible (pn); pn->setExpandable ([this] { openMixerWindow(); }); }
    modPanel.setExpandable ([this] { openSeqWindow(); });
    dlyTime.addItemList (delayTimeNames(), 1);
    dlyTimeAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (s, bDlyTime, dlyTime);
    dlyFb.attach (s, bDlyFb); dlyDamp.attach (s, bDlyDamp); dlyLvl.attach (s, bDlyLvl);
    fltCut.attach (s, bFltCut); fltRes.attach (s, bFltRes);
    modDest.addItemList (modDestNames(), 1);
    modDestAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (s, bModDest, modDest);
    modAmt.attach (s, bModAmt);
    for (auto* c : std::initializer_list<Component*> { &dlyTime, &dlyFb, &dlyDamp, &dlyLvl, &fltCut, &fltRes, &modDest, &modAmt })
        addAndMakeVisible (c);

    startTimerHz (15);
}

LimeSection::~LimeSection() { stopTimer(); }

void LimeSection::openSeqWindow()
{
    if (seqWindow != nullptr) { seqWindow->toFront (true); return; }
    seqWindow = std::make_unique<ExpandWindow> ("Section B - Sequencer", new LimeSeqWindowContent (proc), 1280, 820, [this] { seqWindow.reset(); });
}

void LimeSection::openSamplerWindow (int part)
{
    auto& w = samplerWindows[(size_t) part];
    if (w != nullptr) { w->toFront (true); return; }
    w = std::make_unique<ExpandWindow> (String ("Section B - ") + limePartName (part) + " Sampler",
                                        new LimeSamplerPanel (proc, part, true), 900, 620, [this, part] { samplerWindows[(size_t) part].reset(); });
}

void LimeSection::openMixerWindow()
{
    if (mixerWindow != nullptr) { mixerWindow->toFront (true); return; }
    mixerWindow = std::make_unique<ExpandWindow> ("Section B - Mixer, FX & Output", new LimeMixerView (proc), 1300, 720, [this] { mixerWindow.reset(); });
}

void LimeSection::timerCallback()
{
    const int pat = (int) proc.apvts.getRawParameterValue (bPattern)->load();
    const int playing = proc.lime.playingPattern();
    for (int i = 0; i < slots.size(); ++i)
    {
        slots[i]->setToggleState (i == pat, dontSendNotification);
        slots[i]->setColour (TextButton::buttonColourId, (proc.isPlaying() && i == playing && i != pat) ? theme::accentB.withAlpha (0.35f) : theme::panel2);
    }
    repaint (getWidth() - 24, 0, 24, 50);
}

void LimeSection::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (theme::panel.darker (0.25f));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (theme::accentB);
    g.fillRoundedRectangle (10.0f, 10.0f, 22.0f, 22.0f, 3.0f);
    g.setColour (Colours::black);
    g.setFont (theme::font (15.0f, true));
    g.drawText ("B", 10, 10, 22, 22, Justification::centred);
    g.setColour (theme::text);
    g.setFont (theme::font (14.0f, true));
    g.drawText ("GROOVEBOX", 40, 10, 140, 22, Justification::centredLeft);
    g.setColour (theme::textDim);
    g.setFont (theme::font (11.0f));
    g.drawText ("5 samplers / sequencer / FX", 40, 26, 200, 14, Justification::centredLeft);
    drawMeter (g, Rectangle<float> ((float) getWidth() - 16.0f, 10.0f, 6.0f, 30.0f), proc.meter (1), theme::accentB);
}

void LimeSection::resized()
{
    auto r = getLocalBounds().reduced (8);
    r.removeFromTop (38);
    const int W = r.getWidth();

    auto top = r.removeFromTop (r.getHeight() * 40 / 100);
    seqPanel.setBounds (top.removeFromLeft (W * 46 / 100).reduced (2, 0));
    remixPanel.setBounds (top.removeFromLeft (W * 22 / 100).reduced (2, 0));
    outPanel.setBounds (top.reduced (2, 0));

    auto sc = seqPanel.contentInParent();
    auto sr = sc.removeFromTop (22);
    auto cp = sr.removeFromRight (110);
    copyPat.setBounds (cp.removeFromLeft (55).reduced (1, 0));
    pastePat.setBounds (cp.reduced (1, 0));
    sr.removeFromRight (6);
    const int bw = jmin (34, sr.getWidth() / numLimePatterns);
    for (auto* b : slots) b->setBounds (sr.removeFromLeft (bw).reduced (1, 0));
    sc.removeFromTop (4);
    grid.setBounds (sc);

    layoutRows (remixPanel.contentInParent(), { { &len, &tieLen, &podLen, &modLen, &shuffle },
                                        { &accent, &tieStr, &podStr, &modStr, nullptr } });
    // place stretch toggles directly under their length knobs
    for (auto pr : { std::pair<Component*, Component*> { &tieLen, &tieStr }, { &podLen, &podStr }, { &modLen, &modStr } })
        pr.second->setBounds (pr.second->getBounds().withX (pr.first->getBounds().getCentreX() - pr.second->getWidth() / 2));

    layoutRows (outPanel.contentInParent(), { { &gain, &pitch, &dirt, &norm }, { &lfoRate, &lfoFM, &lfoDM, nullptr } });

    r.removeFromTop (6);
    auto fx = r.removeFromBottom (jmax (96, r.getHeight() * 32 / 100));
    r.removeFromBottom (6);
    const int sw = r.getWidth() / numLimeParts;
    for (int i = 0; i < samplers.size(); ++i)
        samplers[i]->setBounds ((i == samplers.size() - 1 ? r : r.removeFromLeft (sw)).reduced (2, 0));

    dlyPanel.setBounds (fx.removeFromLeft (W * 45 / 100).reduced (2, 0));
    fltPanel.setBounds (fx.removeFromLeft (W * 22 / 100).reduced (2, 0));
    modPanel.setBounds (fx.reduced (2, 0));
    {
        auto d = dlyPanel.contentInParent();
        auto c = d.removeFromLeft (d.getWidth() / 4).reduced (4, 0);
        dlyTime.setBounds (c.withSizeKeepingCentre (c.getWidth(), 24));
        layoutRows (d, { { &dlyFb, &dlyDamp, &dlyLvl } });
    }
    layoutRows (fltPanel.contentInParent(), { { &fltCut, &fltRes } });
    {
        auto m = modPanel.contentInParent();
        auto c = m.removeFromLeft (m.getWidth() / 2).reduced (4, 0);
        modDest.setBounds (c.withSizeKeepingCentre (c.getWidth(), 24));
        layoutRows (m, { { &modAmt } });
    }
}
