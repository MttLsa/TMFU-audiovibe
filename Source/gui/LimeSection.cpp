#include "LimeSection.h"

using namespace juce;
using namespace params;

namespace
{
    void setParam (AudioProcessorValueTreeState& s, const String& id, float value)
    {
        if (auto* p = s.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (value));
            p->endChangeGesture();
        }
    }
    float getParam (AudioProcessorValueTreeState& s, const String& id) { return s.getRawParameterValue (id)->load(); }
    const char* laneNames[] { "KICK", "SNARE", "C-HAT", "O-HAT", "BELT", "CAPSULE", "MOD" };
    int targetOfModule (int m) { static const int t[] { LimeEngine::tKick, LimeEngine::tSnare, LimeEngine::tCHat, LimeEngine::tTie, LimeEngine::tPod }; return t[m]; }
}

//==============================================================================
//  ControlPanel
//==============================================================================
ControlPanel::ControlPanel (TwinboxProcessor& p, const String& t, Colour a) : Panel (t, a), proc (p) {}

Knob* ControlPanel::knob (const String& id, const String& caption)
{
    auto* k = new Knob (caption, accent);
    k->attach (proc.apvts, id);
    owned.add (k); addAndMakeVisible (k);
    return k;
}

Toggle* ControlPanel::toggle (const String& id, const String& caption)
{
    auto* t = new Toggle (caption, accent);
    t->attach (proc.apvts, id);
    owned.add (t); addAndMakeVisible (t);
    return t;
}

Segmented* ControlPanel::seg (const String& id, StringArray labels, const String& caption)
{
    auto* s = new Segmented (accent, false);
    if (caption.isNotEmpty()) s->setCaption (caption);
    s->attach (proc.apvts, id, labels);
    owned.add (s); addAndMakeVisible (s);
    return s;
}

ComboBox* ControlPanel::combo (const String& id)
{
    auto* c = new ComboBox();
    if (auto* prm = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter (id))) c->addItemList (prm->choices, 1);
    comboAtts.add (new AudioProcessorValueTreeState::ComboBoxAttachment (proc.apvts, id, *c));
    c->setTooltip (proc.apvts.getParameter (id)->getName (64));
    owned.add (c); addAndMakeVisible (c);
    return c;
}

Slider* ControlPanel::incDec (const String& id)
{
    auto* s = new Slider (Slider::IncDecButtons, Slider::TextBoxLeft);
    s->setTextBoxStyle (Slider::TextBoxLeft, false, 34, 20);
    sliderAtts.add (new AudioProcessorValueTreeState::SliderAttachment (proc.apvts, id, *s));
    s->setTooltip (proc.apvts.getParameter (id)->getName (64));
    owned.add (s); addAndMakeVisible (s);
    return s;
}

void ControlPanel::resized()
{
    Panel::resized();
    if (customLayout) { customLayout (*this); return; }
    auto r = content();
    if (topH > 0 && ! tops.empty())
    {
        auto t = r.removeFromTop (topH);
        const int w = t.getWidth() / (int) tops.size();
        for (auto* c : tops) c->setBounds (t.removeFromLeft (w).reduced (2, 1));
        r.removeFromTop (3);
    }
    layoutRows (r, rows);
    for (auto& rw : rows)
        for (auto* c : rw)
            if (auto* cb = dynamic_cast<ComboBox*> (c)) cb->setBounds (cb->getBounds().withSizeKeepingCentre (cb->getWidth(), jmin (cb->getHeight(), 24)));
            else if (auto* sl = dynamic_cast<Slider*> (c)) sl->setBounds (sl->getBounds().withSizeKeepingCentre (sl->getWidth(), jmin (sl->getHeight(), 24)));
}

//==============================================================================
//  Panels factory (original control sets of Limelite 1.3)
//==============================================================================
std::unique_ptr<ControlPanel> makeLimePanel (TwinboxProcessor& p, const String& which, bool big)
{
    ignoreUnused (big);
    auto cp = std::make_unique<ControlPanel> (p, which.toUpperCase());
    auto& c = *cp;
    if (which == "remix")
    {
        auto* now = new TextButton ("NOW!");
        now->setTooltip ("Remix the next bar now");
        now->onClick = [&p] { p.lime.remixNow(); };
        c.owned.add (now); c.addAndMakeVisible (now);
        auto* shift = c.incDec (bShift);
        c.row ({ now, shift, c.knob (bRemix, "Remix"), c.combo (bRemixRate) });
        c.row ({ c.knob (bSwing, "Swing"), c.toggle (bHalf, "1/2"), c.knob (bStart, "Start"), nullptr });
        c.row ({ c.knob (bLen, "Len"), c.knob (bTieLen, "Belt"), c.knob (bPodLen, "Capsule"), c.knob (bModLen, "Mod") });
        c.row ({ nullptr, c.toggle (bTieStr, "Stretch"), c.toggle (bPodStr, "Stretch"), c.toggle (bModStr, "Stretch") });
    }
    else if (which == "out")
    {
        c.row ({ c.knob (bDirt, "Trash"), c.knob (bGain, "Gain"), c.knob (bTune, "Tune") });
    }
    else if (which == "global")
    {
        c.row ({ c.toggle (gRun, "CLK"), c.knob (gTempo, "BPM") });
    }
    else if (which == "xfx")
    {
        c.setTitle ("XFX");
        auto* mode = c.combo (bXMode);
        auto* pad = new XYPad (p, bXParam, bXAdd);
        c.owned.add (pad); c.addAndMakeVisible (pad);
        auto* dly = c.knob (bXDly, "Dly"); auto* gain = c.knob (bXGain, "Gain"); auto* lo = c.knob (bXLo, "Lo"); auto* hi = c.knob (bXHi, "Hi");
        auto* spd = c.knob (bXSpd, "Spd"); auto* len = c.knob (bXLen, "Len"); auto* sm = c.knob (bXSm, "Sm");
        auto* play = c.toggle (bXPlay, juce::String::fromUTF8 ("\xe2\x96\xb6 PLAY")); auto* rec = c.toggle (bXRec, juce::String::fromUTF8 ("\xe2\x97\x8f REC"));
        rec->setColour (TextButton::buttonOnColourId, Colours::red.withAlpha (0.8f));
        c.customLayout = [=] (ControlPanel& panel)
        {
            auto r = panel.content();
            auto left = r.removeFromLeft (r.getWidth() * 45 / 100);
            mode->setBounds (left.removeFromTop (22).reduced (1, 0));
            left.removeFromTop (3);
            auto btns = left.removeFromBottom (22);
            pad->setBounds (left.reduced (1, 2));
            play->setBounds (btns.removeFromLeft (btns.getWidth() / 2).reduced (2, 0));
            rec->setBounds (btns.reduced (2, 0));
            layoutRows (r.withTrimmedLeft (4), { { dly, gain }, { lo, hi }, { spd, len, sm } }, 3);
        };
    }
    else if (which == "delay")
    {
        c.row ({ c.knob (bDlyOne, "One"), c.knob (bDlyTwo, "Two"), c.knob (bDlyFb, "FB"), c.knob (bDlyGain, "Gain") });
        c.row ({ c.knob (bDlyLfo, "LFO"), c.knob (bDlyFM, "FM"), c.knob (bDlyDM, "DM"), c.knob (bDlyWide, "Wide") });
        c.row ({ c.knob (bDlyCut, "Cut"), c.knob (bDlyRes, "Res"), c.toggle (bDlyLo, "Lo"), nullptr });
    }
    else if (which == "mix")
    {
        c.row ({ c.knob (bFxMix, "FX Mix") });
        c.row ({ c.toggle (bModInput, "Mod Input"), c.toggle (bModMix, "Mod Mix") });
    }
    else if (which == "eq")
    {
        c.row ({ c.knob (bEqHi, "Hi"), c.knob (bEqAmp, "Amp") });
        c.row ({ c.knob (bEqMid, "Mid"), c.knob (bEqSplit, ">|<") });
        c.row ({ c.knob (bEqLo, "Lo"), nullptr });
    }
    else if (which == "filter")
    {
        c.row ({ c.knob (bFltCut, "Cut"), c.knob (bFltTrk, "Trk"), c.knob (bFltSm, "Sm"), c.knob (bFltMod, "Mod") });
    }
    return cp;
}

//==============================================================================
//  LimeSeqGrid
//==============================================================================
LimeSeqGrid::LimeSeqGrid (TwinboxProcessor& p, bool b2) : proc (p), big (b2)
{
    setWantsKeyboardFocus (true);
    startTimerHz (30);
    setTooltip ("Click: on/off  |  Right-click: accent  |  Drag: paint  |  Value lanes: drag the bar height (right-click = off). "
                "Click a TIE / POD / MOD column, then use the arrows: Up/Down = value (one sample per step with VEL = wav), "
                "Left/Right = step, Delete = off, Shift = bigger steps");
}

int LimeSeqGrid::editPattern() const
{
    const bool chain = getParam (proc.apvts, bChainOn) > 0.5f, lockOn = getParam (proc.apvts, bLock) > 0.5f;
    if (chain && ! lockOn && proc.isPlaying()) return proc.lime.playingPattern();
    return (int) getParam (proc.apvts, bPattern);
}

int LimeSeqGrid::laneLen (int lane) const
{
    const char* id = lane < 4 ? bLen : (lane == 4 ? bTieLen : (lane == 5 ? bPodLen : bModLen));
    return (int) getParam (proc.apvts, id);
}

Rectangle<float> LimeSeqGrid::rowBounds (int row) const
{
    const float lw = big ? 70.0f : 50.0f;
    const float gap = big ? 12.0f : 5.0f;
    const float trigH = big ? 34.0f : ((float) getHeight() - gap) * 0.62f / 4.0f;
    if (row < 4) return { lw, (float) row * trigH, (float) getWidth() - lw, trigH };
    const float top = trigH * 4 + gap;
    const float rest = (float) getHeight() - top;
    const int nLower = big ? 3 : 1;
    const float h = (rest - (big ? 8.0f * (nLower - 1) : 0.0f)) / (float) nLower;
    const int i = row - 4;
    return { lw, top + (float) i * (h + (big ? 8.0f : 0.0f)), (float) getWidth() - lw, h };
}

int LimeSeqGrid::stepAtX (float x) const
{
    const auto r = rowBounds (0);
    const float cw = r.getWidth() / (float) visibleSteps();
    const int s = (int) ((x - r.getX()) / cw);
    return s < 0 || s >= visibleSteps() ? -1 : page * visibleSteps() + s;
}

void LimeSeqGrid::paint (Graphics& g)
{
    const int pat = editPattern();
    auto& P = proc.lime.pattern (pat);
    const bool showPlay = proc.isPlaying() && proc.lime.playingPattern() == pat;
    const int vis = visibleSteps(), first = page * vis;

    for (int row = 0; row < rowCount(); ++row)
    {
        const int lane = rowToLane (row);
        const auto rb = rowBounds (row);
        const int len = laneLen (lane);
        const int uiLane = lane < 4 ? 0 : lane - 3;
        const int ps = showPlay ? proc.lime.laneStep (uiLane) : -1;
        g.setColour (theme::text);
        g.setFont (theme::font (big ? 13.0f : 10.0f, true));
        if (big || row < 4) g.drawText (laneNames[lane], 2, (int) rb.getY(), (int) rb.getX() - 4, (int) rb.getHeight(), Justification::centredLeft);
        const float cw = rb.getWidth() / (float) vis;
        for (int i = 0; i < vis; ++i)
        {
            const int st = first + i;
            auto c = Rectangle<float> (rb.getX() + (float) i * cw, rb.getY(), cw, rb.getHeight()).reduced (big ? 1.5f : 1.0f);
            const bool beyond = st >= len;
            Colour bg = (st / 4) % 2 == 0 ? theme::stepBeat : theme::stepOff;
            g.setColour (beyond ? bg.withMultipliedAlpha (0.3f) : bg);
            g.fillRoundedRectangle (c, 2.0f);
            if (lane < 4)
            {
                const uint8_t f = P.trig[(size_t) lane][(size_t) st].load();
                if (f)
                {
                    auto sq = f >= 2 ? c.reduced (1.0f) : c.reduced (c.getWidth() * 0.2f, c.getHeight() * 0.2f);
                    g.setColour ((lane == 3 ? theme::accentG : theme::accentB).withMultipliedAlpha (beyond ? 0.3f : 1.0f));
                    g.fillRoundedRectangle (sq, 2.0f);
                }
            }
            else
            {
                const float v = P.value[(size_t) (lane - 4)][(size_t) st].load();
                if (v > 0.001f)
                {
                    const float top = c.getBottom() - c.getHeight() * v;
                    const float sv = lane < 6 ? P.vel[(size_t) (lane - 4)][(size_t) st].load() : 1.0f;
                    g.setColour ((lane == 6 ? theme::accentG : theme::accentB).withMultipliedAlpha ((beyond ? 0.3f : 0.9f) * (0.35f + 0.65f * sv)));
                    g.fillRect (Rectangle<float>::leftTopRightBottom (c.getX() + 1, top, c.getRight() - 1, c.getBottom()));
                    if (lane < 6)
                    {
                        // step velocity: white tick (Alt/Option + drag or Alt + arrows to edit)
                        const float vy = c.getBottom() - c.getHeight() * sv;
                        g.setColour (Colours::white.withAlpha (beyond ? 0.25f : 0.8f));
                        g.fillRect (Rectangle<float> (c.getX() + 1, jlimit (c.getY(), c.getBottom() - 2.0f, vy - 1.0f), c.getWidth() - 2, 2.0f));
                    }
                }
                if ((lane == 4 || lane == 5) && v > 0.001f && big && (int) getParam (proc.apvts, b (lane == 4 ? tie : pod, lv::vel)) == 1)
                {
                    // sample selected by the column height (1 = bottom of the list)
                    const auto set = proc.library.set (lane == 4 ? SampleLibrary::tie : SampleLibrary::pod);
                    const int n = jmin (128, set->size());
                    if (n > 0)
                    {
                        const int notch = jlimit (1, n, (int) std::ceil (v * (float) n - 1.0e-4f));
                        g.setColour (Colours::black.withAlpha (0.75f));
                        g.setFont (theme::font (10.0f, true));
                        g.drawText (String (notch), c.withHeight (14.0f).translated (0.0f, jmax (0.0f, c.getBottom() - c.getHeight() * v - c.getY())), Justification::centred);
                    }
                }
            }
            if (st == ps)
            {
                g.setColour (Colours::white.withAlpha (0.6f));
                g.drawRoundedRectangle (c, 2.0f, 1.4f);
            }
            if (lane == selLane && st == selStep)
            {
                g.setColour (hasKeyboardFocus (false) ? theme::accentG : theme::accentG.withAlpha (0.5f));
                g.drawRoundedRectangle (c.expanded (1.0f), 2.0f, 2.0f);
            }
        }
        // name of the sample selected by the highlighted column (VEL = wav)
        if (lane == selLane && selStep >= first && selStep < first + vis && wavMode (lane))
        {
            const float sv = P.value[(size_t) (lane - 4)][(size_t) selStep].load();
            const auto set = proc.library.set (lane == 4 ? SampleLibrary::tie : SampleLibrary::pod);
            const int n = jmin (128, set->size());
            if (n > 0 && sv > 0.0f)
            {
                const int notch = jlimit (1, n, (int) std::ceil (sv * (float) n - 1.0e-4f));
                const String label = String (notch) + ": " + set->names[n - notch];
                g.setFont (theme::font (big ? 12.0f : 10.0f, true));
                const float tw = (float) GlyphArrangement::getStringWidthInt (g.getCurrentFont(), label) + 10.0f;
                const float x = jlimit (rb.getX(), rb.getRight() - tw, rb.getX() + (float) (selStep - first) * cw);
                auto box = Rectangle<float> (x, rb.getY() + 1.0f, tw, big ? 16.0f : 13.0f);
                g.setColour (Colours::black.withAlpha (0.8f));
                g.fillRoundedRectangle (box, 2.0f);
                g.setColour (theme::accentG);
                g.drawText (label, box, Justification::centred);
            }
        }
    }
    // remix lamp
    if (proc.lime.remixActive() && proc.isPlaying())
    {
        g.setColour (theme::accentA);
        g.fillEllipse (4.0f, (float) getHeight() - 10.0f, 6.0f, 6.0f);
    }
}

bool LimeSeqGrid::wavMode (int lane) const
{
    return (lane == 4 || lane == 5) && (int) getParam (proc.apvts, b (lane == 4 ? tie : pod, lv::vel)) == 1;
}

void LimeSeqGrid::setValue (int lane, int st, float v, bool audition)
{
    proc.lime.pattern (editPattern()).value[(size_t) (lane - 4)][(size_t) st].store (jlimit (0.0f, 1.0f, v));
    if (audition && wavMode (lane) && v > 0.0f)
    {
        const int module = lane == 4 ? SampleLibrary::tie : SampleLibrary::pod;
        const int n = jmin (128, proc.library.set (module)->size());
        if (n > 0) proc.lime.auditionSample (module, n - jlimit (1, n, (int) std::ceil (v * (float) n - 1.0e-4f)));
    }
    repaint();
}

bool LimeSeqGrid::keyPressed (const KeyPress& key)
{
    if (selLane < 4 || selStep < 0) return false;
    const int len = jmax (1, laneLen (selLane));
    const float v = proc.lime.pattern (editPattern()).value[(size_t) (selLane - 4)][(size_t) selStep].load();
    const bool shift = key.getModifiers().isShiftDown();
    if (key.isKeyCode (KeyPress::upKey) || key.isKeyCode (KeyPress::downKey))
    {
        const int dir = key.isKeyCode (KeyPress::upKey) ? 1 : -1;
        if (key.getModifiers().isAltDown() && selLane < 6)
        {
            auto& cell = proc.lime.pattern (editPattern()).vel[(size_t) (selLane - 4)][(size_t) selStep];
            cell.store (jlimit (0.05f, 1.0f, cell.load() + (float) dir * (shift ? 0.25f : 0.05f)));
            repaint();
            return true;
        }
        if (wavMode (selLane))
        {
            const int n = jmin (128, proc.library.set (selLane == 4 ? SampleLibrary::tie : SampleLibrary::pod)->size());
            if (n == 0) return true;
            const int notch = v > 0.0f ? jlimit (1, n, (int) std::ceil (v * (float) n - 1.0e-4f)) : 0;
            const int next = jlimit (1, n, notch + dir * (shift ? 8 : 1));
            setValue (selLane, selStep, (float) next / (float) n, true);
        }
        else setValue (selLane, selStep, v + (float) dir * (shift ? 0.125f : 1.0f / 32.0f), false);
        return true;
    }
    if (key.isKeyCode (KeyPress::leftKey) || key.isKeyCode (KeyPress::rightKey))
    {
        selStep = (selStep + (key.isKeyCode (KeyPress::rightKey) ? 1 : -1) + len) % len;
        if (selStep / visibleSteps() != page) setPage (selStep / visibleSteps());
        repaint();
        return true;
    }
    if (key.isKeyCode (KeyPress::deleteKey) || key.isKeyCode (KeyPress::backspaceKey))
    {
        setValue (selLane, selStep, 0.0f, false);
        return true;
    }
    if (key.isKeyCode (KeyPress::escapeKey)) { selLane = -1; repaint(); return true; }
    return false;
}

void LimeSeqGrid::edit (int row, int st, const MouseEvent& e, bool first)
{
    auto& P = proc.lime.pattern (editPattern());
    const int lane = rowToLane (row);
    if (lane < 4)
    {
        auto& cell = P.trig[(size_t) lane][(size_t) st];
        if (first)
        {
            const uint8_t v = cell.load();
            if (e.mods.isRightButtonDown() || e.mods.isShiftDown()) dragVal = v >= 2 ? 1 : 2;
            else dragVal = v ? 0 : 1;
        }
        cell.store (dragVal);
    }
    else
    {
        const auto rb = rowBounds (row);
        float v = jlimit (0.0f, 1.0f, (rb.getBottom() - (float) e.y) / rb.getHeight());
        if (lane < 6 && e.mods.isAltDown())
        {
            // Alt / Option + drag: step velocity (the column height keeps its value / sample)
            P.vel[(size_t) (lane - 4)][(size_t) st].store (jlimit (0.05f, 1.0f, v));
            return;
        }
        if ((lane == 4 || lane == 5) && (int) getParam (proc.apvts, b (lane == 4 ? tie : pod, lv::vel)) == 1)
        {
            // Tie / Pod with VEL = wav: one notch per sample of the module's list (max 128); notch 1 = bottom of the list
            const int n = jmin (128, proc.library.set (lane == 4 ? SampleLibrary::tie : SampleLibrary::pod)->size());
            if (n > 0 && v > 0.0f) v = (float) jlimit (1, n, (int) std::ceil (v * (float) n)) / (float) n;
        }
        if (e.mods.isRightButtonDown()) v = 0.0f;
        P.value[(size_t) (lane - 4)][(size_t) st].store (v);
    }
}

void LimeSeqGrid::mouseDown (const MouseEvent& e)
{
    for (int row = 0; row < rowCount(); ++row)
        if (rowBounds (row).contains (e.position))
        {
            const int st = stepAtX (e.position.x);
            if (st < 0) return;
            dragRow = row; lastStep = st;
            if (rowToLane (row) >= 4) { selLane = rowToLane (row); selStep = st; grabKeyboardFocus(); }
            edit (row, st, e, true);
            repaint();
            return;
        }
}

void LimeSeqGrid::mouseDrag (const MouseEvent& e)
{
    if (dragRow < 0) return;
    const int st = stepAtX (e.position.x);
    if (st < 0 || (st == lastStep && dragRow < 4)) return;
    lastStep = st;
    if (rowToLane (dragRow) >= 4) selStep = st;
    edit (dragRow, st, e, false);
    repaint();
}

void LimeSeqGrid::timerCallback()
{
    bool changed = big;
    for (int i = 0; i < 4; ++i)
    {
        const int s = proc.isPlaying() ? proc.lime.laneStep (i) : -1;
        if (s != lastPlay[(size_t) i]) { lastPlay[(size_t) i] = s; changed = true; }
    }
    if (changed) repaint();
}

//==============================================================================
//  LimeSeqPanel
//==============================================================================
LimeSeqPanel::LimeSeqPanel (TwinboxProcessor& p, bool b2) : Panel ("Sequencer", theme::accentB), proc (p), big (b2), grid (p, b2)
{
    for (int i = 0; i < 4; ++i)
    {
        auto* btn = patBtns.add (new TextButton (String::charToString ((juce_wchar) ('A' + i))));
        btn->setColour (TextButton::buttonOnColourId, theme::accentB);
        btn->setTooltip ("Edit / play pattern " + btn->getButtonText());
        btn->onClick = [this, i] { setParam (proc.apvts, bPattern, (float) i); };
        addAndMakeVisible (btn);
    }
    for (int i = 0; i < grid.numPages(); ++i)
    {
        auto* btn = pageBtns.add (new TextButton (String (i * grid.visibleSteps() + 1) + "-" + String ((i + 1) * grid.visibleSteps())));
        btn->setColour (TextButton::buttonOnColourId, theme::header.brighter (0.3f));
        btn->onClick = [this, i] { grid.setPage (i); };
        addAndMakeVisible (btn);
    }
    if (! big)
        for (int i = 0; i < 3; ++i)
        {
            auto* btn = laneBtns.add (new TextButton (laneNames[4 + i]));
            btn->setColour (TextButton::buttonOnColourId, theme::accentB.withAlpha (0.6f));
            btn->onClick = [this, i] { grid.setLowerLane (i); };
            addAndMakeVisible (btn);
        }
    for (int i = 0; i < 8; ++i)
    {
        auto* btn = chainBtns.add (new TextButton ("A"));
        btn->setTooltip ("Pattern sequencer slot " + String (i + 1) + " (click to cycle A-D)");
        btn->onClick = [this, i]
        {
            const int v = ((int) getParam (proc.apvts, bChainSlot (i)) + 1) % 4;
            setParam (proc.apvts, bChainSlot (i), (float) v);
        };
        addAndMakeVisible (btn);
    }
    chainOn.attach (p.apvts, bChainOn);
    lock.attach (p.apvts, bLock);
    chainLen.setSliderStyle (Slider::IncDecButtons);
    chainLen.setTextBoxStyle (Slider::TextBoxLeft, false, 22, 18);
    chainLenAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, bChainLen, chainLen);
    copyBtn.onClick = [this] { clip = (int) getParam (proc.apvts, bPattern); };
    pasteBtn.onClick = [this]
    {
        if (clip >= 0) proc.lime.pattern ((int) getParam (proc.apvts, bPattern)).copyFrom (proc.lime.pattern (clip));
        grid.repaint();
    };
    for (auto* c : std::initializer_list<Component*> { &chainOn, &lock, &chainLen, &copyBtn, &pasteBtn, &grid }) addAndMakeVisible (c);
    for (auto* lb : laneBtns) lb->toFront (false);   // the lane buttons sit over the grid's label column: keep them clickable
    startTimerHz (10);
}

void LimeSeqPanel::timerCallback()
{
    const int pat = (int) getParam (proc.apvts, bPattern);
    const int playing = proc.lime.playingPattern();
    for (int i = 0; i < patBtns.size(); ++i)
    {
        patBtns[i]->setToggleState (i == pat, dontSendNotification);
        patBtns[i]->setColour (TextButton::buttonColourId, proc.isPlaying() && i == playing && i != pat ? theme::accentB.withAlpha (0.35f) : theme::panel2);
    }
    for (int i = 0; i < pageBtns.size(); ++i) pageBtns[i]->setToggleState (i == grid.getPage(), dontSendNotification);
    for (int i = 0; i < laneBtns.size(); ++i) laneBtns[i]->setToggleState (i == grid.lowerLaneShown(), dontSendNotification);
    const int clen = (int) getParam (proc.apvts, bChainLen);
    const bool on = getParam (proc.apvts, bChainOn) > 0.5f;
    for (int i = 0; i < 8; ++i)
    {
        auto* b2 = chainBtns[i];
        b2->setButtonText (String::charToString ((juce_wchar) ('A' + (int) getParam (proc.apvts, bChainSlot (i)))));
        b2->setAlpha (i < clen ? 1.0f : 0.35f);
        b2->setColour (TextButton::buttonColourId, on && proc.isPlaying() && proc.lime.chainSlot() == i ? theme::accentB.withAlpha (0.6f) : theme::panel2);
    }
}

void LimeSeqPanel::paint (Graphics& g)
{
    Panel::paint (g);
}

void LimeSeqPanel::resized()
{
    Panel::resized();
    auto r = content();
    auto top = r.removeFromTop (big ? 28 : 20);
    for (auto* b2 : patBtns) b2->setBounds (top.removeFromLeft (big ? 40 : 24).reduced (1, 0));
    top.removeFromLeft (8);
    chainOn.setBounds (top.removeFromLeft (big ? 44 : 30).reduced (1, 0));
    chainLen.setBounds (top.removeFromLeft (big ? 80 : 62).reduced (1, 0));
    for (auto* b2 : chainBtns) b2->setBounds (top.removeFromLeft (big ? 30 : 18).reduced (1, 0));
    lock.setBounds (top.removeFromLeft (big ? 54 : 38).reduced (1, 0));
    pasteBtn.setBounds (top.removeFromRight (big ? 60 : 42).reduced (1, 0));
    copyBtn.setBounds (top.removeFromRight (big ? 60 : 40).reduced (1, 0));
    top.removeFromRight (6);
    for (int i = pageBtns.size(); --i >= 0;) pageBtns[i]->setBounds (top.removeFromRight (big ? 70 : 38).reduced (1, 0));
    r.removeFromTop (4);
    grid.setBounds (r);
    if (! big)
    {
        auto lower = grid.rowBounds (4).toNearestInt().translated (r.getX(), r.getY());
        auto col = Rectangle<int> (r.getX(), lower.getY(), 48, lower.getHeight());
        const int h = col.getHeight() / 3;
        for (auto* b2 : laneBtns) b2->setBounds (col.removeFromTop (h).reduced (1, 1));
    }
}

//==============================================================================
//  SampleSelector / WaveView
//==============================================================================
SampleSelector::SampleSelector (TwinboxProcessor& p, int m, bool b2) : proc (p), module (m), big (b2)
{
    twb.setTooltip ("Factory folder: Dualbox " + SampleLibrary::moduleName (m));
    leg.setTooltip ("Legacy folder: original Limelite samples (use IMPORT LEGACY first)");
    usr.setTooltip ("Choose your own folder for this module");
    play.setTooltip ("Audition");
    const bool isClick = m == SampleLibrary::click;                 // Section A kick CLICK layer: factory or user folder only
    for (auto* b3 : { &twb, &leg, &usr }) b3->setColour (TextButton::buttonOnColourId, isClick ? theme::accentA : theme::accentB);
    twb.onClick = [this] { proc.library.setSource (module, SampleLibrary::factory); };
    leg.onClick = [this] { proc.library.setSource (module, SampleLibrary::legacy); };
    usr.onClick = [this] { chooseFolder(); };
    play.onClick = [this]
    {
        if (module == SampleLibrary::click) proc.micro.auditionChannel (0, 1.0f);      // kick + click
        else proc.lime.audition (targetOfModule (module), 1.0f);
    };
    box.onChange = [this]
    {
        const int i = box.getSelectedItemIndex();
        if (i >= 0 && i != shownIndex) { shownIndex = i; setParam (proc.apvts, wavParamId(), (float) i); }
    };
    for (auto* c : std::initializer_list<Component*> { &twb, &leg, &usr, &box, &play }) addAndMakeVisible (c);
    leg.setVisible (! isClick);                                      // no Legacy folder for the CLICK layer
    proc.library.addChangeListener (this);
    refresh();
    startTimerHz (8);
}

SampleSelector::~SampleSelector() { proc.library.removeChangeListener (this); stopTimer(); }

void SampleSelector::chooseFolder()
{
    chooser = std::make_unique<FileChooser> ("Choose a folder with " + SampleLibrary::moduleName (module) + " samples",
                                             proc.library.userFolder (module).isDirectory() ? proc.library.userFolder (module) : SampleLibrary::rootFolder());
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories, [this] (const FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f.isDirectory()) proc.library.setUserFolder (module, f);
    });
}

void SampleSelector::refresh()
{
    auto set = proc.library.set (module);
    box.clear (dontSendNotification);
    for (int i = 0; i < set->names.size(); ++i) box.addItem (set->names[i], i + 1);
    if (set->size() == 0) box.setText (proc.library.isLoading() ? "loading..." : "(empty folder)", dontSendNotification);
    shownIndex = -1;
    timerCallback();
    const auto src = proc.library.source (module);
    twb.setToggleState (src == SampleLibrary::factory, dontSendNotification);
    leg.setToggleState (src == SampleLibrary::legacy, dontSendNotification);
    usr.setToggleState (src == SampleLibrary::user, dontSendNotification);
    usr.setTooltip (src == SampleLibrary::user ? "User folder: " + proc.library.userFolder (module).getFullPathName() : "Choose your own folder for this module");
}

void SampleSelector::timerCallback()
{
    const int n = box.getNumItems();
    if (n == 0) return;
    const int idx = ((int) getParam (proc.apvts, wavParamId())) % n;
    if (idx != shownIndex) { shownIndex = idx; box.setSelectedItemIndex (idx, dontSendNotification); }
}

void SampleSelector::paint (Graphics&) {}

void SampleSelector::resized()
{
    auto r = getLocalBounds();
    const int bw = big ? 46 : 30;
    twb.setBounds (r.removeFromLeft (bw).reduced (1, 0));
    if (leg.isVisible()) leg.setBounds (r.removeFromLeft (bw).reduced (1, 0));
    usr.setBounds (r.removeFromLeft (big ? 34 : 24).reduced (1, 0));
    play.setBounds (r.removeFromRight (big ? 34 : 22).reduced (1, 0));
    box.setBounds (r.reduced (1, 0));
}

WavKnob::WavKnob (TwinboxProcessor& p, int m, Colour accent) : proc (p), module (m)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setColour (Slider::rotarySliderFillColourId, accent);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
    slider.setMouseDragSensitivity (300);
    slider.dragPixels = 300;                                  // sample select keeps a coarse drag (one sample per few pixels)
    slider.setRange (0, 1, 1);
    slider.onValueChange = [this]
    {
        if (size > 0) setParam (proc.apvts, b (module, lv::wav), (float) jlimit (0, size - 1, (int) slider.getValue()));
    };
    slider.onDragStart = [this] { proc.apvts.getParameter (b (module, lv::wav))->beginChangeGesture(); };
    slider.onDragEnd   = [this] { proc.apvts.getParameter (b (module, lv::wav))->endChangeGesture(); };
    addAndMakeVisible (slider);
    setTooltip ("WAV: sample select (steps through the samples of the current folder). Click it, then use the arrow keys");
    startTimerHz (15);
    timerCallback();
}

void WavKnob::timerCallback()
{
    auto set = proc.library.set (module);
    const int n = set->size();
    if (n != size)
    {
        size = n;
        slider.setRange (0, jmax (1, n - 1), 1);
    }
    if (n == 0) { name = "-"; repaint(); return; }
    const int idx = ((int) getParam (proc.apvts, b (module, lv::wav))) % n;
    if (! slider.isMouseButtonDown() && (int) slider.getValue() != idx) slider.setValue (idx, dontSendNotification);
    const auto nm = set->names[idx];
    if (nm != name) { name = nm; repaint(); }
}

void WavKnob::resized()
{
    auto r = getLocalBounds();
    const int capH = jlimit (10, 16, getHeight() / 6);
    r.removeFromTop (capH); r.removeFromBottom (capH);
    const int d = jmin (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (d, d));
}

void WavKnob::paint (Graphics& g)
{
    const int capH = jlimit (10, 16, getHeight() / 6);
    const float fh = jlimit (9.5f, 14.0f, (float) capH * 0.85f);
    g.setFont (theme::font (fh));
    g.setColour (theme::textDim);
    g.drawFittedText ("Wav", 0, 0, getWidth(), capH, Justification::centred, 1, 0.7f);
    g.setColour (slider.isMouseOverOrDragging() || slider.hasKeyboardFocus (false) ? slider.findColour (Slider::rotarySliderFillColourId) : theme::text);
    if (slider.hasKeyboardFocus (false))                      // keyboard focus: the arrows step through the samples
    {
        g.drawRoundedRectangle (slider.getBounds().toFloat().expanded (2.0f), 4.0f, 1.2f);
    }
    // show the sample name without the numeric prefix ("K03 Warehouse 9" -> "Warehouse 9")
    String shown = name;
    if (shown.length() > 4 && CharacterFunctions::isDigit (shown[1]) && shown[3] == ' ') shown = shown.substring (4);
    g.drawFittedText (shown, 0, getHeight() - capH, getWidth(), capH, Justification::centred, 1, 0.6f);
}

void WaveView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (theme::outline);
    g.fillRoundedRectangle (r, 3.0f);
    auto set = proc.library.set (module);
    if (set->size() == 0) return;
    const int idx = ((int) getParam (proc.apvts, b (module, lv::wav))) % set->size();
    auto s = set->samples[(size_t) idx];
    const auto& d = s->L;
    const int w = getWidth();
    const float mid = r.getCentreY(), hh = r.getHeight() * 0.45f;
    g.setColour (theme::accentB.withAlpha (0.75f));
    for (int x = 0; x < w; ++x)
    {
        const size_t a0 = (size_t) ((double) x / w * d.size()), a1 = jmin (d.size(), (size_t) ((double) (x + 1) / w * d.size()) + 1);
        float mn = 0, mx = 0;
        for (size_t i = a0; i < a1; ++i) { mn = jmin (mn, d[i]); mx = jmax (mx, d[i]); }
        g.drawVerticalLine (x, mid - mx * hh, mid - mn * hh + 1.0f);
    }
    g.setColour (theme::text);
    g.setFont (theme::font (12.0f));
    g.drawText (s->name + "   " + String ((double) d.size() / s->sourceRate, 2) + " s" + (s->stereo() ? "  stereo" : ""), r.reduced (6, 4).toNearestInt(), Justification::topLeft);
    g.setColour (theme::textDim);
    g.drawText (set->folder.getFullPathName(), r.reduced (6, 4).toNearestInt(), Justification::bottomLeft);
}

//==============================================================================
//  LimeModulePanel
//==============================================================================
LimeModulePanel::LimeModulePanel (TwinboxProcessor& p, int v, bool b2) : ControlPanel (p, limeVoiceName (v)), voice (v), big (b2)
{
    selector = std::make_unique<SampleSelector> (p, v, b2);
    addAndMakeVisible (*selector);
    power = std::make_unique<Toggle> ("", theme::accentB);
    power->attach (p.apvts, b (v, lv::on));
    power->setTooltip (String (limeVoiceName (v)) + " on / mute");
    addAndMakeVisible (*power);
    if (big) { wave = std::make_unique<WaveView> (p, v); addAndMakeVisible (*wave); }
    nudge = std::make_unique<NudgeBox> (theme::accentB);              // per-module timing offset
    nudge->attach (p.apvts, b (v, lv::nudge));
    addAndMakeVisible (*nudge);
    if (v == hat)
    {
        // switching the Hat filter type from the UI moves an unusable cutoff to a sensible one (LP at 20 Hz would be silent)
        if (auto* ft = p.apvts.getParameter (b (v, lv::ft)))
        {
            ftWatch = std::make_unique<ParameterAttachment> (*ft, [this, &p] (float m)
            {
                const int mode = roundToInt (m);
                if (lastFt >= 0 && mode != lastFt)
                    if (auto* c = p.apvts.getParameter (b (hat, lv::cut)))
                    {
                        const float hz = c->convertFrom0to1 (c->getValue());
                        float target = -1.0f;
                        if (mode == 0 && hz < 200.0f) target = 20000.0f;
                        else if (mode == 2 && hz > 15000.0f) target = 20.0f;
                        else if (mode == 1 && (hz < 200.0f || hz > 15000.0f)) target = 6000.0f;
                        if (target > 0.0f) c->setValueNotifyingHost (c->convertTo0to1 (target));
                    }
                lastFt = mode;
            });
            ftWatch->sendInitialUpdate();
        }
    }
    if (v == tie || v == pod)
    {
        scaleBtn = std::make_unique<Toggle> ("SCALE", theme::accentB);
        scaleBtn->attach (p.apvts, b (v, lv::scale));
        scaleBtn->setTooltip (v == tie ? "SCALE on: every sample is transposed into the shared Root / Scale from its detected key (high-quality pitch shift, same length). Off = MANUAL per-sample pitch"
                                       : "SCALE on: every sample is re-pitched to the shared Root / Scale (from its detected pitch). Off = MANUAL per-sample pitch");
        addAndMakeVisible (*scaleBtn);
        nameBtn = std::make_unique<Toggle> ("NAME", theme::accentB);
        nameBtn->attach (p.apvts, b (v, lv::nameKey));
        nameBtn->setTooltip ("NAME on: the sample key is read from the file name (e.g. 'pad chord Db', 'Rhodes Cm9', 'Bass F#2'); "
                             "if the name has no key the audio analysis is used. Off: audio analysis only");
        addAndMakeVisible (*nameBtn);
        noteBtn.setTooltip ("Per-sample pitch window: detected notes, play and -32..+32 st knobs (MANUAL mode)");
        noteBtn.onClick = [this] { if (onOpenPitch) onOpenPitch(); };
        addAndMakeVisible (noteBtn);
    }

    auto K = [this] (const char* key, const char* cap) -> Component* { return knob (b (voice, key), cap); };
    auto* wavKnob = new WavKnob (p, v, accent);
    owned.add (wavKnob); addAndMakeVisible (wavKnob);
    Component* W = wavKnob;
    auto T = [this] (const char* key, const char* cap) -> Component* { return toggle (b (voice, key), cap); };
    switch (v)
    {
        case kick:
            row ({ K (lv::gain, "Gain"), K (lv::acc, "Acc"), K (lv::pan, "Pan"), W, K (lv::tune, "Tune"), K (lv::st, "St") });
            row ({ K (lv::att, "A"), K (lv::hold, "H"), K (lv::dcy, "D"), K (lv::sat, "Sat"), K (lv::shp, "Shp"), K (lv::sm, "Sm") });
            row ({ combo (b (v, lv::mod)), T (lv::lo, "Lo"), K (lv::cut, "Cut"), K (lv::res, "Res"), nullptr });
            break;
        case snare:
            row ({ K (lv::gain, "Gain"), K (lv::acc, "Acc"), K (lv::pan, "Pan"), W, K (lv::tune, "Tune"), K (lv::st, "St") });
            row ({ K (lv::att, "A"), K (lv::hold, "H"), K (lv::dcy, "D"), K (lv::sat, "Sat"), K (lv::shp, "Shp"), K (lv::sm, "Sm") });
            row ({ combo (b (v, lv::mod)), seg (b (v, lv::ft)), K (lv::cut, "Cut"), K (lv::res, "Res"), K (lv::flam, "Flam") });
            break;
        case hat:
            row ({ K (lv::gain, "Gain"), K (lv::acc, "Acc"), K (lv::pan, "Pan"), W, K (lv::tune, "Tune") });
            row ({ K (lv::att, "A"), K (lv::cd, "CD"), K (lv::od, "OD"), T (lv::link, "Link"), K (lv::sat, "Sat") });
            row ({ combo (b (v, lv::mod)), seg (b (v, lv::ft)), K (lv::cut, "Cut"), K (lv::res, "Res"), nullptr });
            break;
        case tie:
            row ({ K (lv::gain, "Gain"), K (lv::vamt, "V>A"), K (lv::pan, "Pan"), W, K (lv::tune, "Tune"), K (lv::port, "Port") });
            row ({ K (lv::st, "St"), K (lv::att, "A"), K (lv::dcy, "D"), K (lv::fm, "FM"), K (lv::mp, "MP") });
            row ({ combo (b (v, lv::vel)), K (lv::noise, "Noise"), K (lv::sat, "Sat"), seg (b (v, lv::ft)), K (lv::cut, "Cut"), K (lv::res, "Res") });
            break;
        default: // pod
            row ({ K (lv::gain, "Gain"), K (lv::vamt, "V>A"), K (lv::pan, "Pan"), W, K (lv::tune, "Tune"), K (lv::st, "St") });
            row ({ K (lv::att, "A"), K (lv::dcy, "D"), K (lv::zap, "Zap"), K (lv::sat, "Sat"), nullptr, nullptr });
            row ({ combo (b (v, lv::vel)), seg (b (v, lv::ft)), K (lv::cut, "Cut"), K (lv::res, "Res"), nullptr, nullptr });
            break;
    }
}

void LimeModulePanel::resized()
{
    Panel::resized();
    power->setBounds (getWidth() - 46, 3, 18, headerH - 6);
    if (scaleBtn)
    {
        noteBtn.setBounds (getWidth() - 70, 2, 22, headerH - 4);
        scaleBtn->setBounds (getWidth() - (big ? 128 : 118), 3, big ? 54 : 46, headerH - 6);
        nameBtn->setBounds (scaleBtn->getX() - (big ? 50 : 42), 3, big ? 46 : 40, headerH - 6);
    }
    auto r = content();
    auto selRow = r.removeFromTop (big ? 28 : 20);
    nudge->setBounds (selRow.removeFromRight (big ? 78 : 60));
    selRow.removeFromRight (3);
    selector->setBounds (selRow);
    r.removeFromTop (3);
    if (wave) { wave->setBounds (r.removeFromTop (jmax (100, r.getHeight() / 3))); r.removeFromTop (6); }
    layoutRows (r, rows);
    for (auto& rw : rows)
        for (auto* c : rw)
            if (auto* cb = dynamic_cast<ComboBox*> (c)) cb->setBounds (cb->getBounds().withSizeKeepingCentre (cb->getWidth(), jmin (cb->getHeight(), big ? 26 : 20)));
            else if (auto* sg = dynamic_cast<Segmented*> (c)) sg->setBounds (sg->getBounds().withSizeKeepingCentre (sg->getWidth(), jmin (sg->getHeight(), big ? 26 : 18)));
}

//==============================================================================
//  PitchWindowContent
//==============================================================================
PitchWindowContent::Row::Row (PitchWindowContent& o, int idx) : owner (o), index (idx)
{
    play.onClick = [this] { owner.proc.lime.auditionSample (owner.module, index); };
    play.setTooltip ("Play this sample");
    knob.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle (Slider::TextBoxRight, false, 56, 20);
    knob.setRange (-32.0, 32.0, 1.0);
    knob.setDoubleClickReturnValue (true, 0.0);
    knob.setColour (Slider::rotarySliderFillColourId, theme::accentB);
    knob.setRotaryParameters (MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
    knob.setTextValueSuffix (" st");
    knob.setValue (owner.proc.library.sampleOffset (owner.module, index), dontSendNotification);
    knob.onValueChange = [this] { owner.proc.library.setSampleOffset (owner.module, index, (float) knob.getValue()); repaint(); };
    knob.setTooltip ("Pitch of this sample in semitones (-32..+32), on top of the module Tune. Double-click = 0");
    addAndMakeVisible (play); addAndMakeVisible (knob);
}

void PitchWindowContent::Row::paint (Graphics& g)
{
    auto set = owner.proc.library.set (owner.module);
    if (index >= set->size()) return;
    auto smp = set->samples[(size_t) index];
    g.setColour (index % 2 ? theme::panel : theme::panel2);
    g.fillRect (getLocalBounds());
    g.setColour (theme::textDim);
    g.setFont (theme::font (12.0f));
    g.drawText (String (index + 1).paddedLeft ('0', 3), 40, 0, 34, getHeight(), Justification::centredLeft);
    g.setColour (theme::text);
    g.setFont (theme::font (13.0f));
    g.drawText (smp->name, 78, 0, getWidth() - 78 - 370, getHeight(), Justification::centredLeft);
    // detected pitch: audio analysis and file-name key; the one in use is highlighted
    const bool nameOn = owner.proc.apvts.getRawParameterValue (b (owner.module, lv::nameKey))->load() > 0.5f;
    if (smp->hasKey)
    {
        // Belt: key analysis (key + mode, tuning in cents) and key from the file name
        const auto& K = smp->key;
        const bool usingName = nameOn && K.nameRoot >= 0;
        const int cents = roundToInt (K.tuneCents);
        g.setFont (theme::font (12.0f, ! usingName));
        g.setColour (K.keyRoot >= 0 ? (usingName ? theme::textDim : theme::accentB) : theme::textDim);
        g.drawText ("audio " + (K.keyRoot >= 0 ? keyanalysis::keyName (K.keyRoot, K.minor) + (cents != 0 ? (cents > 0 ? " +" : " ") + String (cents) + "c" : String())
                                              : String ("no key")), getWidth() - 372, 0, 122, getHeight(), Justification::centredLeft);
        g.setFont (theme::font (12.0f, usingName));
        g.setColour (usingName ? theme::accentB : theme::textDim);
        g.drawText ("name " + keyanalysis::keyName (K.nameRoot, K.nameMinor), getWidth() - 248, 0, 84, getHeight(), Justification::centredLeft);
        return;
    }
    const bool usingName = nameOn && smp->namePitch >= 0;
    g.setFont (theme::font (12.0f, ! usingName));
    g.setColour (smp->pitch >= 0 ? (usingName ? theme::textDim : theme::accentB) : theme::textDim);
    g.drawText ("audio " + (smp->pitch >= 0 ? SampleLibrary::noteName (smp->pitch) : String ("no pitch")), getWidth() - 360, 0, 110, getHeight(), Justification::centredLeft);
    g.setFont (theme::font (12.0f, usingName));
    g.setColour (usingName ? theme::accentB : theme::textDim);
    g.drawText ("name " + (smp->namePitch >= 0 ? SampleLibrary::noteName (smp->namePitch) : String ("-")), getWidth() - 248, 0, 80, getHeight(), Justification::centredLeft);
}

void PitchWindowContent::Row::resized()
{
    play.setBounds (6, 4, 28, getHeight() - 8);
    knob.setBounds (getWidth() - 162, 1, 156, getHeight() - 2);
}

PitchWindowContent::PitchWindowContent (TwinboxProcessor& p, int m) : proc (p), module (m)
{
    viewport.setViewedComponent (&list, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
    resetAll.onClick = [this]
    {
        for (auto* r : rows) r->knob.setValue (0.0, sendNotificationSync);
    };
    addAndMakeVisible (resetAll);
    nameKey.attach (proc.apvts, b (m, lv::nameKey));
    nameKey.setTooltip ("Read the key of each sample from its file name (fallback: audio analysis)");
    nameKey.onClick = [this] { for (auto* r : rows) r->repaint(); };
    addAndMakeVisible (nameKey);
    proc.library.addChangeListener (this);
    rebuild();
    startTimerHz (8);
}

PitchWindowContent::~PitchWindowContent() { proc.library.removeChangeListener (this); stopTimer(); }

void PitchWindowContent::rebuild()
{
    rows.clear();
    auto set = proc.library.set (module);
    for (int i = 0; i < set->size(); ++i) list.addAndMakeVisible (rows.add (new Row (*this, i)));
    resized();
    repaint();
}

void PitchWindowContent::timerCallback()
{
    const bool scaleOn = proc.apvts.getRawParameterValue (b (module, lv::scale))->load() > 0.5f;
    if (scaleOn != scaleWasOn)
    {
        scaleWasOn = scaleOn;
        for (auto* r : rows) r->knob.setEnabled (! scaleOn);
        resetAll.setEnabled (! scaleOn);
        repaint();
    }
}

void PitchWindowContent::paint (Graphics& g)
{
    g.fillAll (theme::bg);
    auto set = proc.library.set (module);
    g.setColour (theme::text);
    g.setFont (theme::font (14.0f, true));
    g.drawText (String (limeVoiceName (module)) + "  -  " + String (set->size()) + " samples", 12, 8, getWidth() - 330, 20, Justification::centredLeft);
    g.setColour (theme::textDim);
    g.setFont (theme::font (11.5f));
    g.drawText (set->folder.getFullPathName(), 12, 28, getWidth() - 24, 16, Justification::centredLeft);
    const bool scaleOn = proc.apvts.getRawParameterValue (b (module, lv::scale))->load() > 0.5f;
    const int root = (int) proc.apvts.getRawParameterValue (bScaleRoot)->load(), type = (int) proc.apvts.getRawParameterValue (bScaleType)->load();
    static const char* rootN[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    g.setColour (scaleOn ? theme::accentB : theme::accentA);
    g.drawText (scaleOn ? String ("SCALE ON (") + rootN[root] + " " + scales()[(size_t) type].name + "): samples are re-pitched automatically, the knobs are inactive"
                        : String ("MANUAL: turn the knobs to re-pitch each sample (-32..+32 st). Changes are heard live, also while playing."),
                12, 46, getWidth() - 24, 16, Justification::centredLeft);
}

void PitchWindowContent::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (68);
    resetAll.setBounds (getWidth() - 150, 8, 138, 24);
    nameKey.setBounds (getWidth() - 310, 8, 152, 24);
    viewport.setBounds (r.reduced (8, 4));
    const int w = viewport.getWidth() - viewport.getScrollBarThickness();
    list.setSize (w, rows.size() * 34);
    for (int i = 0; i < rows.size(); ++i) rows[i]->setBounds (0, i * 34, w, 34);
}

//==============================================================================
//  XYPad
//==============================================================================
XYPad::XYPad (TwinboxProcessor& p, const String& xId, const String& yId)
    : proc (p), px (p.apvts.getParameter (xId)), py (p.apvts.getParameter (yId))
{
    ax = std::make_unique<ParameterAttachment> (*px, [this] (float v) { if (! proc.lime.xyPlayingBack()) { vx = px->convertTo0to1 (v); repaint(); } });
    ay = std::make_unique<ParameterAttachment> (*py, [this] (float v) { if (! proc.lime.xyPlayingBack()) { vy = py->convertTo0to1 (v); repaint(); } });
    ax->sendInitialUpdate(); ay->sendInitialUpdate();
    setTooltip ("Drag to move X / Y.  REC on + drag = record the movement (loop length = LEN).  PLAY = loop the recorded movement.");
    startTimerHz (30);
}

void XYPad::timerCallback()
{
    if (proc.lime.xyPlayingBack() || proc.lime.xyRecording())
    {
        vx = proc.lime.xyCurrentX(); vy = proc.lime.xyCurrentY();
        trail.push_back ({ vx, vy });
        if (trail.size() > 40) trail.erase (trail.begin());
        repaint();
    }
    else if (! trail.empty()) { trail.clear(); repaint(); }
    else repaint (getWidth() - 60, 0, 60, 18);
}

void XYPad::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (theme::outline);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (theme::header);
    for (int i = 1; i < 4; ++i)
    {
        g.drawVerticalLine ((int) (r.getX() + r.getWidth() * i / 4), r.getY(), r.getBottom());
        g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * i / 4), r.getX(), r.getRight());
    }
    // trail of the recorded / played motion
    for (size_t i = 0; i < trail.size(); ++i)
    {
        const float a = (float) (i + 1) / (float) trail.size();
        g.setColour (theme::accentB.withAlpha (0.35f * a));
        g.fillEllipse (r.getX() + trail[i].x * r.getWidth() - 2.0f, r.getBottom() - trail[i].y * r.getHeight() - 2.0f, 4.0f, 4.0f);
    }
    const float x = r.getX() + vx * r.getWidth(), y = r.getBottom() - vy * r.getHeight();
    g.setColour (theme::accentB);
    g.drawRect (Rectangle<float> (x - 6, y - 6, 12, 12), 2.0f);
    // loop position bar
    if (proc.lime.xyHasMotion())
    {
        g.setColour (theme::accentB.withAlpha (0.5f));
        g.fillRect (r.getX(), r.getBottom() - 2.0f, r.getWidth() * proc.lime.xyLoopPos(), 2.0f);
    }
    g.setFont (theme::font (10.0f, true));
    const bool recArmed = proc.apvts.getRawParameterValue (bXRec)->load() > 0.5f;
    if (proc.lime.xyRecording())          { g.setColour (Colours::red);   g.drawText ("REC", r.reduced (4, 2).toNearestInt(), Justification::topRight); }
    else if (proc.lime.xyPlayingBack())   { g.setColour (theme::accentB); g.drawText ("PLAY", r.reduced (4, 2).toNearestInt(), Justification::topRight); }
    else if (recArmed)                    { g.setColour (Colours::red.withAlpha (0.6f)); g.drawText ("REC ARMED", r.reduced (4, 2).toNearestInt(), Justification::topRight); }
    g.setColour (theme::textDim);
    g.setFont (theme::font (10.0f));
    g.drawText ("X " + String (roundToInt (vx * 100)) + "  Y " + String (roundToInt (vy * 100)), r.reduced (4, 4).toNearestInt(), Justification::bottomRight);
}

void XYPad::set (const MouseEvent& e)
{
    if (! dragging) { ax->beginGesture(); ay->beginGesture(); dragging = true; trail.clear(); }
    vx = jlimit (0.0f, 1.0f, (float) e.x / (float) getWidth());
    vy = jlimit (0.0f, 1.0f, 1.0f - (float) e.y / (float) getHeight());
    ax->setValueAsPartOfGesture (px->convertFrom0to1 (vx));
    ay->setValueAsPartOfGesture (py->convertFrom0to1 (vy));
    proc.lime.xyTouch (vx, vy, true);
    repaint();
}

void XYPad::mouseUp (const MouseEvent&)
{
    if (dragging) { ax->endGesture(); ay->endGesture(); dragging = false; }
    proc.lime.xyTouch (vx, vy, false);
}

//==============================================================================
//  FX window
//==============================================================================
LimeFxWindowContent::LimeFxWindowContent (TwinboxProcessor& p)
{
    for (auto* w : { "remix", "out", "xfx", "delay", "mix", "eq", "filter" })
        addAndMakeVisible (panels.add (makeLimePanel (p, w, true).release()));
}

void LimeFxWindowContent::resized()
{
    auto r = getLocalBounds().reduced (8);
    const int h = r.getHeight() / 3;
    for (int row = 0; row < 3; ++row)
    {
        auto rr = r.removeFromTop (h);
        const int w = rr.getWidth() / 3;
        for (int col = 0; col < 3; ++col)
        {
            const int i = row * 3 + col;
            auto cell = col == 2 ? rr : rr.removeFromLeft (w);
            if (i < panels.size()) panels[i]->setBounds (cell.reduced (4));
        }
    }
}

//==============================================================================
//  LimeSection
//==============================================================================
LimeSection::LimeSection (TwinboxProcessor& p) : proc (p)
{
    browserBtn.onClick = [this] { if (onOpenBrowser) onOpenBrowser(); };
    browserBtn.setTooltip ("Genre preset browser (128 kits + patterns)");
    legacyBtn.onClick = [this] { importLegacy(); };
    legacyBtn.setTooltip ("Import the original Limelite samples from your own 'Limelite 1.3.ens' into the Legacy folders");
    levelB.attach (p.apvts, bGain);
    scaleRoot.addItemList (StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    for (auto& sc : scales()) scaleType.addItem (sc.name, scaleType.getNumItems() + 1);
    scaleRootAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, bScaleRoot, scaleRoot);
    scaleTypeAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, bScaleType, scaleType);
    scaleRoot.setTooltip ("SCALE root (shared by Belt and Capsule)");
    scaleType.setTooltip ("SCALE type (shared by Belt and Capsule) - same scales as Ableton Live");
    addAndMakeVisible (scaleRoot); addAndMakeVisible (scaleType);
    for (auto* c : std::initializer_list<Component*> { &browserBtn, &legacyBtn, &levelB }) addAndMakeVisible (c);
    normBtn = std::make_unique<NormButton> (p, 1, theme::accentB);
    midiBtn = std::make_unique<MidiTargetButton> (p, 1, theme::accentB);
    addAndMakeVisible (*normBtn); addAndMakeVisible (*midiBtn);
    seqBtn.attach (p.apvts, bSeq);
    seqBtn.setTooltip ("SEQ: sequencer of Section B on / off. Off: the patterns keep running silently, the MIDI keyboard still plays.");
    addAndMakeVisible (seqBtn);

    seq = std::make_unique<LimeSeqPanel> (p, false);
    lfo = std::make_unique<LfoPanel> (p, 1);
    addAndMakeVisible (*lfo);
    seq->setExpandable ([this] { openWindow ("seq", "Section B - Sequencer (64 steps)", [this] { return new LimeSeqPanel (proc, true); }, 1400, 640); });
    addAndMakeVisible (*seq);

    auto fxWin = [this] { openWindow ("fx", "Section B - Remix, Output & FX", [this] { return new LimeFxWindowContent (proc); }, 1300, 860); };
    const std::pair<std::unique_ptr<ControlPanel>*, const char*> fxPanels[] {
        { &remix, "remix" }, { &out, "out" }, { &global, "global" }, { &xfx, "xfx" }, { &delay, "delay" },
        { &mix, "mix" }, { &eq, "eq" }, { &filter, "filter" } };
    for (auto& fp : fxPanels)
    {
        *fp.first = makeLimePanel (p, fp.second, false);
        if (String (fp.second) != "global") (*fp.first)->setExpandable (fxWin);
        addAndMakeVisible (**fp.first);
    }
    for (int v = 0; v < 5; ++v)
    {
        auto* m = modules.add (new LimeModulePanel (p, v, false));
        m->setExpandable ([this, v] { openWindow ("mod" + String (v), String ("Section B - ") + limeVoiceName (v),
                                                  [this, v] { return new LimeModulePanel (proc, v, true); }, 900, 640); });
        if (v == tie || v == pod)
            m->onOpenPitch = [this, v] { openWindow ("pitch" + String (v), String ("Section B - ") + limeVoiceName (v) + " - sample pitch",
                                                     [this, v] { return new PitchWindowContent (proc, v); }, 760, 640); };
        addAndMakeVisible (m);
    }
    startTimerHz (10);
}

LimeSection::~LimeSection() { stopTimer(); }

void LimeSection::openWindow (const String& key, const String& title, std::function<Component*()> make, int w, int h)
{
    auto& win = windows[key];
    if (win != nullptr) { win->toFront (true); return; }
    win = std::make_unique<ExpandWindow> (title, make(), w, h, [this, key] { windows[key].reset(); });
}

void LimeSection::importLegacy()
{
    chooser = std::make_unique<FileChooser> ("Select your 'Limelite 1.3.ens'", File::getSpecialLocation (File::userHomeDirectory), "*.ens");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (! f.existsAsFile()) return;
        status = "Importing Legacy samples...";
        repaint();
        SafePointer<LimeSection> safe (this);
        proc.importLegacy (f, [safe] (LegacyImporter::Result r)
        {
            if (safe == nullptr) return;
            safe->status = r.message;
            safe->repaint();
            AlertWindow::showMessageBoxAsync (r.ok ? MessageBoxIconType::InfoIcon : MessageBoxIconType::WarningIcon, "Import Legacy",
                                              r.message + (r.ok ? "\n\nSelect LEG in a sampler to use them.\nFolder: " + SampleLibrary::rootFolder().getFullPathName() : String()));
        },
        [safe] (float prog)
        {
            MessageManager::callAsync ([safe, prog] { if (safe != nullptr) { safe->status = "Importing Legacy... " + String (roundToInt (prog * 100)) + " %"; safe->repaint(); } });
        });
    });
}

void LimeSection::timerCallback() { repaint (getWidth() - 24, 0, 24, 46); }

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
    g.drawText ("5 voices / sequencer / remix / FX / LFO", 40, 26, 220, 14, Justification::centredLeft);
    g.setColour (theme::textDim);
    g.setFont (theme::font (11.0f, true));
    g.drawText ("SCALE", scaleRoot.getX() - 46, scaleRoot.getY(), 42, scaleRoot.getHeight(), Justification::centredRight);
    if (status.isNotEmpty()) { g.setFont (theme::font (11.0f)); g.drawText (status, scaleType.getRight() + 12, 12, 380, 20, Justification::centredLeft); }
    drawMeter (g, Rectangle<float> ((float) getWidth() - 16.0f, 10.0f, 6.0f, 30.0f), proc.meter (1), theme::accentB);
}

void LimeSection::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto header = r.removeFromTop (40);
    header.removeFromLeft (260);
    browserBtn.setBounds (header.removeFromLeft (90).withSizeKeepingCentre (90, 24));
    header.removeFromLeft (6);
    legacyBtn.setBounds (header.removeFromLeft (120).withSizeKeepingCentre (120, 24));
    header.removeFromLeft (24);
    scaleRoot.setBounds (header.removeFromLeft (56).withSizeKeepingCentre (56, 24).translated (44, 0));
    scaleType.setBounds (header.removeFromLeft (150).withSizeKeepingCentre (150, 24).translated (48, 0));
    header.removeFromRight (16);
    levelB.setBounds (header.removeFromRight (64).expanded (0, 4));
    header.removeFromRight (10);
    normBtn->setBounds (header.removeFromRight (58).withSizeKeepingCentre (54, 24));
    midiBtn->setBounds (header.removeFromRight (58).withSizeKeepingCentre (54, 24));
    seqBtn.setBounds (header.removeFromRight (58).withSizeKeepingCentre (54, 24));
    r.removeFromTop (4);
    const int W = r.getWidth();

    auto row1 = r.removeFromTop (r.getHeight() * 38 / 100);
    seq->setBounds (row1.removeFromLeft (W * 47 / 100).reduced (2, 0));
    remix->setBounds (row1.removeFromLeft (W * 26 / 100).reduced (2, 0));
    auto outCol = row1.reduced (2, 0);
    out->setBounds (outCol.removeFromTop (outCol.getHeight() * 64 / 100));
    outCol.removeFromTop (4);
    global->setBounds (outCol);

    r.removeFromTop (6);
    auto row2 = r.removeFromTop (r.getHeight() * 54 / 100);
    const int mw = row2.getWidth() / 5;
    for (int i = 0; i < modules.size(); ++i)
        modules[i]->setBounds ((i == modules.size() - 1 ? row2 : row2.removeFromLeft (mw)).reduced (2, 0));

    r.removeFromTop (6);
    auto row3 = r;
    xfx->setBounds (row3.removeFromLeft (W * 22 / 100).reduced (2, 0));
    delay->setBounds (row3.removeFromLeft (W * 19 / 100).reduced (2, 0));
    lfo->setBounds (row3.removeFromLeft (W * 21 / 100).reduced (2, 0));
    mix->setBounds (row3.removeFromLeft (W * 9 / 100).reduced (2, 0));
    eq->setBounds (row3.removeFromLeft (W * 11 / 100).reduced (2, 0));
    filter->setBounds (row3.reduced (2, 0));
}
