#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Small animated mascot drawn with paths: a stylised, generic purple kaiju (dorsal plates, tail, two red eyes)
// playing a little keyboard with white and black keys.
// t = time in seconds, level = output level 0..1 (keys light up, the jaw roars with the sound).
inline void drawKaiju (juce::Graphics& g, juce::Rectangle<float> r, float t, float level)
{
    using namespace juce;
    const float s = jmin (r.getWidth(), r.getHeight()) / 100.0f;
    auto P = [&] (float x, float y) { return Point<float> (r.getX() + x * s, r.getY() + y * s); };
    auto R = [&] (float x, float y, float w, float h) { return Rectangle<float> (P (x, y).x, P (x, y).y, w * s, h * s); };
    const Colour body (0xff7b4fc9), dark (0xff47288a), belly (0xffcdb6f2), plate (0xffe6d8ff), eye (0xffff2a2a);
    const float bob = std::sin (t * 6.0f) * 1.6f * (0.3f + level);
    const float roar = jlimit (0.0f, 1.0f, level * 1.6f);

    // tail
    Path tail;
    tail.startNewSubPath (P (26, 70)); tail.quadraticTo (P (4, 76), P (2, 58 + std::sin (t * 3.0f) * 3.0f));
    tail.quadraticTo (P (10, 70), P (28, 62)); tail.closeSubPath();
    g.setColour (body); g.fillPath (tail);
    // body
    g.setColour (body); g.fillRoundedRectangle (R (24, 40 + bob, 30, 42), 12 * s);
    g.setColour (belly); g.fillRoundedRectangle (R (34, 48 + bob, 14, 30), 6 * s);
    g.setColour (dark.withAlpha (0.5f));
    for (int k = 0; k < 4; ++k) g.drawLine (P (35, 54 + bob + k * 6.0f).x, P (35, 54 + bob + k * 6.0f).y, P (47, 54 + bob + k * 6.0f).x, P (47, 54 + bob + k * 6.0f).y, 0.8f * s);
    // dorsal plates
    g.setColour (plate);
    for (int i = 0; i < 4; ++i)
    {
        Path pl; const float px = 25.0f - (float) i * 1.5f, py = 44.0f + (float) i * 8.0f + bob;
        pl.addTriangle (P (px, py), P (px - 8, py + 3), P (px, py + 7));
        g.fillPath (pl);
    }
    // head
    const float hy = 20 + bob * 1.5f;
    g.setColour (body); g.fillRoundedRectangle (R (32, hy, 32, 22), 9 * s);
    Path plateH; plateH.addTriangle (P (35, hy + 1), P (39, hy - 8), P (43, hy + 1)); plateH.addTriangle (P (43, hy + 1), P (47, hy - 10), P (51, hy + 1));
    g.setColour (plate); g.fillPath (plateH);
    // jaw (roars with the level)
    g.setColour (dark); g.fillRoundedRectangle (R (42, hy + 15 + roar * 5, 23, 8), 4 * s);
    if (roar > 0.05f) { g.setColour (Colour (0xffd94f7d)); g.fillRect (R (46, hy + 17, 15, roar * 4)); }
    g.setColour (Colours::white);
    for (int k = 0; k < 3; ++k) g.fillRect (R (46 + k * 5.0f, hy + 16, 2, 2));
    // two red eyes (3/4 view), blinking, with a glow
    const bool blink = std::fmod (t, 3.7f) > 3.58f;
    for (const float ex : { 46.0f, 55.5f })
    {
        const auto e = R (ex, hy + 3.5f, 7.5f, blink ? 1.2f : 7.5f);
        if (! blink) { g.setColour (eye.withAlpha (0.35f)); g.fillEllipse (e.expanded (1.6f * s)); }
        g.setColour (eye); g.fillEllipse (e);
        if (! blink) { g.setColour (Colour (0xff400000)); g.fillEllipse (R (ex + 2.8f, hy + 5.3f, 2.4f, 3.8f)); g.setColour (Colours::white.withAlpha (0.8f)); g.fillEllipse (R (ex + 1.0f, hy + 4.8f, 1.4f, 1.4f)); }
    }
    // keyboard: 7 white keys + 5 black keys
    const auto kb = R (46, 70, 52, 18);
    g.setColour (Colour (0xff1c1c1c)); g.fillRoundedRectangle (kb.expanded (1.5f * s), 2 * s);
    const int nk = 7;
    const float kw = kb.getWidth() / nk;
    const int lit = (int) std::fmod (t * 8.0f, 12.0f);
    static const int whiteNote[7] = { 0, 2, 4, 5, 7, 9, 11 };
    for (int k = 0; k < nk; ++k)
    {
        auto key = Rectangle<float> (kb.getX() + (float) k * kw + 0.4f, kb.getY(), kw - 0.8f, kb.getHeight());
        g.setColour ((whiteNote[k] == lit && level > 0.02f) ? Colour (0xffb98cff) : Colours::white);
        g.fillRect (key);
    }
    static const int blackAfter[5] = { 0, 1, 3, 4, 5 };
    static const int blackNote[5] = { 1, 3, 6, 8, 10 };
    for (int b = 0; b < 5; ++b)
    {
        auto key = Rectangle<float> (kb.getX() + (float) (blackAfter[b] + 1) * kw - kw * 0.3f, kb.getY(), kw * 0.6f, kb.getHeight() * 0.6f);
        g.setColour ((blackNote[b] == lit && level > 0.02f) ? Colour (0xff7a3fe0) : Colours::black);
        g.fillRect (key);
    }
    // arms: alternate on the keys
    const float a1 = std::sin (t * 10.0f) > 0 ? 0.0f : 3.0f, a2 = 3.0f - a1;
    g.setColour (body);
    Path arm1; arm1.startNewSubPath (P (48, 54 + bob)); arm1.lineTo (P (58, 68 - a1));
    Path arm2; arm2.startNewSubPath (P (52, 52 + bob)); arm2.lineTo (P (80, 68 - a2));
    g.strokePath (arm1, PathStrokeType (4.5f * s, PathStrokeType::curved, PathStrokeType::rounded));
    g.strokePath (arm2, PathStrokeType (4.5f * s, PathStrokeType::curved, PathStrokeType::rounded));
    // feet
    g.setColour (dark);
    g.fillRoundedRectangle (R (26, 80, 12, 6), 3 * s);
    g.fillRoundedRectangle (R (40, 80, 12, 6), 3 * s);
}
