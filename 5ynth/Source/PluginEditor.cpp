#include "PluginEditor.h"
#include "BinaryData.h"

using namespace params;
using skins::Skin;

// =====================================================================================================
//  Fonts (Inter, SIL OFL: the closest freely licensed match for Ableton Live's interface font) + relief
// =====================================================================================================
juce::Typeface::Ptr ui::typeface (bool bold)
{
    static juce::Typeface::Ptr reg = juce::Typeface::createSystemTypefaceFor (BinaryData::InterRegular_ttf, BinaryData::InterRegular_ttfSize);
    static juce::Typeface::Ptr semi = juce::Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_ttf, BinaryData::InterSemiBold_ttfSize);
    return bold ? semi : reg;
}
juce::Font ui::font (float h, bool bold) { return juce::Font (juce::FontOptions (typeface (bold)).withHeight (h)); }

// =====================================================================================================
//  5ynth logo
// =====================================================================================================
static void fillSteel (juce::Graphics& g, const juce::Path& q)   // brushed metal: diagonal light / dark gradient
{
    using namespace juce;
    const auto b = q.getBounds();
    ColourGradient gr (Colour (0xffe8e8e8), b.getX(), b.getY(), Colour (0xff8a8a8a), b.getRight(), b.getBottom(), false);
    gr.addColour (0.45, Colour (0xffbcbcbc)); gr.addColour (0.7, Colour (0xffd4d4d4));
    g.setGradientFill (gr); g.fillPath (q);
}

// The "5" in design units (cap band y 52..345, stroke 46): top bar drawn as a stylised keyboard, straight stem,
// five glowing bars (5 synths) cut flush with the bowl, round bowl with horizontal ends
struct FiveGeometry
{
    juce::Path metal, bars, keyMarks;   // keyMarks: dark black keys / key gaps over the top bar
    FiveGeometry()
    {
        using namespace juce;
        metal.addRectangle (7, 52, 226, 46);    // top bar = keyboard, flush with the end of the bars
        metal.addRectangle (7, 52, 51, 161);    // stem
        for (int i = 0; i < 5; ++i) bars.addRectangle (66, 120.0f + i * 19.5f, 233 - 66, 12);
        Path bowl;
        const float cx = 120, cy = 232, a0 = MathConstants<float>::halfPi, a1 = 1.5f * MathConstants<float>::pi;
        bowl.addCentredArc (cx, cy, 113, 113, 0, a0, a1, true);
        bowl.addCentredArc (cx, cy, 64, 60, 0, a1, a0, false);
        bowl.closeSubPath();
        metal.addPath (bowl);
        constexpr float kx = 7, kw = 226.0f / 11.0f;   // 11 white keys, black keys in groups of 2 and 3
        for (int k = 1; k < 11; ++k) keyMarks.addRectangle (kx + k * kw - 1.2f, 52, 2.4f, 46);
        for (int k : { 1, 2, 4, 5, 6, 8, 9 }) keyMarks.addRectangle (kx + k * kw - kw * 0.28f, 52, kw * 0.56f, 27);
    }
};

static void drawFive (juce::Graphics& g, const FiveGeometry& f, const juce::AffineTransform& T)
{
    using namespace juce;
    const float s = std::sqrt (std::abs (T.getDeterminant()));
    auto p = f.metal; p.applyTransform (T);
    fillSteel (g, p);
    auto km = f.keyMarks; km.applyTransform (T);
    g.setColour (Colour (0xff1c1c1c)); g.fillPath (km);
    auto bars = f.bars; bars.applyTransform (T);
    for (float rad : { 24.0f, 9.0f })   // glow
        DropShadow (Colour (0xffd8f0ff).withAlpha (rad > 15 ? 0.4f : 0.6f), jmax (1, (int) (rad * s)), {}).drawForPath (g, bars);
    const auto bb = bars.getBounds();
    ColourGradient gb (Colour (0xffd4dee6), bb.getX(), 0, Colour (0xffd4dee6), bb.getRight(), 0, false);
    gb.addColour (0.35, Colours::white); gb.addColour (0.75, Colour (0xfff2f6f8));
    g.setGradientFill (gb); g.fillPath (bars);
}

// "YNTH" drawn with the elements of the 5: straight bars of the same thickness, a round cup like its bowl;
// upright, on the 5's cap band (y 52..345). Proportions: cap height 293 (the 5), stroke 46 (the 5's bars), every letter as wide and tall as the 5 (226 x 293);
// optical spacing: more room next to closed / round forms, less around the open sides of T
static std::vector<juce::Path> wordLetters (float x0, float w)
{
    using namespace juce;
    constexpr float top = 52, bot = 345, t = 46;
    const float advance[4] = { 0, w + 38, w + 30, w + 30 };   // Y->N, N->T, T->H
    std::vector<Path> out;
    float x = x0;
    for (int k = 0; k < 4; ++k)
    {
        x += advance[k];
        const float mid = x + w * 0.5f;
        Path p;
        if (k == 0)        // Y: a cup like the bowl of the 5, on a straight stem
        {
            const float ro = w * 0.5f, ri = ro - t, cy = 150;
            p.addRectangle (x, top, t, cy - top);
            p.addRectangle (x + w - t, top, t, cy - top);
            Path cup;
            cup.addCentredArc (mid, cy, ro, ro, 0, MathConstants<float>::halfPi, 1.5f * MathConstants<float>::pi, true);
            cup.addCentredArc (mid, cy, ri, ri, 0, 1.5f * MathConstants<float>::pi, MathConstants<float>::halfPi, false);
            cup.closeSubPath();
            p.addPath (cup);
            p.addRectangle (mid - t * 0.5f, cy + ri + 2, t, bot - cy - ri - 2);
        }
        else if (k == 1)   // N
        {
            p.addRectangle (x, top, t, bot - top);
            p.addRectangle (x + w - t, top, t, bot - top);
            p.addQuadrilateral (x, top, x + 1.10f * t, top, x + w, bot, x + w - 1.10f * t, bot);
        }
        else if (k == 2)   // T
        {
            p.addRectangle (x, top, w, t);
            p.addRectangle (mid - t * 0.5f, top, t, bot - top);
        }
        else               // H: crossbar centred on the 5's group of bars (y 133..213)
        {
            p.addRectangle (x, top, t, bot - top);
            p.addRectangle (x + w - t, top, t, bot - top);
            p.addRectangle (x + t * 0.5f, 173 - t * 0.5f, w - t, t);
        }
        out.push_back (p);
    }
    return out;
}

// frame that follows the outer contour of a shape at a distance `gap` (holes ignored), `line` thick;
// rendered as an alpha mask at the device resolution and cached
static void drawContourFrame (juce::Graphics& g, const juce::Path& shape, const juce::AffineTransform& T, float gap, float line)
{
    using namespace juce;
    const auto area = shape.getBounds().expanded (gap + line + 4).transformedBy (T).getSmallestIntegerContainer().toFloat();
    const float ps = jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const int w = (int) std::ceil (area.getWidth() * ps), h = (int) std::ceil (area.getHeight() * ps);
    struct Cache { int w = 0, h = 0; float gap = 0, line = 0; Image img; };
    static Cache cache;
    if (cache.w != w || cache.h != h || cache.gap != gap || cache.line != line)
    {
        const auto M = T.translated (-area.getX(), -area.getY()).scaled (ps);
        // filled dilation of the shape by d, with the enclosed holes filled (flood fill of the outside from the border)
        auto filled = [&] (float d)
        {
            Path st; PathStrokeType (2 * d, PathStrokeType::curved, PathStrokeType::rounded).createStrokedPath (st, shape);
            st.addPath (shape); st.setUsingNonZeroWinding (true); st.applyTransform (M);
            Image im (Image::SingleChannel, w, h, true);
            { Graphics mg (im); mg.setColour (Colours::white); mg.fillPath (st); }
            Image::BitmapData bd (im, Image::BitmapData::readWrite);
            std::vector<uint8_t> out ((size_t) (w * h), 0);
            std::vector<int> stack;
            auto px = [&] (int x, int y) -> uint8& { return *bd.getPixelPointer (x, y); };
            for (int x = 0; x < w; ++x) { stack.push_back (x); stack.push_back ((h - 1) * w + x); }
            for (int y = 0; y < h; ++y) { stack.push_back (y * w); stack.push_back (y * w + w - 1); }
            while (! stack.empty())
            {
                const int i = stack.back(); stack.pop_back();
                if (out[(size_t) i]) continue;
                const int x = i % w, y = i / w;
                if (px (x, y) >= 128) continue;
                out[(size_t) i] = 1;
                if (x > 0) stack.push_back (i - 1);
                if (x < w - 1) stack.push_back (i + 1);
                if (y > 0) stack.push_back (i - w);
                if (y < h - 1) stack.push_back (i + w);
            }
            std::vector<uint8_t> a ((size_t) (w * h));
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    // inside or a hole: opaque; outside region: keep the antialiased edge
                    const uint8 v = px (x, y);
                    a[(size_t) (y * w + x)] = out[(size_t) (y * w + x)] ? v : (uint8) 255;
                }
            return a;
        };
        const auto inner = filled (gap), outer = filled (gap + line);
        Image img (Image::SingleChannel, w, h, true);
        Image::BitmapData bd (img, Image::BitmapData::writeOnly);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                *bd.getPixelPointer (x, y) = (uint8) jmax (0, (int) outer[(size_t) (y * w + x)] - (int) inner[(size_t) (y * w + x)]);
        cache = { w, h, gap, line, img };
    }
    ColourGradient gr (Colour (0xffaaaaaa), area.getX(), area.getY(), Colour (0xff5c5c5c), area.getRight(), area.getBottom(), false);
    gr.addColour (0.5, Colour (0xff8e8e8e));
    g.setGradientFill (gr);
    g.drawImage (cache.img, area, RectanglePlacement::stretchToFit, true);
}

void logo::drawWordmark (juce::Graphics& g, juce::Rectangle<float> r, juce::RectanglePlacement place)
{
    using namespace juce;
    constexpr float gap = 30, line = 8;                    // contour frame: distance from the letters, thickness
    FiveGeometry five;
    auto letters = wordLetters (233 + 44, 226);           // letters as wide as the 5; Y starts 44 after it (open at the top)
    Path shape (five.metal);                               // everything the frame goes around
    shape.addPath (five.bars);
    for (auto& l : letters) shape.addPath (l);
    const auto T = place.getTransformToFit (shape.getBounds().expanded (gap + line + 2), r);
    drawContourFrame (g, shape, T, gap, line);
    drawFive (g, five, T);
    for (auto& l : letters) { l.applyTransform (T); fillSteel (g, l); }   // each letter lit like the 5
}

void LogoSplash::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.92f));
    auto r = getLocalBounds().toFloat().withSizeKeepingCentre (760.0f, 300.0f);
    logo::drawWordmark (g, r.removeFromTop (250.0f));
    g.setColour (ui::dim); g.setFont (ui::font (13.0f));
    g.drawText ("TMFU audiovibe  v" + juce::String (JucePlugin_VersionString), r.removeFromTop (30.0f), juce::Justification::centred);
}

void ui::relief (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour base, float corner, bool raised)
{
    juce::ColourGradient gr (raised ? base.brighter (0.10f) : base.darker (0.25f), 0, r.getY(), raised ? base.darker (0.12f) : base.darker (0.05f), 0, r.getBottom(), false);
    g.setGradientFill (gr);
    g.fillRoundedRectangle (r, corner);
    g.setColour (juce::Colours::white.withAlpha (raised ? 0.10f : 0.03f));
    g.drawLine (r.getX() + corner, r.getY() + 0.5f, r.getRight() - corner, r.getY() + 0.5f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (raised ? 0.45f : 0.30f));
    g.drawRoundedRectangle (r, corner, 1.0f);
}

// =====================================================================================================
//  LookAndFeel
// =====================================================================================================
FivesynthLook::FivesynthLook() { setSkin (skins::find ("neutral-g", false)); }

juce::Typeface::Ptr FivesynthLook::getTypefaceForFont (const juce::Font& f) { return ui::typeface (f.isBold()); }

void FivesynthLook::setSkin (const Skin& s)
{
    skin = &s;
    setColour (juce::ComboBox::textColourId, s.text);
    setColour (juce::PopupMenu::backgroundColourId, ui::panel2);
    setColour (juce::PopupMenu::textColourId, ui::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, s.accent.withAlpha (0.45f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextButton::textColourOffId, s.text);
    setColour (juce::TextButton::textColourOnId, s.accent.getPerceivedBrightness() > 0.6f ? juce::Colours::black : juce::Colours::white);
    setColour (juce::Label::textColourId, s.text);
    setColour (juce::ListBox::backgroundColourId, ui::panel);
    setColour (juce::TextEditor::backgroundColourId, ui::panel2);
    setColour (juce::TextEditor::textColourId, ui::text);
    setColour (juce::TextEditor::outlineColourId, ui::line);
    setColour (juce::ScrollBar::thumbColourId, ui::track);
    setColour (juce::BubbleComponent::backgroundColourId, ui::header);
    setColour (juce::BubbleComponent::outlineColourId, ui::line);
    setColour (juce::TooltipWindow::textColourId, ui::text);
}

void FivesynthLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& sl)
{
    const auto& s = *skin;
    const float size = (float) juce::jmin (w, h) - 4.0f;
    const auto c = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const float r = size * 0.5f;
    const float ang = a0 + pos * (a1 - a0);
    const bool bip = sl.getMinimum() < 0 && sl.getMaximum() > 0;
    auto pointer = [&] (float from, float to, float thick, juce::Colour col)
    {
        juce::Path p;
        p.startNewSubPath (c.x + std::sin (ang) * r * from, c.y - std::cos (ang) * r * from);
        p.lineTo (c.x + std::sin (ang) * r * to, c.y - std::cos (ang) * r * to);
        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    auto shadow = [&] (float rr) { g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillEllipse (c.x - rr + 1.0f, c.y - rr + 2.0f, rr * 2, rr * 2); };
    auto ticks = [&] (int n, float rr, juce::Colour col)
    {
        g.setColour (col);
        for (int i = 0; i <= n; ++i) { const float a = a0 + (a1 - a0) * (float) i / (float) n; g.fillEllipse (c.x + std::sin (a) * rr - 0.9f, c.y - std::cos (a) * rr - 0.9f, 1.8f, 1.8f); }
    };
    switch (s.knobStyle)
    {
        case skins::Skirt:
        {
            ticks (10, r + 1.0f, s.label.withAlpha (0.8f));
            const float rs = r - 2.0f;
            shadow (rs);
            g.setColour (s.knob); g.fillEllipse (c.x - rs, c.y - rs, rs * 2, rs * 2);
            g.setColour (s.knob.brighter (0.3f));
            for (int i = 0; i < 24; ++i) { const float a = (float) i / 24.0f * juce::MathConstants<float>::twoPi + ang;
                g.drawLine (c.x + std::sin (a) * rs * 0.78f, c.y - std::cos (a) * rs * 0.78f, c.x + std::sin (a) * rs, c.y - std::cos (a) * rs, 1.0f); }
            const float rc = rs * 0.62f;
            juce::ColourGradient cg (s.cap.brighter (0.5f), c.x - rc, c.y - rc, s.cap.darker (0.6f), c.x + rc, c.y + rc, false);
            g.setGradientFill (cg); g.fillEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2);
            pointer (0.1f, 0.95f, 2.0f, juce::Colours::white);
            break;
        }
        case skins::Pointer:
        case skins::CapKnob:
        {
            ticks (10, r + 1.0f, s.label.withAlpha (0.7f));
            const float rs = r - 2.5f;
            shadow (rs);
            juce::ColourGradient kg (s.knob.brighter (0.45f), c.x, c.y - rs, s.knob.darker (0.4f), c.x, c.y + rs, false);
            g.setGradientFill (kg); g.fillEllipse (c.x - rs, c.y - rs, rs * 2, rs * 2);
            g.setColour (juce::Colours::white.withAlpha (0.12f)); g.drawEllipse (c.x - rs + 1, c.y - rs + 1, rs * 2 - 2, rs * 2 - 2, 1.0f);
            if (s.knobStyle == skins::CapKnob)
            {
                const float rc = rs * 0.5f;
                juce::ColourGradient cg (s.cap.brighter (0.3f), c.x, c.y - rc, s.cap.darker (0.3f), c.x, c.y + rc, false);
                g.setGradientFill (cg); g.fillEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2);
                pointer (0.55f, 0.95f, 2.0f, juce::Colours::white);
            }
            else pointer (0.2f, 0.95f, 2.2f, juce::Colours::white);
            break;
        }
        case skins::Membrane:
        {
            auto box = juce::Rectangle<float> (c.x - r, c.y - r * 0.62f, r * 2, r * 1.24f);
            ui::relief (g, box, s.lcd ? s.lcdBg : s.body2.darker (0.4f), 3.0f, false);
            auto bar = box.reduced (4.0f, box.getHeight() * 0.36f);
            g.setColour ((s.lcd ? s.lcdText : s.accent).withAlpha (0.25f)); g.fillRect (bar);
            g.setColour (s.lcd ? s.lcdText : s.accent);
            if (bip) { const float mid = bar.getCentreX(), px = bar.getX() + bar.getWidth() * pos; g.fillRect (juce::Rectangle<float> (juce::jmin (mid, px), bar.getY(), std::abs (px - mid) + 1.0f, bar.getHeight())); }
            else g.fillRect (bar.withWidth (juce::jmax (1.0f, bar.getWidth() * pos)));
            g.setColour (s.accent); g.fillRoundedRectangle (c.x - 6.0f, box.getBottom() + 2.0f, 12.0f, 3.0f, 1.0f);
            break;
        }
        default:   // Twinbox / Ableton arc knob with a little depth
        {
            const float lineW = juce::jmax (2.0f, r * 0.16f), arcR = r - lineW * 0.5f;
            juce::Path bgArc; bgArc.addCentredArc (c.x, c.y, arcR, arcR, 0, a0, a1, true);
            g.setColour (ui::track); g.strokePath (bgArc, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
            const float from = bip ? (a0 + a1) * 0.5f : a0;
            juce::Path val; val.addCentredArc (c.x, c.y, arcR, arcR, 0, juce::jmin (from, ang), juce::jmax (from, ang), true);
            g.setColour (s.accent); g.strokePath (val, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
            const float ir = arcR - lineW * 1.2f;
            shadow (ir);
            juce::ColourGradient kg (ui::panel2.brighter (0.25f), c.x, c.y - ir, ui::panel2.darker (0.2f), c.x, c.y + ir, false);
            g.setGradientFill (kg); g.fillEllipse (c.x - ir, c.y - ir, ir * 2, ir * 2);
            pointer (0.0f, ir / r * 0.95f, juce::jmax (1.5f, lineW * 0.6f), ui::text);
            break;
        }
    }
}

void FivesynthLook::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& sl)
{
    const auto& s = *skin;
    const float cx = (float) x + (float) w * 0.5f;
    ui::relief (g, juce::Rectangle<float> (cx - 2.5f, (float) y - 2.0f, 5.0f, (float) h + 4.0f), juce::Colours::black, 2.5f, false);
    g.setColour (s.label.withAlpha (0.5f));
    for (int i = 0; i <= 10; ++i) { const float ty = (float) y + (float) h * (float) i / 10.0f; g.drawHorizontalLine ((int) ty, cx - 11.0f, cx - 6.0f); g.drawHorizontalLine ((int) ty, cx + 6.0f, cx + 11.0f); }
    const float capH = 14.0f, capW = 22.0f;
    const int idx = (int) (std::hash<std::string>{} (sl.getName().toStdString()) % 4);
    auto cap = juce::Rectangle<float> (cx - capW * 0.5f, pos - capH * 0.5f, capW, capH);
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (cap.translated (0, 2.0f), 2.0f);
    ui::relief (g, cap, s.knob.brighter (0.15f), 2.0f, true);
    g.setColour (s.stripes[idx]); g.fillRect (cap.reduced (2.0f, 5.5f));
}

void FivesynthLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto& s = *skin;
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    ui::relief (g, r, on ? s.accent : s.body2.brighter (over ? 0.08f : 0.0f).darker (down ? 0.2f : 0.0f), 3.0f, ! down);
}

void FivesynthLook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (3, 1), juce::Justification::centred, 1, 0.8f);
}

void FivesynthLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& cb)
{
    const auto& s = *skin;
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    ui::relief (g, r, s.lcd ? s.lcdBg : s.body2.darker (0.3f), 3.0f, false);
    juce::Path p; const float ax = (float) w - 9.0f, ay = (float) h * 0.5f;
    p.addTriangle (ax - 3.5f, ay - 2.0f, ax + 3.5f, ay - 2.0f, ax, ay + 2.5f);
    g.setColour (s.lcd ? s.lcdText : s.accent); g.fillPath (p);
    cb.setColour (juce::ComboBox::textColourId, s.lcd ? s.lcdText : s.text);
}

// =====================================================================================================
//  FineSlider / Control
// =====================================================================================================
double FineSlider::snapValue (double v, DragMode)
{
    const double step = fader ? 0.001 : 0.01;
    return juce::jlimit (getMinimum(), getMaximum(), std::round (v / step) * step);
}

bool FineSlider::keyPressed (const juce::KeyPress& k)
{
    const int code = k.getKeyCode();
    const bool up = code == juce::KeyPress::upKey || code == juce::KeyPress::rightKey;
    const bool dn = code == juce::KeyPress::downKey || code == juce::KeyPress::leftKey;
    if (! up && ! dn) return Slider::keyPressed (k);
    double stepN = fader ? 0.001 : 0.01;
    if (k.getModifiers().isShiftDown()) stepN *= 0.1;
    const double p = juce::jlimit (0.0, 1.0, valueToProportionOfLength (getValue()) + (up ? stepN : -stepN));
    setValue (snapValue (proportionOfLengthToValue (p), notDragging), juce::sendNotificationSync);
    return true;
}

Control::Control (juce::AudioProcessorValueTreeState& st, int p, const juce::String& label) : pid (p), caption (label)
{
    const auto& d = def (p);
    if (d.kind == C)
    {
        combo.addItemList (d.choices, 1);
        addAndMakeVisible (combo);
        ca = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (st, d.id, combo);
        wide = true;
    }
    else if (d.kind == B)
    {
        isToggle = true;
        toggle.setClickingTogglesState (true);
        toggle.setButtonText (label);
        addAndMakeVisible (toggle);
        ba = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (st, d.id, toggle);
    }
    else
    {
        knob.setName (d.id);
        knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        knob.setPopupDisplayEnabled (true, true, nullptr);
        knob.setMouseDragSensitivity (400);
        knob.setVelocityModeParameters (0.5, 1, 0.02, false, juce::ModifierKeys::shiftModifier);
        knob.setWantsKeyboardFocus (true);
        addAndMakeVisible (knob);
        sa = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (st, d.id, knob);
        knob.setDoubleClickReturnValue (true, (double) d.def);
    }
}

void Control::setSkin (const Skin& s)
{
    if (sa != nullptr)
    {
        isFader = s.knobStyle == skins::Fader && def (pid).fader;
        knob.fader = isFader;
        knob.setSliderStyle (isFader ? juce::Slider::LinearVertical : juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setMouseDragSensitivity (isFader ? 1000 : 400);
    }
    repaint();
}

void Control::resized()
{
    auto r = getLocalBounds();
    if (combo.isVisible()) combo.setBounds (r.removeFromTop (r.getHeight() - 16).withSizeKeepingCentre (r.getWidth() - 4, 22));
    else if (toggle.isVisible()) toggle.setBounds (r.withSizeKeepingCentre (r.getWidth() - 6, 22));
    else knob.setBounds (r.removeFromTop (r.getHeight() - 15).reduced (2, isFader ? 6 : 0));
}

void Control::paint (juce::Graphics& g)
{
    if (toggle.isVisible()) return;
    auto* lf = dynamic_cast<FivesynthLook*> (&getLookAndFeel());
    g.setColour (lf && lf->skin ? lf->skin->label : ui::dim);
    g.setFont (ui::font (10.0f, true));
    g.drawFittedText (caption.toUpperCase(), getLocalBounds().removeFromBottom (15), juce::Justification::centred, 1, 0.8f);
}

// =====================================================================================================
//  FM algorithm diagram
// =====================================================================================================
void AlgDiagram::paint (juce::Graphics& g)
{
    const auto& al = FmAlgorithms::get().a[juce::jlimit (0, 31, alg)];
    // depth: carriers = 0, a modulator sits one row above the deepest operator it modulates
    int depth[6] {}, target[6];
    for (int o = 0; o < 6; ++o) { target[o] = -1; for (int t = 0; t < 6; ++t) if ((al.mods[t] >> o) & 1) target[o] = t; }
    for (int pass = 0; pass < 6; ++pass)
        for (int o = 0; o < 6; ++o) if (target[o] >= 0) depth[o] = depth[target[o]] + 1;
    int maxD = 0; for (int o = 0; o < 6; ++o) maxD = juce::jmax (maxD, depth[o]);
    // columns: carriers left to right, modulators above their target
    float colX[6] {}; float nextCol = 0;
    for (int o = 0; o < 6; ++o) if (depth[o] == 0) colX[o] = nextCol++;
    for (int d = 1; d <= maxD; ++d)
    {
        int used[6] {};
        for (int o = 0; o < 6; ++o) if (depth[o] == d)
        {
            const int t = target[o];
            colX[o] = colX[t] + (float) used[t] * 1.0f;
            if (used[t] > 0) nextCol = juce::jmax (nextCol, colX[o] + 1);
            used[t]++;
        }
    }
    float maxC = 0; for (int o = 0; o < 6; ++o) maxC = juce::jmax (maxC, colX[o]);
    auto r = getLocalBounds().toFloat().reduced (4);
    const float bw = juce::jmin (26.0f, r.getWidth() / (maxC + 1.5f)), bh = juce::jmin (18.0f, r.getHeight() / ((float) maxD + 1.6f));
    auto pos = [&] (int o) { return juce::Point<float> (r.getX() + (colX[o] + 0.5f) * r.getWidth() / (maxC + 1.0f), r.getBottom() - bh * 0.6f - (float) depth[o] * (r.getHeight() - bh) / (float) juce::jmax (1, maxD)); };
    g.setColour (txt.withAlpha (0.6f));
    for (int o = 0; o < 6; ++o)
        for (int t = 0; t < 6; ++t) if ((al.mods[t] >> o) & 1) g.drawLine (juce::Line<float> (pos (o), pos (t)), 1.5f);
    for (int o = 0; o < 6; ++o)
    {
        auto c = pos (o);
        auto box = juce::Rectangle<float> (bw, bh).withCentre (c);
        ui::relief (g, box, ((al.carriers >> o) & 1) ? col : col.darker (0.6f), 3.0f, true);
        g.setColour (juce::Colours::white); g.setFont (ui::font (11.0f, true));
        g.drawText (juce::String (o + 1), box, juce::Justification::centred);
        if (o == al.fb) { g.setColour (txt); g.drawRect (box.expanded (2.0f), 1.0f); }
    }
    g.setColour (txt); g.setFont (ui::font (10.0f, true));
    g.drawText ("ALG " + juce::String (alg + 1), getLocalBounds().removeFromTop (12), juce::Justification::topLeft);
}

// =====================================================================================================
//  ModulePanel
// =====================================================================================================
static constexpr int cellW = 52, knobH = 62, faderH = 128, grpHead = 16, panelHead = 52;

ModulePanel::ModulePanel (FivesynthProcessor& p, int m) : proc (p), module (m)
{
    setLookAndFeel (&look);
    auto M = [m] (int s) { return mod (m, s); };
    groups = {
        { "PITCH", { { M (mOct), "Oct" }, { M (mSemi), "Semi" }, { M (mFine), "Fine" } } },
        // analog page (engine 0)
        { "OSC 1", { { M (mO1Wave), "Wave" }, { M (mO1Pw), "PW" }, { M (mO1Lvl), "Level" } }, 0, 0 },
        { "OSC 2", { { M (mO2Wave), "Wave" }, { M (mO2Oct), "Oct" }, { M (mO2Semi), "Semi" }, { M (mO2Fine), "Fine" }, { M (mO2Pw), "PW" }, { M (mO2Lvl), "Level" } }, 0, 0 },
        { "MIXER / MOD", { { M (mSub), "Sub" }, { M (mNoise), "Noise" }, { M (mSync), "Sync" }, { M (mXmod), "X-Mod" }, { M (mRing), "Ring" }, { M (mPwm), "PWM" } }, 0, 0 },
        // FM page (engine 1): built as a 6 x 7 grid
        { "FM OPERATORS", {}, 7, 1, 1 },
        { "WAVETABLE", { { M (mWt), "Table" }, { M (mWPos), "Position" }, { M (mWEnv), "Env>Pos" }, { M (mWLfo), "LFO>Pos" } }, 0, 2 },
        { "SUPERSAW", { { M (mSsDet), "Detune" }, { M (mSsMix), "Mix" } }, 0, 3 },
        { "PHASE DISTORTION", { { M (mPdW), "Wave" }, { M (mPdAmt), "DCW" }, { M (mPdEnv), "Env>DCW" } }, 0, 4 },
        { "ADDITIVE", { { M (mAddM), "Model" }, { M (mBright), "Bright" }, { M (mTilt), "Dec.Tilt" }, { M (mInh), "Inharm" }, { M (mEven), "Even" } }, 0, 5 },
        { "UNISON", { { M (mUni), "Voices" }, { M (mDetune), "Detune" }, { M (mDrift), "Drift" } } },
        { "FILTER  LP / BP / HP", { { M (mFlt), "Type" }, { M (mCut), "Cutoff" }, { M (mRes), "Reso" }, { M (mFDrive), "Drive" }, { M (mEnv), "Env" }, { M (mKey), "Key" }, { M (mVelF), "Vel" } } },
        { "DRIVE (PEDAL)", { { M (mDist), "Model" }, { M (mDGain), "Gain" }, { M (mDTone), "Tone" }, { M (mDMix), "Mix" } } },
        { "FILTER ENV", { { M (mFA), "A" }, { M (mFD), "D" }, { M (mFS), "S" }, { M (mFR), "R" } } },
        { "AMP ENV", { { M (mAA), "A" }, { M (mAD), "D" }, { M (mAS), "S" }, { M (mAR), "R" } } },
        { "LFO", { { M (mLfoShape), "Shape" }, { M (mLfoRate), "Rate" }, { M (mLfoDelay), "Delay" }, { M (mLfoPitch), "Pitch" }, { M (mLfoCut), "Cutoff" }, { M (mLfoTim), "PWM/Tmb" } } },
        { "PITCH ENV / ACCENT", { { M (mPenv), "P.Env" }, { M (mPdec), "P.Dec" }, { M (mAccent), "Accent" } } },
        { "LO-FI", { { M (mBits), "Bits" }, { M (mSrr), "Rate" } } },
        { "DELAY", { { M (mDlyDiv), "Time" }, { M (mDlyFb), "Feedbk" }, { M (mDlyMix), "Mix" }, { M (mDlyPing), "Ping" } } },
        { "OUT", { { M (mLevel), "Level" }, { M (mPan), "Pan" } } },
    };
    // FM grid content: per operator Coarse, Fine, Level, A, D, S, R  + algorithm / feedback / velocity
    for (auto& gd : groups)
        if (gd.id == 1)
        {
            gd.items.push_back ({ M (mAlg), "Algorithm" });
            gd.items.push_back ({ M (mFb), "Feedback" });
            gd.items.push_back ({ M (mFmVel), "Vel>Idx" });
            for (int o = 0; o < numOps; ++o)
                for (int q = 0; q < numOpParams; ++q) gd.items.push_back ({ M (opSuffix (o, q)), "" });
        }
    for (auto& gd : groups)
    {
        controls.emplace_back();
        for (auto& [pid, label] : gd.items)
        {
            auto c = std::make_unique<Control> (p.apvts, pid, label);
            addAndMakeVisible (*c);
            controls.back().push_back (std::move (c));
        }
    }
    onBtn = std::make_unique<Control> (p.apvts, M (mOn), "ON");
    engineBox = std::make_unique<Control> (p.apvts, M (mEngine), "Engine");
    modernBtn = std::make_unique<Control> (p.apvts, M (mFmModern), "MODERN");
    addAndMakeVisible (*onBtn); addAndMakeVisible (*engineBox); addAndMakeVisible (*modernBtn);
    addAndMakeVisible (algView);
    for (int s : { mOn, mModel, mEngine, mFmModern, mAlg })
        p.apvts.addParameterListener (def (M (s)).id, this);
    refreshSkin();
}

ModulePanel::~ModulePanel()
{
    for (int s : { mOn, mModel, mEngine, mFmModern, mAlg })
        proc.apvts.removeParameterListener (def (mod (module, s)).id, this);
    setLookAndFeel (nullptr);
}

void ModulePanel::applyFmMode()
{
    const bool modern = proc.apvts.getRawParameterValue (def (mod (module, mFmModern)).id)->load() > 0.5f;
    static const char* dxCap[numOpParams] = { "COARSE", "FINE", "OUT LVL", "R1 ATK", "R2 DEC", "L3 SUS", "R4 REL" };
    static const char* mdCap[numOpParams] = { "Coarse", "Fine", "Level", "Attack", "Decay", "Sustain", "Release" };
    for (size_t gi = 0; gi < groups.size(); ++gi)
    {
        if (groups[gi].id != 1) continue;
        for (auto& c : controls[gi])
        {
            const auto& d = def (c->pid);
            const int s = d.suffix;
            if (s >= mOpBase && s < mOpBase + numOps * numOpParams)
            {
                const int o = (s - mOpBase) / numOpParams, q = (s - mOpBase) % numOpParams;
                c->setCaption (juce::String (modern ? "OP" : "OP") + juce::String (o + 1) + " " + (modern ? mdCap[q] : dxCap[q]));
                auto* prm = proc.apvts.getParameter (d.id);
                if (q == opCrs) c->setFormatter ([modern] (double v) { const double r = v < 0.5 ? 0.5 : v; return modern ? "x" + juce::String (r, r < 1 ? 2 : 0) : juce::String (r, 2); });
                else if (q == opFin) c->setFormatter ([modern] (double v) { return modern ? "+" + juce::String (v, 2) : juce::String (juce::roundToInt (v * 100)); });
                else if (q == opLvl) c->setFormatter ([modern] (double v) { return modern ? (v < 0.001 ? juce::String ("-inf dB") : juce::String (20.0 * std::log10 (v), 1) + " dB") : juce::String (juce::roundToInt (v * 99)); });
                else if (q == opSus) c->setFormatter ([modern] (double v) { return modern ? (v < 0.001 ? juce::String ("-inf dB") : juce::String (20.0 * std::log10 (v), 1) + " dB") : juce::String (juce::roundToInt (v * 99)); });
                else c->setFormatter ([modern, prm] (double v) {
                    if (modern) return v >= 1000 ? juce::String (v / 1000.0, 2) + " s" : juce::String (v, 1) + " ms";
                    return juce::String (juce::roundToInt (99.0 * (1.0 - prm->convertTo0to1 ((float) v)))); });   // DX7 rate: high = fast
            }
            else if (s == mFb) { c->setCaption (modern ? "Feedback" : "FEEDBACK"); c->setFormatter ([modern] (double v) { return modern ? juce::String (juce::roundToInt (v * 100)) + " %" : juce::String (juce::roundToInt (v * 7)); }); }
            else if (s == mAlg) c->setCaption (modern ? "Algorithm" : "ALGORITHM");
            else if (s == mFmVel) c->setCaption (modern ? "Vel>Index" : "KEY VEL");
        }
    }
}

void ModulePanel::refreshSkin()
{
    const int mi = (int) proc.apvts.getRawParameterValue (def (mod (module, mModel)).id)->load();
    const auto* md = PresetLibrary::get().model (mi);
    const int engine = (int) proc.apvts.getRawParameterValue (def (mod (module, mEngine)).id)->load();
    digitalLook = engine != 0;
    const bool modern = proc.apvts.getRawParameterValue (def (mod (module, mFmModern)).id)->load() > 0.5f;
    // FM "modern" = Ableton Operator-like: clean flat knobs on the neutral skin
    juce::String key = md != nullptr ? md->skin : juce::String (digitalLook ? "neutral-d" : "neutral-a");
    if (engine == 1 && modern) key = "neutral-d";
    skin = &skins::find (key, digitalLook);
    look.setSkin (*skin);
    for (auto& grp : controls) for (auto& c : grp) c->setSkin (*skin);
    for (auto* c : { onBtn.get(), engineBox.get(), modernBtn.get() }) c->setSkin (*skin);
    applyFmMode();
    algView.setAlg ((int) proc.apvts.getRawParameterValue (def (mod (module, mAlg)).id)->load(), skin->accent, skin->text);
    if (auto* vp = findParentComponentOfClass<juce::Viewport>()) setSize (vp->getMaximumVisibleWidth(), preferredHeight (vp->getMaximumVisibleWidth()));
    resized();
    repaint();
}

int ModulePanel::layoutGroups (int width, bool apply)
{
    const int engine = (int) proc.apvts.getRawParameterValue (def (mod (module, mEngine)).id)->load();
    const bool wood = skin != nullptr && skin->wood;
    const int x0 = 10 + (wood ? 16 : 0), xMax = width - 10 - (wood ? 16 : 0);
    int x = x0, y = panelHead + 8, rowH = 0;
    if (apply) groupBounds.assign (groups.size(), {});
    for (size_t gi = 0; gi < groups.size(); ++gi)
    {
        const auto& gd = groups[gi];
        bool visible = gd.page < 0 || gd.page == engine;
        if (gd.title == "UNISON" && (engine == 3 || engine == 5)) visible = false;
        if (apply) for (auto& c : controls[gi]) c->setVisible (visible);
        if (! visible) continue;
        bool anyFader = false;
        for (auto& c : controls[gi]) anyFader |= c->isFader;
        const int ch = anyFader ? faderH : knobH;
        int gw, gh;
        if (gd.id == 1)   // FM grid: header row (alg diagram + 3 controls) + 6 operator rows of 7
        {
            gw = juce::jmin (xMax - x0, 8 * (cellW + 8) + 200);
            gh = grpHead + knobH + 6 * knobH + 8;
            if (x + gw > xMax && x > x0) { x = x0; y += rowH + 8; rowH = 0; }
            if (apply)
            {
                groupBounds[gi] = { x, y, gw, gh };
                auto& cs = controls[gi];
                algArea = { x + 8, y + grpHead, 190, knobH };
                cs[0]->setBounds (x + 210, y + grpHead, cellW * 2, knobH);
                cs[1]->setBounds (x + 210 + cellW * 2, y + grpHead, cellW, knobH);
                cs[2]->setBounds (x + 210 + cellW * 3, y + grpHead, cellW, knobH);
                const int cw = (gw - 16) / 7;
                for (int o = 0; o < numOps; ++o)
                    for (int q = 0; q < numOpParams; ++q)
                        cs[(size_t) (3 + o * numOpParams + q)]->setBounds (x + 8 + q * cw, y + grpHead + knobH + o * knobH, cw, knobH);
            }
        }
        else
        {
            int units = 0;
            for (auto& c : controls[gi]) units += c->wide ? 2 : 1;
            const int cols = gd.cols > 0 ? gd.cols : units;
            const int rows = (units + cols - 1) / cols;
            gw = cols * cellW + 10; gh = rows * ch + grpHead + 6;
            if (x + gw > xMax && x > x0) { x = x0; y += rowH + 8; rowH = 0; }
            if (apply)
            {
                groupBounds[gi] = { x, y, gw, gh };
                int u = 0;
                for (auto& c : controls[gi])
                {
                    const int w = c->wide ? 2 : 1;
                    if (u % cols + w > cols) u += cols - u % cols;
                    const int cellHgt = (c->isFader || ! anyFader) ? ch : ch;
                    c->setBounds (x + 5 + (u % cols) * cellW, y + grpHead + (u / cols) * ch + (c->isFader || c->wide || c->isToggle ? 0 : (ch - knobH) / 2), cellW * w, c->isFader ? cellHgt : (c->wide || c->isToggle ? ch : knobH));
                    u += w;
                }
            }
        }
        x += gw + 8;
        rowH = juce::jmax (rowH, gh);
    }
    algView.setVisible (engine == 1);
    if (apply) algView.setBounds (algArea);
    modernBtn->setVisible (engine == 1);
    return y + rowH + 12;
}

int ModulePanel::preferredHeight (int width) { return layoutGroups (width, false); }

void ModulePanel::resized()
{
    const bool wood = skin != nullptr && skin->wood;
    const int right = getWidth() - 12 - (wood ? 16 : 0);
    onBtn->setBounds (right - 50, 13, 50, 26);
    modernBtn->setBounds (right - 50 - 84, 13, 78, 26);
    engineBox->setBounds (right - 50 - 84 - 170, 2, 160, 48);
    layoutGroups (getWidth(), true);
}

void ModulePanel::paint (juce::Graphics& g)
{
    const auto& s = *skin;
    auto r = getLocalBounds().toFloat();
    ui::relief (g, r, s.body, 6.0f, true);
    switch (s.pattern)
    {
        case skins::Brushed:
            for (int yy = 0; yy < getHeight(); yy += 2) { g.setColour (juce::Colours::white.withAlpha ((yy % 6 == 0) ? 0.06f : 0.025f)); g.drawHorizontalLine (yy, 0, r.getWidth()); }
            break;
        case skins::Rack:
            for (float sx : { 8.0f, r.getWidth() - 8.0f }) for (float sy : { 8.0f, r.getHeight() - 8.0f })
            { g.setColour (juce::Colours::grey); g.fillEllipse (sx - 3.5f, sy - 3.5f, 7, 7); g.setColour (juce::Colours::black); g.drawLine (sx - 2, sy, sx + 2, sy, 1.2f); }
            break;
        case skins::PinMatrix:
            for (int i = 0; i < 16; ++i) for (int j = 0; j < 3; ++j) { g.setColour (s.label.withAlpha (0.35f)); g.fillEllipse (r.getWidth() * 0.40f + (float) i * 9.0f, 10.0f + (float) j * 9.0f, 3.0f, 3.0f); }
            break;
        case skins::PatchBay:
            for (int i = 0; i < 8; ++i) { const float jx = r.getWidth() * 0.42f + (float) i * 15.0f; g.setColour (juce::Colours::black); g.fillEllipse (jx, 18.0f, 9, 9); g.setColour (s.label.withAlpha (0.6f)); g.drawEllipse (jx, 18.0f, 9, 9, 1.2f); }
            break;
        default: break;
    }
    if (s.wood)
        for (auto cheek : { juce::Rectangle<float> (0, 0, 16, r.getHeight()), juce::Rectangle<float> (r.getWidth() - 16, 0, 16, r.getHeight()) })
        {
            juce::ColourGradient wg (juce::Colour (0xff8a5530), cheek.getX(), 0, juce::Colour (0xff4a2a14), cheek.getRight(), 0, false);
            g.setGradientFill (wg); g.fillRect (cheek);
            g.setColour (juce::Colour (0x22000000));
            for (int k = 0; k < 7; ++k) g.drawLine (cheek.getX() + 2.0f + (float) k * 2.0f, 0, cheek.getX() + 2.5f + (float) k * 2.0f, r.getHeight(), 0.6f);
        }
    const float inset = s.wood ? 16.0f : 0.0f;
    if (s.pattern == skins::Stripes)
    {
        const float bw = (r.getWidth() - 2 * inset) / 4.0f;
        for (int i = 0; i < 4; ++i) { g.setColour (s.stripes[i]); g.fillRect (inset + bw * (float) i, (float) panelHead - 4.0f, bw, 3.0f); }
    }
    else { g.setColour (s.accent.withAlpha (0.8f)); g.fillRect (inset, (float) panelHead - 3.0f, r.getWidth() - 2 * inset, 2.0f); }

    const int mi = (int) proc.apvts.getRawParameterValue (def (mod (module, mModel)).id)->load();
    const auto* md = PresetLibrary::get().model (mi);
    g.setColour (s.accent); g.setFont (ui::font (26.0f, true));
    g.drawText (moduleLetter (module), (int) inset + 12, 8, 28, 36, juce::Justification::centredLeft);
    g.setColour (s.text); g.setFont (ui::font (16.0f, true));
    g.drawText (md ? md->name : juce::String ("Synth"), (int) inset + 42, 8, 360, 20, juce::Justification::centredLeft);
    g.setColour (s.label); g.setFont (ui::font (11.0f));
    g.drawText ((md ? md->maker + "  " + juce::String (md->year) + "   " : juce::String()) + (digitalLook ? "DIGITAL" : "ANALOG") + " ENGINE",
                (int) inset + 42, 28, 360, 16, juce::Justification::centredLeft);
    if (s.lcd)
    {
        const auto& all = PresetLibrary::get().all();
        const int pi = proc.presetIndex();
        juce::String txt = juce::isPositiveAndBelow (pi, (int) all.size()) ? (all[(size_t) pi].original.isNotEmpty() ? all[(size_t) pi].original : all[(size_t) pi].name) : juce::String ("INIT");
        auto lcd = juce::Rectangle<float> (r.getWidth() * 0.36f, 10.0f, 230.0f, 30.0f);
        ui::relief (g, lcd.expanded (2.0f), juce::Colours::black, 3.0f, false);
        g.setColour (s.lcdBg); g.fillRoundedRectangle (lcd, 2.0f);
        if (s.pattern == skins::Crt) for (float yy = lcd.getY(); yy < lcd.getBottom(); yy += 2.0f) { g.setColour (juce::Colours::black.withAlpha (0.25f)); g.drawHorizontalLine ((int) yy, lcd.getX(), lcd.getRight()); }
        g.setColour (s.lcdText);
        g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::bold)));
        g.drawFittedText (txt.toUpperCase(), lcd.reduced (6, 2).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
    }
    for (size_t gi = 0; gi < groups.size(); ++gi)
    {
        const auto& b = groupBounds[gi];
        if (b.isEmpty()) continue;
        ui::relief (g, b.toFloat(), s.body2.darker (s.body.getPerceivedBrightness() > 0.5f ? 0.05f : 0.25f), 4.0f, false);
        g.setColour (s.pattern == skins::Stripes ? s.stripes[gi % 4] : s.accent);
        g.setFont (ui::font (10.0f, true));
        g.drawText (groups[gi].title, b.getX() + 7, b.getY() + 2, b.getWidth() - 10, grpHead - 3, juce::Justification::centredLeft);
    }
}

void ModulePanel::paintOverChildren (juce::Graphics& g)
{
    if (proc.apvts.getRawParameterValue (def (mod (module, mOn)).id)->load() > 0.5f) return;
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRect (getLocalBounds().withTrimmedTop (panelHead));
    g.setColour (ui::text); g.setFont (ui::font (14.0f, true));
    g.drawText ("MODULE OFF - press ON to play it in layer", getLocalBounds().withTrimmedTop (panelHead).withHeight (120), juce::Justification::centred);
}

// =====================================================================================================
//  EQ view
// =====================================================================================================
EqView::EqView (FivesynthProcessor& p) : proc (p) { startTimerHz (20); }

void EqView::makeBig()
{
    big = true;
    look.setSkin (skins::find ("neutral-d", true));
    setLookAndFeel (&look);
    static const char* names[numEqP] = { "ON", "Type", "Freq", "Gain", "Q" };
    for (int e = 0; e < numEqBands; ++e)
        for (int k = 0; k < numEqP; ++k)
        {
            auto c = std::make_unique<Control> (proc.apvts, layout().eqBase[e] + k, k == 0 ? juce::String (e + 1) : juce::String (names[k]));
            c->setSkin (*look.skin);
            addAndMakeVisible (*c);
            bandCtl.push_back (std::move (c));
        }
}

void EqView::resized()
{
    if (! big) return;
    auto r = getLocalBounds().reduced (30, 8).removeFromBottom (78);
    const int bw = r.getWidth() / numEqBands;
    for (int e = 0; e < numEqBands; ++e)
    {
        auto b = r.removeFromLeft (bw).reduced (6, 0);
        int x = b.getX();
        for (int k = 0; k < numEqP; ++k)
        {
            auto& c = bandCtl[(size_t) (e * numEqP + k)];
            const int w = k == 0 ? 30 : k == 1 ? 74 : 44;
            c->setBounds (x, b.getY() + (k <= 1 ? 8 : 0), w, k <= 1 ? 62 : 70);
            x += w + 2;
        }
    }
}

juce::Rectangle<float> EqView::plot() const
{
    auto r = getLocalBounds().toFloat().reduced (big ? 30.0f : 6.0f, big ? 20.0f : 14.0f).withTrimmedTop (big ? 10.0f : 4.0f);
    return big ? r.withTrimmedBottom (96.0f) : r;
}
float EqView::fToX (float f) const { auto p = plot(); return p.getX() + p.getWidth() * std::log (f / 20.0f) / std::log (1000.0f); }
float EqView::xToF (float x) const { auto p = plot(); return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, (x - p.getX()) / p.getWidth())); }
float EqView::gToY (float db) const { auto p = plot(); return p.getCentreY() - db / range * p.getHeight() * 0.5f; }
float EqView::yToG (float y) const { auto p = plot(); return juce::jlimit (-18.0f, 18.0f, (p.getCentreY() - y) / (p.getHeight() * 0.5f) * range); }

void EqView::paint (juce::Graphics& g)
{
    const auto& L = layout();
    auto pr = plot();
    ui::relief (g, getLocalBounds().toFloat(), juce::Colour (0xff202020), 4.0f, false);
    g.setFont (ui::font (big ? 11.0f : 9.0f));
    for (float f : { 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f })
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f)); g.drawVerticalLine ((int) fToX (f), pr.getY(), pr.getBottom());
        if (big) { g.setColour (ui::dim); g.drawText (f >= 1000 ? juce::String ((int) f / 1000) + "k" : juce::String ((int) f), (int) fToX (f) - 15, (int) pr.getBottom() + 2, 30, 14, juce::Justification::centred); }
    }
    for (float db : { -12.f, -6.f, 0.f, 6.f, 12.f })
    {
        g.setColour (juce::Colours::white.withAlpha (db == 0 ? 0.15f : 0.05f)); g.drawHorizontalLine ((int) gToY (db), pr.getX(), pr.getRight());
        if (big) { g.setColour (ui::dim); g.drawText (juce::String ((int) db), (int) pr.getX() - 28, (int) gToY (db) - 7, 24, 14, juce::Justification::right); }
    }
    static const juce::Colour bandCol[5] = { juce::Colour (0xffff6b6b), juce::Colour (0xffffb84d), juce::Colour (0xff7bd88f), juce::Colour (0xff4fc3f7), juce::Colour (0xffb388ff) };
    dsp::Biquad bq[5];
    bool on[5];
    const float sr = (float) juce::jmax (44100.0, proc.sampleRate());
    for (int e = 0; e < numEqBands; ++e)
    {
        const int b = L.eqBase[e];
        auto rv = [&] (int k) { return proc.apvts.getRawParameterValue (def (b + k).id)->load(); };
        on[e] = rv (eqOn) > 0.5f;
        bq[e].set ((int) rv (eqType), rv (eqFreq), rv (eqGain), rv (eqQ), sr);
    }
    juce::Path curve, fill;
    for (int x = (int) pr.getX(); x <= (int) pr.getRight(); x += 2)
    {
        const float f = xToF ((float) x);
        float db = 0;
        for (int e = 0; e < numEqBands; ++e) if (on[e]) db += bq[e].magDb (f, sr);
        const float y = juce::jlimit (pr.getY(), pr.getBottom(), gToY (db));
        if (x == (int) pr.getX()) { curve.startNewSubPath ((float) x, y); fill.startNewSubPath ((float) x, gToY (0)); }
        else curve.lineTo ((float) x, y);
        fill.lineTo ((float) x, y);
    }
    fill.lineTo (pr.getRight(), gToY (0)); fill.closeSubPath();
    g.setColour (ui::colD.withAlpha (0.12f)); g.fillPath (fill);
    g.setColour (ui::colD); g.strokePath (curve, juce::PathStrokeType (big ? 2.0f : 1.5f));
    for (int e = 0; e < numEqBands; ++e)
    {
        const int b = L.eqBase[e];
        auto rv = [&] (int k) { return proc.apvts.getRawParameterValue (def (b + k).id)->load(); };
        const int type = (int) rv (eqType);
        const float gy = (type == 0 || type == 4 || type == 5) ? 0.0f : rv (eqGain);
        const float x = fToX (rv (eqFreq)), y = gToY (gy);
        const float rad = big ? 8.0f : 5.0f;
        g.setColour (bandCol[e].withAlpha (on[e] ? 1.0f : 0.3f));
        g.fillEllipse (x - rad, y - rad, rad * 2, rad * 2);
        g.setColour (juce::Colours::black); g.setFont (ui::font (big ? 10.0f : 8.0f, true));
        g.drawText (juce::String (e + 1), juce::Rectangle<float> (x - rad, y - rad, rad * 2, rad * 2), juce::Justification::centred);
    }
    g.setColour (ui::text); g.setFont (ui::font (big ? 12.0f : 10.0f, true));
    g.drawText (big ? "5-BAND EQ  -  drag: freq / gain, wheel: Q, double-click: band on/off" : "EQ", getLocalBounds().reduced (6, 2).removeFromTop (14), juce::Justification::topLeft);
}

void EqView::mouseDown (const juce::MouseEvent& e)
{
    const auto& L = layout();
    dragBand = -1; float best = 400;
    for (int b = 0; b < numEqBands; ++b)
    {
        auto rv = [&] (int k) { return proc.apvts.getRawParameterValue (def (L.eqBase[b] + k).id)->load(); };
        const float d = e.position.getDistanceFrom ({ fToX (rv (eqFreq)), gToY (rv (eqGain)) });
        if (d < best) { best = d; dragBand = b; }
    }
    if (best > (big ? 30.0f : 16.0f)) dragBand = -1;
    if (dragBand < 0 && ! big && onExpand) onExpand();
}

void EqView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragBand < 0) return;
    const int b = layout().eqBase[dragBand];
    auto set = [&] (int k, float v) { auto* p = proc.apvts.getParameter (def (b + k).id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    set (eqFreq, juce::jlimit (20.0f, 20000.0f, xToF (e.position.x)));
    set (eqGain, yToG (e.position.y));
}

void EqView::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (dragBand < 0) return;
    auto* p = proc.apvts.getParameter (def (layout().eqBase[dragBand] + eqQ).id);
    p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, p->getValue() + w.deltaY * 0.05f));
}

void EqView::mouseDoubleClick (const juce::MouseEvent& e)
{
    mouseDown (e);
    if (dragBand < 0) return;
    auto* p = proc.apvts.getParameter (def (layout().eqBase[dragBand] + eqOn).id);
    p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
}

// =====================================================================================================
//  LFO panel (8 LFOs, any parameter as target)
// =====================================================================================================
LfoPanel::LfoPanel (FivesynthProcessor& p, bool showAll) : proc (p), all (showAll)
{
    setLookAndFeel (&look);
    look.setSkin (skins::find ("neutral-g", false));
    const auto& L = layout();
    for (int l = 0; l < numLfos; ++l)
    {
        Row r;
        const int b = L.lfoBase[l];
        r.target = std::make_unique<juce::ComboBox>();
        r.target->addItem ("-- off", 1);
        // grouped target menu
        juce::String lastGroup;
        for (int t = 0; t < L.numTargets; ++t)
        {
            const auto& d = def (t);
            const juce::String grp = d.module >= 0 ? juce::String ("Module ") + moduleLetter (d.module) : d.id.startsWith ("eq") ? juce::String ("EQ") : d.id.startsWith ("of_") ? juce::String ("Output filters") : juce::String ("Global");
            if (grp != lastGroup) { r.target->addSectionHeading (grp); lastGroup = grp; }
            r.target->addItem (d.name, t + 2);
        }
        auto* tp = p.apvts.getParameter (def (b + lTarget).id);
        auto* cb = r.target.get();
        r.tAtt = std::make_unique<juce::ParameterAttachment> (*tp, [cb] (float v) { cb->setSelectedId ((int) v + 1, juce::dontSendNotification); });
        auto* att = r.tAtt.get();
        cb->onChange = [cb, att] { att->setValueAsCompleteGesture ((float) (cb->getSelectedId() - 1)); };
        r.tAtt->sendInitialUpdate();
        r.shape = std::make_unique<Control> (p.apvts, b + lShape, "Shape");
        r.rate = std::make_unique<Control> (p.apvts, b + lRate, "Rate");
        r.sync = std::make_unique<Control> (p.apvts, b + lSync, "SYNC");
        r.div = std::make_unique<Control> (p.apvts, b + lDiv, "Division");
        r.depth = std::make_unique<Control> (p.apvts, b + lDepth, "Depth");
        r.phase = std::make_unique<Control> (p.apvts, b + lPhase, "Phase");
        for (auto* c : { (juce::Component*) r.target.get(), (juce::Component*) r.shape.get(), (juce::Component*) r.rate.get(), (juce::Component*) r.sync.get(),
                         (juce::Component*) r.div.get(), (juce::Component*) r.depth.get(), (juce::Component*) r.phase.get() })
            addAndMakeVisible (c);
        for (auto* c : { r.shape.get(), r.rate.get(), r.sync.get(), r.div.get(), r.depth.get(), r.phase.get() }) c->setSkin (*look.skin);
        rows.push_back (std::move (r));
        tabs[l].setButtonText (juce::String (l + 1));
        tabs[l].setClickingTogglesState (true);
        tabs[l].setRadioGroupId (811);
        tabs[l].onClick = [this, l] { select (l); };
        if (! all) addAndMakeVisible (tabs[l]);
    }
    expand.onClick = [this] { if (onExpand) onExpand(); };
    if (! all) addAndMakeVisible (expand);
    tabs[0].setToggleState (true, juce::dontSendNotification);
    select (0);
    startTimerHz (20);
}

void LfoPanel::select (int l)
{
    sel = l;
    if (! all)
        for (int i = 0; i < numLfos; ++i)
        {
            auto& r = rows[(size_t) i];
            for (auto* c : { (juce::Component*) r.target.get(), (juce::Component*) r.shape.get(), (juce::Component*) r.rate.get(), (juce::Component*) r.sync.get(),
                             (juce::Component*) r.div.get(), (juce::Component*) r.depth.get(), (juce::Component*) r.phase.get() })
                c->setVisible (i == l);
        }
    resized();
}

void LfoPanel::resized()
{
    auto r = getLocalBounds().reduced (6);
    r.removeFromTop (16);
    if (! all)
    {
        auto tr = r.removeFromTop (22);
        expand.setBounds (tr.removeFromRight (24));
        tr.removeFromRight (4);
        const int tw = tr.getWidth() / numLfos;
        for (int l = 0; l < numLfos; ++l) tabs[l].setBounds (tr.getX() + l * tw, tr.getY(), tw - 2, 22);
        r.removeFromTop (4);
        auto& row = rows[(size_t) sel];
        row.target->setBounds (r.removeFromTop (24));
        r.removeFromTop (4);
        const int cw = r.getWidth() / 6;
        int x = r.getX();
        for (auto* c : { row.shape.get(), row.rate.get(), row.sync.get(), row.div.get(), row.depth.get(), row.phase.get() })
        { c->setBounds (x, r.getY(), c->wide ? cw * 2 - 4 : cw, r.getHeight()); x += c->wide ? cw * 2 - 4 : cw; }
        row.shape->setBounds (row.shape->getBounds().withWidth (cw * 2 - 4));
        // shape wide, then rate, sync, div wide, depth, phase -> reflow into two rows if needed
        const int half = r.getHeight() / 2;
        row.shape->setBounds (r.getX(), r.getY(), cw * 2, half);
        row.div->setBounds (r.getX() + cw * 2, r.getY(), cw * 2, half);
        row.sync->setBounds (r.getX() + cw * 4, r.getY(), cw * 2, half);
        row.rate->setBounds (r.getX(), r.getY() + half, cw * 2, half);
        row.depth->setBounds (r.getX() + cw * 2, r.getY() + half, cw * 2, half);
        row.phase->setBounds (r.getX() + cw * 4, r.getY() + half, cw * 2, half);
    }
    else
    {
        const int rh = r.getHeight() / numLfos;
        for (int l = 0; l < numLfos; ++l)
        {
            auto row = r.removeFromTop (rh).reduced (0, 2);
            row.removeFromLeft (70);
            auto& rr = rows[(size_t) l];
            rr.target->setBounds (row.removeFromLeft (320).withSizeKeepingCentre (316, 24));
            for (auto* c : { rr.shape.get(), rr.div.get() }) c->setBounds (row.removeFromLeft (130));
            for (auto* c : { rr.sync.get(), rr.rate.get(), rr.depth.get(), rr.phase.get() }) c->setBounds (row.removeFromLeft (80));
        }
    }
}

void LfoPanel::paint (juce::Graphics& g)
{
    ui::relief (g, getLocalBounds().toFloat(), ui::panel, 5.0f, true);
    g.setColour (ui::colG); g.setFont (ui::font (10.0f, true));
    g.drawText (all ? "LFO 1-8  -  each LFO can modulate any parameter of the plug-in" : "LFO  (8, any parameter)", getLocalBounds().reduced (8, 3).removeFromTop (14), juce::Justification::topLeft);
    if (all)
    {
        auto r = getLocalBounds().reduced (6); r.removeFromTop (16);
        const int rh = r.getHeight() / numLfos;
        for (int l = 0; l < numLfos; ++l)
        {
            auto row = r.removeFromTop (rh).reduced (0, 2).removeFromLeft (64);
            const float v = proc.lfoValue (l);
            g.setColour (ui::text); g.setFont (ui::font (13.0f, true));
            g.drawText ("LFO " + juce::String (l + 1), row.removeFromLeft (44), juce::Justification::centredLeft);
            g.setColour (ui::colD); g.fillRect (row.withSizeKeepingCentre (6, (int) (std::abs (v) * row.getHeight() * 0.8f) + 1));
        }
    }
    else
    {
        const float v = proc.lfoValue (sel);
        auto meterR = getLocalBounds().reduced (8, 3).removeFromTop (12).removeFromRight (60);
        g.setColour (ui::track); g.fillRect (meterR);
        g.setColour (ui::colD); g.fillRect (meterR.withWidth ((int) ((v * 0.5f + 0.5f) * meterR.getWidth())));
    }
}

// =====================================================================================================
//  Preset browser: one list ordered by year, "Preset (Genre - Song)"
// =====================================================================================================
PresetBrowser::PresetBrowser (FivesynthProcessor& p) : proc (p)
{
    list.setRowHeight (24);
    list.setColour (juce::ListBox::backgroundColourId, ui::panel);
    addAndMakeVisible (list);
    search.setTextToShowWhenEmpty ("search: preset, song, artist, synth, genre...", ui::dim);
    search.setFont (ui::font (13.0f));
    search.onTextChange = [this] { apply(); };
    addAndMakeVisible (search);
    close.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (close);
    for (auto& d : PresetLibrary::get().decades)
    {
        auto* b = decadeBtns.add (new juce::TextButton (d));
        b->onClick = [this, d] {
            const auto& all = PresetLibrary::get().all();
            for (int r = 0; r < (int) rows.size(); ++r)
                if (all[(size_t) rows[(size_t) r]].decade == d) { list.scrollToEnsureRowIsOnscreen (juce::jmin ((int) rows.size() - 1, r + 18)); list.scrollToEnsureRowIsOnscreen (r); break; } };
        addAndMakeVisible (b);
    }
    info.setFont (ui::font (13.0f));
    info.setColour (juce::Label::textColourId, ui::text);
    info.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (info);
    apply();
}

void PresetBrowser::paint (juce::Graphics& g)
{
    g.fillAll (ui::bg);
    g.setColour (ui::dim); g.setFont (ui::font (11.0f));
    g.drawText (juce::String ((int) rows.size()) + " presets, ordered by year", getLocalBounds().reduced (12, 8).removeFromTop (16), juce::Justification::topRight);
}

void PresetBrowser::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto top = r.removeFromTop (28);
    close.setBounds (top.removeFromRight (80));
    top.removeFromRight (180);
    search.setBounds (top.removeFromLeft (340));
    top.removeFromLeft (10);
    for (auto* b : decadeBtns) { b->setBounds (top.removeFromLeft (64)); top.removeFromLeft (3); }
    r.removeFromTop (6);
    info.setBounds (r.removeFromBottom (58));
    list.setBounds (r);
}

void PresetBrowser::apply()
{
    const auto& all = PresetLibrary::get().all();
    rows.clear();
    const auto q = search.getText().trim().toLowerCase();
    for (int i = 0; i < (int) all.size(); ++i)
    {
        const auto& p = all[(size_t) i];
        if (q.isNotEmpty() && ! (p.name + " " + p.song + " " + p.artist + " " + p.synth + " " + p.genre + " " + p.original + " " + juce::String (p.year)).toLowerCase().contains (q)) continue;
        rows.push_back (i);
    }
    list.updateContent();
    list.repaint();
    repaint();
    refresh();
}

void PresetBrowser::refresh()
{
    const int cur = proc.presetIndex();
    for (int i = 0; i < (int) rows.size(); ++i)
        if (rows[(size_t) i] == cur) { list.selectRow (i, false, true); list.scrollToEnsureRowIsOnscreen (i); break; }
    const auto& all = PresetLibrary::get().all();
    if (juce::isPositiveAndBelow (cur, (int) all.size()))
    {
        const auto& p = all[(size_t) cur];
        juce::String s = p.name + "   |   " + p.synth + "   |   " + juce::String (p.year) + "   |   " + p.genre + "   |   " + p.artist + " - " + p.song;
        if (p.original.isNotEmpty()) s << "   |   factory preset: \"" << p.original << "\"";
        if (p.notes.isNotEmpty()) s << "\n" << p.notes;
        info.setText (s, juce::dontSendNotification);
    }
}

void PresetBrowser::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (! juce::isPositiveAndBelow (row, (int) rows.size())) return;
    const auto& p = PresetLibrary::get().all()[(size_t) rows[(size_t) row]];
    g.fillAll (selected ? ui::header.brighter (0.15f) : (row % 2 ? ui::panel : ui::panel.brighter (0.03f)));
    g.setColour (ui::dim); g.setFont (ui::font (12.5f));
    g.drawText (juce::String (p.year), 8, 0, 44, h, juce::Justification::centredLeft);
    g.setColour (ui::text); g.setFont (ui::font (13.0f, true));
    const int nameW = juce::jmin (w / 2, (int) ui::font (13.0f, true).getStringWidthFloat (p.name) + 8);
    g.drawText (p.name, 56, 0, nameW, h, juce::Justification::centredLeft, true);
    g.setColour (ui::dim); g.setFont (ui::font (12.5f));
    g.drawText ("(" + p.genre + " - " + p.song + ")", 56 + nameW, 0, w - 56 - nameW - 300, h, juce::Justification::centredLeft, true);
    g.drawText (p.synth, w - 296, 0, 230, h, juce::Justification::centredLeft, true);
    g.setColour (p.modules.size() > 1 ? juce::Colour (0xffc68cff) : ui::colA);
    g.setFont (ui::font (12.0f, true));
    g.drawText (p.sections(), w - 62, 0, 56, h, juce::Justification::centredLeft);
}

void PresetBrowser::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (! juce::isPositiveAndBelow (row, (int) rows.size())) return;
    proc.loadPreset (rows[(size_t) row]);
    refresh();
}

// =====================================================================================================
//  Editor
// =====================================================================================================
FivesynthEditor::FivesynthEditor (FivesynthProcessor& p)
    : AudioProcessorEditor (p), proc (p), eqSmall (p), eqBig (p), lfoSmall (p, false), lfoBig (p, true),
      keys (p.keyboard, juce::MidiKeyboardComponent::horizontalKeyboard), browser (p)
{
    setLookAndFeel (&look);
    addAndMakeVisible (content);
    const auto& L = layout();
    for (int m = 0; m < numModules; ++m)
    {
        panels[m] = std::make_unique<ModulePanel> (p, m);
        auto& c = cards[m];
        c.m = m;
        c.onSelect = [this, m] { selectModule (m); };
        content.addAndMakeVisible (c);
        cardOn[m] = std::make_unique<Control> (p.apvts, mod (m, mOn), moduleLetter (m));
        cardModel[m] = std::make_unique<Control> (p.apvts, mod (m, mModel), "");
        cardLevel[m] = std::make_unique<Control> (p.apvts, mod (m, mLevel), "Lvl");
        cardPan[m] = std::make_unique<Control> (p.apvts, mod (m, mPan), "Pan");
        for (auto* ctl : { cardOn[m].get(), cardModel[m].get(), cardLevel[m].get(), cardPan[m].get() }) { c.addAndMakeVisible (ctl); ctl->setSkin (*look.skin); }
        cardModel[m]->combo.onChange = [this, m] { selectModule (m); };
        cardRemove[m].setButtonText ("x");
        cardRemove[m].setTooltip ("remove this module");
        cardRemove[m].onClick = [this, m] {
            auto* sl = proc.apvts.getParameter (def (layout().gSlots).id);
            const int slots = (int) proc.apvts.getRawParameterValue (def (layout().gSlots).id)->load();
            if (m == slots - 1 && slots > 2) { sl->setValueNotifyingHost (sl->convertTo0to1 ((float) (slots - 1))); if (selected >= slots - 1) selectModule (0); } };
        c.addChildComponent (cardRemove[m]);
    }
    moduleView.setScrollBarsShown (true, false);
    moduleView.setScrollBarThickness (8);
    content.addAndMakeVisible (moduleView);
    // global strip
    auto addG = [&] (const juce::String& title, std::vector<std::pair<int, juce::String>> items)
    {
        globalTitles.add (title);
        for (auto& [pid, lab] : items) { auto c = std::make_unique<Control> (p.apvts, pid, lab); c->setSkin (*look.skin); content.addAndMakeVisible (*c); c->getProperties().set ("grp", globalTitles.size() - 1); globals.push_back (std::move (c)); }
    };
    addG ("VOICE", { { L.gMode, "Mode" }, { L.gGlide, "Glide" }, { L.gBend, "Bend" }, { L.gVel, "Vel" }, { L.gWidth, "Width" }, { L.gMwVib, "MW Vib" } });
    addG ("FX", { { L.gDrive, "Drive" }, { L.gChoMode, "Chorus" }, { L.gChoMix, "Mix" }, { L.gRevSize, "Rev Size" }, { L.gRevDamp, "Damp" }, { L.gRevMix, "Rev Mix" } });
    addG ("OUTPUT FILTERS", { { L.ofLpOn, "LP" }, { L.ofLpCut, "LP Cut" }, { L.ofLpRes, "LP Res" }, { L.ofBpOn, "BP" }, { L.ofBpCut, "BP Cut" }, { L.ofBpRes, "BP Res" },
                              { L.ofHpOn, "HP" }, { L.ofHpCut, "HP Cut" }, { L.ofHpRes, "HP Res" } });
    addG ("MASTER", { { L.gMaster, "Master" } });
    content.addAndMakeVisible (eqSmall);
    content.addAndMakeVisible (lfoSmall);
    eqSmall.onExpand = [this] { showOverlay (&eqBig); };
    eqExpand.onClick = [this] { showOverlay (&eqBig); };
    eqExpand.setComponentID ("eqExpand");
    browse.setComponentID ("browse");
    overlayClose.setComponentID ("overlayClose");
    content.addAndMakeVisible (eqExpand);
    lfoSmall.onExpand = [this] { showOverlay (&lfoBig); };
    eqBig.makeBig();
    content.addChildComponent (eqBig);
    content.addChildComponent (lfoBig);
    overlayClose.onClick = [this] { showOverlay (nullptr); };
    content.addChildComponent (overlayClose);
    // header
    for (auto* b : { &prev, &next, &browse, &custom }) content.addAndMakeVisible (b);
    prev.onClick = [this] { const int n = (int) PresetLibrary::get().all().size(); proc.loadPreset (((proc.presetIndex() - 1) % n + n) % n); };
    next.onClick = [this] { const int n = (int) PresetLibrary::get().all().size(); proc.loadPreset ((proc.presetIndex() + 1) % n); };
    browse.setClickingTogglesState (true);
    browse.onClick = [this] { browser.setVisible (browse.getToggleState()); if (browser.isVisible()) { browser.toFront (false); browser.refresh(); } };
    browser.onClose = [this] { browse.setToggleState (false, juce::sendNotification); };
    content.addChildComponent (browser);
    custom.setTooltip ("add a synth module (up to C, D, E)");
    custom.onClick = [this] {
        auto* sl = proc.apvts.getParameter (def (layout().gSlots).id);
        const int slots = (int) proc.apvts.getRawParameterValue (def (layout().gSlots).id)->load();
        if (slots < numModules)
        {
            sl->setValueNotifyingHost (sl->convertTo0to1 ((float) (slots + 1)));
            auto* on = proc.apvts.getParameter (def (mod (slots, mOn)).id);
            on->setValueNotifyingHost (1.0f);
            proc.loadModelPatch (slots, (int) proc.apvts.getRawParameterValue (def (mod (slots, mModel)).id)->load());
            selectModule (slots);
        } };
    presetName.setFont (ui::font (21.0f, true));
    presetName.setColour (juce::Label::textColourId, ui::text);
    presetInfo.setFont (ui::font (12.5f));
    presetInfo.setColour (juce::Label::textColourId, ui::dim);
    presetName.setInterceptsMouseClicks (false, false);
    content.addAndMakeVisible (presetName);
    content.addAndMakeVisible (presetInfo);
    keys.setKeyWidth (17.0f);
    keys.setLowestVisibleKey (24);
    keys.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, ui::colA.withAlpha (0.6f));
    content.addAndMakeVisible (keys);

    content.addChildComponent (splash);
    content.setInterceptsMouseClicks (false, true);   // clicks on the header logo reach the editor
    content.setBounds (0, 0, W, H);
    setResizable (true, true);
    setResizeLimits (W / 2, H / 2, W * 2, H * 2);
    getConstrainer()->setFixedAspectRatio ((double) W / H);
    const double sc = (double) proc.uiState.getProperty ("scale", 1.0);
    setSize ((int) (W * sc), (int) (H * sc));
    selectModule (0);
    startTimerHz (30);
    updateHeader();
}

FivesynthEditor::~FivesynthEditor()
{
    moduleView.setViewedComponent (nullptr, false);
    setLookAndFeel (nullptr);
}

void FivesynthEditor::selectModule (int m)
{
    selected = m;
    moduleView.setViewedComponent (panels[m].get(), false);
    panels[m]->setSize (moduleView.getMaximumVisibleWidth() > 0 ? moduleView.getMaximumVisibleWidth() : W - 24, 10);
    panels[m]->refreshSkin();
    panels[m]->setSize (panels[m]->getWidth(), juce::jmax (moduleView.getHeight(), panels[m]->preferredHeight (panels[m]->getWidth())));
    for (auto& c : cards) c.repaint();
    repaint();
}

void FivesynthEditor::showOverlay (juce::Component* c)
{
    for (auto* o : { (juce::Component*) &eqBig, (juce::Component*) &lfoBig }) o->setVisible (o == c);
    overlay = c;
    overlayClose.setVisible (c != nullptr);
    if (c != nullptr) { c->toFront (false); overlayClose.toFront (false); }
}

void FivesynthEditor::layoutSlots()
{
    const int slots = (int) proc.apvts.getRawParameterValue (def (layout().gSlots).id)->load();
    shownSlots = slots;
    const int cw = (W - 16 - 130) / numModules;
    for (int m = 0; m < numModules; ++m)
    {
        auto& c = cards[m];
        c.setVisible (m < slots);
        c.setBounds (8 + m * cw, 70, cw - 6, 56);
        auto r = c.getLocalBounds().reduced (6, 4);
        cardOn[m]->setBounds (r.removeFromLeft (34).withSizeKeepingCentre (34, 24));
        r.removeFromLeft (4);
        cardRemove[m].setVisible (m >= 2 && m == slots - 1);
        if (cardRemove[m].isVisible()) cardRemove[m].setBounds (r.removeFromRight (20).withSizeKeepingCentre (20, 20));
        cardPan[m]->setBounds (r.removeFromRight (40));
        cardLevel[m]->setBounds (r.removeFromRight (40));
        cardModel[m]->setBounds (r.withTrimmedTop (2));
    }
    custom.setVisible (slots < numModules);
    custom.setBounds (W - 8 - 124, 76, 124, 44);
}

void FivesynthEditor::resized()
{
    const float s = (float) getWidth() / (float) W;
    content.setTransform (juce::AffineTransform::scale (s));
    proc.uiState.setProperty ("scale", (double) s, nullptr);
    // header
    logoArea = { 8, 3, 252, 58 };
    splash.setBounds (0, 0, W, H);
    presetName.setBounds (272, 6, 640, 28);
    presetInfo.setBounds (272, 34, 790, 22);
    prev.setBounds (W - 420, 16, 34, 34); next.setBounds (W - 382, 16, 34, 34); browse.setBounds (W - 344, 16, 110, 34);
    layoutSlots();
    moduleView.setBounds (8, 132, W - 16, 486);
    browser.setBounds (8, 66, W - 16, 552);
    // bottom strip
    const int y0 = 626, gh = 158;
    int x = 8;
    globalGroups.clear();
    auto place = [&] (int grp, int cols)
    {
        int n = 0;
        for (auto& c : globals) if ((int) c->getProperties()["grp"] == grp) ++n;
        const int units = [&] { int u = 0; for (auto& c : globals) if ((int) c->getProperties()["grp"] == grp) u += c->wide ? 2 : 1; return u; }();
        const int rows = (units + cols - 1) / cols;
        const int w = cols * cellW + 10;
        globalGroups.push_back ({ x, y0, w, gh });
        int u = 0;
        for (auto& c : globals)
            if ((int) c->getProperties()["grp"] == grp)
            {
                const int cwid = c->wide ? 2 : 1;
                if (u % cols + cwid > cols) u += cols - u % cols;
                c->setBounds (x + 5 + (u % cols) * cellW, y0 + grpHead + (u / cols) * 68, cellW * cwid, 64);
                u += cwid;
            }
        juce::ignoreUnused (rows, n);
        x += w + 6;
    };
    place (0, 4); place (1, 4); place (2, 6); place (3, 1);
    eqSmall.setBounds (x, y0, 250, gh);
    eqExpand.setBounds (x + 250 - 26, y0 + 4, 22, 18);
    x += 256;
    lfoSmall.setBounds (x, y0, W - 8 - x, gh);
    keys.setBounds (8, 792, W - 16, 100);
    eqBig.setBounds (40, 140, W - 80, 600);
    lfoBig.setBounds (40, 140, W - 80, 600);
    overlayClose.setBounds (W - 40 - 90, 146, 80, 26);
    if (panels[selected]) selectModule (selected);
}

void FivesynthEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::bg);
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / (float) W));
    ui::relief (g, juce::Rectangle<float> (0, 0, (float) W, 64), ui::panel, 0.0f, true);
    drawLogo (g);
    // output meter
    const float m = juce::jlimit (0.0f, 1.0f, proc.meter());
    ui::relief (g, juce::Rectangle<float> ((float) W - 100, 24, 84, 10), ui::panel2, 3.0f, false);
    g.setColour (m > 0.95f ? juce::Colours::red : ui::colG); g.fillRoundedRectangle ((float) W - 100, 24, 84 * m, 10, 3);
    g.setColour (ui::dim); g.setFont (ui::font (10.0f)); g.drawText ("OUT", W - 100, 36, 84, 14, juce::Justification::centred);
    // module cards
    for (int mm = 0; mm < numModules; ++mm)
    {
        if (! cards[mm].isVisible()) continue;
        auto b = cards[mm].getBounds().toFloat();
        const int mi = (int) proc.apvts.getRawParameterValue (def (mod (mm, mModel)).id)->load();
        const auto* md = PresetLibrary::get().model (mi);
        const bool on = proc.apvts.getRawParameterValue (def (mod (mm, mOn)).id)->load() > 0.5f;
        const auto col = md && md->digital ? ui::colD : ui::colA;
        ui::relief (g, b, mm == selected ? ui::header : ui::panel, 5.0f, true);
        if (mm == selected) { g.setColour (col); g.drawRoundedRectangle (b.reduced (1.0f), 5.0f, 2.0f); }
        const float lvl = juce::jlimit (0.0f, 1.0f, proc.moduleMeter (mm));
        g.setColour (col.withAlpha (on ? 0.9f : 0.2f)); g.fillRect (b.getX() + 4, b.getBottom() - 4, (b.getWidth() - 8) * lvl, 2.0f);
    }
    // global groups
    for (size_t i = 0; i < globalGroups.size(); ++i)
    {
        ui::relief (g, globalGroups[i].toFloat(), ui::panel, 5.0f, true);
        g.setColour (ui::colG); g.setFont (ui::font (10.0f, true));
        g.drawText (globalTitles[(int) i], globalGroups[i].getX() + 7, globalGroups[i].getY() + 2, 200, 13, juce::Justification::centredLeft);
    }
    if (overlay != nullptr) { g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRect (0, 64, W, H - 64); }
}

// header logo: the framed 5YNTH wordmark
void FivesynthEditor::drawLogo (juce::Graphics& g)
{
    logo::drawWordmark (g, logoArea, juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid);
}

void FivesynthEditor::mouseDown (const juce::MouseEvent& e)
{
    const float s = (float) getWidth() / (float) W;
    if (logoArea.contains (e.position / s)) { splash.setVisible (true); splash.toFront (false); }
}

void FivesynthEditor::updateHeader()
{
    const auto& all = PresetLibrary::get().all();
    const int idx = proc.presetIndex();
    shownPreset = idx;
    if (! juce::isPositiveAndBelow (idx, (int) all.size())) { presetName.setText ("Init", juce::dontSendNotification); presetInfo.setText ({}, juce::dontSendNotification); return; }
    const auto& p = all[(size_t) idx];
    presetName.setText (p.name + "  (" + p.genre + " - " + p.song + ")", juce::dontSendNotification);
    presetInfo.setText (juce::String (p.year) + "  -  " + p.artist + "  -  " + p.synth + "  -  modules " + p.sections()
                        + (p.original.isNotEmpty() ? "  -  factory preset \"" + p.original + "\"" : juce::String()), juce::dontSendNotification);
    for (auto& pn : panels) pn->refreshSkin();
    selectModule (p.modules.empty() ? 0 : p.modules.front().slot);
    if (browser.isVisible()) browser.refresh();
}

void FivesynthEditor::timerCallback()
{
    if (proc.presetIndex() != shownPreset) updateHeader();
    const int slots = (int) proc.apvts.getRawParameterValue (def (layout().gSlots).id)->load();
    if (slots != shownSlots) { layoutSlots(); if (selected >= slots) selectModule (0); }
    const float s = (float) getWidth() / (float) W;
    repaint ((int) ((W - 104) * s), 0, (int) (104 * s), (int) (60 * s));
    repaint (0, (int) (70 * s), getWidth(), (int) (60 * s));
}
