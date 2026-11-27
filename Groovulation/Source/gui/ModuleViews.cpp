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
    if (interactive)
    {
        setTooltip ("SLICE: drag the S / E markers (SNAP ms: 1 ms steps, shift = 0.1 ms; arrow keys nudge the last marker: 1 ms, shift 0.1 ms, alt/cmd 10 ms).  "
                    "LOOP MARKERS: drag LS / LE (flags at the bottom).  "
                    "VOL.A: click to add a point, drag to move, double-click to delete.");
        setWantsKeyboardFocus (true);
        for (auto* b : { &zIn, &zOut, &zFit }) { addAndMakeVisible (b); b->setWantsKeyboardFocus (false); }
        zIn.onClick = [this] { zoomAround (0.5, plot().getCentreX()); };
        zOut.onClick = [this] { zoomAround (2.0, plot().getCentreX()); };
        zFit.onClick = [this] { setView (0.0, 1.0); };
        zIn.setTooltip ("Zoom in (or mouse wheel / pinch)");
        zOut.setTooltip ("Zoom out");
        zFit.setTooltip ("Show the whole loop");
    }
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

double WaveDisplay::regionMs() const
{
    auto l = proc.currentLoop (module);
    const double tempo = 1.0;
    const bool raw = proc.apvts.getRawParameterValue ("raw")->load() > 0.5f;
    const double bpm = raw && l != nullptr ? l->bpm : jmax (1.0, proc.currentBpm());
    return regionBeats() * 60000.0 / bpm / tempo;
}

void WaveDisplay::setMarkerMs (int k, double ms)
{
    const double R = jmax (1.0e-6, regionMs());
    const float p = (float) jlimit (0.0, 1.0, ms / R);
    if ((k & 1) == 0) setParam (markerId (k), jmin (p, param (markerId (k + 1)) - (float) (0.1 / R)));
    else setParam (markerId (k), jmax (p, param (markerId (k - 1)) + (float) (0.1 / R)));
}

void WaveDisplay::setMarkerPos (int k, float pos)
{
    if ((k & 1) == 0) setParam (markerId (k), jmin (pos, param (markerId (k + 1)) - 0.002f));
    else setParam (markerId (k), jmax (pos, param (markerId (k - 1)) + 0.002f));
}

// slice markers: whole height (upper half when the loop markers are on too), flags at the top;
// loop markers: whole height (lower half when SLICE is on too), flags at the bottom
int WaveDisplay::markerAt (Point<float> p) const
{
    const bool sliceOn = param ("slice") > 0.5f, loopOn = param ("lmOn") > 0.5f;
    const auto r = plot();
    const bool upper = p.y < r.getCentreY();
    for (int k = 0; k < 4; ++k)
    {
        const bool isSlice = k < 2;
        if (isSlice ? ! sliceOn : ! loopOn) continue;
        const float dx = std::abs (p.x - posToX (param (markerId (k))));
        const bool zoneOk = ! (sliceOn && loopOn) || (isSlice == upper);
        const bool onFlag = isSlice ? p.y < r.getY() + 14 : p.y > r.getBottom() - 14;
        if ((dx < 7 && zoneOk) || (onFlag && dx < 14)) return k;
    }
    return -1;
}

bool WaveDisplay::keyPressed (const KeyPress& k)
{
    if (! interactive) return false;
    if (lastMarker < 2 ? param ("slice") < 0.5f : param ("lmOn") < 0.5f) return false;
    const int code = k.getKeyCode();
    if (code != KeyPress::leftKey && code != KeyPress::rightKey) return false;
    const auto mods = k.getModifiers();
    const double step = (mods.isShiftDown() ? 0.1 : (mods.isAltDown() || mods.isCommandDown() ? 10.0 : 1.0)) * (code == KeyPress::leftKey ? -1.0 : 1.0);
    const double R = regionMs();
    const double cur = param (markerId (lastMarker)) * R;
    setMarkerMs (lastMarker, std::round ((cur + step) * 10.0) / 10.0);
    repaint();
    return true;
}

Rectangle<float> WaveDisplay::plot() const { return getLocalBounds().toFloat().reduced (5.0f, 5.0f); }
float WaveDisplay::xToPos (float x) const { auto r = plot(); return jlimit (0.0f, 1.0f, (float) (viewStart + (x - r.getX()) / r.getWidth() * viewLen)); }
float WaveDisplay::posToX (float p) const { auto r = plot(); return r.getX() + (float) ((p - viewStart) / viewLen) * r.getWidth(); }

Rectangle<float> WaveDisplay::scrollBar() const { auto r = plot(); return r.removeFromBottom (6.0f); }

double WaveDisplay::minViewLen() const
{
    auto l = proc.currentLoop (module);
    if (l == nullptr) return 1.0;
    const double regionSamples = regionBeats() / jmax (1, l->beats) * l->numSamples();
    return jlimit (1.0e-7, 1.0, 0.04 * plot().getWidth() / jmax (1.0, regionSamples));   // down to ~25 pixels per sample
}

void WaveDisplay::setView (double start, double len)
{
    viewLen = jlimit (minViewLen(), 1.0, len);
    viewStart = jlimit (0.0, 1.0 - viewLen, start);
    repaint();
}

void WaveDisplay::zoomAround (double factor, float x)
{
    const auto r = plot();
    const double anchor = viewStart + (x - r.getX()) / r.getWidth() * viewLen;
    const double nl = jlimit (minViewLen(), 1.0, viewLen * factor);
    setView (anchor - (x - r.getX()) / r.getWidth() * nl, nl);
}

void WaveDisplay::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    if (! interactive) { Component::mouseWheelMove (e, w); return; }
    if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY))
        setView (viewStart - (std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY) * viewLen * 0.6, viewLen);   // scroll
    else
        zoomAround (std::exp (-w.deltaY * 2.5), e.position.x);                                                                 // zoom at the mouse
}

void WaveDisplay::mouseMagnify (const MouseEvent& e, float f)
{
    if (interactive && f > 0.0f) zoomAround (1.0 / f, e.position.x);
}

void WaveDisplay::resized()
{
    if (! interactive) return;
    auto r = plot().toNearestInt();
    auto b = Rectangle<int> (r.getRight() - 112, r.getBottom() - 30, 108, 20);
    zFit.setBounds (b.removeFromRight (44)); b.removeFromRight (4);
    zIn.setBounds (b.removeFromRight (28)); b.removeFromRight (4);
    zOut.setBounds (b.removeFromRight (28));
    waveKey = {};
}

void WaveDisplay::buildPeaks (const LoopPtr& l)
{
    peaksLoop = l;
    const int N = l->numSamples();
    mono.resize ((size_t) N);
    const float* a = l->audio.getReadPointer (0);
    const float* b = l->audio.getReadPointer (1);
    float pk = 1.0e-6f;
    for (int i = 0; i < N; ++i) { mono[(size_t) i] = 0.5f * (a[i] + b[i]); pk = jmax (pk, std::abs (mono[(size_t) i])); }
    pkNorm = 1.0f / pk;
    const int nb = (N + pkBlock - 1) / pkBlock;
    pkMin.assign ((size_t) nb, 0.0f); pkMax.assign ((size_t) nb, 0.0f);
    for (int k = 0; k < nb; ++k)
    {
        float mn = 1e9f, mx = -1e9f;
        for (int i = k * pkBlock; i < jmin (N, (k + 1) * pkBlock); ++i) { mn = jmin (mn, mono[(size_t) i]); mx = jmax (mx, mono[(size_t) i]); }
        pkMin[(size_t) k] = mn; pkMax[(size_t) k] = mx;
    }
}

// waveform of the visible part of the region: true min / max per pixel from the audio (block cache when zoomed out,
// raw samples when zoomed in, a sample-accurate line when there is less than one sample per pixel)
void WaveDisplay::renderWave (const LoopData& l, double R, int w, int h)
{
    waveImg = Image (Image::ARGB, jmax (1, w), jmax (1, h), true);
    Graphics g (waveImg);
    const int N = l.numSamples();
    const double loopBeats = jmax (1, l.beats);
    const float cy = (float) h * 0.5f, hh = (float) h * 0.47f;
    auto srcAt = [&] (double p) { const double b = std::fmod (jmax (0.0, p) * R, loopBeats); return b / loopBeats * N; };   // region pos -> sample
    const double spp = viewLen * R / loopBeats * N / w;   // samples per pixel
    g.setColour (theme::lcdInk);
    auto rangeMinMax = [&] (int i0, int i1, float& mn, float& mx)
    {
        // [i0, i1) without wrap
        int i = i0;
        while (i < i1 && (i % pkBlock) != 0) { mn = jmin (mn, mono[(size_t) i]); mx = jmax (mx, mono[(size_t) i]); ++i; }
        while (i + pkBlock <= i1) { mn = jmin (mn, pkMin[(size_t) (i / pkBlock)]); mx = jmax (mx, pkMax[(size_t) (i / pkBlock)]); i += pkBlock; }
        while (i < i1) { mn = jmin (mn, mono[(size_t) i]); mx = jmax (mx, mono[(size_t) i]); ++i; }
    };
    if (spp >= 1.0)
    {
        float prevY0 = 0, prevY1 = 0; bool havePrev = false;
        for (int px = 0; px < w; ++px)
        {
            const double p0 = viewStart + viewLen * px / w, p1 = viewStart + viewLen * (px + 1) / w;
            const int a = jlimit (0, N - 1, (int) srcAt (p0));
            int b = (int) std::ceil (srcAt (p1));
            float mn = 1e9f, mx = -1e9f;
            if (b > a) rangeMinMax (a, jmin (N, b), mn, mx);
            else { rangeMinMax (a, N, mn, mx); rangeMinMax (0, jlimit (0, N, b), mn, mx); }   // the loop wraps inside this pixel
            if (mn > mx) continue;
            float y0 = cy - mx * pkNorm * hh, y1 = cy - mn * pkNorm * hh;
            // join with the previous column so the trace is continuous (no dotted look when zoomed in)
            if (havePrev) { y0 = jmin (y0, prevY1 - 0.5f); y1 = jmax (y1, prevY0 + 0.5f); }
            prevY0 = cy - mx * pkNorm * hh; prevY1 = cy - mn * pkNorm * hh; havePrev = true;
            g.fillRect ((float) px, std::floor (y0), 1.0f, jmax (1.5f, std::ceil (y1) - std::floor (y0)));
        }
    }
    else
    {
        // zoomed in below one sample per pixel: the real waveform line through the samples
        Path line;
        for (int px = 0; px <= w; ++px)
        {
            const double sp = srcAt (viewStart + viewLen * px / w);
            const int i0 = jlimit (0, N - 1, (int) sp), i1 = (i0 + 1) % N;
            const float f = (float) (sp - std::floor (sp));
            const float v = mono[(size_t) i0] + f * (mono[(size_t) i1] - mono[(size_t) i0]);
            const float y = cy - v * pkNorm * hh;
            if (px == 0) line.startNewSubPath (0.0f, y); else line.lineTo ((float) px, y);
        }
        g.strokePath (line, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
        if (spp < 0.25)   // sample points
        {
            const double first = std::ceil (srcAt (viewStart));
            for (double sIdx = first; ; sIdx += 1.0)
            {
                const double x = (sIdx - srcAt (viewStart)) / spp;
                if (x > w) break;
                const int i = ((int) sIdx) % N;
                g.fillEllipse ((float) x - 2.0f, cy - mono[(size_t) i] * pkNorm * hh - 2.0f, 4.0f, 4.0f);
            }
        }
    }
    g.setColour (theme::lcdInk.withAlpha (0.25f));
    g.drawHorizontalLine ((int) cy, 0.0f, (float) w);
}
float WaveDisplay::yToVol (float y) const { auto r = plot(); return jlimit (0.0f, 1.0f, 1.0f - (y - r.getY()) / r.getHeight()); }
float WaveDisplay::volToY (float v) const { auto r = plot(); return r.getY() + (1.0f - v) * r.getHeight(); }

void WaveDisplay::timerCallback()
{
    const float pos = proc.modulePlayPos (module);
    if (std::abs (pos - lastPos) > 1.0e-6f || isMouseButtonDown()) { lastPos = pos; repaint(); }
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
        g.drawText (module == lb::custom ? "DROP AN AUDIO FILE HERE  /  FLOPPY = LOAD" : (proc.library->isScanning() ? "SCANNING..." : "NO LOOP"), r, Justification::centred);
        return;
    }
    const double R = regionBeats(), loopBeats = jmax (1, l->beats);
    const double gb = lb::gridBeats ((int) param ("grid"));
    const bool sliceOn = param ("slice") > 0.5f, volOn = param ("vola") > 0.5f;
    const float s0 = param ("sstart"), s1 = param ("send");

    Graphics::ScopedSaveState clip (g);
    g.reduceClipRegion (r.toNearestInt());
    // grid (transparent), bars stronger: only the visible part, only when the lines are not too dense
    if (viewLen * R / gb < r.getWidth() / 3.0)
        for (double b = std::floor (viewStart * R / gb) * gb; b < (viewStart + viewLen) * R + 1e-9; b += gb)
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
    // waveform (cached image, rebuilt only when loop / view / size change)
    {
        if (peaksLoop != l) { buildPeaks (l); waveKey = {}; }
        if (viewLen < minViewLen() || viewStart + viewLen > 1.0 + 1e-9) setView (viewStart, viewLen);
        const int w = (int) r.getWidth(), h = (int) r.getHeight();
        auto differs = [] (double a, double b) { return std::abs (a - b) > 1.0e-12; };
        if (waveKey.loop != l.get() || differs (waveKey.vs, viewStart) || differs (waveKey.vl, viewLen) || differs (waveKey.R, R) || waveKey.w != w || waveKey.h != h)
        {
            renderWave (*l, R, w, h);
            waveKey = { l.get(), viewStart, viewLen, R, w, h };
        }
        g.drawImageAt (waveImg, (int) r.getX(), (int) r.getY());
    }
    // slice
    if (sliceOn)
    {
        g.setColour (theme::lcdBg.withAlpha (0.7f));
        g.fillRect (Rectangle<float> (r.getX(), r.getY(), jmax (0.0f, posToX (s0) - r.getX()), r.getHeight()));
        g.fillRect (Rectangle<float> (posToX (s1), r.getY(), jmax (0.0f, r.getRight() - posToX (s1)), r.getHeight()));
        g.setColour (theme::lcdInk.withAlpha (0.12f));
        g.fillRect (Rectangle<float> (r.getX(), r.getY(), jmax (0.0f, posToX (s0) - r.getX()), r.getHeight()));
        g.fillRect (Rectangle<float> (posToX (s1), r.getY(), jmax (0.0f, r.getRight() - posToX (s1)), r.getHeight()));
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
    // slice positions in milliseconds
    if (sliceOn && interactive)
    {
        const double Rms = regionMs();
        g.setFont (theme::mono (11.5f, true));
        for (int k = 0; k < 2; ++k)
        {
            const float pk = k == 0 ? s0 : s1;
            const String txt = (k == 0 ? "S " : "E ") + String (pk * Rms, 1) + " ms";
            const float x = posToX (pk);
            auto box = Rectangle<float> (k == 0 ? x + 3.0f : x - 93.0f, r.getBottom() - 18.0f, 90.0f, 15.0f);
            g.setColour (theme::lcdBg.withAlpha (0.85f)); g.fillRect (box);
            g.setColour (lastMarker == k && hasKeyboardFocus (false) ? theme::red.darker (0.3f) : theme::lcdInk);
            g.drawText (txt, box, k == 0 ? Justification::centredLeft : Justification::centredRight);
        }
        g.setColour (theme::lcdInk);
        g.drawText ("SLICE " + String ((s1 - s0) * Rms, 1) + " ms" + (param ("slsnap") < 0.5f ? "  (SNAP ms)" : "  (SNAP grid)") + (param ("slloop") > 0.5f ? "  LOOP" : ""),
                    r.reduced (4, 2).translated (0, 15), Justification::topLeft);
    }
    // loop markers (LS / LE)
    if (param ("lmOn") > 0.5f)
    {
        const float l0 = param ("lmStart"), l1 = param ("lmEnd");
        const Colour lc (0xff1f4e8c);
        g.setColour (theme::lcdBg.withAlpha (0.35f));
        g.fillRect (Rectangle<float> (r.getX(), r.getY(), jmax (0.0f, posToX (l0) - r.getX()), r.getHeight()));
        g.fillRect (Rectangle<float> (posToX (l1), r.getY(), jmax (0.0f, r.getRight() - posToX (l1)), r.getHeight()));
        g.setColour (lc.withAlpha (0.5f));
        g.fillRect (Rectangle<float> (posToX (l0), r.getBottom() - 3.0f, jmax (0.0f, posToX (l1) - posToX (l0)), 3.0f));
        for (int k = 0; k < 2; ++k)
        {
            const float x = posToX (k == 0 ? l0 : l1);
            g.setColour (lc);
            g.drawLine (x, r.getY(), x, r.getBottom(), 2.0f);
            if (interactive)
            {
                Path flag;
                const float d = k == 0 ? 1.0f : -1.0f;
                flag.addTriangle (x, r.getBottom(), x + d * 14.0f, r.getBottom(), x, r.getBottom() - 14.0f);
                g.fillPath (flag);
                g.setColour (theme::white);
                g.setFont (theme::font (8.0f, true));
                g.drawText (k == 0 ? "LS" : "LE", Rectangle<float> (k == 0 ? x : x - 11, r.getBottom() - 8, 11, 8), Justification::centred);
            }
        }
        if (interactive)
        {
            const double Rms = regionMs();
            g.setFont (theme::mono (11.5f, true));
            for (int k = 0; k < 2; ++k)
            {
                const float pk = k == 0 ? l0 : l1;
                const float x = posToX (pk);
                auto box = Rectangle<float> (k == 0 ? x + 3.0f : x - 93.0f, r.getBottom() - 36.0f, 90.0f, 15.0f);
                g.setColour (theme::lcdBg.withAlpha (0.85f)); g.fillRect (box);
                g.setColour (lastMarker == k + 2 && hasKeyboardFocus (false) ? lc.brighter (0.3f) : lc);
                g.drawText ((k == 0 ? "LS " : "LE ") + String (pk * Rms, 1) + " ms", box, k == 0 ? Justification::centredLeft : Justification::centredRight);
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
    if (interactive && viewLen < 0.999)
    {
        auto sb = scrollBar();
        g.setColour (theme::lcdInk.withAlpha (0.15f)); g.fillRect (sb);
        g.setColour (theme::lcdInk.withAlpha (0.6f));
        g.fillRoundedRectangle (sb.getX() + (float) viewStart * sb.getWidth(), sb.getY(), jmax (6.0f, (float) viewLen * sb.getWidth()), sb.getHeight(), 2.0f);
        g.setFont (theme::mono (11.0f, true));
        const double z = 1.0 / viewLen;
        g.drawText ("ZOOM x" + (z < 10.0 ? String (z, 1) : String (roundToInt (z))), r.reduced (4, 2).withTrimmedBottom (30), Justification::bottomRight);
    }
    if (interactive)
    {
        const int steps = (int) param ("steps");
        g.setColour (theme::lcdInk);
        g.setFont (theme::mono (12.0f, true));
        const String len = steps == 0 ? "LENGTH ORIG = " + String (loopBeats / 4.0, 2) + " bars"
                                      : "LENGTH " + String (steps) + " x " + lb::gridNames()[(int) param ("grid")] + " = " + String (R, 3) + " beats";
        g.drawText (len + (std::abs (R - loopBeats) > 1.0e-9 ? "   (loop " + String (loopBeats / 4.0, 2) + " bars)" : String()), r.reduced (4, 2), Justification::topLeft);
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
    const bool nearMarker = markerAt (e.position) >= 0;
    setMouseCursor (nearMarker ? MouseCursor::LeftRightResizeCursor
                               : (param ("vola") > 0.5f ? (hitVolPoint (e.position) >= 0 ? MouseCursor::DraggingHandCursor : MouseCursor::CrosshairCursor)
                                                        : MouseCursor::NormalCursor));
}

void WaveDisplay::mouseDown (const MouseEvent& e)
{
    if (! interactive) { if (onClick) onClick(); return; }
    grabKeyboardFocus();
    if (viewLen < 0.999 && scrollBar().expanded (0, 4).contains (e.position))
    {
        draggingScroll = true;
        const auto sb = scrollBar();
        setView ((e.position.x - sb.getX()) / sb.getWidth() - viewLen * 0.5, viewLen);
        return;
    }
    drag = none;
    const bool sliceOn = param ("slice") > 0.5f, volOn = param ("vola") > 0.5f;
    const bool loopOn = param ("lmOn") > 0.5f;
    if (const int hit = markerAt (e.position); hit >= 0) drag = (Drag) (sliceStart + hit);
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
    if (drag == none && (sliceOn || loopOn))
    {
        // click elsewhere: move the nearest marker there (slice markers in the upper half, loop markers in the lower half)
        const float pos = xToPos (e.position.x);
        const int base = sliceOn && (! loopOn || e.position.y < plot().getCentreY()) ? 0 : 2;
        const int k = std::abs (pos - param (markerId (base))) < std::abs (pos - param (markerId (base + 1))) ? base : base + 1;
        drag = (Drag) (sliceStart + k);
        lastMarker = k;
        if (param ("slsnap") < 0.5f) { setMarkerMs (k, std::round (pos * regionMs())); dragStartPos = pos; dragStartX = e.position.x; return; }
        mouseDrag (e);
        return;
    }
    if (drag >= sliceStart && drag <= loopEnd)
    {
        lastMarker = drag - sliceStart;
        dragStartPos = param (markerId (lastMarker));
        dragStartX = e.position.x;
    }
}

void WaveDisplay::mouseDrag (const MouseEvent& e)
{
    if (! interactive) return;
    if (draggingScroll)
    {
        const auto sb = scrollBar();
        setView ((e.position.x - sb.getX()) / sb.getWidth() - viewLen * 0.5, viewLen);
        return;
    }
    const double R = regionBeats();
    auto l = proc.currentLoop (module);
    float pos = xToPos (e.position.x);
    const bool markerDrag = drag >= sliceStart && drag <= loopEnd;
    if (markerDrag && param ("slsnap") < 0.5f)
    {
        // SNAP ms: free movement in 1 ms steps; shift = fine (10x slower, 0.1 ms steps)
        const bool fine = e.mods.isShiftDown();
        const double w = jmax (1.0f, plot().getWidth());
        const double p = dragStartPos + (e.position.x - dragStartX) / w * viewLen * (fine ? 0.1 : 1.0);
        const double stepMs = fine ? 0.1 : 1.0;
        const double Rms = regionMs();
        setMarkerMs (drag - sliceStart, std::round (jlimit (0.0, 1.0, p) * Rms / stepMs) * stepMs);
        repaint();
        return;
    }
    if (! e.mods.isShiftDown() && R > 0)   // snap to the grid (shift = free)
    {
        const double gb = lb::gridBeats ((int) param ("grid"));
        pos = (float) (std::round (pos * R / gb) * gb / R);
    }
    if (markerDrag) setMarkerPos (drag - sliceStart, pos);
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

void WaveDisplay::mouseUp (const MouseEvent&) { drag = none; draggingScroll = false; repaint(); }

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
    const bool isCustom = m == lb::custom;
    for (auto* b : { &prev, &next, &rnd, &sim }) addChildComponent (b), b->setVisible (! isCustom);
    addChildComponent (floppy); floppy.setVisible (isCustom);
    addChildComponent (clear); clear.setVisible (isCustom);
    floppy.onClick = [this]
    {
        chooser = std::make_unique<FileChooser> ("Load an audio file into CUSTOM", File::getSpecialLocation (File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.m4a;*.caf");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc)
        {
            if (fc.getResult().existsAsFile()) { proc.loadCustomFile (fc.getResult()); select (module); }
        });
    };
    clear.onClick = [this] { proc.clearCustom(); };
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

bool ModuleStrip::isInterestedInFileDrag (const StringArray& files)
{
    if (module != lb::custom) return false;
    for (auto& f : files) if (File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg;m4a;caf")) return true;
    return false;
}

void ModuleStrip::filesDropped (const StringArray& files, int, int)
{
    dragOver = false;
    for (auto& f : files)
        if (File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg;m4a;caf")) { proc.loadCustomFile (File (f)); select (module); break; }
    repaint();
}

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
    g.drawFittedText (shownLoop ? shownLoop->name : String (module == lb::custom ? "drop a file / floppy" : "-"), infoArea.withHeight (15), Justification::centredLeft, 1, 0.75f);
    g.setColour (theme::textDim);
    g.setFont (theme::font (10.5f));
    String info = shownLoop ? loopInfo (*shownLoop) : String();
    const float st = proc.appliedSemitones (module);
    if (shownLoop && std::abs (st) > 0.004f) info << "  " << (st > 0 ? "+" : "") << String (st, 2) << "st";
    g.drawFittedText (info, infoArea.withTrimmedTop (15), Justification::centredLeft, 1, 0.7f);
    if (dragOver)
    {
        g.setColour (c.withAlpha (0.25f)); g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
        g.setColour (c); g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 4.0f, 3.0f);
    }
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
    {
        auto bb = b;
        floppy.setBounds (bb.removeFromLeft (bw * 40 / 100).reduced (1, 0));
        clear.setBounds (bb.reduced (1, 0));
    }
    prev.setBounds (b.removeFromLeft (bw * 18 / 100).reduced (1, 0));
    next.setBounds (b.removeFromLeft (bw * 18 / 100).reduced (1, 0));
    rnd.setBounds (b.removeFromLeft (bw * 32 / 100).reduced (1, 0));
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
    display.setComponentID ("bigDisplay");
    addAndMakeVisible (display);
    for (auto* c : std::initializer_list<Component*> { &grid, &keySrc, &slsnap, &lenMinus, &lenPlus, &lenOrig, &volReset, &slice, &slLoop, &lmOn, &fade, &volA, &dpp,
                                                       &hpOn, &bpOn, &lpOn, &chOn, &mono, &chMode,
                                                       &prev, &next, &rnd, &sim, &browser,
                                                       &pAmp, &pAdsr, &pPitch, &pFilter, &pDirt, &pDelay, &pReverb, &pLfo1, &pLfo2, &pLfo3, &pChorus, &pUtil })
        addAndMakeVisible (c);
    addChildComponent (floppy); addChildComponent (clear);
    floppy.onClick = [this]
    {
        chooser = std::make_unique<FileChooser> ("Load an audio file into CUSTOM", File::getSpecialLocation (File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.m4a;*.caf");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc)
        {
            if (fc.getResult().existsAsFile()) proc.loadCustomFile (fc.getResult());
        });
    };
    clear.onClick = [this] { proc.clearCustom(); };
    for (auto* k : { &gain, &pan, &atk, &dec, &sus, &rel, &pitch, &hpCut, &hpRes, &bpCut, &bpRes, &lpCut, &lpRes, &dirt, &dtime, &dfb, &dmix,
                     &rsize, &rdecay, &rdamp, &rpre, &rmix, &l1rate, &l1depth, &l2rate, &l2depth, &l3rate, &l3depth, &chRate, &chDepth, &chMix, &width })
        addAndMakeVisible (k);
    chMode.addItemList (lb::chorusModes(), 1);
    chMode.setTooltip ("Chorus I / II: chorus BBD a due linee (lento / piu' veloce e profondo). Dimension 1-4: spaziale, senza vibrato. Custom: Rate e Depth liberi");
    chOn.setTooltip ("Attiva / disattiva il chorus");
    width.slider.setTooltip ("Ampiezza stereo: 0 % = mono, 100 % = invariato, fino a 400 %");
    mono.setTooltip ("Somma il modulo in mono");
    hpOn.setTooltip ("Attiva / disattiva il passa-alto");
    bpOn.setTooltip ("Attiva / disattiva il passa-banda");
    lpOn.setTooltip ("Attiva / disattiva il passa-basso");
    hpCut.slider.setTooltip ("Passa-alto: a sinistra aperto, girando a destra chiude (speculare al LP)");
    lpCut.slider.setTooltip ("Passa-basso: a destra aperto, girando a sinistra chiude (speculare all'HP)");
    lmOn.setTooltip ("LOOP MARKERS: la riproduzione si ripete tra i marker LS e LE (in modo normale e in SLICE; trascina i marker in basso nel display)");
    // SLICE off -> SLICE LOOP off
    slice.onClick = [this]
    {
        if (! slice.getToggleState() && proc.apvts.getRawParameterValue (lb::pid (module, "slloop"))->load() > 0.5f)
            setParamValue (proc, lb::pid (module, "slloop"), 0.0f);
    };
    for (auto* c : { &l1shape, &l1target, &l2shape, &l2target, &l3shape, &l3target }) addAndMakeVisible (c);
    // the panels are backgrounds: keep them behind the selectors / toggles placed on them (LP|BP|HP, WARP, PING-PONG)
    for (auto* pn : { &pAmp, &pAdsr, &pPitch, &pFilter, &pDirt, &pDelay, &pReverb, &pLfo1, &pLfo2, &pLfo3, &pChorus, &pUtil }) pn->toBack();
    for (auto* c : { &l1shape, &l2shape, &l3shape }) c->addItemList (lb::lfoShapes(), 1);
    for (auto* c : { &l1target, &l2target, &l3target })
    {
        c->addItemList (lb::lfoTargets(), 1);
        c->setTooltip ("LFO destination (Loop = switches between up to 8 similar loops on every grid step)");
    }
    keySrc.setCaption ("TYPE (key from)");
    grid.setCaption ("GRID");

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
    slLoop.setTooltip ("SLICE LOOP: the part between S and E repeats continuously (5 ms crossfade at every repeat)");
    volA.setTooltip ("Clip-gain automation drawn on the display");
    setModule (0);
    startTimerHz (10);
}

void ModuleEditor::attachAll()
{
    const int m = module;
    auto& s = proc.apvts;
    gain.attach (s, lb::pid (m, "gain")); pan.attach (s, lb::pid (m, "pan"));
    atk.attach (s, lb::pid (m, "atk")); dec.attach (s, lb::pid (m, "dec")); sus.attach (s, lb::pid (m, "sus")); rel.attach (s, lb::pid (m, "rel"));
    pitch.attach (s, lb::pid (m, "pitch"));
    hpCut.attach (s, lb::pid (m, "hpCut")); hpRes.attach (s, lb::pid (m, "hpRes")); hpOn.attach (s, lb::pid (m, "hpOn"));
    bpCut.attach (s, lb::pid (m, "bpCut")); bpRes.attach (s, lb::pid (m, "bpRes")); bpOn.attach (s, lb::pid (m, "bpOn"));
    lpCut.attach (s, lb::pid (m, "lpCut")); lpRes.attach (s, lb::pid (m, "lpRes")); lpOn.attach (s, lb::pid (m, "lpOn"));
    dirt.attach (s, lb::pid (m, "dirt"));
    chOn.attach (s, lb::pid (m, "chOn")); chRate.attach (s, lb::pid (m, "chRate")); chDepth.attach (s, lb::pid (m, "chDepth")); chMix.attach (s, lb::pid (m, "chMix"));
    width.attach (s, lb::pid (m, "width")); mono.attach (s, lb::pid (m, "mono"));
    lmOn.attach (s, lb::pid (m, "lmOn"));
    dtime.attach (s, lb::pid (m, "dtime")); dfb.attach (s, lb::pid (m, "dfb")); dmix.attach (s, lb::pid (m, "dmix"));
    rsize.attach (s, lb::pid (m, "rsize")); rdecay.attach (s, lb::pid (m, "rdecay")); rdamp.attach (s, lb::pid (m, "rdamp"));
    rpre.attach (s, lb::pid (m, "rpre")); rmix.attach (s, lb::pid (m, "rmix"));
    l1rate.attach (s, lb::pid (m, "lfo1rate")); l1depth.attach (s, lb::pid (m, "lfo1depth"));
    l2rate.attach (s, lb::pid (m, "lfo2rate")); l2depth.attach (s, lb::pid (m, "lfo2depth"));
    l3rate.attach (s, lb::pid (m, "lfo3rate")); l3depth.attach (s, lb::pid (m, "lfo3depth"));
    grid.attach (s, lb::pid (m, "grid"));
    keySrc.attach (s, lb::pid (m, "keysrc"), { "AUDIO", "FILE NAME" });
    slsnap.attach (s, lb::pid (m, "slsnap"), { "MS", "GRID" });
    slsnap.setTooltip ("SLICE markers: MS = move in milliseconds (shift = 0.1 ms, arrow keys nudge), GRID = snap to the grid");
    slice.attach (s, lb::pid (m, "slice")); slLoop.attach (s, lb::pid (m, "slloop")); fade.attach (s, lb::pid (m, "fade")); volA.attach (s, lb::pid (m, "vola")); dpp.attach (s, lb::pid (m, "dpp"));
    comboAtt.clear();
    comboAtt.push_back (std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (s, lb::pid (m, "lfo1shape"), l1shape));
    comboAtt.push_back (std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (s, lb::pid (m, "lfo1target"), l1target));
    comboAtt.push_back (std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (s, lb::pid (m, "lfo2shape"), l2shape));
    comboAtt.push_back (std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (s, lb::pid (m, "lfo2target"), l2target));
    comboAtt.push_back (std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (s, lb::pid (m, "lfo3shape"), l3shape));
    comboAtt.push_back (std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (s, lb::pid (m, "lfo3target"), l3target));
    comboAtt.push_back (std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (s, lb::pid (m, "chMode"), chMode));
}

void ModuleEditor::setModule (int m)
{
    module = jlimit (0, lb::numModules - 1, m);
    display.setModule (module);
    const auto c = theme::sectionColour (module);
    for (auto* k : { &gain, &pan, &atk, &dec, &sus, &rel, &pitch, &hpCut, &hpRes, &bpCut, &bpRes, &lpCut, &lpRes, &dirt, &dtime, &dfb, &dmix,
                     &rsize, &rdecay, &rdamp, &rpre, &rmix, &l1rate, &l1depth, &l2rate, &l2depth, &l3rate, &l3depth, &chRate, &chDepth, &chMix, &width })
        k->setAccent (c);
    for (auto* p : { &pAmp, &pAdsr, &pPitch, &pFilter, &pDirt, &pDelay, &pReverb, &pLfo1, &pLfo2, &pLfo3, &pChorus, &pUtil }) p->setAccent (c);
    const bool isCustom = module == lb::custom;
    for (auto* b : std::initializer_list<Component*> { &prev, &next, &rnd, &sim, &browser }) b->setVisible (! isCustom);
    floppy.setVisible (isCustom); clear.setVisible (isCustom);
    for (auto* sg : { &grid, &keySrc, &slsnap }) sg->setAccent (c);
    for (auto* t : { &slice, &slLoop, &lmOn, &fade, &volA, &dpp, &hpOn, &bpOn, &lpOn, &chOn, &mono }) t->setColour (TextButton::buttonOnColourId, c);
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
    grid.setBounds (tb.removeFromLeft (330).withTrimmedBottom (2));
    tb.removeFromLeft (10);
    auto lenA = tb.removeFromLeft (130).withTrimmedTop (14).withTrimmedBottom (2);
    lenMinus.setBounds (lenA.removeFromLeft (32).reduced (1, 0));
    lenPlus.setBounds (lenA.removeFromLeft (32).reduced (1, 0));
    lenOrig.setBounds (lenA.reduced (1, 0));
    tb.removeFromLeft (10);
    auto tg = tb.removeFromLeft (574).withTrimmedTop (14).withTrimmedBottom (2);
    slice.setBounds (tg.removeFromLeft (60).reduced (1, 0));
    slLoop.setBounds (tg.removeFromLeft (54).reduced (1, 0));
    lmOn.setBounds (tg.removeFromLeft (110).reduced (1, 0));
    slsnap.setBounds (tg.removeFromLeft (96).reduced (2, 0));
    fade.setBounds (tg.removeFromLeft (90).reduced (1, 0));
    volA.setBounds (tg.removeFromLeft (70).reduced (1, 0));
    volReset.setBounds (tg.removeFromLeft (90).reduced (1, 0));
    tb.removeFromLeft (10);
    keySrc.setBounds (tb.removeFromLeft (140).withTrimmedBottom (2));
    tb.removeFromLeft (10);
    auto ls = tb.withTrimmedTop (14).withTrimmedBottom (2);
    prev.setBounds (ls.removeFromLeft (28).reduced (1, 0));
    next.setBounds (ls.removeFromLeft (28).reduced (1, 0));
    rnd.setBounds (ls.removeFromLeft (48).reduced (1, 0));
    sim.setBounds (ls.removeFromLeft (80).reduced (1, 0));
    {
        auto fl = prev.getBounds().getUnion (sim.getBounds());
        floppy.setBounds (fl.removeFromLeft (60).reduced (1, 0));
        clear.setBounds (fl.removeFromLeft (80).reduced (1, 0));
    }
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
    place (row1, { { &pAmp, 2 }, { &pAdsr, 4 }, { &pPitch, 1 }, { &pFilter, 6 }, { &pDirt, 1 }, { &pUtil, 2 } });
    place (row2, { { &pChorus, 4 }, { &pDelay, 3 }, { &pReverb, 5 }, { &pLfo1, 3 }, { &pLfo2, 3 }, { &pLfo3, 3 } });
    auto inP = [] (Panel& p) { return p.getBounds().withTrimmedTop (Panel::headerH + 2).reduced (4, 2); };
    layoutRow (inP (pAmp), { &gain, &pan });
    layoutRow (inP (pAdsr), { &atk, &dec, &sus, &rel });
    layoutRow (inP (pPitch), { &pitch });
    {
        auto f = inP (pFilter);
        const int cw = f.getWidth() / 3;
        Toggle* tg3[3] { &hpOn, &bpOn, &lpOn };
        Knob* fk[3] { &hpCut, &bpCut, &lpCut };
        Knob* rk[3] { &hpRes, &bpRes, &lpRes };
        for (int k = 0; k < 3; ++k)
        {
            auto col = (k < 2 ? f.removeFromLeft (cw) : f).reduced (3, 0);
            tg3[k]->setBounds (col.removeFromTop (22).withSizeKeepingCentre (jmin (80, col.getWidth()), 22));
            layoutRow (col, { fk[k], rk[k] });
        }
    }
    {
        auto u = inP (pUtil);
        mono.setBounds (u.removeFromBottom (22).withSizeKeepingCentre (jmin (80, u.getWidth()), 22));
        layoutRow (u, { &width });
    }
    {
        auto c = inP (pChorus);
        auto top = c.removeFromTop (24);
        chOn.setBounds (top.removeFromLeft (50).reduced (1, 1));
        chMode.setBounds (top.reduced (4, 1));
        layoutRow (c, { &chRate, &chDepth, &chMix });
    }
    layoutRow (inP (pDirt), { &dirt });
    {
        auto d = inP (pDelay);
        dpp.setBounds (d.removeFromBottom (22).withSizeKeepingCentre (100, 22));
        layoutRow (d, { &dtime, &dfb, &dmix });
    }
    layoutRow (inP (pReverb), { &rsize, &rdecay, &rdamp, &rpre, &rmix });
    for (int k = 0; k < 3; ++k)
    {
        auto a = inP (k == 0 ? pLfo1 : (k == 1 ? pLfo2 : pLfo3));
        auto combos = a.removeFromRight (a.getWidth() * 45 / 100);
        auto& sh = k == 0 ? l1shape : (k == 1 ? l2shape : l3shape);
        auto& tg2 = k == 0 ? l1target : (k == 1 ? l2target : l3target);
        sh.setBounds (combos.removeFromTop (combos.getHeight() / 2).withSizeKeepingCentre (combos.getWidth(), 24));
        tg2.setBounds (combos.withSizeKeepingCentre (combos.getWidth(), 24));
        layoutRow (a, { k == 0 ? &l1rate : (k == 1 ? &l2rate : &l3rate), k == 0 ? &l1depth : (k == 1 ? &l2depth : &l3depth) });
    }
}
