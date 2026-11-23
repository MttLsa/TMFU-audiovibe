#include "PluginEditor.h"
#include "BinaryData.h"
#include <set>

using namespace params;
using skins::Skin;

// =====================================================================================================
//  Fonts: Inter Regular / SemiBold (SIL OFL) for all text + relief
// =====================================================================================================
juce::Typeface::Ptr ui::typeface (bool bold)
{
    static juce::Typeface::Ptr reg = juce::Typeface::createSystemTypefaceFor (BinaryData::InterRegular_ttf, BinaryData::InterRegular_ttfSize);
    static juce::Typeface::Ptr bld = juce::Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_ttf, BinaryData::InterSemiBold_ttfSize);
    return bold ? bld : reg;
}
juce::Font ui::font (float h, bool bold) { return juce::Font (juce::FontOptions (typeface (bold)).withHeight (h)); }

void LogoSplash::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.92f));
    auto r = getLocalBounds().toFloat().withSizeKeepingCentre (760.0f, 300.0f);
    logo::drawFamPat (g, r.removeFromTop (250.0f).reduced (60.0f, 40.0f));
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
FamPatLook::FamPatLook() { setSkin (skins::find ("neutral-g", false)); }

juce::Typeface::Ptr FamPatLook::getTypefaceForFont (const juce::Font& f) { return ui::typeface (f.isBold()); }

void FamPatLook::setSkin (const Skin& s)
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

void FamPatLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& sl)
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

void FamPatLook::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& sl)
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

void FamPatLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto& s = *skin;
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    ui::relief (g, r, on ? s.accent : s.body2.brighter (over ? 0.08f : 0.0f).darker (down ? 0.2f : 0.0f), 3.0f, ! down);
}

void FamPatLook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (3, 1), juce::Justification::centred, 1, 0.8f);
}

void FamPatLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& cb)
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
    // fine resolution along the control's travel: 1000 steps on knobs, 10000 on faders (the audio is smoothed too)
    const double step = fader ? 0.0001 : 0.001;
    const double p = juce::jlimit (0.0, 1.0, std::round (valueToProportionOfLength (v) / step) * step);
    return juce::jlimit (getMinimum(), getMaximum(), proportionOfLengthToValue (p));
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
    setInterceptsMouseClicks (false, true);   // only the knob / menu / button takes clicks: the empty rest of the cell passes them on
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

void Control::restrictChoices (juce::AudioProcessorValueTreeState& st, const std::vector<int>& allowed)
{
    const auto& d = def (pid);
    auto* prm = st.getParameter (d.id);
    if (prm == nullptr || d.kind != C) return;
    ca.reset();
    combo.clear (juce::dontSendNotification);
    std::vector<int> items = allowed;
    const int cur = (int) std::lround (st.getRawParameterValue (d.id)->load());
    if (std::find (items.begin(), items.end(), cur) == items.end()) items.push_back (cur);   // a preset may use another one
    for (int i : items) if (juce::isPositiveAndBelow (i, d.choices.size())) combo.addItem (d.choices[i], i + 1);   // id = index + 1
    pa = std::make_unique<juce::ParameterAttachment> (*prm, [this] (float v) { combo.setSelectedId ((int) std::lround (v) + 1, juce::dontSendNotification); });
    combo.onChange = [this] { if (pa != nullptr && combo.getSelectedId() > 0) pa->setValueAsCompleteGesture ((float) (combo.getSelectedId() - 1)); };
    pa->sendInitialUpdate();
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
    auto* lf = dynamic_cast<FamPatLook*> (&getLookAndFeel());
    g.setColour (lf && lf->skin ? lf->skin->label : ui::dim);
    g.setFont (ui::font (10.0f, true));
    g.drawFittedText (caption.toUpperCase(), getLocalBounds().removeFromBottom (15).reduced (1, 0), juce::Justification::centred, 1, 0.55f);   // long original names shrink to fit their cell
}

// =====================================================================================================
//  Preset browser: one list ordered by year, "Preset (Genre - Song)"
// =====================================================================================================
PresetBrowser::PresetBrowser (FamPatProcessor& p) : proc (p)
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
    sortBox.addItemList ({ "Sort: Year", "Sort: Song", "Sort: Preset", "Sort: Artist" }, 1);
    sortBox.setSelectedId (1, juce::dontSendNotification);
    sortBox.setTooltip ("ordine dei preset: anno, titolo della canzone, nome del preset, artista");
    sortBox.onChange = [this] { apply(); resized(); };
    addAndMakeVisible (sortBox);
    userBtn.setClickingTogglesState (true);
    userBtn.setTooltip ("i tuoi preset (salvati con SAVE); tasto destro su un preset: elimina / mostra nella cartella");
    userBtn.onClick = [this] { search.setText ({}, juce::dontSendNotification); apply(); };
    addAndMakeVisible (userBtn);
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
    g.drawText (userMode() ? juce::String ((int) rows.size()) + " user presets  -  " + FamPatProcessor::userPresetFolder().getFullPathName()
                           : juce::String ((int) rows.size()) + " presets, ordered by " + juce::String (sortBox.getSelectedId() == 2 ? "song" : sortBox.getSelectedId() == 3 ? "preset name" : sortBox.getSelectedId() == 4 ? "artist" : "year"), getLocalBounds().reduced (10).removeFromTop (28).withTrimmedRight (90),
                juce::Justification::centredRight);
}

void PresetBrowser::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto top = r.removeFromTop (28);
    close.setBounds (top.removeFromRight (80));
    top.removeFromRight (180);
    search.setBounds (top.removeFromLeft (340));
    top.removeFromLeft (10);
    userBtn.setBounds (top.removeFromLeft (70)); top.removeFromLeft (10);
    sortBox.setBounds (top.removeFromLeft (130)); top.removeFromLeft (10);
    sortBox.setVisible (! userMode());
    const bool byYear = sortBox.getSelectedId() <= 1;
    for (auto* b : decadeBtns) { b->setBounds (top.removeFromLeft (64)); top.removeFromLeft (3); b->setVisible (! userMode() && byYear); }
    r.removeFromTop (6);
    info.setBounds (r.removeFromBottom (58));
    list.setBounds (r);
}

void PresetBrowser::apply()
{
    const juce::ScopedValueSetter<bool> guard (syncing, true);   // rebuilding the list must not load a preset
    const auto& all = PresetLibrary::get().all();
    rows.clear();
    const auto q = search.getText().trim().toLowerCase();
    for (auto* b : decadeBtns) b->setVisible (! userMode() && sortBox.getSelectedId() <= 1);
    sortBox.setVisible (! userMode());
    if (userMode())
    {
        userFiles = proc.userPresets();
        for (int i = 0; i < userFiles.size(); ++i)
            if (q.isEmpty() || userFiles[i].getFileNameWithoutExtension().toLowerCase().contains (q)) rows.push_back (i);
        list.updateContent(); list.repaint(); repaint(); refresh();
        return;
    }
    for (int i = 0; i < (int) all.size(); ++i)
    {
        const auto& p = all[(size_t) i];
        if (q.isNotEmpty() && ! (p.name + " " + p.song + " " + p.artist + " " + p.synth + " " + p.genre + " " + p.original + " " + juce::String (p.year)).toLowerCase().contains (q)) continue;
        rows.push_back (i);
    }
    // the library is stored by year; other orders: song title, preset name, artist (A-Z, natural order)
    const int sortBy = sortBox.getSelectedId();
    if (sortBy > 1)
        std::stable_sort (rows.begin(), rows.end(), [&] (int a, int b) {
            const auto& pa = all[(size_t) a]; const auto& pb = all[(size_t) b];
            const auto& ka = sortBy == 2 ? pa.song : sortBy == 3 ? pa.name : pa.artist;
            const auto& kb = sortBy == 2 ? pb.song : sortBy == 3 ? pb.name : pb.artist;
            return ka.compareNatural (kb) < 0; });
    list.updateContent();
    list.repaint();
    repaint();
    refresh();
}

void PresetBrowser::refresh()
{
    const juce::ScopedValueSetter<bool> guard (syncing, true);
    if (userMode())
    {
        list.deselectAllRows();
        for (int i = 0; i < (int) rows.size(); ++i)
            if (userFiles[rows[(size_t) i]].getFileNameWithoutExtension() == juce::File::createLegalFileName (proc.userPresetName())) { list.selectRow (i, false, true); break; }
        info.setText (proc.userPresetName().isNotEmpty() ? proc.userPresetName() + "   |   user preset" : juce::String ("Click a preset to load it."), juce::dontSendNotification);
        return;
    }
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
    if (userMode())
    {
        const auto& f = userFiles[rows[(size_t) row]];
        g.fillAll (selected ? ui::header.brighter (0.15f) : (row % 2 ? ui::panel : ui::panel.brighter (0.03f)));
        g.setColour (ui::text); g.setFont (ui::font (13.0f, true));
        g.drawText (f.getFileNameWithoutExtension(), 12, 0, w - 220, h, juce::Justification::centredLeft, true);
        g.setColour (ui::dim); g.setFont (ui::font (12.5f));
        g.drawText (f.getLastModificationTime().formatted ("%d/%m/%Y %H:%M"), w - 200, 0, 190, h, juce::Justification::centredRight);
        return;
    }
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

void PresetBrowser::selectedRowsChanged (int row)
{
    if (! syncing && row >= 0) loadRow (row);
}

void PresetBrowser::loadRow (int row)
{
    if (! juce::isPositiveAndBelow (row, (int) rows.size())) return;
    if (userMode()) proc.loadUserPreset (userFiles[rows[(size_t) row]]);
    else if (proc.presetIndex() != rows[(size_t) row]) proc.loadPreset (rows[(size_t) row]);
    refresh();
}

void PresetBrowser::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (! juce::isPositiveAndBelow (row, (int) rows.size())) return;
    list.grabKeyboardFocus();   // from now on the up / down arrows move through the presets
    if (userMode())
    {
        const auto f = userFiles[rows[(size_t) row]];
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu m;
            m.addItem (1, "Elimina il preset \"" + f.getFileNameWithoutExtension() + "\"");
            m.addItem (2, "Mostra nella cartella");
            m.showMenuAsync (juce::PopupMenu::Options(), [this, f] (int r) {
                if (r == 1) { f.moveToTrash(); apply(); }
                else if (r == 2) f.revealToUser(); });
            return;
        }
        return;   // loading is done by selectedRowsChanged (mouse or arrow keys)
    }
}

// =====================================================================================================
//  Save preset window
// =====================================================================================================
SavePresetWindow::SavePresetWindow (FamPatProcessor& p) : proc (p)
{
    name.setFont (ui::font (15.0f));
    name.setTextToShowWhenEmpty ("preset name", ui::dim);
    name.onReturnKey = [this] { save(); };
    name.onEscapeKey = [this] { if (onDone) onDone(); };
    name.onTextChange = [this] {
        const bool exists = FamPatProcessor::userPresetFolder().getChildFile (juce::File::createLegalFileName (name.getText().trim()) + ".fampat").existsAsFile();
        status.setText (exists ? "esiste già: SAVE lo sovrascrive" : juce::String(), juce::dontSendNotification); };
    addAndMakeVisible (name);
    list.setRowHeight (24);
    list.setColour (juce::ListBox::backgroundColourId, ui::panel);
    addAndMakeVisible (list);
    saveBtn.onClick = [this] { save(); };
    cancelBtn.onClick = [this] { if (onDone) onDone(); };
    addAndMakeVisible (saveBtn); addAndMakeVisible (cancelBtn);
    status.setFont (ui::font (12.5f));
    status.setColour (juce::Label::textColourId, ui::colA);
    addAndMakeVisible (status);
}

void SavePresetWindow::open()
{
    files = proc.userPresets();
    list.updateContent();
    const auto& all = PresetLibrary::get().all();
    const int idx = proc.presetIndex();
    name.setText (proc.userPresetName().isNotEmpty() ? proc.userPresetName()
                  : juce::isPositiveAndBelow (idx, (int) all.size()) ? all[(size_t) idx].name + " (user)" : juce::String ("My preset"), juce::sendNotification);
    setVisible (true);
    toFront (true);
    name.grabKeyboardFocus();
    name.selectAll();
}

void SavePresetWindow::paint (juce::Graphics& g)
{
    ui::relief (g, getLocalBounds().toFloat(), ui::panel2, 8.0f, true);
    g.setColour (ui::colG); g.setFont (ui::font (15.0f, true));
    g.drawText ("SAVE PRESET", 16, 10, 300, 22, juce::Justification::centredLeft);
    g.setColour (ui::dim); g.setFont (ui::font (12.0f));
    g.drawText ("Existing user presets (click one to overwrite it)", 16, 84, getWidth() - 32, 18, juce::Justification::centredLeft);
    g.drawText (FamPatProcessor::userPresetFolder().getFullPathName(), 16, getHeight() - 30, getWidth() - 220, 18, juce::Justification::centredLeft, true);
}

void SavePresetWindow::resized()
{
    auto r = getLocalBounds().reduced (16);
    r.removeFromTop (28);
    name.setBounds (r.removeFromTop (30));
    status.setBounds (r.removeFromTop (22));
    r.removeFromTop (20);
    auto bottom = r.removeFromBottom (34);
    cancelBtn.setBounds (bottom.removeFromRight (90));
    bottom.removeFromRight (8);
    saveBtn.setBounds (bottom.removeFromRight (90));
    r.removeFromBottom (24);
    list.setBounds (r);
}

void SavePresetWindow::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (! juce::isPositiveAndBelow (row, files.size())) return;
    g.fillAll (selected ? ui::header.brighter (0.15f) : (row % 2 ? ui::panel : ui::panel.brighter (0.03f)));
    g.setColour (ui::text); g.setFont (ui::font (13.0f));
    g.drawText (files[row].getFileNameWithoutExtension(), 10, 0, w - 20, h, juce::Justification::centredLeft, true);
}

void SavePresetWindow::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (juce::isPositiveAndBelow (row, files.size())) name.setText (files[row].getFileNameWithoutExtension(), juce::sendNotification);
}

void SavePresetWindow::save()
{
    if (name.getText().trim().isEmpty()) { status.setText ("scrivi un nome per il preset", juce::dontSendNotification); return; }
    if (! proc.saveUserPreset (name.getText())) { status.setText ("impossibile salvare nella cartella dei preset", juce::dontSendNotification); return; }
    if (onDone) onDone();
}
