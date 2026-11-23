#include "DeckWindows.h"

using namespace juce;

namespace
{
        constexpr float kTwoPi = MathConstants<float>::twoPi;
    float wrapPi (float a) { while (a > MathConstants<float>::pi) a -= kTwoPi; while (a < -MathConstants<float>::pi) a += kTwoPi; return a; }
    const char* actionName (int a)
    {
        static const char* n[] { "", "STOP", "START", "WOBBLE", "PULL", "REWIND", "SCRATCH", "BRAKE", "SPINBACK", "RANDOM" };
        return n[jlimit (0, 9, a)];
    }
}

DeckView::DeckView (GroovulationProcessor& p, int t) : proc (p), type (t)
{
    struct B { const char* label; int action; };
    std::vector<B> list;
    if (type == 1) list = { { "STOP", fx::Deck::stop }, { "START", fx::Deck::start }, { "WOBBLE", fx::Deck::wobble }, { "PULL", fx::Deck::pull },
                            { "REWIND", fx::Deck::rewind }, { "RANDOM", fx::Deck::random } };
    else list = { { "SPINBACK", fx::Deck::spinback }, { "SCRATCH", fx::Deck::scratch }, { "WOBBLE", fx::Deck::wobble },
                  { "RANDOM", fx::Deck::random }, { "RESYNC", -1 } };
    for (auto& b : list)
    {
        auto* btn = buttons.add (new TextButton (b.label));
        const int a = b.action;
        btn->onClick = [this, a] { if (a < 0) proc.requestResync(); else trigger (a); };
        if (a < 0) btn->setTooltip ("Back in sync with the DAW (after pitch-fader / 33-45 changes)");
        if (a == fx::Deck::random) { btn->setColour (TextButton::buttonColourId, theme::red); btn->setColour (TextButton::textColourOffId, theme::white); }
        addAndMakeVisible (btn);
    }
    lastTime = Time::getMillisecondCounterHiRes() * 0.001;
    startTimerHz (60);
}

DeckView::~DeckView()
{
    proc.deck.handHeld = false;
}

void DeckView::trigger (int action) { proc.deck.pendingAction = action; }

float DeckView::vinylRate() const
{
    return (proc.apvts.getRawParameterValue ("vinyl45")->load() > 0.5f ? 45.0f / (100.0f / 3.0f) : 1.0f)
         * (1.0f + proc.apvts.getRawParameterValue ("vinylPitch")->load() / 100.0f);
}

void DeckView::resized()
{
    auto r = getLocalBounds();
    auto btnRow = r.removeFromBottom (54).reduced (20, 10);
    const int w = btnRow.getWidth() / jmax (1, buttons.size());
    for (auto* b : buttons) b->setBounds (btnRow.removeFromLeft (w).reduced (5, 0));
    deckArea = r.toFloat().reduced (24.0f, 16.0f);
    if (type == 1)
    {
        reelRadius = jmin (deckArea.getWidth() * 0.19f, deckArea.getHeight() * 0.3f);
        reelL = { deckArea.getX() + deckArea.getWidth() * 0.27f, deckArea.getY() + deckArea.getHeight() * 0.4f };
        reelR = { deckArea.getX() + deckArea.getWidth() * 0.73f, reelL.y };
    }
    else
    {
        platterR = jmin (deckArea.getHeight() * 0.42f, deckArea.getWidth() * 0.3f);
        platter = { deckArea.getX() + deckArea.getWidth() * 0.42f, deckArea.getCentreY() };
        startStopBtn = Rectangle<float> (deckArea.getX() + 22.0f, deckArea.getBottom() - 70.0f, 92.0f, 48.0f);
    }
    renderBackdrop();
}

void DeckView::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes() * 0.001;
    const float dt = (float) jlimit (0.0, 0.1, now - lastTime);
    lastTime = now;
    logoPhase += dt;
    auto& deck = proc.deck;
    const bool running = proc.isTransportRunning();
    float speed = deck.engagedFlag.load() ? deck.currentSpeed.load() : 1.0f;
    if (type == 2) speed *= vinylRate();   // pitch fader + 33/45 (the motor ramp is inside the deck speed)
    shownSpeed = speed;
    const float motion = running ? speed : (deck.engagedFlag.load() ? speed : 0.0f);
    if (type == 1)
    {
        angleR += dt * kTwoPi * 0.45f * motion;
        angleL += dt * kTwoPi * 0.38f * motion;
        packL = jlimit (0.45f, 0.9f, packL - dt * 0.002f * motion);
        if (packL <= 0.46f) packL = 0.9f;
    }
    else recordAngle += dt * (kTwoPi * 33.333f / 60.0f) * (running || deck.engagedFlag.load() ? speed : 0.0f);

    // grip handling (held still on the record = stop; left reel relaxes back)
    if (grip == record && now - lastMoveTime > 0.05) deck.handSpeed = deck.handSpeed.load() * 0.6f;
    if (grip == leftReel && now - lastMoveTime > 0.06) deck.handSpeed = deck.handSpeed.load() + (0.97f - deck.handSpeed.load()) * 0.3f;

    repaint();
}

//==============================================================================
void DeckView::mouseMove (const MouseEvent& e) { lastPos = e.position; }

void DeckView::mouseDown (const MouseEvent& e)
{
    pressPos = lastPos = e.position;
    lastMoveTime = Time::getMillisecondCounterHiRes() * 0.001;
    auto& deck = proc.deck;
    grip = none;
    if (type == 1)
    {
        if (e.position.getDistanceFrom (reelR) < reelRadius * 1.05f) grip = rightReel;
        else if (e.position.getDistanceFrom (reelL) < reelRadius * 1.05f) grip = leftReel;
        else if (deckArea.contains (e.position)) { trigger (fx::Deck::random); return; }   // between the reels
        if (grip != none) { deck.handSpeed = 1.0f; deck.handHeld = true; }
    }
    else
    {
        auto setParam = [this] (const char* id, float v)
        {
            if (auto* prm = proc.apvts.getParameter (id)) { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); prm->endChangeGesture(); }
        };
        if (startStopBtn.contains (e.position))   // real START / STOP: the platter brakes and stays stopped until START
        {
            setParam ("vinylMotor", proc.apvts.getRawParameterValue ("vinylMotor")->load() > 0.5f ? 0.0f : 1.0f);
            return;
        }
        if (rpm33.contains (e.position)) { setParam ("vinyl45", 0.0f); return; }
        if (rpm45.contains (e.position)) { setParam ("vinyl45", 1.0f); return; }
        if (faderArea.expanded (8.0f).contains (e.position)) { grip = fader; mouseDrag (e); return; }
        if (e.position.getDistanceFrom (platter) < platterR * 1.02f)
        {
            grip = record;
            lastAngle = std::atan2 (e.position.y - platter.y, e.position.x - platter.x);
            lastAngleTime = lastMoveTime;
            deck.handSpeed = 0.0f;   // hand on the record: it stops under the fingers
            deck.handHeld = true;
        }
    }
}

void DeckView::mouseDrag (const MouseEvent& e)
{
    const double now = Time::getMillisecondCounterHiRes() * 0.001;
    auto& deck = proc.deck;
    const auto prev = lastPos;
    lastPos = e.position;
    const float dtm = (float) jmax (1.0e-3, now - lastMoveTime);
    lastMoveTime = now;
    if (grip == rightReel)
    {
        // up = pull the tape faster, down = slow down, stop, reverse
        deck.handSpeed = jlimit (-1.5f, 2.5f, 1.0f + (pressPos.y - e.position.y) / 140.0f);
    }
    else if (grip == leftReel)
    {
        // shaking the supply reel drags / wobbles the tape
        const float v = (e.position.y - prev.y) / dtm + 0.5f * (e.position.x - prev.x) / dtm;
        const float target = jlimit (-1.0f, 2.5f, 0.97f - v / 900.0f);
        deck.handSpeed = deck.handSpeed.load() * 0.5f + target * 0.5f;
    }
    else if (grip == fader)
    {
        // like a real deck: top = -8 %, bottom = +8 %, centre click; changes speed AND pitch
        float v = jlimit (-8.0f, 8.0f, (e.position.y - faderArea.getCentreY()) / (faderArea.getHeight() * 0.45f) * 8.0f);
        if (std::abs (v) < 0.25f) v = 0.0f;
        if (auto* prm = proc.apvts.getParameter ("vinylPitch")) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    }
    else if (grip == record)
    {
        const float a = std::atan2 (e.position.y - platter.y, e.position.x - platter.x);
        const float da = wrapPi (a - lastAngle);
        const float dta = (float) jmax (1.0e-3, now - lastAngleTime);
        lastAngle = a; lastAngleTime = now;
        const float omega0 = kTwoPi * 33.333f / 60.0f;
        const float inst = jlimit (-6.0f, 6.0f, (da / dta) / omega0);   // clockwise = forward
        deck.handSpeed = deck.handSpeed.load() * 0.4f + inst * 0.6f;
        recordAngle += da;
    }
}

void DeckView::mouseUp (const MouseEvent&)
{
    if (grip != none && grip != fader) proc.deck.handHeld = false;
    grip = none;
}

//==============================================================================
//  Drawing: illustrated, half-realistic look (thin dark outlines, painted gradients, hard highlights,
//  drop shadows).
namespace
{
    const Colour ink { 0xff15100c };
    void inked (Graphics& g, const Path& p, const ColourGradient& fill, float w)
    {
        g.setGradientFill (fill); g.fillPath (p);
        g.setColour (ink.withAlpha (0.85f)); g.strokePath (p, PathStrokeType (w * 0.5f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    void inked (Graphics& g, const Path& p, Colour fill, float w)
    {
        g.setColour (fill); g.fillPath (p);
        g.setColour (ink.withAlpha (0.85f)); g.strokePath (p, PathStrokeType (w * 0.5f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    void softShadow (Graphics& g, const Path& p, Point<float> offset, float spread)
    {
        for (int i = 6; i >= 1; --i)
        {
            Path s (p);
            const auto b = s.getBounds();
            const float k = 1.0f + spread * (float) i / 6.0f / jmax (1.0f, b.getWidth());
            s.applyTransform (AffineTransform::scale (k, k, b.getCentreX(), b.getCentreY()).translated (offset.x, offset.y));
            g.setColour (Colours::black.withAlpha (0.07f));
            g.fillPath (s);
        }
    }
    Path ellipsePath (Point<float> c, float rx, float ry) { Path p; p.addEllipse (c.x - rx, c.y - ry, rx * 2, ry * 2); return p; }
    ColourGradient metalGrad (Rectangle<float> b)
    {
        ColourGradient m (Colour (0xfff4f6f8), b.getX(), b.getY(), Colour (0xff6d737a), b.getRight(), b.getBottom(), false);
        m.addColour (0.35, Colour (0xffc3c8ce));
        m.addColour (0.5, Colour (0xfff9fbfc));
        m.addColour (0.62, Colour (0xff9aa0a7));
        return m;
    }
}

void DeckView::renderBackdrop()
{
    // neutral backdrop in the plug-in colours (warm grey, soft light in the centre, vignette)
    const int w = jmax (1, getWidth()), h = jmax (1, getHeight());
    backdrop = Image (Image::RGB, w, h, true);
    Graphics g (backdrop);
    const float W = (float) w, H = (float) h;
    ColourGradient bg (theme::bg.brighter (0.25f), W * 0.5f, H * 0.42f, theme::bg.darker (0.35f), 0.0f, 0.0f, true);
    g.setGradientFill (bg);
    g.fillRect (0.0f, 0.0f, W, H);
}

void DeckView::paint (Graphics& g)
{
    if (backdrop.isValid()) g.drawImageAt (backdrop, 0, 0); else g.fillAll (theme::bg);
    if (type == 1) drawTape (g); else drawVinyl (g);
    // touch indicator where the deck is being played
    if (grip != none)
    {
        g.setColour (theme::red.withAlpha (0.35f)); g.fillEllipse (lastPos.x - 18, lastPos.y - 18, 36, 36);
        g.setColour (theme::red); g.drawEllipse (lastPos.x - 18, lastPos.y - 18, 36, 36, 2.0f);
    }
    // status LCD
    auto lcd = Rectangle<float> (deckArea.getRight() - 236.0f, deckArea.getY() + 4.0f, 220.0f, 48.0f);
    g.setColour (Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (lcd.translated (4, 5), 5.0f);
    drawLcd (g, lcd);
    g.setColour (ink); g.drawRoundedRectangle (lcd, 4.0f, 2.5f);
    g.setColour (theme::lcdInk);
    g.setFont (theme::mono (14.0f, true));
    g.drawText (type == 2 ? String (proc.apvts.getRawParameterValue ("vinyl45")->load() > 0.5f ? "45" : "33") + " RPM  " + String (shownSpeed, 3) + "x"
                          : "SPEED " + String (shownSpeed, 2) + "x", lcd.reduced (10, 4).removeFromTop (20), Justification::centredLeft);
    g.setFont (theme::mono (12.0f));
    const int act = proc.deck.currentAction.load();
    g.drawText (grip != none ? String ("HAND ON THE ") + (type == 1 ? "REEL" : "RECORD") : String (act != 0 ? actionName (act) : (proc.isTransportRunning() ? "PLAY" : "WAITING FOR PLAY")),
                lcd.reduced (10, 4).removeFromBottom (18), Justification::centredLeft);
    const String help = type == 1 ? "Drag the RIGHT reel up / down  -  shake the LEFT reel  -  click between the reels for a random move"
                                  : "Grab the record to scratch (hold still = stop)  -  START/STOP, 33/45 and the pitch fader work like a real deck";
    auto hr = Rectangle<float> (deckArea.getX(), deckArea.getBottom() + 2.0f, deckArea.getWidth(), 16.0f);
    g.setFont (theme::font (12.5f, true));
    g.setColour (Colours::black.withAlpha (0.6f)); g.drawText (help, hr.translated (1, 1), Justification::centred);
    g.setColour (Colour (0xfffff1d8)); g.drawText (help, hr, Justification::centred);
}

void DeckView::drawReel (Graphics& g, Point<float> c, float r, float angle, float pack)
{
    const float ow = jmax (2.0f, r * 0.022f);
    // tape pack: glossy brown with a sheen
    auto packP = ellipsePath (c, r * pack, r * pack);
    ColourGradient pg (Colour (0xff6e4a30), c.x - r * pack * 0.5f, c.y - r * pack * 0.6f, Colour (0xff24160c), c.x + r * pack, c.y + r * pack, true);
    inked (g, packP, pg, ow * 0.8f);
    for (float k = 0.42f; k < pack; k += 0.05f) { g.setColour (Colours::black.withAlpha (0.2f)); g.drawEllipse (c.x - r * k, c.y - r * k, r * k * 2, r * k * 2, 0.7f); }
    g.setColour (Colours::white.withAlpha (0.18f));
    { Path sh; sh.addCentredArc (c.x, c.y, r * pack * 0.8f, r * pack * 0.8f, 0.0f, -2.4f, -1.2f, true); g.strokePath (sh, PathStrokeType (r * 0.05f)); }
    // metal flange with three cut-outs
    Path flange = ellipsePath (c, r, r);
    for (int i = 0; i < 3; ++i)
    {
        const float a = angle + (float) i * kTwoPi / 3.0f;
        Path hole;
        hole.addPieSegment (c.x - r * 0.86f, c.y - r * 0.86f, r * 1.72f, r * 1.72f, a - 0.55f, a + 0.55f, 0.38f);
        flange.addPath (hole);
    }
    flange.setUsingNonZeroWinding (false);
    softShadow (g, ellipsePath (c, r, r), { r * 0.05f, r * 0.08f }, r * 0.12f);
    g.setOpacity (0.95f);
    inked (g, flange, metalGrad ({ c.x - r, c.y - r, r * 2, r * 2 }), ow);
    g.setOpacity (1.0f);
    // rim highlight + specular streak
    g.setColour (Colours::white.withAlpha (0.7f));
    { Path sp; sp.addCentredArc (c.x, c.y, r * 0.95f, r * 0.95f, 0.0f, -1.3f, -0.4f, true); g.strokePath (sp, PathStrokeType (r * 0.025f, PathStrokeType::curved, PathStrokeType::rounded)); }
    // hub: round label with the Groovulation logo, turning with the reel
    const float hr = r * 0.3f;
    Graphics::ScopedSaveState ss (g);
    g.addTransform (AffineTransform::rotation (angle, c.x, c.y));
    inked (g, ellipsePath (c, hr * 1.1f, hr * 1.1f), Colour (0xff2a2c30), ow);
    inked (g, ellipsePath (c, hr, hr), theme::panel, ow * 0.6f);
    g.setColour (theme::red); g.drawEllipse (c.x - hr * 0.95f, c.y - hr * 0.95f, hr * 1.9f, hr * 1.9f, hr * 0.06f);
    drawGroovLogo (g, Rectangle<float> (c.x - hr * 0.82f, c.y - hr * 0.82f, hr * 1.64f, hr * 1.64f), proc.masterLevel(), logoPhase);
}

void DeckView::drawTape (Graphics& g)
{
    auto body = deckArea.reduced (8.0f, 2.0f);
    const float ow = 3.0f;
    Path bodyP; bodyP.addRoundedRectangle (body, 16.0f);
    softShadow (g, bodyP, { 10.0f, 16.0f }, 26.0f);
    ColourGradient bg (Colour (0xfff0e7d2), body.getX(), body.getY(), Colour (0xffa8987b), body.getX(), body.getBottom(), false);
    bg.addColour (0.08, Colour (0xfffff8e8));
    inked (g, bodyP, bg, ow);
    // bevel + lower control strip
    g.setColour (Colours::white.withAlpha (0.55f)); g.drawRoundedRectangle (body.reduced (5.0f), 12.0f, 2.0f);
    auto strip = body.withTop (body.getBottom() - body.getHeight() * 0.2f).reduced (14.0f, 10.0f);
    Path stripP; stripP.addRoundedRectangle (strip, 8.0f);
    ColourGradient sg (Colour (0xff3b3631), strip.getX(), strip.getY(), Colour (0xff1c1916), strip.getX(), strip.getBottom(), false);
    inked (g, stripP, sg, 2.0f);
    for (auto sx : { body.getX() + 16.0f, body.getRight() - 16.0f })
        for (auto sy : { body.getY() + 16.0f, body.getBottom() - 16.0f })
        { inked (g, ellipsePath ({ sx, sy }, 5, 5), Colour (0xffb9bec4), 1.5f); g.setColour (ink); g.drawLine (sx - 3, sy, sx + 3, sy, 1.5f); }
    // brand plate
    auto plate = Rectangle<float> (body.getX() + 22.0f, body.getY() + 14.0f, 250.0f, 30.0f);
    Path plateP; plateP.addRoundedRectangle (plate, 5.0f);
    inked (g, plateP, metalGrad (plate), 2.0f);
    g.setColour (ink); g.setFont (theme::nameFont (16.0f).withExtraKerningFactor (0.08f));
    g.drawText ("GROOVULATION  TAPE 60", plate, Justification::centred);

    // tape path
    const float yHead = body.getBottom() - body.getHeight() * 0.27f;
    const Point<float> guideL { reelL.x + reelRadius * 0.25f, yHead - 6.0f }, guideR { reelR.x - reelRadius * 0.25f, yHead - 6.0f };
    const Point<float> head { (reelL.x + reelR.x) * 0.5f, yHead };
    Path tape;
    tape.startNewSubPath (reelL + Point<float> (reelRadius * packL * 0.55f, reelRadius * packL * 0.83f));
    tape.lineTo (guideL); tape.lineTo (head + Point<float> (-64.0f, 6.0f)); tape.lineTo (head + Point<float> (64.0f, 6.0f));
    tape.lineTo (guideR); tape.lineTo (reelR + Point<float> (-reelRadius * (1.35f - packL) * 0.55f, reelRadius * (1.35f - packL) * 0.83f));
    g.setColour (ink); g.strokePath (tape, PathStrokeType (5.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Colour (0xff6b4529)); g.strokePath (tape, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
    for (auto p : { guideL, guideR })
    {
        inked (g, ellipsePath (p, 11, 11), metalGrad ({ p.x - 11, p.y - 11, 22, 22 }), 2.0f);
        inked (g, ellipsePath (p, 3.5f, 3.5f), Colour (0xff2b2d31), 1.0f);
    }
    // head block
    auto hb = Rectangle<float> (head.x - 78.0f, head.y + 8.0f, 156.0f, 40.0f);
    Path hbP; hbP.addRoundedRectangle (hb, 6.0f);
    ColourGradient hg (Colour (0xff4a4540), hb.getX(), hb.getY(), Colour (0xff1e1b18), hb.getX(), hb.getBottom(), false);
    inked (g, hbP, hg, 2.5f);
    for (int i = 0; i < 3; ++i)
    {
        auto hd = Rectangle<float> (hb.getX() + 20.0f + i * 44.0f, hb.getY() - 10.0f, 28.0f, 18.0f);
        Path hdP; hdP.addRoundedRectangle (hd, 5.0f);
        inked (g, hdP, metalGrad (hd), 2.0f);
    }
    inked (g, ellipsePath (head + Point<float> (92.0f, 4.0f), 9, 9), ColourGradient (theme::red.brighter (0.4f), head.x + 86, head.y, theme::red.darker (0.5f), head.x + 100, head.y + 12, false), 2.0f);
    drawReel (g, reelL, reelRadius, angleL, packL);
    drawReel (g, reelR, reelRadius, angleR, 1.35f - packL);
    // VU meters
    const float lv = proc.masterLevel();
    for (int k = 0; k < 2; ++k)
    {
        auto vu = Rectangle<float> (strip.getX() + 16.0f + k * 98.0f, strip.getY() + 6.0f, 88.0f, strip.getHeight() - 12.0f);
        Path vuP; vuP.addRoundedRectangle (vu, 4.0f);
        ColourGradient vg (Colour (0xfffff3c9), vu.getX(), vu.getY(), Colour (0xffe1b964), vu.getX(), vu.getBottom(), false);
        inked (g, vuP, vg, 2.0f);
        const float db = Decibels::gainToDecibels (lv * (k == 0 ? 1.0f : 0.95f), -40.0f);
        const float a = -2.25f + 1.5f * jlimit (0.0f, 1.0f, (db + 40.0f) / 43.0f);
        const Point<float> pivot { vu.getCentreX(), vu.getBottom() - 4.0f };
        g.setColour (theme::red.withAlpha (0.8f)); g.fillRect (vu.getX() + vu.getWidth() * 0.66f, vu.getY() + 6.0f, vu.getWidth() * 0.22f, 3.0f);
        g.setColour (ink); g.drawLine (Line<float> (pivot, pivot + Point<float> (std::cos (a), std::sin (a)) * (vu.getHeight() - 8.0f)), 2.0f);
        g.setFont (theme::font (9.0f, true)); g.drawText ("VU", vu.withTrimmedTop (vu.getHeight() * 0.45f).withHeight (12.0f), Justification::centred);
    }
}

void DeckView::drawVinyl (Graphics& g)
{
    auto plinth = deckArea.reduced (4.0f, 0.0f);
    Path plP; plP.addRoundedRectangle (plinth, 14.0f);
    softShadow (g, plP, { 10.0f, 16.0f }, 26.0f);
    ColourGradient pg (Colour (0xff5c6168), plinth.getX(), plinth.getY(), Colour (0xff1f2226), plinth.getRight(), plinth.getBottom(), false);
    pg.addColour (0.15, Colour (0xff8a9098));
    inked (g, plP, pg, 3.0f);
    g.setColour (Colours::white.withAlpha (0.25f)); g.drawRoundedRectangle (plinth.reduced (5.0f), 11.0f, 1.5f);
    for (float y = plinth.getY() + 6; y < plinth.getBottom() - 6; y += 3.0f)
    { g.setColour (Colours::white.withAlpha (0.025f)); g.drawHorizontalLine ((int) y, plinth.getX() + 6, plinth.getRight() - 6); }

    const float R = platterR;
    // platter rim with strobe dots
    softShadow (g, ellipsePath (platter, R * 1.07f, R * 1.07f), { 6.0f, 10.0f }, 16.0f);
    inked (g, ellipsePath (platter, R * 1.07f, R * 1.07f), metalGrad ({ platter.x - R, platter.y - R, R * 2, R * 2 }), 3.0f);
    for (int i = 0; i < 90; ++i)
    {
        const float a = recordAngle + (float) i * kTwoPi / 90.0f;
        const auto p = platter + Point<float> (std::cos (a), std::sin (a)) * R * 1.035f;
        g.setColour (ink); g.fillEllipse (p.x - 1.6f, p.y - 1.6f, 3.2f, 3.2f);
    }
    // record: deep black, grooves, glossy highlights
    ColourGradient rg (Colour (0xff2a2a2e), platter.x - R * 0.4f, platter.y - R * 0.5f, Colour (0xff050506), platter.x + R, platter.y + R, true);
    inked (g, ellipsePath (platter, R, R), rg, 2.5f);
    for (float k = 0.37f; k < 0.97f; k += 0.016f)
    {
        g.setColour (Colours::white.withAlpha (std::fmod (k * 41.0f, 1.0f) > 0.5f ? 0.06f : 0.025f));
        g.drawEllipse (platter.x - R * k, platter.y - R * k, R * 2 * k, R * 2 * k, 1.0f);
    }
    for (auto ang : { -0.62f, 2.52f })
    {
        Path sheen;
        sheen.addPieSegment (platter.x - R * 0.96f, platter.y - R * 0.96f, R * 1.92f, R * 1.92f, ang - 0.32f, ang + 0.32f, 0.4f);
        ColourGradient sg (Colours::white.withAlpha (0.22f), platter.x + std::sin (ang) * R * 0.7f, platter.y - std::cos (ang) * R * 0.7f,
                           Colours::transparentWhite, platter.x, platter.y, true);
        g.setGradientFill (sg); g.fillPath (sheen);
    }
    // label with the logo (rotates with the record)
    const float L = R * 0.34f;
    {
        Graphics::ScopedSaveState ss (g);
        g.addTransform (AffineTransform::rotation (recordAngle, platter.x, platter.y));
        inked (g, ellipsePath (platter, L, L), ColourGradient (theme::panel.brighter (0.1f), platter.x - L, platter.y - L, theme::panel.darker (0.15f), platter.x + L, platter.y + L, false), 2.0f);
        g.setColour (theme::red); g.drawEllipse (platter.x - L * 0.93f, platter.y - L * 0.93f, L * 1.86f, L * 1.86f, L * 0.06f);
        drawGroovLogo (g, Rectangle<float> (platter.x - L * 0.5f, platter.y - L * 0.66f, L, L), proc.masterLevel(), logoPhase);
        g.setColour (ink);
        g.setFont (theme::nameFont (L * 0.14f));
        g.drawText ("GROOVULATION", Rectangle<float> (platter.x - L, platter.y + L * 0.4f, L * 2, L * 0.2f), Justification::centred);
        g.setFont (theme::font (L * 0.1f, true));
        g.drawText ("33 1/3  -  SIDE A", Rectangle<float> (platter.x - L, platter.y + L * 0.6f, L * 2, L * 0.16f), Justification::centred);
    }
    inked (g, ellipsePath (platter, 5, 5), metalGrad ({ platter.x - 5, platter.y - 5, 10, 10 }), 1.5f);

    // tonearm
    const Point<float> base { plinth.getRight() - plinth.getWidth() * 0.16f, plinth.getY() + plinth.getHeight() * 0.2f };
    const Point<float> tip = platter + Point<float> (R * 0.55f, -R * 0.45f);
    softShadow (g, ellipsePath (base, 38, 38), { 4.0f, 8.0f }, 10.0f);
    inked (g, ellipsePath (base, 38, 38), ColourGradient (Colour (0xff3b3f44), base.x - 38, base.y - 38, Colour (0xff121315), base.x + 38, base.y + 38, false), 2.5f);
    inked (g, ellipsePath (base, 24, 24), metalGrad ({ base.x - 24, base.y - 24, 48, 48 }), 2.0f);
    { Path cw; cw.addRoundedRectangle (base.x - 18, base.y - 72, 36, 30, 6.0f); inked (g, cw, metalGrad ({ base.x - 18, base.y - 72, 36, 30 }), 2.0f); }
    Path arm;
    arm.startNewSubPath (base);
    arm.quadraticTo (base.x - 10.0f, (base.y + tip.y) * 0.5f + 40.0f, tip.x + 14.0f, tip.y - 10.0f);
    g.setColour (ink); g.strokePath (arm, PathStrokeType (10.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Colour (0xffe9ecef)); g.strokePath (arm, PathStrokeType (6.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Colours::white); g.strokePath (arm, PathStrokeType (1.5f, PathStrokeType::curved, PathStrokeType::rounded));
    {
        Graphics::ScopedSaveState hs (g);
        g.addTransform (AffineTransform::rotation (-0.5f, tip.x + 14.0f, tip.y - 10.0f));
        Path shell; shell.addRoundedRectangle (tip.x + 2.0f, tip.y - 14.0f, 24.0f, 36.0f, 4.0f);
        inked (g, shell, Colour (0xff222326), 2.0f);
        g.setColour (theme::red); g.fillRect (tip.x + 8.0f, tip.y + 16.0f, 12.0f, 7.0f);
    }

    // start / stop, 33 / 45, pitch fader
    Path ssP; ssP.addRoundedRectangle (startStopBtn, 7.0f);
    softShadow (g, ssP, { 3.0f, 5.0f }, 6.0f);
    inked (g, ssP, metalGrad (startStopBtn), 2.5f);
    g.setColour (ink); g.setFont (theme::font (12.0f, true));
    g.drawText ("START / STOP", startStopBtn, Justification::centred);
    const bool is45 = proc.apvts.getRawParameterValue ("vinyl45")->load() > 0.5f;
    const bool motorOn = proc.apvts.getRawParameterValue ("vinylMotor")->load() > 0.5f;
    g.setColour (motorOn ? Colour (0xffff5a3c) : Colour (0xff6a2a20));
    g.fillEllipse (startStopBtn.getRight() - 14.0f, startStopBtn.getY() + 6.0f, 7.0f, 7.0f);   // motor lamp
    rpm33 = Rectangle<float> (startStopBtn.getRight() + 16.0f, startStopBtn.getBottom() - 24.0f, 44.0f, 22.0f);
    rpm45 = rpm33.translated (52.0f, 0.0f);
    for (int k = 0; k < 2; ++k)
    {
        auto b = k == 0 ? rpm33 : rpm45;
        const bool on = (k == 1) == is45;
        Path bp; bp.addRoundedRectangle (b, 4.0f);
        inked (g, bp, on ? Colour (0xff4a4d52) : Colour (0xff2b2d30), 2.0f);
        g.setColour (on ? Colour (0xffff5a3c) : Colour (0xff6d7177)); g.fillEllipse (b.getX() + 5, b.getCentreY() - 3, 6, 6);
        g.setColour (Colour (0xffe7e9eb)); g.setFont (theme::font (11.0f, true));
        g.drawText (k == 0 ? "33" : "45", b.withTrimmedLeft (10.0f), Justification::centred);
    }
    faderArea = Rectangle<float> (plinth.getRight() - 66.0f, plinth.getBottom() - plinth.getHeight() * 0.55f, 28.0f, plinth.getHeight() * 0.45f);
    const auto fd = faderArea;
    Path slot; slot.addRoundedRectangle (fd.withSizeKeepingCentre (9.0f, fd.getHeight()), 4.0f);
    inked (g, slot, Colour (0xff0d0e10), 2.0f);
    for (int t = -4; t <= 4; ++t)
    {
        const float ty = fd.getCentreY() + (float) t / 4.0f * fd.getHeight() * 0.45f;
        g.setColour (t == 0 ? Colour (0xff7cd06a) : Colour (0xffb7bcc2)); g.drawHorizontalLine ((int) ty, fd.getRight() + 2, fd.getRight() + (t == 0 ? 12.0f : 7.0f));
    }
    const float pitchPct = proc.apvts.getRawParameterValue ("vinylPitch")->load();
    const float fy = fd.getCentreY() + pitchPct / 8.0f * fd.getHeight() * 0.45f;
    Path cap; cap.addRoundedRectangle (fd.getX(), fy - 10.0f, 28.0f, 20.0f, 4.0f);
    inked (g, cap, metalGrad ({ fd.getX(), fy - 10.0f, 28.0f, 20.0f }), 2.0f);
    g.setColour (Colour (0xffe7e9eb)); g.setFont (theme::font (10.0f, true));
    g.drawText ((pitchPct >= 0 ? "+" : "") + String (pitchPct, 2) + "%", Rectangle<float> (fd.getX() - 30.0f, fd.getBottom() + 6.0f, 88.0f, 14.0f), Justification::centred);
    g.setColour (theme::red); g.drawHorizontalLine ((int) fy, fd.getX() + 4, fd.getRight() - 4);
}

//==============================================================================
DeckWindow::DeckWindow (GroovulationProcessor& p, int type, std::function<void()> onClose)
    : DocumentWindow (type == 1 ? "Groovulation - Tape Machine" : "Groovulation - Vinyl", theme::bg, DocumentWindow::closeButton, true),
      proc (p), onCloseFn (std::move (onClose))
{
    if (auto* prm = proc.apvts.getParameter ("deck")) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) type));
    setUsingNativeTitleBar (false);
    setTitleBarHeight (26);
    setContentOwned (new DeckView (p, type), false);
    setResizable (true, true);
    setResizeLimits (640, 460, 2000, 1400);
    centreWithSize (980, 680);
    setAlwaysOnTop (true);
    setVisible (true);
}

void DeckWindow::closeButtonPressed()
{
    if (auto* prm = proc.apvts.getParameter ("deck")) prm->setValueNotifyingHost (0.0f);
    if (onCloseFn) onCloseFn();
}
