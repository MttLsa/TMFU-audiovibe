#include "PluginEditor.h"

using namespace params;
using skins::Skin;

// =====================================================================================================
//  LookAndFeel (skinnable)
// =====================================================================================================
TwinLook::TwinLook()
{
    setSkin (skins::find ("neutral-a", false));
}

void TwinLook::setSkin (const Skin& s)
{
    skin = &s;
    setColour (juce::ComboBox::backgroundColourId, s.body2.darker (0.25f));
    setColour (juce::ComboBox::textColourId, s.text);
    setColour (juce::ComboBox::outlineColourId, s.label.withAlpha (0.4f));
    setColour (juce::ComboBox::arrowColourId, s.accent);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff202227));
    setColour (juce::PopupMenu::textColourId, ui::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, s.accent.withAlpha (0.35f));
    setColour (juce::TextButton::textColourOffId, s.text);
    setColour (juce::TextButton::textColourOnId, s.accent.getPerceivedBrightness() > 0.6f ? juce::Colours::black : juce::Colours::white);
    setColour (juce::Slider::textBoxTextColourId, s.text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, s.text);
    setColour (juce::ListBox::backgroundColourId, ui::panel);
    setColour (juce::TextEditor::backgroundColourId, ui::panel2);
    setColour (juce::TextEditor::textColourId, ui::text);
    setColour (juce::TextEditor::outlineColourId, ui::line);
    setColour (juce::ScrollBar::thumbColourId, ui::line);
}

void TwinLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& sl)
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
    auto ticks = [&] (int n, float rr, juce::Colour col)
    {
        g.setColour (col);
        for (int i = 0; i <= n; ++i)
        {
            const float a = a0 + (a1 - a0) * (float) i / (float) n;
            g.fillEllipse (c.x + std::sin (a) * rr - 0.9f, c.y - std::cos (a) * rr - 0.9f, 1.8f, 1.8f);
        }
    };

    switch (s.knobStyle)
    {
        case skins::Skirt:   // Moog-style: knurled black skirt, metal cap
        {
            ticks (10, r + 1.0f, s.label.withAlpha (0.8f));
            const float rs = r - 2.0f;
            g.setColour (s.knob);
            g.fillEllipse (c.x - rs, c.y - rs, rs * 2, rs * 2);
            g.setColour (s.knob.brighter (0.25f));
            for (int i = 0; i < 24; ++i)
            {
                const float a = (float) i / 24.0f * juce::MathConstants<float>::twoPi + ang;
                g.drawLine (c.x + std::sin (a) * rs * 0.78f, c.y - std::cos (a) * rs * 0.78f, c.x + std::sin (a) * rs, c.y - std::cos (a) * rs, 1.0f);
            }
            const float rc = rs * 0.62f;
            juce::ColourGradient cg (s.cap.brighter (0.4f), c.x - rc, c.y - rc, s.cap.darker (0.5f), c.x + rc, c.y + rc, false);
            g.setGradientFill (cg);
            g.fillEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2);
            pointer (0.1f, 0.95f, 2.0f, juce::Colours::white);
            break;
        }
        case skins::Pointer:  // classic black knob with white line
        case skins::CapKnob:  // black knob with coloured cap
        {
            ticks (10, r + 1.0f, s.label.withAlpha (0.7f));
            const float rs = r - 2.5f;
            juce::ColourGradient kg (s.knob.brighter (0.35f), c.x, c.y - rs, s.knob.darker (0.3f), c.x, c.y + rs, false);
            g.setGradientFill (kg);
            g.fillEllipse (c.x - rs, c.y - rs, rs * 2, rs * 2);
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.drawEllipse (c.x - rs, c.y - rs, rs * 2, rs * 2, 1.0f);
            if (s.knobStyle == skins::CapKnob)
            {
                const float rc = rs * 0.5f;
                g.setColour (s.cap);
                g.fillEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2);
                pointer (0.55f, 0.95f, 2.0f, juce::Colours::white);
            }
            else pointer (0.2f, 0.95f, 2.2f, juce::Colours::white);
            break;
        }
        case skins::Membrane:   // digital: flat value cell, like a data-entry readout
        {
            auto box = juce::Rectangle<float> (c.x - r, c.y - r * 0.62f, r * 2, r * 1.24f);
            g.setColour (s.lcd ? s.lcdBg : s.body2.darker (0.4f));
            g.fillRoundedRectangle (box, 3.0f);
            g.setColour (s.lcd ? s.lcdText.withAlpha (0.25f) : s.label.withAlpha (0.25f));
            g.drawRoundedRectangle (box, 3.0f, 1.0f);
            auto bar = box.reduced (4.0f, box.getHeight() * 0.36f);
            g.setColour ((s.lcd ? s.lcdText : s.accent).withAlpha (0.25f));
            g.fillRect (bar);
            g.setColour (s.lcd ? s.lcdText : s.accent);
            if (bip) { const float mid = bar.getCentreX(), px = bar.getX() + bar.getWidth() * pos; g.fillRect (juce::Rectangle<float> (juce::jmin (mid, px), bar.getY(), std::abs (px - mid) + 1.0f, bar.getHeight())); }
            else g.fillRect (bar.withWidth (juce::jmax (1.0f, bar.getWidth() * pos)));
            // small coloured membrane key under the cell
            g.setColour (s.accent);
            g.fillRoundedRectangle (c.x - 6.0f, box.getBottom() + 2.0f, 12.0f, 3.0f, 1.0f);
            break;
        }
        default:   // Modern arc
        {
            const float rr = r - 3.0f;
            juce::Path track; track.addCentredArc (c.x, c.y, rr, rr, 0, a0, a1, true);
            g.setColour (s.knob.brighter (0.15f));
            g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            juce::Path val;
            const float from = bip ? (a0 + a1) * 0.5f : a0;
            val.addCentredArc (c.x, c.y, rr, rr, 0, juce::jmin (from, ang), juce::jmax (from, ang), true);
            g.setColour (s.accent);
            g.strokePath (val, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (s.knob);
            g.fillEllipse (c.x - rr * 0.62f, c.y - rr * 0.62f, rr * 1.24f, rr * 1.24f);
            pointer (0.15f, 0.6f, 2.0f, s.text);
            break;
        }
    }
}

void TwinLook::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& sl)
{
    const auto& s = *skin;
    const float cx = (float) x + (float) w * 0.5f;
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillRoundedRectangle (cx - 2.0f, (float) y, 4.0f, (float) h, 2.0f);
    g.setColour (s.label.withAlpha (0.5f));
    for (int i = 0; i <= 10; ++i) { const float ty = (float) y + (float) h * (float) i / 10.0f; g.drawHorizontalLine ((int) ty, cx - 9.0f, cx - 5.0f); }
    const float capH = 9.0f, capW = 16.0f;
    // the colour of the cap comes from the skin's stripe set, so fader banks look like the original panels
    const int idx = (int) (std::hash<std::string>{} (sl.getName().toStdString()) % 4);
    const auto capCol = s.stripes[idx];
    auto cap = juce::Rectangle<float> (cx - capW * 0.5f, pos - capH * 0.5f, capW, capH);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (cap.translated (0, 1.5f), 1.5f);
    g.setColour (s.knob);
    g.fillRoundedRectangle (cap, 1.5f);
    g.setColour (capCol);
    g.fillRect (cap.reduced (1.5f, 3.5f));
}

void TwinLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto& s = *skin;
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    g.setColour (on ? s.accent : s.body2.darker (0.35f).brighter (over ? 0.1f : 0.0f).darker (down ? 0.2f : 0.0f));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (s.label.withAlpha (0.35f));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
}

void TwinLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& cb)
{
    const auto& s = *skin;
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (s.lcd ? s.lcdBg : s.body2.darker (0.35f));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (s.label.withAlpha (0.35f));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    juce::Path p; const float ax = (float) w - 9.0f, ay = (float) h * 0.5f;
    p.addTriangle (ax - 3.5f, ay - 2.0f, ax + 3.5f, ay - 2.0f, ax, ay + 2.5f);
    g.setColour (s.lcd ? s.lcdText : s.accent);
    g.fillPath (p);
    cb.setColour (juce::ComboBox::textColourId, s.lcd ? s.lcdText : s.text);
}

// =====================================================================================================
//  Control
// =====================================================================================================
Control::Control (juce::AudioProcessorValueTreeState& st, int p, const juce::String& label, juce::Colour acc)
    : pid (p), caption (label), accent (acc)
{
    const auto& d = def (p);
    if (d.kind == C)
    {
        combo.addItemList (choicesOf (p), 1);
        addAndMakeVisible (combo);
        ca = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (st, d.id, combo);
        wide = true;
    }
    else if (d.kind == B)
    {
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
        knob.setMouseDragSensitivity (220);
        knob.setVelocityModeParameters (0.6, 1, 0.05, true, juce::ModifierKeys::shiftModifier);
        addAndMakeVisible (knob);
        sa = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (st, d.id, knob);
        knob.setDoubleClickReturnValue (true, (double) d.def);
    }
}

void Control::setSkin (const Skin& s)
{
    if (sa != nullptr)
        knob.setSliderStyle (s.knobStyle == skins::Fader ? juce::Slider::LinearVertical : juce::Slider::RotaryHorizontalVerticalDrag);
    repaint();
}

void Control::resized()
{
    auto r = getLocalBounds();
    if (combo.isVisible()) { combo.setBounds (r.removeFromTop (r.getHeight() - 16).withSizeKeepingCentre (r.getWidth() - 4, 22)); }
    else if (toggle.isVisible()) toggle.setBounds (r.withSizeKeepingCentre (r.getWidth() - 6, 22));
    else knob.setBounds (r.removeFromTop (r.getHeight() - 15).reduced (2, 0));
}

void Control::paint (juce::Graphics& g)
{
    if (toggle.isVisible()) return;
    auto* lf = dynamic_cast<TwinLook*> (&getLookAndFeel());
    g.setColour (lf && lf->skin ? lf->skin->label : ui::dim);
    g.setFont (ui::font (10.5f, true));
    g.drawFittedText (caption.toUpperCase(), getLocalBounds().removeFromBottom (15), juce::Justification::centred, 1, 0.8f);
}

// =====================================================================================================
//  SectionPanel
// =====================================================================================================
static constexpr int cellW = 48, cellH = 60, grpHead = 15;

SectionPanel::SectionPanel (juce::AudioProcessorValueTreeState& s, const juce::String& t, const juce::String& sub,
                            juce::Colour acc, std::vector<GroupDef> g, int onP, int pageP)
    : state (s), title (t), subtitle (sub), accent (acc), groups (std::move (g)), onPid (onP), pagePid (pageP)
{
    setLookAndFeel (&look);
    for (auto& gd : groups)
    {
        controls.emplace_back();
        for (auto& [pid, label] : gd.items)
        {
            auto c = std::make_unique<Control> (s, pid, label, accent);
            addAndMakeVisible (*c);
            controls.back().push_back (std::move (c));
        }
    }
    if (onPid >= 0) { onButton = std::make_unique<Control> (s, onPid, "ON", accent); addAndMakeVisible (*onButton); state.addParameterListener (def (onPid).id, this); }
    if (pagePid >= 0) state.addParameterListener (def (pagePid).id, this);
    digital = title.startsWith ("D");
    compact = title.startsWith ("GLOBAL");
    setSkin (digital ? "neutral-d" : (title.startsWith ("A") ? "neutral-a" : "neutral-g"), {}, {});
}

SectionPanel::~SectionPanel()
{
    if (onPid >= 0) state.removeParameterListener (def (onPid).id, this);
    if (pagePid >= 0) state.removeParameterListener (def (pagePid).id, this);
    setLookAndFeel (nullptr);
}

void SectionPanel::setSkin (const juce::String& key, const juce::String& m, const juce::String& lcd)
{
    skin = &skins::find (key, digital);
    look.setSkin (*skin);
    model = m; lcdText = lcd;
    for (auto& grp : controls) for (auto& c : grp) c->setSkin (*skin);
    if (onButton) onButton->setSkin (*skin);
    resized();
    repaint();
    for (auto* c : getChildren()) c->repaint();
}

void SectionPanel::resized()
{
    const int page = pagePid >= 0 ? (int) state.getRawParameterValue (def (pagePid).id)->load() : -1;
    const int x0 = 10 + (skin && skin->wood ? 14 : 0), xMax = getWidth() - 10 - (skin && skin->wood ? 14 : 0);
    const int headH = compact ? 4 : 50;
    int x = x0, y = headH + 6, rowH = 0;
    groupBounds.assign (groups.size(), {});
    if (onButton) onButton->setBounds (getWidth() - 58 - (skin && skin->wood ? 14 : 0), 12, 46, 26);
    for (size_t gi = 0; gi < groups.size(); ++gi)
    {
        const auto& gd = groups[gi];
        const bool visible = gd.page < 0 || gd.page == page;
        for (auto& c : controls[gi]) c->setVisible (visible);
        if (! visible) continue;
        int units = 0;
        for (auto& c : controls[gi]) units += c->wide ? 2 : 1;
        const int cols = gd.cols > 0 ? gd.cols : units;
        const int rows = (units + cols - 1) / cols;
        const int gw = cols * cellW + 8, gh = rows * cellH + grpHead + 6;
        if (x + gw > xMax && x > x0) { x = x0; y += rowH + 6; rowH = 0; }
        groupBounds[gi] = { x, y, gw, gh };
        int u = 0;
        for (auto& c : controls[gi])
        {
            const int w = c->wide ? 2 : 1;
            if (u % cols + w > cols) u += cols - u % cols;
            c->setBounds (x + 4 + (u % cols) * cellW, y + grpHead + (u / cols) * cellH, cellW * w, cellH);
            u += w;
        }
        x += gw + 6;
        rowH = juce::jmax (rowH, gh);
    }
}

void SectionPanel::paint (juce::Graphics& g)
{
    const auto& s = *skin;
    auto r = getLocalBounds().toFloat();
    juce::ColourGradient bgGrad (s.body2, 0, 0, s.body, 0, r.getHeight(), false);
    g.setGradientFill (bgGrad);
    g.fillRoundedRectangle (r, 6.0f);

    // panel material
    switch (s.pattern)
    {
        case skins::Brushed:
            for (int yy = 0; yy < getHeight(); yy += 2) { g.setColour (juce::Colours::white.withAlpha ((yy % 6 == 0) ? 0.06f : 0.025f)); g.drawHorizontalLine (yy, 0, r.getWidth()); }
            break;
        case skins::Rack:
            g.setColour (s.body.darker (0.4f));
            g.fillRect (r.removeFromLeft (0).withWidth (0));
            for (float sx : { 8.0f, r.getWidth() - 8.0f }) for (float sy : { 8.0f, r.getHeight() - 8.0f })
            { g.setColour (juce::Colours::grey); g.fillEllipse (sx - 3.5f, sy - 3.5f, 7, 7); g.setColour (juce::Colours::black); g.drawLine (sx - 2, sy, sx + 2, sy, 1.2f); }
            break;
        case skins::PinMatrix:
            for (int i = 0; i < 16; ++i) for (int j = 0; j < 3; ++j)
            { g.setColour (s.label.withAlpha (0.35f)); g.fillEllipse (r.getWidth() * 0.42f + (float) i * 9.0f, 10.0f + (float) j * 9.0f, 3.0f, 3.0f); }
            break;
        case skins::PatchBay:
            for (int i = 0; i < 10; ++i)
            { const float jx = r.getWidth() * 0.45f + (float) i * 15.0f; g.setColour (juce::Colours::black); g.fillEllipse (jx, 16.0f, 9, 9); g.setColour (s.label.withAlpha (0.6f)); g.drawEllipse (jx, 16.0f, 9, 9, 1.2f); }
            break;
        default: break;
    }

    // wood end cheeks
    if (s.wood)
    {
        for (auto cheek : { juce::Rectangle<float> (0, 0, 14, r.getHeight()), juce::Rectangle<float> (r.getWidth() - 14, 0, 14, r.getHeight()) })
        {
            juce::ColourGradient wg (juce::Colour (0xff7a4a26), cheek.getX(), 0, juce::Colour (0xff4a2a14), cheek.getRight(), 0, false);
            g.setGradientFill (wg);
            g.fillRect (cheek);
            g.setColour (juce::Colour (0x22000000));
            for (int k = 0; k < 6; ++k) g.drawLine (cheek.getX() + 2.0f + (float) k * 2.0f, 0, cheek.getX() + 2.5f + (float) k * 2.0f, r.getHeight(), 0.6f);
        }
    }

    const float inset = s.wood ? 14.0f : 0.0f;
    const int headH = compact ? 4 : 50;
    if (! compact) {
    // stripe band under the header
    if (s.pattern == skins::Stripes)
    {
        const float bw = (r.getWidth() - 2 * inset) / 4.0f;
        for (int i = 0; i < 4; ++i) { g.setColour (s.stripes[i]); g.fillRect (inset + bw * (float) i, (float) headH - 4.0f, bw, 3.0f); }
    }
    else { g.setColour (s.accent.withAlpha (0.8f)); g.fillRect (inset, (float) headH - 3.0f, r.getWidth() - 2 * inset, 2.0f); }

    // header: section tag + instrument name
    g.setColour (s.accent);
    g.setFont (ui::font (22.0f, true));
    g.drawText (title.substring (0, 1), (int) inset + 10, 6, 26, 36, juce::Justification::centredLeft);
    g.setColour (s.text);
    g.setFont (ui::font (15.0f, true));
    g.drawText (model.isNotEmpty() ? model : subtitle, (int) inset + 36, 6, 330, 20, juce::Justification::centredLeft);
    g.setColour (s.label);
    g.setFont (ui::font (10.5f));
    g.drawText (subtitle, (int) inset + 36, 26, 330, 16, juce::Justification::centredLeft);

    // LCD display (digital instruments)
    if (s.lcd && lcdText.isNotEmpty())
    {
        auto lcd = juce::Rectangle<float> (r.getWidth() - inset - 300.0f, 9.0f, 228.0f, 30.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (lcd.expanded (2.0f), 3.0f);
        g.setColour (s.lcdBg);
        g.fillRoundedRectangle (lcd, 2.0f);
        if (s.pattern == skins::Crt)
            for (float yy = lcd.getY(); yy < lcd.getBottom(); yy += 2.0f) { g.setColour (juce::Colours::black.withAlpha (0.25f)); g.drawHorizontalLine ((int) yy, lcd.getX(), lcd.getRight()); }
        g.setColour (s.lcdText);
        g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::bold)));
        g.drawFittedText (lcdText.toUpperCase(), lcd.reduced (6, 2).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
    }
    }

    // groups
    g.setFont (ui::font (10.0f, true));
    for (size_t gi = 0; gi < groups.size(); ++gi)
    {
        const auto& b = groupBounds[gi];
        if (b.isEmpty()) continue;
        g.setColour (juce::Colours::black.withAlpha (s.body.getPerceivedBrightness() > 0.5f ? 0.06f : 0.18f));
        g.fillRoundedRectangle (b.toFloat(), 4.0f);
        g.setColour (s.label.withAlpha (0.3f));
        g.drawRoundedRectangle (b.toFloat(), 4.0f, 1.0f);
        g.setColour (s.pattern == skins::Stripes ? s.stripes[gi % 4] : s.accent);
        g.drawText (groups[gi].title, b.getX() + 6, b.getY() + 1, b.getWidth() - 8, grpHead - 2, juce::Justification::centredLeft);
    }
}

void SectionPanel::paintOverChildren (juce::Graphics& g)
{
    if (onPid < 0) return;
    if (state.getRawParameterValue (def (onPid).id)->load() > 0.5f) return;
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (getLocalBounds().toFloat().withTrimmedTop (50.0f), 6.0f);
    g.setColour (ui::dim);
    g.setFont (ui::font (13.0f, true));
    g.drawText ("SECTION OFF - not used by this preset (press ON to layer it)", getLocalBounds().withTrimmedTop (50), juce::Justification::centred);
}

// =====================================================================================================
//  Preset browser
// =====================================================================================================
PresetBrowser::PresetBrowser (TwinsynthProcessor& p) : proc (p)
{
    const auto& lib = PresetLibrary::get();
    decadeModel.items.add ("All decades"); decadeModel.items.addArray (lib.decades);
    genreModel.items.add ("All genres"); genreModel.items.addArray (lib.genres);
    decadeModel.onChange = [this] { apply(); };
    genreModel.onChange = [this] { apply(); };
    decadeList.setModel (&decadeModel);
    genreList.setModel (&genreModel);
    for (auto* lb : { &decadeList, &genreList })
    {
        lb->setRowHeight (21);
        lb->setColour (juce::ListBox::backgroundColourId, ui::panel);
        addAndMakeVisible (*lb);
    }
    auto& h = table.getHeader();
    h.addColumn ("Preset", 1, 190); h.addColumn ("Original synth", 2, 200); h.addColumn ("Year", 3, 50);
    h.addColumn ("Genre", 4, 130); h.addColumn ("Sec.", 5, 46); h.addColumn ("Type", 6, 70); h.addColumn ("Reference", 7, 330);
    table.setRowHeight (21);
    table.setColour (juce::ListBox::backgroundColourId, ui::panel);
    table.getHeader().setColour (juce::TableHeaderComponent::backgroundColourId, ui::panel2);
    table.getHeader().setColour (juce::TableHeaderComponent::textColourId, ui::text);
    addAndMakeVisible (table);
    int i = 0;
    for (auto* b : { &secAll, &secA, &secD, &secAD })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (77);
        b->onClick = [this, i] { secSel = i; apply(); };
        addAndMakeVisible (*b);
        ++i;
    }
    secAll.setToggleState (true, juce::dontSendNotification);
    danceOnly.setClickingTogglesState (true);
    danceOnly.onClick = [this] { apply(); };
    addAndMakeVisible (danceOnly);
    close.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (close);
    search.setTextToShowWhenEmpty ("search name, synth, artist...", ui::dim);
    search.onTextChange = [this] { apply(); };
    addAndMakeVisible (search);
    info.setColour (juce::Label::textColourId, ui::text);
    info.setFont (ui::font (13.0f));
    info.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (info);
    decadeList.selectRow (0); genreList.selectRow (0);
    apply();
}

void PresetBrowser::paint (juce::Graphics& g)
{
    g.fillAll (ui::bg);
    g.setColour (ui::text);
    g.setFont (ui::font (12.0f, true));
    g.drawText ("DECADE", 12, 8, 150, 16, juce::Justification::left);
    g.drawText ("GENRE", 12, 214, 150, 16, juce::Justification::left);
    g.setColour (ui::dim);
    g.setFont (ui::font (11.0f));
    g.drawText (juce::String ((int) rows.size()) + " presets", getWidth() - 300, 8, 200, 16, juce::Justification::right);
}

void PresetBrowser::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto left = r.removeFromLeft (170);
    left.removeFromTop (18);
    decadeList.setBounds (left.removeFromTop (180));
    left.removeFromTop (24);
    genreList.setBounds (left);
    r.removeFromLeft (10);
    auto top = r.removeFromTop (26);
    close.setBounds (top.removeFromRight (80));
    top.removeFromRight (100);
    for (auto* b : { &secAll, &secA, &secD, &secAD }) { b->setBounds (top.removeFromLeft (50)); top.removeFromLeft (4); }
    top.removeFromLeft (8);
    danceOnly.setBounds (top.removeFromLeft (190));
    top.removeFromLeft (8);
    search.setBounds (top.removeFromLeft (260));
    r.removeFromTop (6);
    info.setBounds (r.removeFromBottom (64));
    table.setBounds (r);
}

void PresetBrowser::refresh()
{
    const int cur = proc.presetIndex();
    for (int i = 0; i < (int) rows.size(); ++i)
        if (rows[(size_t) i] == cur) { table.selectRow (i, false, true); table.scrollToEnsureRowIsOnscreen (i); break; }
    showSelected();
}

void PresetBrowser::apply()
{
    const auto& all = PresetLibrary::get().all();
    rows.clear();
    const auto q = search.getText().trim().toLowerCase();
    for (int i = 0; i < (int) all.size(); ++i)
    {
        const auto& p = all[(size_t) i];
        if (decadeModel.selected > 0 && p.decade != decadeModel.items[decadeModel.selected]) continue;
        if (genreModel.selected > 0 && p.genre != genreModel.items[genreModel.selected]) continue;
        if (secSel == 1 && p.type != "A") continue;
        if (secSel == 2 && p.type != "D") continue;
        if (secSel == 3 && p.type != "AD") continue;
        if (danceOnly.getToggleState() && ! p.dance) continue;
        if (q.isNotEmpty() && ! (p.name + " " + p.synth + " " + p.reference + " " + p.genre + " " + p.category + " " + p.original).toLowerCase().contains (q)) continue;
        rows.push_back (i);
    }
    table.updateContent();
    table.repaint();
    repaint();
    refresh();
}

int PresetBrowser::getNumRows() { return (int) rows.size(); }

void PresetBrowser::paintRowBackground (juce::Graphics& g, int row, int, int, bool selected)
{
    g.fillAll (selected ? ui::line.brighter (0.2f) : (row % 2 ? ui::panel : ui::panel.brighter (0.03f)));
}

void PresetBrowser::paintCell (juce::Graphics& g, int row, int col, int w, int h, bool)
{
    if (! juce::isPositiveAndBelow (row, (int) rows.size())) return;
    const auto& p = PresetLibrary::get().all()[(size_t) rows[(size_t) row]];
    juce::String t;
    juce::Colour c = ui::text;
    switch (col)
    {
        case 1: t = p.name; break;
        case 2: t = p.synth; c = ui::dim; break;
        case 3: t = juce::String (p.year); c = ui::dim; break;
        case 4: t = p.genre; break;
        case 5: t = p.type == "AD" ? "A+D" : p.type; c = p.type == "A" ? ui::colA : p.type == "D" ? ui::colD : juce::Colour (0xffc68cff); break;
        case 6: t = p.category; c = ui::dim; break;
        default: t = p.reference; c = ui::dim; break;
    }
    g.setColour (c);
    g.setFont (ui::font (12.5f, col == 1 || col == 5));
    g.drawText (t, 6, 0, w - 8, h, juce::Justification::centredLeft, true);
}

void PresetBrowser::selectedRowsChanged (int row)
{
    if (! juce::isPositiveAndBelow (row, (int) rows.size())) return;
    const int idx = rows[(size_t) row];
    if (idx != proc.presetIndex()) proc.loadPreset (idx);
    showSelected();
}

void PresetBrowser::showSelected()
{
    const auto& all = PresetLibrary::get().all();
    const int idx = proc.presetIndex();
    if (! juce::isPositiveAndBelow (idx, (int) all.size())) { info.setText ({}, juce::dontSendNotification); return; }
    const auto& p = all[(size_t) idx];
    juce::String s = p.name + "   |   " + (p.original.isNotEmpty() ? "factory preset name: \"" + p.original + "\"   |   " : juce::String ("studio patch (no factory name)   |   "))
                   + p.synth + "   |   " + juce::String (p.year) + "   |   " + p.genre + "\n" + p.reference;
    if (p.notes.isNotEmpty()) s << "\n" << p.notes;
    info.setText (s, juce::dontSendNotification);
}

void FilterListModel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool sel)
{
    g.fillAll (sel ? ui::line.brighter (0.2f) : ui::panel);
    g.setColour (sel ? ui::colG : ui::text);
    g.setFont (ui::font (12.5f, sel));
    g.drawText (items[row], 8, 0, w - 10, h, juce::Justification::centredLeft, true);
}

// =====================================================================================================
//  Editor
// =====================================================================================================
namespace
{
    std::vector<GroupDef> groupsA()
    {
        return {
            { "OSC 1", { { aO1Wave, "Wave" }, { aO1Oct, "Oct" }, { aO1Pw, "PW" }, { aO1Lvl, "Level" } } },
            { "OSC 2", { { aO2Wave, "Wave" }, { aO2Oct, "Oct" }, { aO2Semi, "Semi" }, { aO2Fine, "Fine" }, { aO2Pw, "PW" }, { aO2Lvl, "Level" } } },
            { "MIXER / MOD", { { aSub, "Sub" }, { aNoise, "Noise" }, { aSync, "Sync" }, { aXmod, "X-Mod" }, { aRing, "Ring" }, { aPwm, "PWM" } } },
            { "UNISON", { { aUni, "Voices" }, { aDetune, "Detune" }, { aDrift, "Drift" } } },
            { "FILTER", { { aFltType, "Type" }, { aCut, "Cutoff" }, { aRes, "Reso" }, { aDrive, "Drive" }, { aEnv, "Env" }, { aKey, "Key" }, { aVelF, "Vel" } } },
            { "FILTER ENV", { { aFA, "A" }, { aFD, "D" }, { aFS, "S" }, { aFR, "R" } } },
            { "AMP ENV", { { aAA, "A" }, { aAD, "D" }, { aAS, "S" }, { aAR, "R" } } },
            { "LFO", { { aLfoShape, "Shape" }, { aLfoRate, "Rate" }, { aLfoDelay, "Delay" }, { aLfoPitch, "Pitch" }, { aLfoCut, "Cutoff" } } },
            { "PITCH ENV / ACCENT / OUT", { { aPenv, "P.Env" }, { aPdec, "P.Dec" }, { aAccent, "Accent" }, { aLevel, "Level" }, { aPan, "Pan" } } },
        };
    }
    std::vector<GroupDef> groupsD()
    {
        return {
            { "ENGINE", { { dEngine, "Engine" }, { dOct, "Oct" }, { dSemi, "Semi" }, { dFine, "Fine" }, { dUni, "Voices" }, { dDetune, "Detune" }, { dNoise, "Noise" } } },
            { "AMP ENV", { { dAA, "A" }, { dAD, "D" }, { dAS, "S" }, { dAR, "R" } } },
            { "FM  -  4 OPERATORS", { { dAlg, "Algorithm" }, { dFb, "Feedbk" }, { dFmVel, "Vel>Idx" }, { dFmAtk, "Mod Atk" },
                                     { dR1, "Ratio 1" }, { dR2, "Ratio 2" }, { dR3, "Ratio 3" }, { dR4, "Ratio 4" }, { dL1, "Level 1" }, { dL2, "Level 2" }, { dL3, "Level 3" }, { dL4, "Level 4" },
                                     { dE1, "Decay 1" }, { dE2, "Decay 2" }, { dE3, "Decay 3" }, { dE4, "Decay 4" }, { dS1, "Sus 1" }, { dS2, "Sus 2" }, { dS3, "Sus 3" }, { dS4, "Sus 4" } }, 11, 0 },
            { "WAVETABLE", { { dWt, "Table" }, { dWPos, "Position" }, { dWEnv, "Env>Pos" }, { dWLfo, "LFO>Pos" } }, 0, 1 },
            { "SUPERSAW", { { dSsDet, "Detune" }, { dSsMix, "Mix" } }, 0, 2 },
            { "PHASE DISTORTION", { { dPdW, "Wave" }, { dPdAmt, "DCW" }, { dPdEnv, "Env>DCW" } }, 0, 3 },
            { "ADDITIVE", { { dAddM, "Model" }, { dBright, "Bright" }, { dTilt, "Dec.Tilt" }, { dInh, "Inharm" }, { dEven, "Even" } }, 0, 4 },
            { "FILTER", { { dFltType, "Type" }, { dCut, "Cutoff" }, { dRes, "Reso" }, { dEnv, "Env" }, { dKey, "Key" }, { dVelF, "Vel" } } },
            { "FILTER ENV", { { dFA, "A" }, { dFD, "D" }, { dFS, "S" }, { dFR, "R" } } },
            { "LFO", { { dLfoShape, "Shape" }, { dLfoRate, "Rate" }, { dLfoDelay, "Delay" }, { dLfoPitch, "Pitch" }, { dLfoCut, "Cutoff" }, { dLfoTim, "Timbre" } } },
            { "PITCH ENV / LO-FI / OUT", { { dPenv, "P.Env" }, { dPdec, "P.Dec" }, { dBits, "Bits" }, { dSrr, "Rate" }, { dLevel, "Level" }, { dPan, "Pan" } } },
        };
    }
    std::vector<GroupDef> groupsG()
    {
        return {
            { "VOICE", { { gMode, "Mode" }, { gGlide, "Glide" }, { gBend, "Bend" }, { gVel, "Velocity" }, { gWidth, "Width" }, { gMwVib, "MW Vib" }, { gMwCut, "MW Cut" } } },
            { "DRIVE / CHORUS", { { gDrive, "Drive" }, { gChoMode, "Chorus" }, { gChoMix, "Mix" } } },
            { "DELAY", { { gDlyDiv, "Time" }, { gDlyFb, "Feedbk" }, { gDlyMix, "Mix" }, { gDlyPing, "Ping" } } },
            { "REVERB", { { gRevSize, "Size" }, { gRevDamp, "Damp" }, { gRevMix, "Mix" } } },
            { "OUT", { { gMaster, "Master" } } },
        };
    }
}

TwinsynthEditor::TwinsynthEditor (TwinsynthProcessor& p)
    : AudioProcessorEditor (p), proc (p), keys (p.keyboard, juce::MidiKeyboardComponent::horizontalKeyboard), browser (p)
{
    setLookAndFeel (&look);
    look.setSkin (skins::find ("neutral-g", false));
    panelA = std::make_unique<SectionPanel> (p.apvts, "A", "ANALOG SECTION", ui::colA, groupsA(), aOn, -1);
    panelD = std::make_unique<SectionPanel> (p.apvts, "D", "DIGITAL SECTION", ui::colD, groupsD(), dOn, dEngine);
    panelG = std::make_unique<SectionPanel> (p.apvts, "GLOBAL", "VOICE / EFFECTS", ui::colG, groupsG(), -1, -1);
    addAndMakeVisible (content);
    for (auto* c : { (juce::Component*) panelA.get(), (juce::Component*) panelD.get(), (juce::Component*) panelG.get(), (juce::Component*) &keys })
        content.addAndMakeVisible (c);
    for (auto* b : { &prev, &next, &browse }) content.addAndMakeVisible (b);
    prev.onClick = [this] { step (-1); };
    next.onClick = [this] { step (1); };
    browse.setClickingTogglesState (true);
    browse.onClick = [this] { browser.setVisible (browse.getToggleState()); if (browser.isVisible()) browser.refresh(); };
    browser.onClose = [this] { browse.setToggleState (false, juce::sendNotification); };
    content.addChildComponent (browser);
    presetName.setFont (ui::font (22.0f, true));
    presetName.setColour (juce::Label::textColourId, ui::text);
    presetInfo.setFont (ui::font (12.5f));
    presetInfo.setColour (juce::Label::textColourId, ui::dim);
    content.addAndMakeVisible (presetName);
    content.addAndMakeVisible (presetInfo);
    keys.setKeyWidth (16.0f);
    keys.setLowestVisibleKey (24);
    keys.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, ui::colA.withAlpha (0.6f));
    content.setBounds (0, 0, W, H);
    panelA->setBounds (8, 64, 670, 500);
    panelD->setBounds (682, 64, 670, 500);
    panelG->setBounds (8, 570, 1344, 100);
    keys.setBounds (8, 676, 1344, 72);
    browser.setBounds (8, 64, 1344, 500);
    prev.setBounds (W - 290, 16, 34, 34);
    next.setBounds (W - 252, 16, 34, 34);
    browse.setBounds (W - 210, 16, 110, 34);
    presetName.setBounds (200, 6, 620, 30);
    presetInfo.setBounds (200, 34, 860, 22);

    setResizable (true, true);
    setResizeLimits (W / 2, H / 2, W * 2, H * 2);
    getConstrainer()->setFixedAspectRatio ((double) W / H);
    setSize (W, H);
    startTimerHz (15);
    updateHeader();
}

TwinsynthEditor::~TwinsynthEditor() { setLookAndFeel (nullptr); }

void TwinsynthEditor::resized()
{
    const float s = (float) getWidth() / (float) W;
    content.setTransform (juce::AffineTransform::scale (s));
}

void TwinsynthEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::bg);
    const float s = (float) getWidth() / (float) W;
    g.addTransform (juce::AffineTransform::scale (s));
    g.setColour (ui::text);
    g.setFont (ui::font (24.0f, true));
    g.drawText ("TWINSYNTH", 16, 10, 170, 28, juce::Justification::left);
    g.setFont (ui::font (11.0f, true));
    g.setColour (ui::colA); g.drawText ("A  ANALOG", 18, 38, 80, 16, juce::Justification::left);
    g.setColour (ui::colD); g.drawText ("D  DIGITAL", 96, 38, 90, 16, juce::Justification::left);
    // output meter
    const float m = juce::jlimit (0.0f, 1.0f, proc.meter());
    g.setColour (ui::panel2); g.fillRoundedRectangle ((float) W - 86, 22, 70, 8, 3);
    g.setColour (m > 0.95f ? juce::Colours::red : ui::colG); g.fillRoundedRectangle ((float) W - 86, 22, 70 * m, 8, 3);
    g.setColour (ui::dim); g.setFont (ui::font (10.0f)); g.drawText ("OUT", W - 86, 32, 70, 14, juce::Justification::centred);
}

void TwinsynthEditor::step (int d)
{
    const int n = (int) PresetLibrary::get().all().size();
    if (n == 0) return;
    proc.loadPreset (((proc.presetIndex() + d) % n + n) % n);
}

void TwinsynthEditor::updateHeader()
{
    const auto& all = PresetLibrary::get().all();
    const int idx = proc.presetIndex();
    shownPreset = idx;
    if (! juce::isPositiveAndBelow (idx, (int) all.size()))
    {
        presetName.setText ("Init", juce::dontSendNotification); presetInfo.setText ({}, juce::dontSendNotification);
        panelA->setSkin ("neutral-a", {}, {}); panelD->setSkin ("neutral-d", {}, {});
        return;
    }
    const auto& p = all[(size_t) idx];
    presetName.setText (p.name, juce::dontSendNotification);
    presetInfo.setText (juce::String (p.year) + "  \xc2\xb7  " + p.genre + "  \xc2\xb7  " + p.synth + "  \xc2\xb7  "
                        + (p.type == "AD" ? "A+D layer" : p.type == "A" ? "Section A" : "Section D") + "  \xc2\xb7  " + p.reference,
                        juce::dontSendNotification);
    const auto lcd = p.original.isNotEmpty() ? p.original : p.name;
    panelA->setSkin (p.skinA.isNotEmpty() ? p.skinA : "neutral-a", p.synthA, lcd);
    panelD->setSkin (p.skinD.isNotEmpty() ? p.skinD : "neutral-d", p.synthD, lcd);
}

void TwinsynthEditor::timerCallback()
{
    if (proc.presetIndex() != shownPreset) { updateHeader(); if (browser.isVisible()) browser.showSelected(); }
    repaint (getWidth() - 100, 0, 100, 60);
}
