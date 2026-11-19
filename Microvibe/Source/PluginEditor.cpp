#include "PluginEditor.h"
#include "BinaryData.h"
#include <map>

using namespace prm;

juce::Colour ui::sourceColour (int s)
{
    if (s >= srcLfo1 && s < srcLfo1 + NLFO) return juce::Colour (0xffc792ff);
    if (s >= srcEnv1 && s < srcEnv1 + NENV) return juce::Colour (0xffff8fc7);
    if (s >= srcRnd1 && s < srcRnd1 + NRND) return juce::Colour (0xffffd166);
    if (s >= srcFol1 && s < srcFol1 + NFOL) return juce::Colour (0xff4fd1c5);
    if (s >= srcLane1 && s < srcLane1 + NLANE) return juce::Colour (0xffff7a59);
    if (s >= srcMac1 && s < srcMac1 + NMAC) return juce::Colour (0xfff2f2f2);
    return juce::Colour (0xffa0a8b4);
}

static juce::String shortSource (int s)
{
    if (s >= srcLfo1 && s < srcLfo1 + NLFO) return "LFO" + juce::String (s - srcLfo1 + 1);
    if (s >= srcEnv1 && s < srcEnv1 + NENV) return "ENV" + juce::String (s - srcEnv1 + 1);
    if (s >= srcRnd1 && s < srcRnd1 + NRND) return "RND" + juce::String (s - srcRnd1 + 1);
    if (s >= srcFol1 && s < srcFol1 + NFOL) return "FOL" + juce::String (s - srcFol1 + 1);
    if (s >= srcLane1 && s < srcLane1 + NLANE) return "STEP" + juce::String (s - srcLane1 + 1);
    if (s >= srcMac1 && s < srcMac1 + NMAC) return "M" + juce::String (s - srcMac1 + 1);
    static const char* m[] = { "VEL", "NOTE", "MW", "AT", "PB" };
    if (s >= srcVel && s <= srcPB) return m[s - srcVel];
    return "-";
}

juce::Typeface::Ptr ui::typeface (bool bold)
{
    static const juce::Typeface::Ptr regular = juce::Typeface::createSystemTypefaceFor (BinaryData::UrbanistLight_ttf, BinaryData::UrbanistLight_ttfSize);
    static const juce::Typeface::Ptr heavy   = juce::Typeface::createSystemTypefaceFor (BinaryData::UrbanistSemiBold_ttf, BinaryData::UrbanistSemiBold_ttfSize);
    return bold ? heavy : regular;
}

// =============================================================================================== look
MvLook::MvLook()
{
    setDefaultSansSerifTypeface (ui::typeface (false));
    setColour (juce::ComboBox::backgroundColourId, ui::panel2);
    setColour (juce::ComboBox::textColourId, ui::text);
    setColour (juce::ComboBox::outlineColourId, ui::line);
    setColour (juce::ComboBox::arrowColourId, ui::dim);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1d2026));
    setColour (juce::PopupMenu::textColourId, ui::text);
    setColour (juce::PopupMenu::headerTextColourId, ui::tune);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, ui::line);
    setColour (juce::TextButton::buttonColourId, ui::panel2);
    setColour (juce::TextButton::textColourOffId, ui::dim);
    setColour (juce::TextButton::textColourOnId, ui::bg);
    setColour (juce::Label::textColourId, ui::text);
    setColour (juce::ScrollBar::thumbColourId, ui::line);
    setColour (juce::TooltipWindow::backgroundColourId, ui::panel2);
}

void MvLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
    const float d = (float) juce::jmin (w, h) - 8.0f;
    const auto c = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const float r = d * 0.5f;
    juce::Path bgArc; bgArc.addCentredArc (c.x, c.y, r, r, 0, a0, a1, true);
    g.setColour (ui::line); g.strokePath (bgArc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const bool bip = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float ref = bip ? a0 + (a1 - a0) * (float) s.valueToProportionOfLength (0.0) : a0;
    const float av = a0 + (a1 - a0) * pos;
    juce::Path v; v.addCentredArc (c.x, c.y, r, r, 0, juce::jmin (ref, av), juce::jmax (ref, av), true);
    g.setColour (accent); g.strokePath (v, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (ui::panel2); g.fillEllipse (c.x - r + 5, c.y - r + 5, (r - 5) * 2, (r - 5) * 2);
    juce::Path p; p.addRoundedRectangle (-1.2f, -(r - 6), 2.4f, (r - 6) * 0.55f, 1.0f);
    g.setColour (ui::text); g.fillPath (p, juce::AffineTransform::rotation (av).translated (c.x, c.y));
}

void MvLook::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& s)
{
    const auto accent = s.findColour (juce::Slider::trackColourId);
    const float cy = (float) y + (float) h * 0.5f;
    g.setColour (ui::line); g.fillRoundedRectangle ((float) x, cy - 2, (float) w, 4, 2);
    const float zero = (float) x + (float) w * (float) s.valueToProportionOfLength (juce::jlimit (s.getMinimum(), s.getMaximum(), 0.0));
    g.setColour (accent); g.fillRoundedRectangle (juce::jmin (zero, pos), cy - 2, std::abs (pos - zero), 4, 2);
    g.setColour (ui::text); g.fillEllipse (pos - 4, cy - 4, 8, 8);
}

void MvLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const auto accent = b.findColour (juce::TextButton::buttonOnColourId);
    g.setColour (b.getToggleState() ? accent : (over ? ui::line : ui::panel2));
    if (down) g.setColour (accent.withAlpha (0.7f));
    g.fillRoundedRectangle (r, 4);
    g.setColour (ui::line); g.drawRoundedRectangle (r, 4, 1);
}

void MvLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (ui::panel2); g.fillRoundedRectangle (r, 4);
    g.setColour (ui::line); g.drawRoundedRectangle (r, 4, 1);
    juce::Path p; const float cx = (float) w - 9, cy = (float) h * 0.5f;
    p.addTriangle (cx - 3.5f, cy - 2, cx + 3.5f, cy - 2, cx, cy + 2.5f);
    g.setColour (ui::dim); g.fillPath (p);
}

void MvLook::positionComboBoxText (juce::ComboBox& b, juce::Label& l)
{
    l.setBounds (2, 0, b.getWidth() - 16, b.getHeight());
    l.setFont (getComboBoxFont (b));
}

// =============================================================================================== controls
ParamControl::ParamControl (MicrovibeProcessor& p, int id, juce::Colour a, const juce::String&) : pid (id), accent (a), proc (p) {}
ParamControl::~ParamControl() = default;

namespace
{
    juce::String captionFor (int pid, const juce::String& caption)
    {
        if (caption.isNotEmpty()) return caption;
        juce::String n = def (pid).name;
        // drop the group prefix ("Perc 3 Decay" -> "Decay", "LFO 2 Rate" -> "Rate")
        for (auto pre : { "Perc ", "Lane ", "LFO ", "Env ", "Random ", "Follower ", "Mod " })
            if (n.startsWith (pre))
            {
                const juce::String rest = n.fromFirstOccurrenceOf (pre, false, false);
                if (rest.containsOnly ("0123456789 ") == false && rest.upToFirstOccurrenceOf (" ", false, false).containsOnly ("0123456789"))
                    n = rest.fromFirstOccurrenceOf (" ", false, false);
                else if (! rest.upToFirstOccurrenceOf (" ", false, false).containsOnly ("0123456789"))
                    n = rest;
                break;
            }
        for (auto pre : { "Texture ", "Tape ", "Vinyl ", "Wobble ", "Delay ", "Reverb ", "Filter ", "Resonator ", "Ring/Shift ", "Crusher ",
                          "Chorus ", "Phaser ", "Comp ", "Shepard FX ", "Shepard ", "Glitch ", "Grain " })
            if (n.startsWith (pre) && n.length() > juce::String (pre).length() + 2) { n = n.substring (juce::String (pre).length()); break; }
        return n;
    }

    class Knob : public ParamControl, public juce::DragAndDropTarget, private juce::Slider::Listener
    {
    public:
        Knob (MicrovibeProcessor& p, int id, juce::Colour a, const juce::String& cap)
            : ParamControl (p, id, a), caption (captionFor (id, cap))
        {
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            slider.setColour (juce::Slider::rotarySliderFillColourId, a);
            slider.setMouseDragSensitivity (400);
            slider.setVelocityModeParameters (1.0, 1, 0.0, false, juce::ModifierKeys::shiftModifier);
            slider.setPopupDisplayEnabled (true, true, nullptr);
            slider.setTooltip (def (id).name);
            addAndMakeVisible (slider);
            att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, def (id).id, slider);
            slider.setDoubleClickReturnValue (true, def (id).def);
            slider.addListener (this);
            slider.setWantsKeyboardFocus (true);
            slider.addMouseListener (this, false);
        }
        ~Knob() override { slider.removeListener (this); }
        void resized() override { auto r = getLocalBounds(); r.removeFromBottom (13); slider.setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()) + 4, r.getHeight())); }
        void paint (juce::Graphics& g) override
        {
            g.setColour (ui::dim); g.setFont (ui::font (10.0f));
            g.drawFittedText (caption, getLocalBounds().removeFromBottom (13), juce::Justification::centred, 1, 0.8f);
        }
        void paintOverChildren (juce::Graphics& g) override
        {
            // modulation ring: assigned depth (thin arc) and live position (dot)
            const auto& I = ids();
            const int dst = destIndexOf (pid);
            float depth = 0; int firstSrc = 0;
            for (int k = 0; k < NSLOT; ++k)
                if ((int) proc.getParam (I.mxDst[k]) == dst && dst > 0 && (int) proc.getParam (I.mxSrc[k]) > 0)
                { depth += std::abs (proc.getParam (I.mxAmt[k])); if (! firstSrc) firstSrc = (int) proc.getParam (I.mxSrc[k]); }
            if (depth <= 0.0001f) return;
            const auto sb = slider.getBounds().toFloat();
            const auto c = sb.getCentre();
            const float r = juce::jmin (sb.getWidth(), sb.getHeight()) * 0.5f - 4.0f + 4.0f;
            const auto params = slider.getRotaryParameters();
            const float a0 = params.startAngleRadians, a1 = params.endAngleRadians;
            const float pos = (float) slider.valueToProportionOfLength (slider.getValue());
            const auto col = ui::sourceColour (firstSrc);
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, r, r, 0, a0 + (a1 - a0) * juce::jlimit (0.0f, 1.0f, pos - depth), a0 + (a1 - a0) * juce::jlimit (0.0f, 1.0f, pos + depth), true);
            g.setColour (col.withAlpha (0.45f)); g.strokePath (arc, juce::PathStrokeType (2.0f));
            const float live = juce::jlimit (0.0f, 1.0f, pos + proc.engine.display.modNorm[(size_t) pid].load());
            const float a = a0 + (a1 - a0) * live;
            g.setColour (col); g.fillEllipse (c.x + std::sin (a) * r - 3, c.y - std::cos (a) * r - 3, 6, 6);
        }
        bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.toString().startsWith ("src:") && destIndexOf (pid) > 0; }
        void itemDragEnter (const SourceDetails&) override { hover = true; repaint(); }
        void itemDragExit (const SourceDetails&) override { hover = false; repaint(); }
        void itemDropped (const SourceDetails& d) override
        {
            hover = false;
            const int s = d.description.toString().fromFirstOccurrenceOf (":", false, false).getIntValue();
            proc.assignModulation (s, pid);
            repaint();
        }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (! e.mods.isPopupMenu()) return;
            const auto& I = ids();
            juce::PopupMenu m;
            m.addSectionHeader (def (pid).name);
            m.addItem (1, "Valore di default");
            const int dst = destIndexOf (pid);
            if (dst > 0)
            {
                juce::PopupMenu add;
                for (int s = 1; s < NUM_SOURCES; ++s) add.addItem (1000 + s, sourceNames()[s]);
                m.addSubMenu ("Aggiungi modulazione", add);
                for (int k = 0; k < NSLOT; ++k)
                    if ((int) proc.getParam (I.mxDst[k]) == dst && (int) proc.getParam (I.mxSrc[k]) > 0)
                        m.addItem (100 + k, "Rimuovi " + sourceNames()[(int) proc.getParam (I.mxSrc[k])] + "  ("
                                            + juce::String (proc.getParam (I.mxAmt[k]), 2) + ", slot " + juce::String (k + 1) + ")");
            }
            juce::Component::SafePointer<Knob> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r) {
                if (safe == nullptr || r == 0) return;
                if (r == 1) safe->proc.setParam (safe->pid, def (safe->pid).def);
                else if (r >= 1000) safe->proc.assignModulation (r - 1000, safe->pid);
                else if (r >= 100) safe->proc.clearModulation (r - 100);
                safe->repaint();
            });
        }
        bool keyPressed (const juce::KeyPress& k) override
        {
            const int code = k.getKeyCode();
            const bool up = code == juce::KeyPress::upKey || code == juce::KeyPress::rightKey;
            const bool dn = code == juce::KeyPress::downKey || code == juce::KeyPress::leftKey;
            if (! up && ! dn) return false;
            const auto& d = def (pid);
            double step = d.kind == prm::I ? 1.0 : (d.mx - d.mn) * (k.getModifiers().isShiftDown() ? 0.0002 : 0.002);
            if (d.kind == F && d.centre > 0)
            {
                const double prop = slider.valueToProportionOfLength (slider.getValue()) + (up ? 1 : -1) * (k.getModifiers().isShiftDown() ? 0.0002 : 0.002);
                slider.setValue (slider.proportionOfLengthToValue (juce::jlimit (0.0, 1.0, prop)));
                return true;
            }
            slider.setValue (slider.getValue() + (up ? step : -step));
            return true;
        }
        void paintOverlayHover (juce::Graphics&) {}
    private:
        void sliderValueChanged (juce::Slider*) override {}
        juce::String caption;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
        bool hover = false;
    };

    class Choice : public ParamControl
    {
    public:
        Choice (MicrovibeProcessor& p, int id, juce::Colour a, const juce::String& cap) : ParamControl (p, id, a), caption (captionFor (id, cap))
        {
            box.addItemList (def (id).choices, 1);
            box.setTooltip (def (id).name);
            addAndMakeVisible (box);
            att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, def (id).id, box);
        }
        void resized() override { auto r = getLocalBounds(); r.removeFromBottom (13); box.setBounds (r.withSizeKeepingCentre (r.getWidth() - 2, 22)); }
        void paint (juce::Graphics& g) override
        {
            g.setColour (ui::dim); g.setFont (ui::font (10.0f));
            g.drawFittedText (caption, getLocalBounds().removeFromBottom (13), juce::Justification::centred, 1, 0.8f);
        }
    private:
        juce::String caption;
        juce::ComboBox box;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
    };

    class Toggle : public ParamControl
    {
    public:
        Toggle (MicrovibeProcessor& p, int id, juce::Colour a, const juce::String& cap) : ParamControl (p, id, a)
        {
            juce::String c = captionFor (id, cap);
            if (c.endsWith (" On") || c == "On") c = "ON";
            button.setButtonText (c.toUpperCase());
            button.setClickingTogglesState (true);
            button.setColour (juce::TextButton::buttonOnColourId, a);
            button.setTooltip (def (id).name);
            addAndMakeVisible (button);
            att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, def (id).id, button);
        }
        void resized() override { auto r = getLocalBounds(); r.removeFromBottom (13); button.setBounds (r.withSizeKeepingCentre (r.getWidth() - 4, 22)); }
    private:
        juce::TextButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
    };
}

std::unique_ptr<ParamControl> makeControl (MicrovibeProcessor& p, int pid, juce::Colour accent, const juce::String& caption)
{
    switch (def (pid).kind)
    {
        case C: return std::make_unique<Choice> (p, pid, accent, caption);
        case B: return std::make_unique<Toggle> (p, pid, accent, caption);
        default: return std::make_unique<Knob> (p, pid, accent, caption);
    }
}

void ParamGrid::setParams (const std::vector<int>& pids, int cols, const juce::StringArray& captions)
{
    controls.clear();
    columns = cols;
    for (size_t i = 0; i < pids.size(); ++i)
    {
        if (pids[i] < 0) { controls.push_back (nullptr); continue; }
        controls.push_back (makeControl (proc, pids[i], accent, (int) i < captions.size() ? captions[(int) i] : juce::String()));
        addAndMakeVisible (*controls.back());
    }
    resized();
}

void ParamGrid::resized()
{
    const int cw = getWidth() / juce::jmax (1, columns);
    for (size_t i = 0; i < controls.size(); ++i)
        if (controls[i]) controls[i]->setBounds ((int) (i % (size_t) columns) * cw, (int) (i / (size_t) columns) * rowH, cw, rowH - 4);
}

// =============================================================================================== chips / tabs
SourceChip::SourceChip (int s, const juce::String& l) : src (s), label (l)
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setTooltip ("Trascina su una manopola per modulare: " + sourceNames()[s]);
}

void SourceChip::paint (juce::Graphics& g)
{
    const auto col = ui::sourceColour (src);
    auto r = getLocalBounds().toFloat().reduced (1);
    g.setColour (col.withAlpha (isMouseOver() ? 0.35f : 0.18f)); g.fillRoundedRectangle (r, 6);
    g.setColour (col); g.drawRoundedRectangle (r, 6, 1);
    // live value bar
    const float v = juce::jlimit (-1.0f, 1.0f, value);
    const float w = r.getWidth() - 6;
    g.setColour (col.withAlpha (0.8f));
    if (sourceIsBipolar (src)) g.fillRect (r.getCentreX() + juce::jmin (0.0f, v) * w * 0.5f, r.getBottom() - 3, std::abs (v) * w * 0.5f, 2.0f);
    else g.fillRect (r.getX() + 3, r.getBottom() - 3, juce::jmax (0.0f, v) * w, 2.0f);
    g.setColour (ui::text); g.setFont (ui::font (10.0f, true));
    g.drawFittedText (label, getLocalBounds().withTrimmedBottom (2), juce::Justification::centred, 1);
}

void SourceChip::mouseDrag (const juce::MouseEvent&)
{
    if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
        if (! c->isDragAndDropActive())
        {
            juce::Image img (juce::Image::ARGB, getWidth(), getHeight(), true);
            juce::Graphics g (img); paint (g);
            c->startDragging ("src:" + juce::String (src), this, juce::ScaledImage (img));
        }
}

Tabs::Tabs (const juce::StringArray& n, juce::Colour a) : names (n), accent (a) {}

void Tabs::paint (juce::Graphics& g)
{
    const float w = (float) getWidth() / (float) juce::jmax (1, names.size());
    for (int i = 0; i < names.size(); ++i)
    {
        auto r = juce::Rectangle<float> (i * w, 0, w, (float) getHeight()).reduced (1.5f, 1);
        g.setColour (i == selected ? accent : ui::panel2); g.fillRoundedRectangle (r, 4);
        g.setColour (i == selected ? ui::bg : ui::dim); g.setFont (ui::font (11.0f, true));
        g.drawFittedText (names[i], r.toNearestInt(), juce::Justification::centred, 1);
    }
}

void Tabs::mouseDown (const juce::MouseEvent& e)
{
    const int i = (int) (e.position.x / ((float) getWidth() / (float) juce::jmax (1, names.size())));
    if (juce::isPositiveAndBelow (i, names.size())) select (i);
}

// =============================================================================================== lanes
LaneView::LaneView (MicrovibeProcessor& p) : proc (p) {}

juce::Rectangle<int> LaneView::rowArea (int l) const
{
    const int h = (getHeight() - 4) / NLANE;
    return { 0, 2 + l * h, getWidth(), h };
}

juce::Rectangle<float> LaneView::cell (int l, int s, int steps) const
{
    auto r = rowArea (l).withTrimmedLeft (64).reduced (0, 3).toFloat();
    const float w = r.getWidth() / (float) juce::jmax (1, steps);
    return { r.getX() + s * w, r.getY(), w, r.getHeight() };
}

bool LaneView::gateOf (int l, int s) const
{
    const auto& I = ids();
    const int steps = (int) proc.getParam (I.lnSteps[l]);
    if ((int) proc.getParam (I.lnMode[l]) == 1) return proc.engine.shared.gate[proc.engine.display.pattern.load()][l][s].load() != 0;
    return mv::Lane::euclid (s, steps, (int) proc.getParam (I.lnPulses[l]), (int) proc.getParam (I.lnRot[l]));
}

void LaneView::paint (juce::Graphics& g)
{
    const auto& I = ids();
    const int pat = proc.engine.display.pattern.load();
    for (int l = 0; l < NLANE; ++l)
    {
        auto row = rowArea (l);
        const bool on = proc.getParam (I.lnOn[l]) > 0.5f;
        g.setColour (l == selectedLane ? ui::panel2 : ui::panel); g.fillRoundedRectangle (row.reduced (0, 1).toFloat(), 4);
        auto lab = row.removeFromLeft (62);
        g.setColour (on ? ui::glitch : ui::dim); g.setFont (ui::font (12.0f, true));
        g.drawText ("L" + juce::String (l + 1), lab.removeFromTop (lab.getHeight() / 2).withTrimmedLeft (6), juce::Justification::bottomLeft);
        const int steps = juce::jmax (1, (int) proc.getParam (I.lnSteps[l]));
        const bool grid = (int) proc.getParam (I.lnMode[l]) == 1;
        g.setColour (ui::dim); g.setFont (ui::font (9.5f));
        const juce::String info = (grid ? juce::String ("grid ") : juce::String ((int) proc.getParam (I.lnPulses[l])) + "/") + juce::String (steps)
                                  + " " + ratioNames()[(int) proc.getParam (I.lnRatio[l])];
        g.drawText (info, lab.withTrimmedLeft (6), juce::Justification::topLeft);
        const int cur = proc.engine.display.laneStep[l].load();
        for (int s = 0; s < steps; ++s)
        {
            auto c = cell (l, s, steps).reduced (steps > 40 ? 0.5f : 1.0f, 0);
            const bool gt = gateOf (l, s);
            const float v = proc.engine.shared.value[pat][l][s].load();
            g.setColour (s % 4 == 0 ? ui::line : ui::line.withAlpha (0.55f)); g.fillRoundedRectangle (c, 2);
            if (gt)
            {
                auto f = c;
                if (grid) f = f.withTrimmedTop (c.getHeight() * (1.0f - (0.35f + 0.65f * v)));
                g.setColour ((on ? ui::glitch : ui::dim).withAlpha (s == cur ? 1.0f : 0.75f)); g.fillRoundedRectangle (f, 2);
            }
            else if (grid)
            {
                g.setColour (ui::dim.withAlpha (0.35f));
                g.fillRect (c.getX() + 1, c.getBottom() - 2 - (c.getHeight() - 4) * v, c.getWidth() - 2, 1.5f);
            }
            if (s == cur && on) { g.setColour (ui::text); g.drawRoundedRectangle (c, 2, 1.2f); }
        }
    }
}

void LaneView::toGrid (int l)
{
    const auto& I = ids();
    if ((int) proc.getParam (I.lnMode[l]) == 1) return;
    const int steps = (int) proc.getParam (I.lnSteps[l]);
    const int pat = proc.engine.display.pattern.load();
    for (int s = 0; s < mv::kMaxSteps; ++s)
        proc.engine.shared.gate[pat][l][s] = (uint8_t) (s < steps && mv::Lane::euclid (s, steps, (int) proc.getParam (I.lnPulses[l]), (int) proc.getParam (I.lnRot[l])));
    proc.setParam (I.lnMode[l], 1);
}

void LaneView::mouseDown (const juce::MouseEvent& e)
{
    const auto& I = ids();
    for (int l = 0; l < NLANE; ++l)
    {
        if (! rowArea (l).contains (e.getPosition())) continue;
        if (selectedLane != l) { selectedLane = l; if (onSelectLane) onSelectLane (l); }
        if (e.x < 64)
        {
            if (e.mods.isPopupMenu())
            {
                juce::PopupMenu m;
                m.addItem (1, "Converti Euclid in Grid"); m.addItem (2, "Random (Grid)"); m.addItem (3, "Svuota (Grid)"); m.addItem (4, "Torna a Euclid");
                juce::Component::SafePointer<LaneView> safe (this);
                m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe, l] (int r) {
                    if (safe == nullptr) return;
                    auto& pr = safe->proc; const int pat = pr.engine.display.pattern.load();
                    if (r == 1) safe->toGrid (l);
                    if (r == 2) { pr.randomizeLane (pat, l); pr.setParam (ids().lnMode[l], 1); }
                    if (r == 3) { pr.clearLane (pat, l); pr.setParam (ids().lnMode[l], 1); }
                    if (r == 4) pr.setParam (ids().lnMode[l], 0);
                    safe->repaint();
                });
            }
            else proc.setParam (I.lnOn[l], proc.getParam (I.lnOn[l]) > 0.5f ? 0.0f : 1.0f);
            repaint();
            return;
        }
        const int steps = juce::jmax (1, (int) proc.getParam (I.lnSteps[l]));
        for (int s = 0; s < steps; ++s)
            if (cell (l, s, steps).contains (e.position))
            {
                toGrid (l);
                const int pat = proc.engine.display.pattern.load();
                dragLane = l;
                if (e.mods.isAltDown() || e.mods.isPopupMenu()) { dragGate = -1; mouseDrag (e); }
                else
                {
                    dragGate = proc.engine.shared.gate[pat][l][s].load() ? 0 : 1;
                    proc.engine.shared.gate[pat][l][s] = (uint8_t) dragGate;
                }
                repaint();
                return;
            }
    }
}

void LaneView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragLane < 0) return;
    const auto& I = ids();
    const int l = dragLane, steps = juce::jmax (1, (int) proc.getParam (I.lnSteps[l]));
    const int pat = proc.engine.display.pattern.load();
    for (int s = 0; s < steps; ++s)
    {
        const auto c = cell (l, s, steps);
        if (e.position.x < c.getX() || e.position.x >= c.getRight()) continue;
        if (dragGate < 0) proc.engine.shared.value[pat][l][s] = juce::jlimit (0.0f, 1.0f, (c.getBottom() - e.position.y) / c.getHeight());
        else proc.engine.shared.gate[pat][l][s] = (uint8_t) dragGate;
    }
    repaint();
}

// =============================================================================================== fx slots
FxSlot::FxSlot (MicrovibeProcessor& p, int i) : index (i), proc (p) {}

static int onParamOf (int type)
{
    const auto& I = ids();
    const int on[NUM_FXTYPES] = { -1, I.tpOn, I.vnOn, I.wbOn, I.dlOn, I.rvOn, I.ftOn, I.rsOn, I.rfOn, I.crOn, I.chOn, I.phOn, I.cpOn, I.stOn, I.sfOn };
    return juce::isPositiveAndBelow (type, (int) NUM_FXTYPES) ? on[type] : -1;
}

void FxSlot::paint (juce::Graphics& g)
{
    const int t = (int) proc.getParam (ids().fxSlot[index]);
    const int onP = onParamOf (t);
    const bool on = onP >= 0 && proc.getParam (onP) > 0.5f;
    bool dup = false;
    for (int s = 0; s < index; ++s) if ((int) proc.getParam (ids().fxSlot[s]) == t && t > 0) dup = true;
    auto r = getLocalBounds().toFloat().reduced (2);
    g.setColour (selected ? ui::panel2.brighter (0.15f) : ui::panel2); g.fillRoundedRectangle (r, 5);
    g.setColour (selected ? ui::fx : ui::line); g.drawRoundedRectangle (r, 5, selected ? 1.6f : 1.0f);
    // power LED
    auto led = juce::Rectangle<float> (r.getX() + 6, r.getCentreY() - 5, 10, 10);
    g.setColour (t == 0 ? ui::line : (on ? ui::fx : ui::dim.withAlpha (0.4f))); g.fillEllipse (led);
    g.setColour (t == 0 ? ui::dim : ui::text); g.setFont (ui::font (11.5f, true));
    g.drawFittedText (juce::String (index + 1) + "  " + fxNames()[t] + (dup ? " (dup)" : ""), r.withTrimmedLeft (20).toNearestInt(), juce::Justification::centredLeft, 1);
}

void FxSlot::mouseDown (const juce::MouseEvent& e)
{
    const int t = (int) proc.getParam (ids().fxSlot[index]);
    if (e.mods.isPopupMenu() || t == 0)
    {
        juce::PopupMenu m;
        for (int k = 0; k < NUM_FXTYPES; ++k) m.addItem (k + 1, fxNames()[k], true, k == t);
        juce::Component::SafePointer<FxSlot> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r) {
            if (safe == nullptr || r == 0) return;
            safe->proc.setParam (ids().fxSlot[safe->index], (float) (r - 1));
            const int onP = onParamOf (r - 1);
            if (onP >= 0) safe->proc.setParam (onP, 1);
            if (safe->onSelect) safe->onSelect (safe->index);
        });
        return;
    }
    if (e.x < 22) { const int onP = onParamOf (t); if (onP >= 0) proc.setParam (onP, proc.getParam (onP) > 0.5f ? 0.0f : 1.0f); repaint(); return; }
    if (onSelect) onSelect (index);
}

void FxSlot::mouseDrag (const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() < 6) return;
    if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
        if (! c->isDragAndDropActive()) c->startDragging ("slot:" + juce::String (index), this);
}

bool FxSlot::isInterestedInDragSource (const SourceDetails& d) { return d.description.toString().startsWith ("slot:"); }

void FxSlot::itemDropped (const SourceDetails& d)
{
    const int from = d.description.toString().fromFirstOccurrenceOf (":", false, false).getIntValue();
    if (from == index) return;
    const auto& I = ids();
    const float a = proc.getParam (I.fxSlot[from]), b = proc.getParam (I.fxSlot[index]);
    proc.setParam (I.fxSlot[from], b); proc.setParam (I.fxSlot[index], a);
    if (onSelect) onSelect (index);
}

// =============================================================================================== modulation panel
void ShapeView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1);
    g.setColour (ui::panel2); g.fillRoundedRectangle (r, 4);
    const auto& I = ids();
    const int shape = (int) proc.getParam (I.lfShape[lfo]);
    const float skew = proc.getParam (I.lfSkew[lfo]);
    juce::Path p;
    for (int i = 0; i <= 128; ++i)
    {
        const float ph = (float) i / 128.0f;
        const float q = ph < skew ? 0.5f * ph / skew : 0.5f + 0.5f * (ph - skew) / (1 - skew);
        float v = 0;
        switch (shape)
        {
            case 0: v = std::sin (mv::kTwoPi * q); break;
            case 1: v = q < 0.25f ? 4 * q : (q < 0.75f ? 2 - 4 * q : 4 * q - 4); break;
            case 2: v = 2 * q - 1; break;
            case 3: v = 1 - 2 * q; break;
            case 4: v = q < 0.5f ? 1.0f : -1.0f; break;
            case 5: v = std::sin ((float) ((int) (ph * 8) * 2.3f)) ; break;
            case 6: v = std::sin (ph * 9.0f) * std::cos (ph * 4.0f); break;
            default:
            {
                const float x = q * 16.0f; const int k = (int) x; const float f = x - k;
                const float a = proc.engine.shared.lfoShape[lfo][k & 15].load(), b = proc.engine.shared.lfoShape[lfo][(k + 1) & 15].load();
                v = (a + (b - a) * f) * 2 - 1;
            }
        }
        const float x = r.getX() + ph * r.getWidth(), y = r.getCentreY() - v * (r.getHeight() * 0.42f);
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (ui::mod); g.strokePath (p, juce::PathStrokeType (1.6f));
    if (shape == 7)
    {
        g.setColour (ui::dim); g.setFont (ui::font (9.5f));
        g.drawText ("disegna con il mouse", r.reduced (4), juce::Justification::topRight);
    }
    // live value
    const float v = proc.engine.display.src[srcLfo1 + lfo].load();
    g.setColour (ui::text); g.fillEllipse (r.getRight() - 9, r.getCentreY() - v * r.getHeight() * 0.42f - 3, 6, 6);
}

void ShapeView::edit (const juce::MouseEvent& e)
{
    if ((int) proc.getParam (ids().lfShape[lfo]) != 7) proc.setParam (ids().lfShape[lfo], 7);
    const int i = juce::jlimit (0, 15, (int) (e.position.x / (float) getWidth() * 16.0f));
    proc.engine.shared.lfoShape[lfo][i] = juce::jlimit (0.0f, 1.0f, 0.5f + (getHeight() * 0.5f - e.position.y) / (getHeight() * 0.84f));
    repaint();
}

void DestButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (ui::panel2); g.fillRoundedRectangle (r, 4);
    g.setColour (ui::line); g.drawRoundedRectangle (r, 4, 1);
    const int d = (int) proc.getParam (pid);
    g.setColour (d > 0 ? ui::text : ui::dim); g.setFont (ui::font (10.5f));
    g.drawFittedText (d > 0 ? def (dests()[(size_t) d - 1]).name : juce::String ("destination..."), getLocalBounds().reduced (4, 0), juce::Justification::centredLeft, 1, 0.7f);
}

void DestButton::mouseDown (const juce::MouseEvent&)
{
    juce::PopupMenu m;
    m.addItem (1, "None");
    std::map<juce::String, juce::PopupMenu> groups;
    juce::StringArray order;
    const auto& dl = dests();
    for (size_t k = 0; k < dl.size(); ++k)
    {
        const auto& d = def (dl[k]);
        juce::String grp = d.group;
        if (grp == "Perc" || grp == "Lanes" || grp == "LFO" || grp == "Envelope" || grp == "Random" || grp == "Follower" || grp == "Matrix")
        {
            // split big groups by their number ("Perc 3", "Lane 2", "Mod 12" ...)
            const juce::String n = d.name;
            grp = n.upToFirstOccurrenceOf (" ", false, false) + " " + n.fromFirstOccurrenceOf (" ", false, false).upToFirstOccurrenceOf (" ", false, false);
        }
        if (! order.contains (grp)) order.add (grp);
        groups[grp].addItem (2 + (int) k, d.name);
    }
    for (auto& gname : order) m.addSubMenu (gname, groups[gname]);
    juce::Component::SafePointer<DestButton> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMaximumNumColumns (3), [safe] (int r) {
        if (safe == nullptr || r == 0) return;
        safe->proc.setParam (safe->pid, (float) (r - 1));
        safe->repaint();
    });
}

MatrixRow::MatrixRow (MicrovibeProcessor& p, int s) : slot (s), proc (p), dst (p, ids().mxDst[s])
{
    const auto& I = ids();
    src = makeControl (p, I.mxSrc[s], ui::mod, " ");
    via = makeControl (p, I.mxVia[s], ui::mod, " ");
    curve = makeControl (p, I.mxCurve[s], ui::mod, " ");
    amtSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    amtSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    amtSlider.setColour (juce::Slider::trackColourId, ui::mod);
    amtSlider.setPopupDisplayEnabled (true, true, nullptr);
    amtSlider.setDoubleClickReturnValue (true, 0.0);
    amtAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, def (I.mxAmt[s]).id, amtSlider);
    addAndMakeVisible (amtSlider);
    for (auto* c : { src.get(), via.get(), curve.get() }) addAndMakeVisible (c);
    addAndMakeVisible (dst);
    addAndMakeVisible (clear);
    clear.onClick = [this] { proc.clearModulation (slot); repaint(); };
    clear.setTooltip ("Svuota lo slot");
}

void MatrixRow::resized()
{
    auto r = getLocalBounds().withTrimmedLeft (18);
    const int h = getHeight();
    auto place = [&] (juce::Component& c, int w) { c.setBounds (r.removeFromLeft (w).withHeight (h + 13)); };
    place (*src, 68); place (*via, 50);
    dst.setBounds (r.removeFromLeft (98).withSizeKeepingCentre (96, 22));
    clear.setBounds (r.removeFromRight (20).withSizeKeepingCentre (18, 18));
    place (*curve, 38);
    amtSlider.setBounds (r.reduced (2, 0));
}

void MatrixRow::paint (juce::Graphics& g)
{
    const int s = (int) proc.getParam (ids().mxSrc[slot]);
    g.setColour (s > 0 ? ui::sourceColour (s) : ui::dim); g.setFont (ui::font (10.0f, true));
    g.drawText (juce::String (slot + 1), 0, 0, 18, getHeight(), juce::Justification::centredRight);
}

ModPanel::ModPanel (MicrovibeProcessor& p)
    : tabs ({ "LFO", "ENV", "RND", "FOL/MIDI", "MATRIX", "TUNING" }, ui::mod), proc (p), grid (p, ui::mod), shape (p),
      degrees (p), prGrid (p, ui::tune)
{
    {
        const auto& I = ids();
        droneTitle.setText ("DRONE NOTES  /  CHORD PROGRESSION", juce::dontSendNotification);
        droneTitle.setFont (ui::font (12.0f, true)); droneTitle.setColour (juce::Label::textColourId, ui::tune);
        droneHint.setFont (ui::font (10.5f)); droneHint.setColour (juce::Label::textColourId, ui::dim);
        droneHint.setJustificationType (juce::Justification::topLeft);
        for (int k = 0; k < NSNAP; ++k)
        {
            auto b = std::make_unique<juce::TextButton> ("CHORD " + juce::String (k + 1));
            b->setColour (juce::TextButton::buttonOnColourId, ui::tune);
            b->setTooltip ("Modifica le note dell'accordo " + juce::String (k + 1));
            b->onClick = [this, k] { degrees.snap = k; updateDrone (false); };
            addChildComponent (*b); snapButtons.push_back (std::move (b));
        }
        addChildComponent (droneTitle); addChildComponent (droneHint); addChildComponent (degrees); addChildComponent (prGrid);
        prGrid.columns = 4; prGrid.rowH = 58;
        prGrid.setParams ({ I.prOn, I.prCount, I.prMode, I.prVoicing,
                            I.prSync, I.prDiv, I.prSecs, I.prGlide,
                            I.prLen[0], I.prLen[1], I.prLen[2], I.prLen[3] }, 4,
                          { "", "", "", "", "", "", "", "", "Length 1", "Length 2", "Length 3", "Length 4" });
    }
    addAndMakeVisible (tabs);
    tabs.onChange = [this] (int i) { page = i; sub = 0; buildPage(); };
    addAndMakeVisible (grid);
    addChildComponent (shape);
    addChildComponent (viewport);
    viewport.setViewedComponent (&matrixHolder, false);
    viewport.setScrollBarsShown (true, false);
    for (int k = 0; k < NSLOT; ++k) { rows.push_back (std::make_unique<MatrixRow> (p, k)); matrixHolder.addAndMakeVisible (*rows.back()); }
    addChildComponent (loadScl); addChildComponent (loadKbm); addChildComponent (scaleInfo);
    scaleInfo.setFont (ui::font (11.0f)); scaleInfo.setJustificationType (juce::Justification::topLeft);
    loadScl.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser> ("Scala .scl", juce::File(), "*.scl");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc) {
            const auto f = fc.getResult();
            if (! f.existsAsFile()) return;
            juce::String err;
            if (proc.loadScale (f, {}, err)) pendingScl = f;
            scaleInfo.setText (err.isEmpty() ? "Scala caricata: " + proc.scaleName() : "Errore: " + err, juce::dontSendNotification);
        });
    };
    loadKbm.onClick = [this] {
        if (! pendingScl.existsAsFile()) { scaleInfo.setText ("Carica prima un file .scl", juce::dontSendNotification); return; }
        chooser = std::make_unique<juce::FileChooser> ("Mappa .kbm", pendingScl.getParentDirectory(), "*.kbm");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc) {
            const auto f = fc.getResult();
            if (! f.existsAsFile()) return;
            juce::String err;
            proc.loadScale (pendingScl, f, err);
            scaleInfo.setText (err.isEmpty() ? "Scala + mappa caricate: " + proc.scaleName() : "Errore: " + err, juce::dontSendNotification);
        });
    };
    buildPage();
}

void ModPanel::showPage (int p) { tabs.select (p); }

void ModPanel::buildPage()
{
    const auto& I = ids();
    selectors.clear(); chips.clear();
    shape.setVisible (page == 0);
    viewport.setVisible (page == 4);
    grid.setVisible (page != 4);
    for (auto* c : { (juce::Component*) &loadScl, (juce::Component*) &loadKbm, (juce::Component*) &scaleInfo }) c->setVisible (page == 5);
    const int nSel = page == 0 ? NLFO : page == 1 ? NENV : page == 2 ? NRND : page == 3 ? NFOL : 0;
    for (int k = 0; k < nSel; ++k)
    {
        auto b = std::make_unique<juce::TextButton> (juce::String (k + 1));
        b->setClickingTogglesState (false);
        b->setToggleState (k == sub, juce::dontSendNotification);
        b->setColour (juce::TextButton::buttonOnColourId, ui::mod);
        b->onClick = [this, k] { sub = k; buildPage(); };
        addAndMakeVisible (*b); selectors.push_back (std::move (b));
    }
    auto addChip = [this] (int s) { auto c = std::make_unique<SourceChip> (s, shortSource (s)); addAndMakeVisible (*c); chips.push_back (std::move (c)); };
    std::vector<int> pids;
    grid.columns = 4;
    switch (page)
    {
        case 0:
            for (int k = 0; k < NLFO; ++k) addChip (srcLfo1 + k);
            shape.lfo = sub;
            pids = { I.lfShape[sub], I.lfSync[sub], I.lfDiv[sub], I.lfRate[sub], I.lfPhase[sub], I.lfSkew[sub], I.lfSmooth[sub], I.lfAmp[sub],
                     I.lfMode[sub], I.lfRetrig[sub], I.lfUni[sub] };
            break;
        case 1:
            for (int k = 0; k < NENV; ++k) addChip (srcEnv1 + k);
            pids = { I.enTrig[sub], I.enA[sub], I.enH[sub], I.enD[sub], I.enS[sub], I.enR[sub], I.enCurve[sub] };
            break;
        case 2:
            for (int k = 0; k < NRND; ++k) addChip (srcRnd1 + k);
            pids = { I.rnMode[sub], I.rnSync[sub], I.rnDiv[sub], I.rnRate[sub], I.rnSmooth[sub] };
            break;
        case 3:
            for (int k = 0; k < NFOL; ++k) addChip (srcFol1 + k);
            for (int k = 0; k < NLANE; ++k) addChip (srcLane1 + k);
            for (int s = srcVel; s <= srcPB; ++s) addChip (s);
            pids = { I.foSrc[sub], I.foAtt[sub], I.foRel[sub], I.foGain[sub] };
            break;
        case 5:
            pids = { I.tuScale, I.tuRoot, I.tuSnap, I.tuMts };
            grid.columns = 4;
            break;
        default: break;
    }
    if (page != 4) grid.setParams (pids, grid.columns);
    refresh();
    resized();
}

void ModPanel::refresh()
{
    for (auto& c : chips) { c->value = proc.engine.display.src[c->src].load(); c->repaint(); }
    if (shape.isVisible()) shape.repaint();
    if (page == 5)
    {
        const auto& I = ids();
        juce::String s;
        s << "Scale: " << tuning::builtinNames()[(int) proc.getParam (I.tuScale)];
        if ((int) proc.getParam (I.tuScale) == tuning::builtinNames().size() - 1) s << "  (" << (proc.scaleName().isEmpty() ? juce::String ("no file") : proc.scaleName()) << ")";
        s << "\nSteps: " << proc.engine.tuning.currentSize();
        s << "\nMTS-ESP: " << (proc.engine.display.mts.load() ? "master active, global tuning" : "no master");
        if (! droneShown) s << "\n\nLa scala quantizza percussioni, risonatori, drone, grani,\npitch del glitch, del delay e dello shimmer (Snap).\n\nCon TEXTURE > Chord = Scale qui compaiono le note del drone\ne la progressione di 4 accordi.";
        if (! scaleInfo.getText().startsWith ("Errore") && ! scaleInfo.getText().startsWith ("Carica")) scaleInfo.setText (s, juce::dontSendNotification);
    }
    updateDrone (false);
}

void ModPanel::updateDrone (bool force)
{
    const auto& I = ids();
    const bool show = page == 5 && (int) proc.getParam (I.txChord) == 7;
    const int sc = (int) proc.getParam (I.tuScale), rt = (int) proc.getParam (I.tuRoot);
    const auto scale = show ? proc.guiScale() : tuning::Scale();
    if (show != droneShown || force || (show && (sc != shownScale || rt != shownRoot || scale.size != shownSize)))
    {
        droneShown = show; shownScale = sc; shownRoot = rt; shownSize = scale.size;
        for (auto* c : { (juce::Component*) &droneTitle, (juce::Component*) &droneHint, (juce::Component*) &degrees, (juce::Component*) &prGrid })
            c->setVisible (show);
        for (auto& b : snapButtons) b->setVisible (show);
        if (show) degrees.setScale (scale, rt);
        resized();
    }
    if (! show) return;
    const int playing = proc.engine.display.snapshot.load();
    const bool progOn = proc.getParam (I.prOn) > 0.5f;
    for (int k = 0; k < NSNAP; ++k)
    {
        auto& b = *snapButtons[(size_t) k];
        b.setToggleState (k == degrees.snap, juce::dontSendNotification);
        b.setButtonText ((progOn && k == playing ? "> " : "") + juce::String ("CHORD ") + juce::String (k + 1));
        b.setEnabled (! progOn || k < (int) proc.getParam (I.prCount) || k == degrees.snap);
    }
    droneHint.setText ("Clic sulle note per comporre l'accordo " + juce::String (degrees.snap + 1)
                       + " (vale per drone e letto risonante).  " + (progOn ? "> = accordo che sta suonando." : "Progressione spenta: suona l'accordo 1."),
                       juce::dontSendNotification);
    degrees.repaint();
}

int DegreeGrid::preferredHeight (int) const
{
    const int rows = (count() + columns() - 1) / columns();
    return rows * 26;
}

juce::Rectangle<float> DegreeGrid::cell (int d) const
{
    const int c = columns();
    const float w = (float) getWidth() / (float) c;
    return { (float) (d % c) * w, (float) (d / c) * 26.0f, w, 24.0f };
}

juce::String DegreeGrid::label (int d) const
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const double c = scale.cents[(size_t) d];
    const double semis = c / 100.0;
    if (std::abs (scale.period - 1200.0) < 0.01 && std::abs (semis - std::round (semis)) < 0.01)
        return names[(root + (int) std::round (semis)) % 12];
    return juce::String (d + 1) + ":" + juce::String ((int) std::round (c));
}

void DegreeGrid::paint (juce::Graphics& g)
{
    const uint64_t m = proc.engine.shared.snapMask[snap].load();
    for (int d = 0; d < count(); ++d)
    {
        auto r = cell (d).reduced (1.5f);
        const bool on = (m >> d) & 1u;
        g.setColour (on ? ui::tune : ui::panel2); g.fillRoundedRectangle (r, 4);
        g.setColour (ui::line); g.drawRoundedRectangle (r, 4, 1);
        g.setColour (on ? ui::bg : ui::text); g.setFont (ui::font (count() > 24 ? 9.0f : 10.5f, true));
        g.drawFittedText (label (d), r.toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }
    if (m == 0)
    {
        g.setColour (ui::dim); g.setFont (ui::font (10.0f));
        g.drawText ("nessuna nota: triade minore automatica", getLocalBounds().removeFromBottom (12), juce::Justification::centredRight);
    }
}

void DegreeGrid::mouseDown (const juce::MouseEvent& e)
{
    for (int d = 0; d < count(); ++d)
        if (cell (d).contains (e.position)) { proc.toggleDroneDegree (snap, d); repaint(); return; }
}

void ModPanel::resized()
{
    auto r = getLocalBounds().reduced (8);
    r.removeFromTop (22);
    tabs.setBounds (r.removeFromTop (24));
    r.removeFromTop (6);
    if (! selectors.empty())
    {
        auto row = r.removeFromTop (24);
        const int w = row.getWidth() / juce::jmax (8, (int) selectors.size());
        for (auto& b : selectors) b->setBounds (row.removeFromLeft (w).reduced (2, 0));
        r.removeFromTop (6);
    }
    if (! chips.empty())
    {
        const int per = 6, cw = r.getWidth() / per;
        const int rowsN = ((int) chips.size() + per - 1) / per;
        auto area = r.removeFromTop (rowsN * 26);
        for (size_t i = 0; i < chips.size(); ++i)
            chips[i]->setBounds (area.getX() + (int) (i % per) * cw + 2, area.getY() + (int) (i / per) * 26 + 2, cw - 4, 22);
        r.removeFromTop (6);
    }
    if (page == 0) { shape.setBounds (r.removeFromTop (92)); r.removeFromTop (6); }
    if (page == 4)
    {
        r.removeFromTop (14);
        viewport.setBounds (r);
        const int rh = 30;
        matrixHolder.setSize (r.getWidth() - 10, rh * NSLOT);
        for (int k = 0; k < NSLOT; ++k) rows[(size_t) k]->setBounds (0, k * rh, matrixHolder.getWidth(), rh - 2);
        return;
    }
    if (page == 5)
    {
        grid.rowH = 60;
        grid.setBounds (r.removeFromTop (62));
        auto b = r.removeFromTop (28);
        loadScl.setBounds (b.removeFromLeft (150).reduced (2)); loadKbm.setBounds (b.removeFromLeft (90).reduced (2));
        r.removeFromTop (6);
        scaleInfo.setBounds (r.removeFromTop (droneShown ? 58 : 160));
        if (droneShown)
        {
            r.removeFromTop (4);
            droneTitle.setBounds (r.removeFromTop (18));
            auto sb = r.removeFromTop (24);
            const int w = sb.getWidth() / NSNAP;
            for (auto& bt : snapButtons) bt->setBounds (sb.removeFromLeft (w).reduced (2, 0));
            r.removeFromTop (4);
            droneHint.setBounds (r.removeFromTop (28));
            degrees.setBounds (r.removeFromTop (degrees.preferredHeight (r.getWidth())).withWidth (r.getWidth()));
            degrees.setBounds (r.getX(), degrees.getY(), r.getWidth(), degrees.preferredHeight (r.getWidth()));
            r.removeFromTop (6);
            prGrid.setBounds (r.removeFromTop (3 * 58));
        }
        return;
    }
    grid.rowH = 64;
    grid.setBounds (r);
}

void ModPanel::paint (juce::Graphics& g)
{
    g.setColour (ui::panel); g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (3), 6);
    g.setColour (ui::mod); g.setFont (ui::font (13.0f, true));
    g.drawText ("MODULATION", getLocalBounds().reduced (12, 6).removeFromTop (18), juce::Justification::left);
    g.setColour (ui::dim); g.setFont (ui::font (10.0f));
    g.drawText ("trascina una sorgente su qualsiasi manopola", getLocalBounds().reduced (12, 6).removeFromTop (18), juce::Justification::right);
    if (page == 4)
    {
        g.setColour (ui::dim); g.setFont (ui::font (9.5f));
        auto r = getLocalBounds().reduced (8).withTrimmedTop (52).removeFromTop (14);
        r.removeFromLeft (20);
        for (auto [t, w] : { std::pair<const char*, int> { "SOURCE", 68 }, { "VIA (x)", 50 }, { "DESTINATION", 98 }, { "CURVE", 38 }, { "AMOUNT", 60 } })
            g.drawText (t, r.removeFromLeft (w), juce::Justification::centred);
    }
}

// =============================================================================================== editor
MicrovibeEditor::MicrovibeEditor (MicrovibeProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      srcTabs ({ "IN / CLOCK", "TEXTURE", "PERC", "SHEPARD" }, ui::src), srcGrid (p, ui::src),
      lanes (p), ctrTabs ({ "LANE", "GLITCH", "GLITCH EDIT" }, ui::glitch), ctrGrid (p, ui::glitch),
      fxGrid (p, ui::fx), modPanel (p)
{
    setLookAndFeel (&look);
    addAndMakeVisible (content);
    const auto& I = ids();

    // header
    content.addAndMakeVisible (presetBox);
    content.addAndMakeVisible (prev); content.addAndMakeVisible (next);
    refreshPresetBox();
    presetBox.onChange = [this] { const int id = presetBox.getSelectedId(); if (id > 0 && id - 1 != proc.presetIndex()) proc.loadPreset (id - 1); };
    prev.onClick = [this] { const int n = (int) PresetLibrary::get().all().size(); proc.loadPreset ((proc.presetIndex() - 1 + n) % n); };
    next.onClick = [this] { const int n = (int) PresetLibrary::get().all().size(); proc.loadPreset ((proc.presetIndex() + 1) % n); };
    for (int k = 0; k < NMAC; ++k)
    {
        headerControls.push_back (makeControl (p, I.macro[k], ui::text, "MACRO " + juce::String (k + 1)));
        macroChips.push_back (std::make_unique<SourceChip> (srcMac1 + k, "M" + juce::String (k + 1)));
        content.addAndMakeVisible (*macroChips.back());
    }
    for (auto [pid, cap] : { std::pair<int, const char*> { I.density, "DENSITY" }, { I.mix, "DRY/WET" }, { I.width, "WIDTH" }, { I.out, "OUTPUT" }, { I.norm, "NORM" } })
        headerControls.push_back (makeControl (p, pid, ui::glitch, cap));
    for (auto& c : headerControls) content.addAndMakeVisible (*c);

    // sources
    content.addAndMakeVisible (srcTabs); content.addAndMakeVisible (srcGrid);
    srcTabs.onChange = [this] (int i) { showSource (i); };
    for (int v = 0; v < NPERC; ++v)
    {
        auto b = std::make_unique<juce::TextButton> ("V" + juce::String (v + 1));
        b->setColour (juce::TextButton::buttonOnColourId, ui::src);
        b->onClick = [this, v] { voice = v; showSource (2); };
        content.addChildComponent (*b); voiceButtons.push_back (std::move (b));
    }

    // centre
    content.addAndMakeVisible (lanes); content.addAndMakeVisible (ctrTabs); content.addAndMakeVisible (ctrGrid);
    lanes.onSelectLane = [this] (int) { if (ctrPage == 0) showLaneParams(); };
    ctrTabs.onChange = [this] (int i) { showCentre (i); };
    for (auto* b : { &laneRandom, &laneClear, &patCopy }) content.addChildComponent (*b);
    laneRandom.onClick = [this] { proc.randomizeLane (proc.engine.display.pattern.load(), lanes.selectedLane); proc.setParam (ids().lnMode[lanes.selectedLane], 1); };
    laneClear.onClick = [this] { proc.clearLane (proc.engine.display.pattern.load(), lanes.selectedLane); proc.setParam (ids().lnMode[lanes.selectedLane], 1); };
    patCopy.onClick = [this] { const int from = proc.engine.display.pattern.load(); for (int t = 0; t < NPAT; ++t) if (t != from) proc.copyPattern (from, t); };

    // fx
    for (int s = 0; s < NFX; ++s)
    {
        slots.push_back (std::make_unique<FxSlot> (p, s));
        slots.back()->onSelect = [this] (int i) { showFx (i); };
        content.addAndMakeVisible (*slots.back());
    }
    content.addAndMakeVisible (fxGrid);
    content.addAndMakeVisible (fxTitle);
    fxTitle.setFont (ui::font (12.0f, true)); fxTitle.setColour (juce::Label::textColourId, ui::fx);

    content.addAndMakeVisible (modPanel);

    proc.onPresetChanged = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MicrovibeEditor> (this)] { if (safe) { safe->shownPreset = -2; safe->showFx (safe->fxSel); } }); };

    showSource (0); showCentre (0); showFx (0);
    setResizable (true, true);
    constrainer.setFixedAspectRatio ((double) W / H);
    constrainer.setSizeLimits (W * 6 / 10, H * 6 / 10, W * 2, H * 2);
    setConstrainer (&constrainer);
    setSize (W, H);
    startTimerHz (30);
}

MicrovibeEditor::~MicrovibeEditor()
{
    proc.onPresetChanged = nullptr;
    setLookAndFeel (nullptr);
}

void MicrovibeEditor::refreshPresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    const auto& all = PresetLibrary::get().all();
    juce::String cat;
    for (int i = 0; i < (int) all.size(); ++i)
    {
        if (all[(size_t) i].category != cat) { cat = all[(size_t) i].category; presetBox.addSectionHeading (cat); }
        presetBox.addItem (all[(size_t) i].name, i + 1);
    }
    presetBox.setSelectedId (proc.presetIndex() + 1, juce::dontSendNotification);
}

void MicrovibeEditor::showTab (const juce::String& spec)
{
    const int n = spec.fromFirstOccurrenceOf (":", false, false).getIntValue();
    if (spec.startsWith ("src:")) srcTabs.select (n);
    else if (spec.startsWith ("ctr:")) ctrTabs.select (n);
    else if (spec.startsWith ("fx:")) showFx (n);
    else if (spec.startsWith ("mod:")) modPanel.showPage (n);
}

void MicrovibeEditor::showSource (int i)
{
    srcPage = i;
    const auto& I = ids();
    std::vector<int> pids;
    for (size_t v = 0; v < voiceButtons.size(); ++v) { voiceButtons[v]->setVisible (i == 2); voiceButtons[v]->setToggleState ((int) v == voice, juce::dontSendNotification); }
    switch (i)
    {
        case 0: pids = { I.inLevel, I.inGate, I.trigThr, I.trigSrc, I.bpm, I.pattern, I.chain, I.chainEnd }; break;
        case 1: pids = { I.txOn, I.txLevel, I.txBed, I.txNoise, I.txResQ, I.txDrone, I.txDrift, I.txOct, I.txChord, I.txTilt, I.txGrain, I.txGSrc,
                         I.txDens, I.txSize, I.txSpray, I.txPitch, I.txPRand, I.txSpread, I.txRev, I.txWin, I.txFreeze }; break;
        case 2:
        {
            const int v = voice;
            pids = { I.pcOn, I.pcLevel, I.pcLane[v], I.pcModel[v],
                     I.pcExc[v], I.pcTune[v], I.pcDecay[v], I.pcLvl[v],
                     I.pcFType[v], I.pcCut[v], I.pcRes[v], I.pcPan[v],
                     I.pcMat[v], I.pcInh[v], I.pcPos[v], I.pcHum[v],
                     I.pcProb[v], I.pcEvery[v], I.pcOffs[v] };
            break;
        }
        default: pids = { I.shOn, I.shLevel, I.shPartials, I.shInterval, I.shCents, I.shSync, I.shBars, I.shRate, I.shDir, I.shBase, I.shWidth, I.shWave }; break;
    }
    srcGrid.rowH = 62;
    juce::StringArray caps;
    if (i == 2) caps = { "", "Perc Level", "", "", "", "", "", "Voice Level" };
    srcGrid.setParams (pids, 4, caps);
    resized();
}

void MicrovibeEditor::showLaneParams()
{
    const auto& I = ids();
    const int l = lanes.selectedLane;
    ctrGrid.setParams ({ I.lnOn[l], I.lnMode[l], I.lnSteps[l], I.lnPulses[l], I.lnRot[l], I.lnDiv[l], I.lnRatio[l], I.lnSwing[l],
                         I.lnSync[l], I.lnHz[l], I.lnProb[l], I.lnRatch[l], I.lnRProb[l], I.lnSrc[l], I.lnCond[l], I.lnHum[l] }, 8);
}

void MicrovibeEditor::showCentre (int i)
{
    ctrPage = i;
    const auto& I = ids();
    for (auto* b : { &laneRandom, &laneClear, &patCopy }) b->setVisible (i == 0);
    ctrGrid.rowH = 62;
    if (i == 0) showLaneParams();
    else if (i == 1)
    {
        std::vector<int> p { I.glOn, I.glMix, I.glLane, I.glProb, I.glLen, I.glXfade, -1, -1 };
        for (int g = 0; g < NUM_GLITCH; ++g) p.push_back (I.glW[g]);
        juce::StringArray caps { "", "", "", "", "", "", "", "" };
        for (int g = 0; g < NUM_GLITCH; ++g) caps.add (glitchNames()[g]);
        ctrGrid.setParams (p, 8, caps);
    }
    else ctrGrid.setParams ({ I.glRptDiv, I.glRptDecay, I.glRptPitch, I.glRptShrink, I.glSlices, I.glStopTime, I.glGateShape, I.glGateDiv,
                              I.glBits, I.glSRate, I.glPitch, I.glFreezeSize }, 8);
    resized();
}

void MicrovibeEditor::showFx (int s)
{
    fxSel = s;
    for (auto& sl : slots) { sl->selected = sl->index == s; sl->repaint(); }
    const auto& I = ids();
    const int t = (int) proc.getParam (I.fxSlot[s]);
    std::vector<int> p;
    switch (t)
    {
        case fxTape:    p = { I.tpOn, I.tpMix, I.tpMode, I.tpSpeed, I.tpDrive, I.tpSat, I.tpBias, I.tpComp, I.tpAge, I.tpHiss, I.tpDrop, I.tpFlut }; break;
        case fxVinyl:   p = { I.vnOn, I.vnMix, I.vnYear, I.vnRpm, I.vnWear, I.vnWarp, I.vnBrake, I.vnMono, I.vnDust, I.vnCrackle, I.vnScratch, I.vnRumble, I.vnHum, I.vnHumHz, I.vnNoise }; break;
        case fxWobble:  p = { I.wbOn, I.wbMix, I.wbAmt, I.wbBal, I.wbWow, I.wbFlut, I.wbFlux, I.wbSpread }; break;
        case fxDelay:   p = { I.dlOn, I.dlMix, I.dlMode, I.dlSync, I.dlDiv, I.dlMs, I.dlFb, I.dlTaps, I.dlLp, I.dlHp, I.dlSat, I.dlWob, I.dlPitch, I.dlDuck, I.dlFreeze }; break;
        case fxReverb:  p = { I.rvOn, I.rvMix, I.rvMode, I.rvSize, I.rvDecay, I.rvPre, I.rvDamp, I.rvDiff, I.rvMod, I.rvLow, I.rvShim, I.rvShInt, I.rvDuck, I.rvFreeze }; break;
        case fxFilter:  p = { I.ftOn, I.ftMix, I.ftType, I.ftSlope, I.ftCut, I.ftRes, I.ftDrive, I.ftVowel }; break;
        case fxReso:    p = { I.rsOn, I.rsMix, I.rsChord, I.rsOct, I.rsDecay, I.rsBright, I.rsSpread }; break;
        case fxRing:    p = { I.rfOn, I.rfMix, I.rfMode, I.rfFreq, I.rfShift }; break;
        case fxCrush:   p = { I.crOn, I.crMix, I.crBits, I.crRate, I.crJitter }; break;
        case fxChorus:  p = { I.chOn, I.chMix, I.chRate, I.chDepth, I.chVoices }; break;
        case fxPhaser:  p = { I.phOn, I.phMix, I.phRate, I.phDepth, I.phFb, I.phStages, I.phCentre }; break;
        case fxComp:    p = { I.cpOn, I.cpMix, I.cpThr, I.cpRatio, I.cpAtt, I.cpRel, I.cpGain, I.cpTAtk, I.cpTSus }; break;
        case fxStereo:  p = { I.stOn, I.stWidth, I.stHaas, I.stPanRate, I.stPanDepth, I.stMonoHz }; break;
        case fxShepard: p = { I.sfOn, I.sfMix, I.sfMode, I.sfSync, I.sfBars, I.sfRate, I.sfDir, I.sfRes, I.sfBands }; break;
        default: break;
    }
    fxTitle.setText ("SLOT " + juce::String (s + 1) + "  -  " + fxNames()[t].toUpperCase()
                     + (t == 0 ? "   (clic sullo slot per scegliere un modulo; trascina gli slot per riordinare)" : "   (trascina gli slot per cambiare l'ordine; tasto destro = cambia modulo)"),
                     juce::dontSendNotification);
    fxGrid.rowH = 64;
    fxGrid.setParams (p, 8);
}

void MicrovibeEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::bg);
    const float s = (float) getWidth() / (float) W;
    g.addTransform (juce::AffineTransform::scale (s));
    auto panel = [&] (juce::Rectangle<int> r, juce::Colour c, const juce::String& title)
    {
        g.setColour (ui::panel); g.fillRoundedRectangle (r.toFloat().reduced (3), 6);
        g.setColour (c); g.setFont (ui::font (13.0f, true));
        g.drawText (title, r.reduced (12, 6).removeFromTop (18), juce::Justification::left);
    };
    // header
    g.setColour (ui::panel); g.fillRect (rHeader);
    g.setColour (ui::text); g.setFont (ui::font (24.0f, true));
    g.drawText ("MICROVIBE", 16, 10, 200, 30, juce::Justification::left);
    g.setColour (ui::text); g.setFont (ui::font (11.0f));
    g.drawText ("TMFU audiovibe  -  glitch / texture / fx", 16, 40, 260, 16, juce::Justification::left);
    if (proc.presetIndex() >= 0 && proc.presetIndex() < (int) PresetLibrary::get().all().size())
    {
        const auto& pr = PresetLibrary::get().all()[(size_t) proc.presetIndex()];
        g.drawFittedText (pr.category + "  -  " + pr.notes, 290, 48, 330, 26, juce::Justification::topLeft, 2, 0.8f);
    }
    // meter
    auto m = juce::Rectangle<float> ((float) W - 26, 10, 14, 60);
    g.setColour (ui::panel2); g.fillRoundedRectangle (m, 3);
    for (int c = 0; c < 2; ++c)
    {
        const float v = juce::jlimit (0.0f, 1.0f, (mv::gainToDb (c ? meterR : meterL) + 48.0f) / 48.0f);
        auto b = juce::Rectangle<float> (m.getX() + 2 + c * 5.5f, m.getBottom() - 2 - v * (m.getHeight() - 4), 4.5f, v * (m.getHeight() - 4));
        g.setColour (v > 0.97f ? juce::Colour (0xffff5a5a) : ui::src); g.fillRect (b);
    }
    if (proc.engine.display.limiting.load()) { g.setColour (juce::Colour (0xffff5a5a)); g.fillEllipse (m.getX() + 3, m.getY() - 7, 6, 6); }

    panel (rSrc, ui::src, "SOURCES");
    panel (rCtr, ui::glitch, "LANES  /  GLITCH");
    panel (rFx, ui::fx, "FX CHAIN");
    // centre extras
    g.setColour (ui::dim); g.setFont (ui::font (10.0f));
    const juce::String pat = juce::String::charToString ((juce::juce_wchar) ('A' + proc.engine.display.pattern.load()));
    const int gt = proc.engine.display.glitchType.load();
    g.drawText ("pattern " + pat + (gt >= 0 ? "   glitch: " + glitchNames()[gt] : juce::String()) + "   beat " + juce::String (proc.engine.display.beat.load(), 1),
                rCtr.reduced (12, 6).removeFromTop (18), juce::Justification::right);
    if (srcPage == 0)
    {
        g.setColour (ui::dim);
        g.drawFittedText ("Clic su una cella = acceso/spento (la corsia passa in Grid).  Alt + trascina = valore dello step.\n"
                          "MIDI: C1-G#1 = tipi di glitch (tenuti), C2-F2 = percussioni 1-6.",
                          rSrc.reduced (14).withTrimmedTop (250).removeFromTop (80), juce::Justification::topLeft, 4);
    }
}

void MicrovibeEditor::resized()
{
    const float s = (float) getWidth() / (float) W;
    content.setTransform (juce::AffineTransform::scale (s));
    content.setBounds (0, 0, W, H);
    rHeader = { 0, 0, W, 84 };
    rSrc = { 0, 84, 310, 470 };
    rCtr = { 310, 84, 600, 470 };
    rFx = { 0, 554, 910, H - 554 };
    rMod = { 910, 84, W - 910, H - 84 };

    // header
    prev.setBounds (290, 14, 26, 26);
    presetBox.setBounds (320, 14, 270, 26);
    next.setBounds (594, 14, 26, 26);
    int x = 640;
    for (int k = 0; k < NMAC; ++k)
    {
        headerControls[(size_t) k]->setBounds (x, 4, 62, 62);
        macroChips[(size_t) k]->setBounds (x + 12, 64, 38, 17);
        x += 64;
    }
    x += 12;
    for (size_t i = NMAC; i < headerControls.size(); ++i) { headerControls[i]->setBounds (x, 6, 66, 70); x += 68; }

    // sources
    auto r = rSrc.reduced (10).withTrimmedTop (22);
    srcTabs.setBounds (r.removeFromTop (24));
    r.removeFromTop (6);
    if (srcPage == 2)
    {
        auto vb = r.removeFromTop (24);
        const int w = vb.getWidth() / NPERC;
        for (auto& b : voiceButtons) b->setBounds (vb.removeFromLeft (w).reduced (2, 0));
        r.removeFromTop (6);
    }
    srcGrid.setBounds (r);

    // centre
    r = rCtr.reduced (10).withTrimmedTop (22);
    lanes.setBounds (r.removeFromTop (200));
    r.removeFromTop (6);
    ctrTabs.setBounds (r.removeFromTop (24));
    r.removeFromTop (6);
    if (ctrPage == 0)
    {
        auto b = r.removeFromBottom (26);
        laneRandom.setBounds (b.removeFromLeft (90).reduced (2)); laneClear.setBounds (b.removeFromLeft (90).reduced (2));
        patCopy.setBounds (b.removeFromLeft (190).reduced (2));
    }
    ctrGrid.setBounds (r);

    // fx
    r = rFx.reduced (10).withTrimmedTop (22);
    auto sr = r.removeFromTop (34);
    const int sw = sr.getWidth() / NFX;
    for (auto& sl : slots) sl->setBounds (sr.removeFromLeft (sw));
    r.removeFromTop (4);
    fxTitle.setBounds (r.removeFromTop (20));
    fxGrid.setBounds (r);

    modPanel.setBounds (rMod);
}

void MicrovibeEditor::timerCallback()
{
    if (shownPreset != proc.presetIndex())
    {
        shownPreset = proc.presetIndex();
        presetBox.setSelectedId (shownPreset + 1, juce::dontSendNotification);
        showSource (srcPage); showCentre (ctrPage); showFx (fxSel);
        repaint();
    }
    meterL = proc.engine.display.peakL.load(); meterR = proc.engine.display.peakR.load();
    lanes.repaint();
    for (auto& sl : slots) sl->repaint();
    for (auto& c : macroChips) { c->value = proc.engine.display.src[c->src].load(); c->repaint(); }
    modPanel.refresh();
    // knobs with modulation (repaint the visible grids: cheap at this size)
    srcGrid.repaint(); ctrGrid.repaint(); fxGrid.repaint();
    for (auto& c : headerControls) c->repaint();
    const float sc = (float) getWidth() / (float) W;
    auto scaled = [sc] (juce::Rectangle<int> r) { return r.toFloat().transformedBy (juce::AffineTransform::scale (sc)).getSmallestIntegerContainer(); };
    repaint (scaled (rHeader.withTrimmedLeft (W - 40))); repaint (scaled (rCtr.withHeight (30)));
}
