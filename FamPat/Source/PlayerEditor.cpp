#include "PlayerEditor.h"
#include "BinaryData.h"

using namespace params;

// =====================================================================================================
//  FamPat logo: "FAMPAT" in Rostex Regular (white), followed by an orange "play" triangle (it is a preset player)
// =====================================================================================================
void logo::drawFamPat (juce::Graphics& g, juce::Rectangle<float> r, juce::RectanglePlacement place)
{
    using namespace juce;
    static Typeface::Ptr tf = Typeface::createSystemTypefaceFor (BinaryData::RostexRegular_ttf, BinaryData::RostexRegular_ttfSize);
    GlyphArrangement ga;
    ga.addLineOfText (Font (FontOptions (tf).withHeight (100.0f)), "FAMPAT", 0, 0);
    Path text; ga.createPath (text);
    const auto tb = text.getBounds();                         // cap height of the lettering
    const float h = tb.getHeight(), gap = h * 0.22f, tw = h * 0.62f;
    Path play;                                               // after the text, as tall as the letters' middle part
    const float px = tb.getRight() + gap, cy = tb.getCentreY();
    play.addTriangle (px, cy - tw * 0.5f, px, cy + tw * 0.5f, px + tw * 0.87f, cy);
    Path all (text); all.addPath (play);
    const auto T = place.getTransformToFit (all.getBounds(), r);
    text.applyTransform (T); play.applyTransform (T);
    g.setColour (Colour (0xfff0f0f0)); g.fillPath (text);
    g.setColour (Colour (0xffff8a3d)); g.fillPath (play);
}

// =====================================================================================================
//  Envelope curve
// =====================================================================================================
void EnvCurve::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (6.0f, 4.0f);
    ui::relief (g, getLocalBounds().toFloat(), ui::panel2, 4.0f, false);
    auto v = [this] (int i) { return proc.apvts.getRawParameterValue (def (i).id)->load(); };
    // log-like time scale so short and long envelopes both read well
    auto t = [] (float ms) { return std::log10 (1.0f + ms / 2.0f); };
    const float a = t (v (ia)), d = t (v (id)), rl = t (v (ir)), sus = v (is);
    const float hold = 0.6f, total = a + d + hold + rl;
    const float k = r.getWidth() / std::max (0.01f, total);
    const float x0 = r.getX(), yb = r.getBottom(), yt = r.getY(), h = r.getHeight();
    juce::Path p;
    p.startNewSubPath (x0, yb);
    p.lineTo (x0 + a * k, yt);
    p.quadraticTo (x0 + (a + d * 0.25f) * k, yb - h * sus, x0 + (a + d) * k, yb - h * sus);
    p.lineTo (x0 + (a + d + hold) * k, yb - h * sus);
    p.quadraticTo (x0 + (a + d + hold + rl * 0.25f) * k, yb, x0 + total * k, yb);
    juce::Path fill (p); fill.lineTo (x0, yb); fill.closeSubPath();
    g.setColour (col.withAlpha (0.18f)); g.fillPath (fill);
    g.setColour (col); g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// =====================================================================================================
//  Player editor
// =====================================================================================================
PlayerEditor::PlayerEditor (FamPatProcessor& p)
    : AudioProcessorEditor (p), proc (p), browser (p), saveWindow (p), keys (p.keyboard, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&look);
    addAndMakeVisible (content);
    content.setInterceptsMouseClicks (false, true);   // clicks on the header logo reach the editor
    const auto& L = layout();
    const auto& skin = skins::find ("neutral-g", false);

    // header
    for (auto* b : { &prev, &next, &browse, &savePreset }) content.addAndMakeVisible (b);
    prev.setComponentID ("prev"); next.setComponentID ("next"); browse.setComponentID ("browse"); savePreset.setComponentID ("savePreset");
    prev.onClick = [this] { const int n = (int) PresetLibrary::get().all().size(); proc.loadPreset (((proc.presetIndex() - 1) % n + n) % n); };
    next.onClick = [this] { const int n = (int) PresetLibrary::get().all().size(); proc.loadPreset ((proc.presetIndex() + 1) % n); };
    browse.setClickingTogglesState (true);
    browse.onClick = [this] { browser.setVisible (browse.getToggleState()); if (browser.isVisible()) { browser.toFront (false); browser.refresh(); } };
    browser.onClose = [this] { browse.setToggleState (false, juce::sendNotification); };
    savePreset.setTooltip ("salva il suono attuale come preset utente");
    savePreset.onClick = [this] { saveWindow.open(); };
    saveWindow.onDone = [this] { saveWindow.setVisible (false); if (browser.isVisible()) browser.refresh(); };
    presetName.setFont (ui::font (21.0f, true));
    presetName.setColour (juce::Label::textColourId, ui::text);
    presetInfo.setFont (ui::font (12.5f));
    presetInfo.setColour (juce::Label::textColourId, ui::dim);
    presetName.setInterceptsMouseClicks (false, false);
    presetInfo.setInterceptsMouseClicks (false, false);
    content.addAndMakeVisible (presetName);
    content.addAndMakeVisible (presetInfo);

    // envelopes: one pair for the whole patch
    const int envP[8] = { L.pFA, L.pFD, L.pFS, L.pFR, L.pAA, L.pAD, L.pAS, L.pAR };
    const char* envL[8] = { "Attack", "Decay", "Sustain", "Release", "Attack", "Decay", "Sustain", "Release" };
    for (int k = 0; k < 8; ++k)
    {
        env[k] = std::make_unique<Control> (p.apvts, envP[k], envL[k]);
        env[k]->setSkin (skin);
        content.addAndMakeVisible (*env[k]);
    }
    fCurve = std::make_unique<EnvCurve> (p, L.pFA, L.pFD, L.pFS, L.pFR, ui::colA);
    aCurve = std::make_unique<EnvCurve> (p, L.pAA, L.pAD, L.pAS, L.pAR, ui::colD);
    content.addAndMakeVisible (*fCurve);
    content.addAndMakeVisible (*aCurve);

    // master output
    auto group = [&] (const juce::String& title, std::vector<std::pair<int, juce::String>> items)
    {
        FxGroup gr; gr.title = title;
        for (auto& [pid, lab] : items)
        {
            auto c = std::make_unique<Control> (p.apvts, pid, lab);
            c->setSkin (skin);
            content.addAndMakeVisible (*c);
            gr.ctl.push_back (std::move (c));
        }
        fx.push_back (std::move (gr));
    };
    group ("DRIVE", { { L.gDrive, "Drive" } });
    group ("PEDAL", { { L.pPedal, "Gain" } });
    group ("CHORUS", { { L.gChoMode, "Mode" }, { L.gChoMix, "Mix" } });
    group ("DELAY", { { L.pDlyDiv, "Time" }, { L.pDlyFb, "Feedback" }, { L.pDlyMix, "Mix" } });
    group ("REVERB", { { L.gRevSize, "Size" }, { L.gRevDamp, "Damp" }, { L.gRevMix, "Mix" } });
    group ("LEVEL", { { L.gMaster, "Master" } });

    keys.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, ui::colA.withAlpha (0.6f));
    content.addAndMakeVisible (keys);
    content.addChildComponent (browser);
    content.addChildComponent (saveWindow);
    content.addChildComponent (splash);

    setResizable (true, true);
    setResizeLimits (BW / 2, BH / 2, 16384, 16384);   // free aspect ratio: maximise / full screen fills the screen
    const double sc = (double) proc.uiState.getProperty ("scale", 1.0);
    setSize ((int) proc.uiState.getProperty ("w", (int) (BW * sc)), (int) proc.uiState.getProperty ("h", (int) (BH * sc)));
    updatePatch();
    startTimerHz (30);
}

PlayerEditor::~PlayerEditor() { setLookAndFeel (nullptr); }

void PlayerEditor::updatePatch()
{
    const auto& all = PresetLibrary::get().all();
    const auto& L = layout();
    const int idx = proc.presetIndex();
    shownSerial = proc.presetSerial();
    auto val = [this] (int i) { return proc.apvts.getRawParameterValue (def (i).id)->load(); };
    if (juce::isPositiveAndBelow (idx, (int) all.size()))
    {
        const auto& p = all[(size_t) idx];
        presetName.setText (p.name + "  (" + p.genre + " - " + p.song + ")", juce::dontSendNotification);
        presetInfo.setText (juce::String (p.year) + "  -  " + p.artist + "  -  " + p.synth, juce::dontSendNotification);
        cardName = p.name;
        cardSong = p.song + "  -  " + p.artist + "  (" + juce::String (p.year) + ", " + p.genre + ")";
        cardSynth = p.synth;
        cardFactory = p.original.isNotEmpty() ? "Factory preset \"" + p.original + "\"" : juce::String();
        cardNotes = p.notes;
        const auto* md = p.modules.empty() ? nullptr : PresetLibrary::get().model (PresetLibrary::get().modelIndex (p.modules.front().model));
        cardAccent = md != nullptr ? skins::find (md->skin, md->digital).accent : ui::colA;
    }
    else
    {
        const auto user = proc.userPresetName();
        presetName.setText (user.isNotEmpty() ? user : juce::String ("Init"), juce::dontSendNotification);
        presetInfo.setText (user.isNotEmpty() ? juce::String ("User preset") : juce::String(), juce::dontSendNotification);
        cardName = user.isNotEmpty() ? user : juce::String ("Init");
        cardSong = "User preset"; cardSynth = {}; cardFactory = {}; cardNotes = {};
        cardAccent = ui::colA;
    }
    // which effects the patch uses (decided when it is loaded)
    bool pedal = false;
    for (int m = 0; m < numModules; ++m)
        if (m < (int) val (L.gSlots) && val (mod (m, mOn)) > 0.5f && val (mod (m, mDist)) > 0.5f) pedal = true;
    fx[0].shown = val (L.gDrive) > 0.0f;
    fx[1].shown = pedal;
    fx[2].shown = val (L.gChoMode) > 0.5f;
    fx[3].shown = val (L.pDlyMix) > 0.0f;
    fx[4].shown = val (L.gRevMix) > 0.0f;
    fx[5].shown = true;
    layoutMaster();
    if (browser.isVisible()) browser.refresh();
    repaint();
}

void PlayerEditor::layoutMaster()
{
    constexpr int cellW = 76, head = 20;
    int x = masterBox.getX() + 10;
    const int y = masterBox.getY() + 28;
    for (auto& gr : fx)
    {
        for (auto& c : gr.ctl) c->setVisible (gr.shown);
        if (! gr.shown) { gr.box = {}; continue; }
        int units = 0;
        for (auto& c : gr.ctl) units += c->wide ? 2 : 1;
        const int w = units * cellW + 12;
        gr.box = { x, y, w, masterBox.getBottom() - 10 - y };
        int u = 0;
        for (auto& c : gr.ctl)
        {
            const int cw = c->wide ? 2 : 1;
            c->setBounds (x + 6 + u * cellW, y + head, cellW * cw, c->wide ? 50 : 64);
            u += cw;
        }
        x += w + 8;
    }
}

void PlayerEditor::resized()
{
    uiScale = juce::jmax (0.25f, juce::jmin ((float) getWidth() / (float) BW, (float) getHeight() / (float) BH));
    W = juce::jmax (BW, juce::roundToInt ((float) getWidth() / uiScale));
    H = juce::jmax (BH, juce::roundToInt ((float) getHeight() / uiScale));
    const int dh = H - BH;
    content.setBounds (0, 0, W, H);
    content.setTransform (juce::AffineTransform::scale (uiScale));
    proc.uiState.setProperty ("w", getWidth(), nullptr);
    proc.uiState.setProperty ("h", getHeight(), nullptr);
    // header
    logoArea = { 8, 3, 252, 58 };
    presetName.setBounds (272, 6, W - 336 - 12 - 272, 28);
    presetInfo.setBounds (272, 34, W - 336 - 12 - 272, 22);
    prev.setBounds (W - 336, 16, 34, 34); next.setBounds (W - 298, 16, 34, 34);
    browse.setBounds (W - 260, 16, 110, 34); savePreset.setBounds (W - 144, 16, 80, 34);   // OUT meter at the right
    // patch card, envelopes, master
    cardBox = { 8, 74, W - 16, 236 + dh };
    const int envY = cardBox.getBottom() + 8, envH = 240, half = (W - 24) / 2;
    fBox = { 8, envY, half, envH };
    aBox = { 16 + half, envY, W - 24 - half, envH };
    for (int k = 0; k < 2; ++k)
    {
        const auto& b = k == 0 ? fBox : aBox;
        (k == 0 ? fCurve : aCurve)->setBounds (b.getX() + 12, b.getY() + 30, b.getWidth() - 24, 110);
        const int cw = (b.getWidth() - 24) / 4;
        for (int j = 0; j < 4; ++j) env[k * 4 + j]->setBounds (b.getX() + 12 + j * cw + (cw - 90) / 2, b.getY() + 152, 90, 76);
    }
    masterBox = { 8, fBox.getBottom() + 8, W - 16, 150 };
    layoutMaster();
    keys.setBounds (8, H - 108, W - 16, 100);
    keys.setKeyWidth ((float) (W - 16) / 75.0f);
    keys.setLowestVisibleKey (0);
    browser.setBounds (8, 66, W - 16, H - 66 - 116);
    saveWindow.setBounds (W / 2 - 260, 120, 520, 480);
    splash.setBounds (0, 0, W, H);
}

void PlayerEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::bg);
    g.addTransform (juce::AffineTransform::scale (uiScale));
    ui::relief (g, juce::Rectangle<float> (0, 0, (float) W, 64), ui::panel, 0.0f, true);
    logo::drawFamPat (g, logoArea.reduced (4.0f, 9.0f), juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid);
    // output meter
    const float m = juce::jlimit (0.0f, 1.0f, proc.meter());
    ui::relief (g, juce::Rectangle<float> ((float) W - 56, 18, 44, 10), ui::panel2, 3.0f, false);
    g.setColour (m > 0.95f ? juce::Colours::red : ui::colG); g.fillRoundedRectangle ((float) W - 56, 18, 44 * m, 10, 3);
    g.setColour (ui::dim); g.setFont (ui::font (10.0f)); g.drawText ("OUT", W - 56, 30, 44, 14, juce::Justification::centred);

    // patch card
    ui::relief (g, cardBox.toFloat(), ui::panel, 6.0f, true);
    g.setColour (cardAccent); g.fillRoundedRectangle (cardBox.toFloat().removeFromLeft (6.0f), 3.0f);
    auto c = cardBox.reduced (28, 18);
    g.setColour (ui::colG); g.setFont (ui::font (40.0f, true));
    g.drawText (cardName, c.removeFromTop (52), juce::Justification::centredLeft, true);
    g.setColour (ui::text); g.setFont (ui::font (18.0f));
    g.drawText (cardSong, c.removeFromTop (30), juce::Justification::centredLeft, true);
    c.removeFromTop (6);
    g.setColour (cardAccent); g.setFont (ui::font (16.0f, true));
    g.drawText (cardSynth, c.removeFromTop (26), juce::Justification::centredLeft, true);
    if (cardFactory.isNotEmpty()) { g.setColour (ui::dim); g.setFont (ui::font (14.0f)); g.drawText (cardFactory, c.removeFromTop (22), juce::Justification::centredLeft, true); }
    if (cardNotes.isNotEmpty()) { c.removeFromTop (6); g.setColour (ui::dim); g.setFont (ui::font (13.5f)); g.drawFittedText (cardNotes, c.removeFromTop (40), juce::Justification::topLeft, 2); }

    // envelopes
    for (int k = 0; k < 2; ++k)
    {
        const auto& b = k == 0 ? fBox : aBox;
        ui::relief (g, b.toFloat(), ui::panel, 5.0f, true);
        g.setColour (k == 0 ? ui::colA : ui::colD); g.setFont (ui::font (13.0f, true));
        g.drawText (k == 0 ? "FILTER ENVELOPE" : "AMP ENVELOPE", b.getX() + 12, b.getY() + 6, 300, 20, juce::Justification::centredLeft);
    }
    // master output
    ui::relief (g, masterBox.toFloat(), ui::panel, 5.0f, true);
    g.setColour (ui::colG); g.setFont (ui::font (13.0f, true));
    g.drawText ("MASTER OUTPUT", masterBox.getX() + 12, masterBox.getY() + 6, 300, 20, juce::Justification::centredLeft);
    for (auto& gr : fx)
        if (gr.shown)
        {
            ui::relief (g, gr.box.toFloat(), ui::panel2, 4.0f, false);
            g.setColour (ui::dim); g.setFont (ui::font (10.5f, true));
            g.drawText (gr.title, gr.box.getX() + 7, gr.box.getY() + 3, gr.box.getWidth() - 10, 14, juce::Justification::centredLeft);
        }
}

void PlayerEditor::mouseDown (const juce::MouseEvent& e)
{
    if (logoArea.contains (e.position / uiScale)) { splash.setVisible (true); splash.toFront (false); }
}

void PlayerEditor::timerCallback()
{
    if (proc.presetSerial() != shownSerial) updatePatch();
    repaint ((int) ((W - 60) * uiScale), 0, (int) (60 * uiScale), (int) (60 * uiScale));
}
