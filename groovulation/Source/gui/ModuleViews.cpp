#include "ModuleViews.h"

using namespace juce;

namespace
{
    void setParamValue (GroovulationProcessor& proc, const String& id, float v)
    {
        if (auto* p = proc.apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (v));
            p->endChangeGesture();
        }
    }
    String loopInfo (const LoopData& l)
    {
        String s = String (l.bpm, 1) + " BPM  " + String (l.beats / 4.0, l.beats % 4 == 0 ? 0 : 2) + (l.beats == 4 ? " bar" : " bars");
        if (lb::isTonal (l.type)) s << "  " << analysis::keyName (l.info.keyRoot, l.info.minor);
        return s;
    }
}

//==============================================================================
WaveDisplay::WaveDisplay (GroovulationProcessor& p, int m, bool inter) : proc (p), module (m), interactive (inter)
{
    startTimerHz (30);
    if (interactive) setTooltip ("SLICE: drag the S / E markers.  VOL.A: click to add a point, drag to move, double-click to delete.");
}

float WaveDisplay::param (const char* name) const { return proc.apvts.getRawParameterValue (lb::pid (module, name))->load(); }
void WaveDisplay::setParam (const char* name, float v) { setParamValue (proc, lb::pid (module, name), v); }

double WaveDisplay::regionBeats() const
{
    auto l = proc.currentLoop (module);
    const int steps = (int) param ("steps");
    if (steps > 0) return steps * lb::gridBeats ((int) param ("grid"));
    return l != nullptr ? (double) jmax (1, l->beats) : 4.0;
}

Rectangle<float> WaveDisplay::plot() const { return getLocalBounds().toFloat().reduced (5.0f, 5.0f); }
float WaveDisplay::xToPos (float x) const { auto r = plot(); return jlimit (0.0f, 1.0f, (x - r.getX()) / r.getWidth()); }
float WaveDisplay::posToX (float p) const { auto r = plot(); return r.getX() + p * r.getWidth(); }
float WaveDisplay::yToVol (float y) const { auto r = plot(); return jlimit (0.0f, 1.0f, 1.0f - (y - r.getY()) / r.getHeight()); }
float WaveDisplay::volToY (float v) const { auto r = plot(); return r.getY() + (1.0f - v) * r.getHeight(); }

void WaveDisplay::timerCallback()
{
    const float pos = proc.modulePlayPos (module);
    if (pos != lastPos || isMouseButtonDown()) { lastPos = pos; repaint(); }
}

void WaveDisplay::paint (Graphics& g)
{
    drawLcd (g, getLocalBounds().toFloat());
    const auto r = plot();
    auto l = proc.currentLoop (module);
    g.setFont (theme::mono (interactive ? 13.0f : 10.0f, true));
    if (l == nullptr)
    {
        g.setColour (theme::lcdDim);
        g.drawText (proc.library->isScanning() ? "SCANNING..." : "NO LOOP", r, Justification::centred);
        return;
    }
    const double R = regionBeats(), loopBeats = jmax (1, l->beats);
    const double gb = lb::gridBeats ((int) param ("grid"));
    const bool sliceOn = param ("slice") > 0.5f, volOn = param ("vola") > 0.5f;
    const float s0 = param ("sstart"), s1 = param ("send");

    // grid (transparent), bars stronger
    if (R / gb < r.getWidth() / 2.0)
        for (double b = 0; b < R - 1e-9; b += gb)
        {
            const float x = posToX ((float) (b / R));
            const bool bar = std::abs (std::fmod (b, 4.0)) < 1e-6, beat = std::abs (std::fmod (b, 1.0)) < 1e-6;
            g.setColour (theme::lcdInk.withAlpha (bar ? 0.4f : (beat ? 0.22f : 0.1f)));
            g.drawVerticalLine (roundToInt (x), r.getY(), r.getBottom());
        }
    // where the loop repeats inside the region
    for (double b = loopBeats; b < R - 1e-9; b += loopBeats)
    {
        const float x = posToX ((float) (b / R));
        const float dash[] { 3.0f, 3.0f };
        g.setColour (theme::lcdInk.withAlpha (0.7f));
        g.drawDashedLine (Line<float> (x, r.getY(), x, r.getBottom()), dash, 2, 1.0f);
    }
    // waveform
    const auto& ov = l->overview;
    const int bins = (int) ov.size();
    const float cy = r.getCentreY(), hh = r.getHeight() * 0.46f;
    for (int px = 0; px < (int) r.getWidth(); ++px)
    {
        const double b0 = R * px / r.getWidth(), b1 = R * (px + 1) / r.getWidth();
        float v = 0.0f;
        for (double b = b0; b < b1 + 1e-9; b += jmax (1e-6, (b1 - b0) / 4.0))
        {
            const int idx = jlimit (0, bins - 1, (int) (std::fmod (b, loopBeats) / loopBeats * bins));
            v = jmax (v, ov[(size_t) idx]);
        }
        const float pos = (float) (b0 / R);
        const bool inside = ! sliceOn || (pos >= s0 && pos < s1);
        g.setColour (theme::lcdInk.withAlpha (inside ? 0.9f : 0.22f));
        const float x = r.getX() + (float) px;
        g.drawLine (x, cy - jmax (0.5f, v * hh), x, cy + jmax (0.5f, v * hh), 1.0f);
    }
    // slice
    if (sliceOn)
    {
        g.setColour (theme::lcdInk.withAlpha (0.12f));
        g.fillRect (Rectangle<float> (r.getX(), r.getY(), posToX (s0) - r.getX(), r.getHeight()));
        g.fillRect (Rectangle<float> (posToX (s1), r.getY(), r.getRight() - posToX (s1), r.getHeight()));
        for (int k = 0; k < 2; ++k)
        {
            const float x = posToX (k == 0 ? s0 : s1);
            g.setColour (theme::red.darker (0.3f));
            g.drawLine (x, r.getY(), x, r.getBottom(), 2.0f);
            if (interactive)
            {
                Path flag;
                const float d = k == 0 ? 1.0f : -1.0f;
                flag.addTriangle (x, r.getY(), x + d * 12.0f, r.getY(), x, r.getY() + 12.0f);
                g.fillPath (flag);
                g.setColour (theme::white);
                g.setFont (theme::font (9.0f, true));
                g.drawText (k == 0 ? "S" : "E", Rectangle<float> (k == 0 ? x + 1 : x - 9, r.getY(), 8, 8), Justification::centred);
            }
        }
    }
    // clip-gain automation
    if (volOn)
    {
        auto e = drag == volPoint ? env : proc.volEnvelope (module);
        Path p;
        for (size_t i = 0; i < e.size(); ++i)
        {
            const Point<float> pt (posToX (e[i].x), volToY (e[i].y));
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        g.setColour (theme::red.darker (0.2f));
        g.strokePath (p, PathStrokeType (1.6f));
        if (interactive)
            for (auto& v : e)
            {
                g.setColour (theme::lcdBg);
                g.fillEllipse (posToX (v.x) - 4, volToY (v.y) - 4, 8, 8);
                g.setColour (theme::red.darker (0.2f));
                g.drawEllipse (posToX (v.x) - 4, volToY (v.y) - 4, 8, 8, 1.5f);
            }
        g.setColour (theme::lcdInk.withAlpha (0.25f));
        const float dash[] { 2.0f, 4.0f };
        g.drawDashedLine (Line<float> (r.getX(), volToY (volUnityY), r.getRight(), volToY (volUnityY)), dash, 2, 1.0f);
    }
    // playhead
    const float pp = proc.modulePlayPos (module);
    if (pp >= 0.0f)
    {
        g.setColour (theme::lcdInk);
        g.drawLine (posToX (pp), r.getY(), posToX (pp), r.getBottom(), 1.5f);
    }
    if (interactive)
    {
        const int steps = (int) param ("steps");
        g.setColour (theme::lcdInk);
        g.setFont (theme::mono (12.0f, true));
        const String len = steps == 0 ? "LENGTH ORIG = " + String (loopBeats / 4.0, 2) + " bars"
                                      : "LENGTH " + String (steps) + " x " + lb::gridNames()[(int) param ("grid")] + " = " + String (R, 3) + " beats";
        g.drawText (len + (R != loopBeats ? "   (loop " + String (loopBeats / 4.0, 2) + " bars)" : String()), r.reduced (4, 2), Justification::topLeft);
        g.drawText ("GRID " + lb::gridNames()[(int) param ("grid")], r.reduced (4, 2), Justification::topRight);
    }
}

int WaveDisplay::hitVolPoint (Point<float> p) const
{
    auto e = proc.volEnvelope (module);
    for (int i = 0; i < (int) e.size(); ++i)
        if (p.getDistanceFrom ({ posToX (e[(size_t) i].x), volToY (e[(size_t) i].y) }) < 7.0f) return i;
    return -1;
}

void WaveDisplay::mouseMove (const MouseEvent& e)
{
    if (! interactive) return;
    const bool sliceOn = param ("slice") > 0.5f;
    const bool nearMarker = sliceOn && (std::abs (e.position.x - posToX (param ("sstart"))) < 7 || std::abs (e.position.x - posToX (param ("send"))) < 7);
    setMouseCursor (nearMarker ? MouseCursor::LeftRightResizeCursor
                               : (param ("vola") > 0.5f ? (hitVolPoint (e.position) >= 0 ? MouseCursor::DraggingHandCursor : MouseCursor::CrosshairCursor)
                                                        : MouseCursor::NormalCursor));
}

void WaveDisplay::mouseDown (const MouseEvent& e)
{
    if (! interactive) { if (onClick) onClick(); return; }
    drag = none;
    const bool sliceOn = param ("slice") > 0.5f, volOn = param ("vola") > 0.5f;
    if (sliceOn)
    {
        const float xs = posToX (param ("sstart")), xe = posToX (param ("send"));
        if (std::abs (e.position.x - xs) < 7 || (e.position.y < plot().getY() + 14 && std::abs (e.position.x - xs) < 14)) drag = sliceStart;
        else if (std::abs (e.position.x - xe) < 7 || (e.position.y < plot().getY() + 14 && std::abs (e.position.x - xe) < 14)) drag = sliceEnd;
    }
    if (drag == none && volOn)
    {
        env = proc.volEnvelope (module);
        dragIndex = hitVolPoint (e.position);
        if (dragIndex < 0)
        {
            const VolPoint np { xToPos (e.position.x), yToVol (e.position.y) };
            auto it = std::upper_bound (env.begin(), env.end(), np.x, [] (float x, const VolPoint& p) { return x < p.x; });
            dragIndex = (int) (it - env.begin());
            env.insert (it, np);
            proc.setVolEnvelope (module, env);
        }
        drag = volPoint;
    }
    if (drag == none && sliceOn)
    {
        // click elsewhere: move the nearest marker there
        const float pos = xToPos (e.position.x);
        drag = std::abs (pos - param ("sstart")) < std::abs (pos - param ("send")) ? sliceStart : sliceEnd;
        mouseDrag (e);
    }
}

void WaveDisplay::mouseDrag (const MouseEvent& e)
{
    if (! interactive) return;
    const double R = regionBeats();
    auto l = proc.currentLoop (module);
    float pos = xToPos (e.position.x);
    if (! e.mods.isShiftDown() && R > 0)   // snap to the grid (shift = free)
    {
        const double gb = lb::gridBeats ((int) param ("grid"));
        pos = (float) (std::round (pos * R / gb) * gb / R);
    }
    if (drag == sliceStart) setParam ("sstart", jmin (pos, param ("send") - 0.002f));
    else if (drag == sliceEnd) setParam ("send", jmax (pos, param ("sstart") + 0.002f));
    else if (drag == volPoint && dragIndex >= 0 && dragIndex < (int) env.size())
    {
        auto& p = env[(size_t) dragIndex];
        if (dragIndex == 0) p.x = 0.0f;
        else if (dragIndex == (int) env.size() - 1) p.x = 1.0f;
        else p.x = jlimit (env[(size_t) dragIndex - 1].x, env[(size_t) dragIndex + 1].x, xToPos (e.position.x));
        p.y = yToVol (e.position.y);
        proc.setVolEnvelope (module, env);
    }
    ignoreUnused (l);
    repaint();
}

void WaveDisplay::mouseUp (const MouseEvent&) { drag = none; repaint(); }

void WaveDisplay::mouseDoubleClick (const MouseEvent& e)
{
    if (! interactive || param ("vola") < 0.5f) return;
    env = proc.volEnvelope (module);
    const int i = hitVolPoint (e.position);
    if (i > 0 && i < (int) env.size() - 1)
    {
        env.erase (env.begin() + i);
        proc.setVolEnvelope (module, env);
        repaint();
    }
}

//==============================================================================
ModuleStrip::ModuleStrip (GroovulationProcessor& p, int m, std::function<void (int)> onSelect)
    : proc (p), module (m), select (std::move (onSelect)), display (p, m, false)
{
    const auto c = theme::sectionColour (m);
    display.onClick = [this] { select (module); };
    addAndMakeVisible (display);
    for (auto* b : { &prev, &next, &rnd, &sim }) addAndMakeVisible (b);
    prev.onClick = [this] { proc.stepLoop (module, -1); };
    next.onClick = [this] { proc.stepLoop (module, 1); };
    rnd.onClick  = [this] { proc.randomLoop (module); };
    sim.onClick  = [this] { proc.similarLoop (module); };
    prev.setTooltip ("LOOP SELECT: previous loop of this type (all other settings are kept)");
    next.setTooltip ("LOOP SELECT: next loop of this type (all other settings are kept)");
    rnd.setTooltip ("Random loop of this type");
    sim.setTooltip ("SIMILAR SELECT: next most similar loop (same type, similar sound and character)");
    gain.setAccent (c); pitch.setAccent (c);
    gain.attach (proc.apvts, lb::pid (m, "gain"));
    pitch.attach (proc.apvts, lb::pid (m, "pitch"));
    on.setColour (TextButton::buttonOnColourId, c);
    on.attach (proc.apvts, lb::pid (m, "on"));
    solo.setColour (TextButton::buttonOnColourId, theme::amber);
    solo.attach (proc.apvts, lb::pid (m, "solo"));
    for (auto* k : std::initializer_list<Component*> { &gain, &pitch, &on, &solo }) addAndMakeVisible (k);
    startTimerHz (20);
}

void ModuleStrip::timerCallback()
{
    auto l = proc.currentLoop (module);
    const float lv = proc.moduleLevel (module);
    if (l != shownLoop || std::abs (lv - level) > 0.003f) { shownLoop = l; level = lv; repaint(); }
}

void ModuleStrip::mouseDown (const MouseEvent&) { select (module); }

void ModuleStrip::paint (Graphics& g)
{
    const auto c = theme::sectionColour (module);
    auto r = getLocalBounds().toFloat();
    g.setColour (theme::panel);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (selected ? c : theme::outline.withAlpha (0.5f));
    g.drawRoundedRectangle (r.reduced (selected ? 1.0f : 0.5f), 4.0f, selected ? 2.5f : 1.0f);
    auto h = r.removeFromTop (20.0f).reduced (selected ? 2.0f : 0.0f, selected ? 2.0f : 0.0f);
    g.setColour (theme::header);
    g.fillRoundedRectangle (h, 3.0f);
    g.setColour (c);
    g.fillRoundedRectangle (h.removeFromLeft (22.0f).reduced (3.0f, 3.0f), 2.0f);
    g.setColour (theme::white);
    g.setFont (theme::font (11.0f, true));
    g.drawText (String (module + 1), h.translated (-22.0f, 0).withWidth (22.0f), Justification::centred);
    g.setColour (theme::headerText);
    g.setFont (theme::font (12.5f, true));
    g.drawText (lb::moduleName (module).toUpperCase(), h.reduced (4.0f, 0), Justification::centredLeft);
    // loop name + info under the LCD
    const auto infoArea = Rectangle<int> (6, display.getBottom() + 2, getWidth() - 12, 30);
    g.setColour (theme::text);
    g.setFont (theme::font (11.5f, true));
    g.drawFittedText (shownLoop ? shownLoop->name : String ("-"), infoArea.withHeight (15), Justification::centredLeft, 1, 0.75f);
    g.setColour (theme::textDim);
    g.setFont (theme::font (10.5f));
    String info = shownLoop ? loopInfo (*shownLoop) : String();
    const float st = proc.appliedSemitones (module);
    if (shownLoop && std::abs (st) > 0.004f) info << "  " << (st > 0 ? "+" : "") << String (st, 2) << "st";
    g.drawFittedText (info, infoArea.withTrimmedTop (15), Justification::centredLeft, 1, 0.7f);
    // meter
    drawMeter (g, Rectangle<float> ((float) getWidth() - 10.0f, (float) gain.getY() + 6.0f, 4.0f, (float) gain.getHeight() - 12.0f), level, c);
}

void ModuleStrip::resized()
{
    auto r = getLocalBounds().reduced (5, 0);
    r.removeFromTop (24);
    display.setBounds (r.removeFromTop (juce::jmax (40, getHeight() / 4)));
    r.removeFromTop (34);
    auto b = r.removeFromTop (22);
    const int bw = b.getWidth();
    prev.setBounds (b.removeFromLeft (bw * 15 / 100).reduced (1, 0));
    next.setBounds (b.removeFromLeft (bw * 15 / 100).reduced (1, 0));
    rnd.setBounds (b.removeFromLeft (bw * 26 / 100).reduced (1, 0));
    sim.setBounds (b.reduced (1, 0));
    r.removeFromTop (3);
    auto bottom = r.removeFromBottom (22);
    on.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2).reduced (1, 1));
    solo.setBounds (bottom.reduced (1, 1));
    r.removeFromBottom (2);
    auto k = r.withTrimmedRight (8);
    gain.setBounds (k.removeFromLeft (k.getWidth() / 2));
    pitch.setBounds (k);
}

//==============================================================================
ModuleEditor::ModuleEditor (GroovulationProcessor& p) : proc (p), display (p, 0, true)
{
    addAndMakeVisible (display);
    for (auto* c : std::initializer_list<Component*> { &grid, &keySrc, &ftype, &lenMinus, &lenPlus, &lenOrig, &volReset, &slice, &fade, &volA, &dpp,
                                                       &prev, &next, &rnd, &sim, &browser,
                                                       &pAmp, &pAdsr, &pPitch, &pFilter, &pDirt, &pDelay, &pReverb, &pLfo1, &pLfo2 })
        addAndMakeVisible (c);
    for (auto* k : { &gain, &pan, &shaper, &atk, &dec, &sus, &rel, &pitch, &tempo, &cut, &res, &dirt, &dtime, &dfb, &dmix,
                     &rsize, &rdecay, &rdamp, &rpre, &rmix, &l1rate, &l1depth, &l2rate, &l2depth })
        addAndMakeVisible (k);
    for (auto* c : { &l1shape, &l1target, &l2shape, &l2target }) addAndMakeVisible (c);
    l1shape.addItemList (lb::lfoShapes(), 1); l2shape.addItemList (lb::lfoShapes(), 1);
    l1target.addItemList (lb::lfoTargets(), 1); l2target.addItemList (lb::lfoTargets(), 1);
    l1target.setTooltip ("LFO 1 destination"); l2target.setTooltip ("LFO 2 destination");
    keySrc.setCaption ("TYPE (key from)");
    grid.setCaption ("GRID");
    ftype.setCaption ("");

    prev.onClick = [this] { proc.stepLoop (module, -1); };
    next.onClick = [this] { proc.stepLoop (module, 1); };
    rnd.onClick  = [this] { proc.randomLoop (module); };
    sim.onClick  = [this] { proc.similarLoop (module); };
    sim.setTooltip ("SIMILAR SELECT: loads the next most similar loop (same type, similar sound and character)");
    browser.setTooltip ("LOOP SELECT: all loops of this type that fit (1 .. 32 bars at the project tempo)");
    browser.onChange = [this]
    {
        const int i = browser.getSelectedItemIndex();
        if (i >= 0 && i < (int) browserIndices.size()) proc.selectLoop (module, browserIndices[(size_t) i]);
    };
    auto stepLen = [this] (int d)
    {
        auto* prm = proc.apvts.getParameter (lb::pid (module, "steps"));
        int steps = (int) proc.apvts.getRawParameterValue (lb::pid (module, "steps"))->load();
        if (steps == 0)
        {
            auto l = proc.currentLoop (module);
            const double gb = lb::gridBeats ((int) proc.apvts.getRawParameterValue (lb::pid (module, "grid"))->load());
            steps = jmax (1, (int) std::lround ((l ? l->beats : 4) / gb));
        }
        steps = jlimit (1, lb::maxSteps, steps + d);
        prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 ((float) steps)); prm->endChangeGesture();
    };
    lenMinus.onClick = [stepLen] { stepLen (-1); };
    lenPlus.onClick = [stepLen] { stepLen (1); };
    lenOrig.onClick = [this] { setParamValue (proc, lb::pid (module, "steps"), 0.0f); };
    lenMinus.setTooltip ("Loop length: one grid step shorter (odd lengths against 4/4 = polyrhythms)");
    lenPlus.setTooltip ("Loop length: one grid step longer");
    lenOrig.setTooltip ("Loop length: original length of the loop");
    volReset.onClick = [this] { proc.setVolEnvelope (module, { { 0.0f, volUnityY }, { 1.0f, volUnityY } }); display.repaint(); };
    fade.setTooltip ("0.5 ms fade in / out at the slice edges (no clicks)");
    volA.setTooltip ("Clip-gain automation drawn on the display");
    setModule (0);
    startTimerHz (10);
}

void ModuleEditor::attachAll()
{
    const int m = module;
    auto& s = proc.apvts;
    gain.attach (s, lb::pid (m, "gain")); pan.attach (s, lb::pid (m, "pan")); shaper.attach (s, lb::pid (m, "shaper"));
    atk.attach (s, lb::pid (m, "atk")); dec.attach (s, lb::pid (m, "dec")); sus.attach (s, lb::pid (m, "sus")); rel.attach (s, lb::pid (m, "rel"));
    pitch.attach (s, lb::pid (m, "pitch")); tempo.attach (s, lb::pid (m, "tempo"));
    cut.attach (s, lb::pid (m, "cut")); res.attach (s, lb::pid (m, "res")); dirt.attach (s, lb::pid (m, "dirt"));
    dtime.attach (s, lb::pid (m, "dtime")); dfb.attach (s, lb::pid (m, "dfb")); dmix.attach (s, lb::pid (m, "dmix"));
    rsize.attach (s, lb::pid (m, "rsize")); rdecay.attach (s, lb::pid (m, "rdecay")); rdamp.attach (s, lb::pid (m, "rdamp"));
    rpre.attach (s, lb::pid (m, "rpre")); rmix.attach (s, lb::pid (m, "rmix"));
    l1rate.attach (s, lb::pid (m, "lfo1rate")); l1depth.attach (s, lb::pid (m, "lfo1depth"));
    l2rate.attach (s, lb::pid (m, "lfo2rate")); l2depth.attach (s, lb::pid (m, "lfo2depth"));
    grid.attach (s, lb::pid (m, "grid"));
    keySrc.attach (s, lb::pid (m, "keysrc"), { "AUDIO", "FILE NAME" });
    ftype.attach (s, lb::pid (m, "ftype"));
    slice.attach (s, lb::pid (m, "slice")); fade.attach (s, lb::pid (m, "fade")); volA.attach (s, lb::pid (m, "vola")); dpp.attach (s, lb::pid (m, "dpp"));
    comboAtt.clear();
    comboAtt.push_back (std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (s, lb::pid (m, "lfo1shape"), l1shape));
    comboAtt.push_back (std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (s, lb::pid (m, "lfo1target"), l1target));
    comboAtt.push_back (std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (s, lb::pid (m, "lfo2shape"), l2shape));
    comboAtt.push_back (std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (s, lb::pid (m, "lfo2target"), l2target));
}

void ModuleEditor::setModule (int m)
{
    module = jlimit (0, lb::numModules - 1, m);
    display.setModule (module);
    const auto c = theme::sectionColour (module);
    for (auto* k : { &gain, &pan, &shaper, &atk, &dec, &sus, &rel, &pitch, &tempo, &cut, &res, &dirt, &dtime, &dfb, &dmix,
                     &rsize, &rdecay, &rdamp, &rpre, &rmix, &l1rate, &l1depth, &l2rate, &l2depth })
        k->setAccent (c);
    for (auto* p : { &pAmp, &pAdsr, &pPitch, &pFilter, &pDirt, &pDelay, &pReverb, &pLfo1, &pLfo2 }) p->setAccent (c);
    for (auto* sg : { &grid, &keySrc, &ftype }) sg->setAccent (c);
    for (auto* t : { &slice, &fade, &volA, &dpp }) t->setColour (TextButton::buttonOnColourId, c);
    attachAll();
    shownLoop = nullptr;
    refreshBrowser();
    repaint();
}

void ModuleEditor::refreshBrowser()
{
    browser.clear (dontSendNotification);
    browserIndices = proc.candidatesFor (module);
    auto d = proc.library->db();
    const int cur = d->indexOf (proc.loopPath (module));
    int family = -1;
    for (int i = 0; i < (int) browserIndices.size(); ++i)
    {
        const auto& e = d->entries[(size_t) browserIndices[(size_t) i]];
        browser.addItem (e.name + "   [fam " + String (e.family) + "]", i + 1);
        if (browserIndices[(size_t) i] == cur) { browser.setSelectedItemIndex (i, dontSendNotification); family = e.family; }
    }
    ignoreUnused (family);
    browser.setTextWhenNothingSelected (browserIndices.empty() ? "no loops of this type" : "choose a loop...");
}

void ModuleEditor::timerCallback()
{
    auto l = proc.currentLoop (module);
    const int dbGen = (int) proc.uiState.getProperty ("dbChanged", 0);
    if (l != shownLoop || dbGen != lastDbGen)
    {
        shownLoop = l; lastDbGen = dbGen;
        refreshBrowser();
    }
    String s;
    if (l != nullptr)
    {
        auto d = proc.library->db();
        const int idx = d->indexOf (l->path);
        const auto& in = l->info;
        s << lb::moduleName (in.type) << (in.typeFromName ? " (name)" : " (audio)") << "   " << String (l->bpm, 1) << " BPM" << (in.bpmFromName ? " (name)" : "")
          << "   " << String (l->beats / 4.0, 2) << " bars";
        if (lb::isTonal (module))
        {
            s << "   KEY " << analysis::keyName (in.keyRoot, in.minor) << " (audio";
            if (in.nameRoot >= 0) s << ", name: " << analysis::keyName (in.nameRoot, in.nameMinor);
            s << ")   f0 " << analysis::noteName (in.f0) << "   tune " << String (roundToInt (in.tuneCents)) << "c";
        }
        if (idx >= 0) s << "   family " << d->entries[(size_t) idx].family;
        const float st = proc.appliedSemitones (module);
        s << "   PLAY " << (st >= 0 ? "+" : "") << String (st, 2) << " st";
    }
    if (s != infoLine) { infoLine = s; repaint (0, 0, getWidth(), 44); }
}

void ModuleEditor::paint (Graphics& g)
{
    const auto c = theme::sectionColour (module);
    auto r = getLocalBounds().toFloat();
    g.setColour (theme::panel);
    g.fillRoundedRectangle (r, 5.0f);
    auto h = r.removeFromTop (24.0f);
    g.setColour (theme::header);
    g.fillRoundedRectangle (h, 5.0f);
    g.fillRect (h.withTrimmedTop (12.0f));
    g.setColour (c);
    g.fillRoundedRectangle (h.removeFromLeft (30.0f).reduced (5.0f, 4.0f), 2.0f);
    g.setColour (theme::white);
    g.setFont (theme::font (12.0f, true));
    g.drawText (String (module + 1), h.translated (-30.0f, 0).withWidth (30.0f), Justification::centred);
    g.setColour (theme::headerText);
    g.setFont (theme::font (14.0f, true));
    const String name = lb::moduleName (module).toUpperCase() + "  /  " + lb::moduleSubtitle (module);
    g.drawText (name, h.withTrimmedLeft (4.0f), Justification::centredLeft);
    g.setColour (theme::textDim);
    g.setFont (theme::font (12.0f));
    g.drawText (infoLine, Rectangle<float> (10.0f, 26.0f, r.getWidth() - 20.0f, 16.0f), Justification::centredLeft);
    // length readout between - and +
    g.setColour (theme::text);
    g.setFont (theme::font (11.0f, true));
    g.drawText ("LENGTH", lenMinus.getBounds().getUnion (lenOrig.getBounds()).translated (0, -14).withHeight (14), Justification::centred);
    g.drawText ("LOOP SELECT", prev.getBounds().getUnion (sim.getBounds()).translated (0, -14).withHeight (14), Justification::centredLeft);
    g.setColour (theme::headerText);
    g.drawText ("LOOP", browser.getBounds().translated (-44, 0).withWidth (40), Justification::centredRight);
}

void ModuleEditor::resized()
{
    auto r = getLocalBounds().reduced (8, 0);
    r.removeFromTop (44);
    // toolbar (two lines)
    auto tb = r.removeFromTop (40);
    grid.setBounds (tb.removeFromLeft (300).withTrimmedBottom (2));
    tb.removeFromLeft (10);
    auto lenA = tb.removeFromLeft (130).withTrimmedTop (14).withTrimmedBottom (2);
    lenMinus.setBounds (lenA.removeFromLeft (32).reduced (1, 0));
    lenPlus.setBounds (lenA.removeFromLeft (32).reduced (1, 0));
    lenOrig.setBounds (lenA.reduced (1, 0));
    tb.removeFromLeft (10);
    auto tg = tb.removeFromLeft (330).withTrimmedTop (14).withTrimmedBottom (2);
    slice.setBounds (tg.removeFromLeft (70).reduced (1, 0));
    fade.setBounds (tg.removeFromLeft (90).reduced (1, 0));
    volA.setBounds (tg.removeFromLeft (70).reduced (1, 0));
    volReset.setBounds (tg.removeFromLeft (90).reduced (1, 0));
    tb.removeFromLeft (10);
    keySrc.setBounds (tb.removeFromLeft (190).withTrimmedBottom (2));
    tb.removeFromLeft (10);
    auto ls = tb.withTrimmedTop (14).withTrimmedBottom (2);
    prev.setBounds (ls.removeFromLeft (28).reduced (1, 0));
    next.setBounds (ls.removeFromLeft (28).reduced (1, 0));
    rnd.setBounds (ls.removeFromLeft (48).reduced (1, 0));
    sim.setBounds (ls.removeFromLeft (80).reduced (1, 0));
    browser.setBounds (getWidth() - 430, 3, 420, 18);
    r.removeFromTop (6);
    display.setBounds (r.removeFromTop (juce::jmax (120, r.getHeight() * 40 / 100)));
    r.removeFromTop (6);
    r.removeFromBottom (6);

    // panels: two rows
    auto row1 = r.removeFromTop (r.getHeight() / 2).withTrimmedBottom (3);
    auto row2 = r.withTrimmedTop (3);
    auto place = [] (Rectangle<int> area, std::vector<std::pair<Panel*, int>> panels)
    {
        int units = 0; for (auto& p : panels) units += p.second;
        const float uw = (float) area.getWidth() / (float) units;
        float x = (float) area.getX();
        for (auto& p : panels)
        {
            const int w = roundToInt (uw * p.second);
            p.first->setBounds (roundToInt (x), area.getY(), w - 6, area.getHeight());
            x += (float) w;
        }
    };
    place (row1, { { &pAmp, 3 }, { &pAdsr, 4 }, { &pPitch, 2 }, { &pFilter, 3 }, { &pDirt, 1 } });
    place (row2, { { &pDelay, 4 }, { &pReverb, 5 }, { &pLfo1, 3 }, { &pLfo2, 3 } });
    auto inP = [] (Panel& p) { return p.getBounds().withTrimmedTop (Panel::headerH + 2).reduced (4, 2); };
    layoutRow (inP (pAmp), { &gain, &pan, &shaper });
    layoutRow (inP (pAdsr), { &atk, &dec, &sus, &rel });
    layoutRow (inP (pPitch), { &pitch, &tempo });
    {
        auto f = inP (pFilter);
        ftype.setBounds (f.removeFromTop (22).reduced (6, 0));
        layoutRow (f, { &cut, &res });
    }
    layoutRow (inP (pDirt), { &dirt });
    {
        auto d = inP (pDelay);
        dpp.setBounds (d.removeFromBottom (22).withSizeKeepingCentre (100, 22));
        layoutRow (d, { &dtime, &dfb, &dmix });
    }
    layoutRow (inP (pReverb), { &rsize, &rdecay, &rdamp, &rpre, &rmix });
    for (int k = 0; k < 2; ++k)
    {
        auto a = inP (k == 0 ? pLfo1 : pLfo2);
        auto combos = a.removeFromRight (a.getWidth() * 45 / 100);
        auto& sh = k == 0 ? l1shape : l2shape;
        auto& tg2 = k == 0 ? l1target : l2target;
        sh.setBounds (combos.removeFromTop (combos.getHeight() / 2).withSizeKeepingCentre (combos.getWidth(), 24));
        tg2.setBounds (combos.withSizeKeepingCentre (combos.getWidth(), 24));
        layoutRow (a, { k == 0 ? &l1rate : &l2rate, k == 0 ? &l1depth : &l2depth });
    }
}
