#include "MicroSection.h"

using namespace juce;
using namespace params;

//==============================================================================
//  MicroStepGrid
//==============================================================================
MicroStepGrid::MicroStepGrid (TwinboxProcessor& p, bool allChannels) : proc (p), all (allChannels)
{
    startTimerHz (30);
}

int MicroStepGrid::editPattern() const
{
    return (int) proc.apvts.getRawParameterValue (aPattern)->load();
}

Rectangle<float> MicroStepGrid::cellRect (int rowIndex, int step) const
{
    const float lw = (float) labelW();
    const float cw = ((float) getWidth() - lw) / (float) visible();
    const int groups = all ? numMicroChannels : 1;
    const float gap = all ? 6.0f : 0.0f;
    const float rh = ((float) getHeight() - gap * (float) (groups - 1)) / (float) numRows();
    const float y = (float) rowIndex * rh + gap * (float) (rowIndex / 3);
    const float extra = (step / 4) * 0.0f;
    return { lw + (float) step * cw + extra, y, cw, rh };
}

bool MicroStepGrid::cellAt (Point<int> pt, int& ch, int& row, int& step) const
{
    if (pt.x < labelW()) return false;
    for (int r = 0; r < numRows(); ++r)
    {
        auto rr = cellRect (r, 0);
        if (pt.y >= rr.getY() && pt.y < rr.getBottom())
        {
            const float cw = ((float) getWidth() - (float) labelW()) / (float) visible();
            step = page * visible() + jlimit (0, visible() - 1, (int) (((float) pt.x - (float) labelW()) / cw));
            ch = all ? r / 3 : channel;
            row = r % 3;
            return true;
        }
    }
    return false;
}

void MicroStepGrid::paint (Graphics& g)
{
    const int pat = editPattern();
    auto& P = proc.micro.pattern (pat);
    const int len = P.length.load();
    const bool showPlay = proc.isPlaying() && proc.micro.playingPattern() == pat;
    const int ps = showPlay ? proc.micro.playingStep() : -1;
    static const char* rowNames[] { "Trigger", "Accent", "Fill" };
    static const char* rowShort[] { "T", "A", "F" };

    for (int r = 0; r < numRows(); ++r)
    {
        const int ch = all ? r / 3 : channel;
        const int sub = r % 3;
        const auto first = cellRect (r, 0);
        // labels
        g.setColour (sub == 0 ? theme::text : theme::textDim);
        g.setFont (theme::font (jlimit (10.0f, 14.0f, first.getHeight() * 0.6f), sub == 0));
        if (all)
        {
            if (sub == 0)
                g.drawText (String (ch + 1) + "  " + proc.channelName (ch), 4, (int) first.getY(), labelW() - 22, (int) first.getHeight(), Justification::centredLeft);
            g.setColour (theme::textDim);
            g.drawText (rowShort[sub], labelW() - 18, (int) first.getY(), 14, (int) first.getHeight(), Justification::centred);
        }
        else
            g.drawText (rowNames[sub], 2, (int) first.getY(), labelW() - 6, (int) first.getHeight(), Justification::centredLeft);

        const bool muted = proc.apvts.getRawParameterValue (a (ch, mt::mute))->load() > 0.5f;
        for (int ci = 0; ci < visible(); ++ci)
        {
            const int s = page * visible() + ci;
            auto c = cellRect (r, ci).reduced (sub == 0 ? 1.5f : 3.0f, sub == 0 ? 1.5f : 2.5f);
            const uint8_t f = P.steps[(size_t) ch][(size_t) s].load();
            const bool on = (f & (1 << sub)) != 0;
            Colour col = (s / 4) % 2 == 0 ? theme::stepBeat : theme::stepOff;
            if (on)
            {
                col = sub == 0 ? theme::accentA : (sub == 1 ? theme::accentA.brighter (0.4f) : theme::accentG);
                if (sub == 0 && (f & MicroEngine::accent)) col = theme::accentA.brighter (0.25f);
                if (muted) col = col.withSaturation (0.15f);
            }
            if (s >= len) col = col.withMultipliedAlpha (0.25f);
            g.setColour (col);
            g.fillRoundedRectangle (c, 2.0f);
            if (s == ps)
            {
                g.setColour (Colours::white.withAlpha (0.55f));
                g.drawRoundedRectangle (c, 2.0f, 1.5f);
            }
        }
    }
    // step numbers
    if (! all) return;
}

void MicroStepGrid::mouseDown (const MouseEvent& e)
{
    int ch, row, step;
    if (! cellAt (e.getPosition(), ch, row, step))
    {
        if (all && e.x < labelW())
        {
            const int r = jlimit (0, numRows() - 1, (int) (e.y / (getHeight() / (float) numRows())));
            if (onChannelClicked) onChannelClicked (r / 3);
        }
        return;
    }
    auto& cell = proc.micro.pattern (editPattern()).steps[(size_t) ch][(size_t) step];
    const uint8_t bit = (uint8_t) (1 << row);
    uint8_t v = cell.load();
    if (e.mods.isRightButtonDown() && row == 0)   // right click on a trigger: toggle accent
    {
        v ^= MicroEngine::accent;
        if (v & MicroEngine::accent) v |= MicroEngine::trig;
        cell.store (v);
        dragRow = -1;
    }
    else
    {
        dragSet = (v & bit) == 0;
        cell.store (dragSet ? (uint8_t) (v | bit) : (uint8_t) (v & ~bit));
        dragRow = row; dragCh = ch; lastDragStep = step;
    }
    if (onChannelClicked && all) onChannelClicked (ch);
    repaint();
}

void MicroStepGrid::mouseDrag (const MouseEvent& e)
{
    if (dragRow < 0) return;
    const float cw = ((float) getWidth() - (float) labelW()) / (float) visible();
    const int step = page * visible() + jlimit (0, visible() - 1, (int) (((float) e.x - (float) labelW()) / cw));
    if (step == lastDragStep) return;
    lastDragStep = step;
    auto& cell = proc.micro.pattern (editPattern()).steps[(size_t) dragCh][(size_t) step];
    const uint8_t bit = (uint8_t) (1 << dragRow);
    const uint8_t v = cell.load();
    cell.store (dragSet ? (uint8_t) (v | bit) : (uint8_t) (v & ~bit));
    repaint();
}

void MicroStepGrid::timerCallback()
{
    const int s = proc.isPlaying() ? proc.micro.playingStep() : -1;
    if (s != lastStep) { lastStep = s; repaint(); }
}

//==============================================================================
//  MicroPatchEditor
//==============================================================================
MicroPatchEditor::MicroPatchEditor (TwinboxProcessor& p) : proc (p)
{
    for (auto* pnl : { &oscPanel, &noisePanel, &mixPanel, &velPanel }) addAndMakeVisible (pnl);
    for (auto* sg : { &oscWave, &pmMode, &nMode, &nEnv }) { sg->setVertical (true); addAndMakeVisible (sg); }
    oscWave.setCaption ("Wave"); pmMode.setCaption ("Pitch Mod"); nMode.setCaption ("Filter"); nEnv.setCaption ("Envelope");
    for (auto* k : { &oscFreq, &oscAtk, &oscDcy, &pmAmt, &pmRate, &nFreq, &nQ, &nAtk, &nDcy, &mix, &dist, &eqFreq, &eqGain,
                     &lvl, &pan, &velOsc, &velNoise, &velMod })
        addAndMakeVisible (k);
    addAndMakeVisible (nStereo);
    addAndMakeVisible (choke);
    setChannel (0);
}

void MicroPatchEditor::setChannel (int ch)
{
    auto& s = proc.apvts;
    oscWave.attach (s, a (ch, mt::oscWave), { "Sine", "Tri", "Saw" });
    oscFreq.attach (s, a (ch, mt::oscFreq));
    oscAtk.attach (s, a (ch, mt::oscAtk));
    oscDcy.attach (s, a (ch, mt::oscDcy));
    pmMode.attach (s, a (ch, mt::pmMode));
    pmAmt.attach (s, a (ch, mt::pmAmt));
    pmRate.attach (s, a (ch, mt::pmRate));
    nMode.attach (s, a (ch, mt::nMode));
    nFreq.attach (s, a (ch, mt::nFreq));
    nQ.attach (s, a (ch, mt::nQ));
    nStereo.attach (s, a (ch, mt::nStereo));
    nEnv.attach (s, a (ch, mt::nEnv));
    nAtk.attach (s, a (ch, mt::nAtk));
    nDcy.attach (s, a (ch, mt::nDcy));
    mix.attach (s, a (ch, mt::mix));
    dist.attach (s, a (ch, mt::dist));
    eqFreq.attach (s, a (ch, mt::eqFreq));
    eqGain.attach (s, a (ch, mt::eqGain));
    lvl.attach (s, a (ch, mt::level));
    pan.attach (s, a (ch, mt::pan));
    choke.attach (s, a (ch, mt::choke));
    velOsc.attach (s, a (ch, mt::velOsc));
    velNoise.attach (s, a (ch, mt::velNoise));
    velMod.attach (s, a (ch, mt::velMod));
}

void MicroPatchEditor::resized()
{
    auto r = getLocalBounds();
    const float unit = (float) r.getWidth() / 14.0f;
    auto take = [&] (float units) { return r.removeFromLeft (roundToInt (unit * units)).reduced (2, 0); };
    oscPanel.setBounds (take (4)); noisePanel.setBounds (take (4)); mixPanel.setBounds (take (4)); velPanel.setBounds (r.reduced (2, 0));

    layoutRows (oscPanel.getBounds().withTrimmedTop (Panel::headerH).reduced (4, 2) ,
                { { &oscWave, &oscFreq, &oscAtk, &oscDcy }, { &pmMode, &pmAmt, &pmRate, nullptr } });
    layoutRows (noisePanel.getBounds().withTrimmedTop (Panel::headerH).reduced (4, 2),
                { { &nMode, &nFreq, &nQ, &nStereo }, { &nEnv, &nAtk, &nDcy, nullptr } });
    layoutRows (mixPanel.getBounds().withTrimmedTop (Panel::headerH).reduced (4, 2),
                { { &mix, &dist, &eqFreq, &eqGain }, { &lvl, &pan, &choke, nullptr } });
    layoutRows (velPanel.getBounds().withTrimmedTop (Panel::headerH).reduced (4, 2),
                { { &velOsc, &velNoise }, { &velMod, nullptr } });
}

//==============================================================================
//  MicroPatchWindowContent
//==============================================================================
MicroPatchWindowContent::MicroPatchWindowContent (TwinboxProcessor& p, int ch, std::function<void (int)> sel)
    : proc (p), editor (p), onSelect (std::move (sel))
{
    for (int i = 0; i < numMicroChannels; ++i)
    {
        auto* b = chButtons.add (new TextButton (String (i + 1) + "  " + proc.channelName (i)));
        b->setClickingTogglesState (false);
        b->setColour (TextButton::buttonOnColourId, theme::accentA);
        b->onClick = [this, i] { setChannel (i); if (onSelect) onSelect (i); };
        addAndMakeVisible (b);
    }
    addAndMakeVisible (editor);
    setChannel (ch);
}

void MicroPatchWindowContent::setChannel (int ch)
{
    for (int i = 0; i < chButtons.size(); ++i)
    {
        chButtons[i]->setButtonText (String (i + 1) + "  " + proc.channelName (i));
        chButtons[i]->setToggleState (i == ch, dontSendNotification);
    }
    editor.setChannel (ch);
}

void MicroPatchWindowContent::paint (Graphics& g) { g.fillAll (theme::bg); }

void MicroPatchWindowContent::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto top = r.removeFromTop (34);
    const int bw = top.getWidth() / numMicroChannels;
    for (auto* b : chButtons) b->setBounds (top.removeFromLeft (bw).reduced (3, 2));
    r.removeFromTop (8);
    editor.setBounds (r);
}

//==============================================================================
//  MicroPatternWindowContent
//==============================================================================
MicroPatternWindowContent::MicroPatternWindowContent (TwinboxProcessor& p, std::function<void (int)> onChannel)
    : proc (p), grid (p, true)
{
    for (int i = 0; i < numMicroPatterns; ++i)
    {
        auto* b = pats.add (new TextButton (String::charToString ((juce_wchar) ('A' + i))));
        b->setColour (TextButton::buttonOnColourId, theme::accentA);
        b->onClick = [this, i]
        {
            auto* prm = proc.apvts.getParameter (aPattern);
            prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i)); prm->endChangeGesture();
        };
        addAndMakeVisible (b);
    }
    lenBox.setSliderStyle (Slider::IncDecButtons);
    lenBox.setRange (1, numSteps, 1);
    lenBox.setTextBoxStyle (Slider::TextBoxLeft, false, 40, 26);
    lenBox.onValueChange = [this]
    {
        proc.micro.pattern ((int) proc.apvts.getRawParameterValue (aPattern)->load()).length.store ((int) lenBox.getValue());
        grid.repaint();
    };
    swing.attach (proc.apvts, aSwing);
    fillRate.setCaption ("Fill Rate (every N loops)");
    fillRate.attach (proc.apvts, aFillRate, { "Manual", "2", "3", "4", "5", "6", "7", "8" });
    fill.attach (proc.apvts, aFill);
    grid.onChannelClicked = std::move (onChannel);
    for (auto* c : std::initializer_list<Component*> { &lenBox, &swing, &fillRate, &fill, &grid }) addAndMakeVisible (c);
    for (int i = 0; i < 2; ++i)
    {
        auto* b = pages.add (new TextButton ("Steps " + String (i * 32 + 1) + "-" + String ((i + 1) * 32)));
        b->setColour (TextButton::buttonOnColourId, theme::header.brighter (0.3f));
        b->onClick = [this, i] { grid.setPage (i); };
        addAndMakeVisible (b);
    }
    startTimerHz (10);
}

void MicroPatternWindowContent::timerCallback()
{
    const int pat = (int) proc.apvts.getRawParameterValue (aPattern)->load();
    for (int i = 0; i < pats.size(); ++i) pats[i]->setToggleState (i == pat, dontSendNotification);
    if (! lenBox.isMouseButtonDown()) lenBox.setValue (proc.micro.pattern (pat).length.load(), dontSendNotification);
    for (int i = 0; i < pages.size(); ++i) pages[i]->setToggleState (i == grid.getPage(), dontSendNotification);
}

void MicroPatternWindowContent::paint (Graphics& g)
{
    g.fillAll (theme::bg);
    g.setColour (theme::textDim);
    g.setFont (theme::font (12.0f));
    g.drawText ("Click: on/off  |  Right-click trigger: accent  |  Drag: paint  |  T = trigger, A = accent, F = fill (plays when FILL is active)",
                12, getHeight() - 24, getWidth() - 24, 20, Justification::centredLeft);
    g.drawText ("LENGTH", lenBox.getX(), lenBox.getY() - 14, 80, 12, Justification::centredLeft);
}

void MicroPatternWindowContent::resized()
{
    auto r = getLocalBounds().reduced (12);
    r.removeFromBottom (24);
    auto top = r.removeFromTop (70);
    auto pr = top.removeFromLeft (top.getWidth() / 2).withSizeKeepingCentre (top.getWidth() / 2, 32);
    const int bw = pr.getWidth() / numMicroPatterns;
    for (auto* b : pats) b->setBounds (pr.removeFromLeft (bw).reduced (2, 0));
    top.removeFromLeft (16);
    lenBox.setBounds (top.removeFromLeft (120).withSizeKeepingCentre (120, 28).translated (0, 6));
    swing.setBounds (top.removeFromLeft (80));
    fill.setBounds (top.removeFromRight (80).withSizeKeepingCentre (80, 30));
    fillRate.setBounds (top.reduced (8, 6));
    r.removeFromTop (6);
    auto pg = r.removeFromTop (26);
    pg.removeFromLeft (110);
    for (auto* b : pages) b->setBounds (pg.removeFromLeft (140).reduced (2, 0));
    r.removeFromTop (6);
    grid.setBounds (r);
}

//==============================================================================
//  MicroSection
//==============================================================================
MicroSection::ChannelRow::ChannelRow (MicroSection& o, int c) : owner (o), ch (c)
{
    mute.attach (owner.proc.apvts, a (ch, mt::mute));
    mute.setTooltip ("Mute channel " + String (ch + 1));
    play.setTooltip ("Audition");
    play.onClick = [this] { owner.proc.micro.auditionChannel (ch, 1.0f); };
    addAndMakeVisible (mute);
    addAndMakeVisible (play);
}

void MicroSection::ChannelRow::paint (Graphics& g)
{
    const bool sel = owner.selected == ch;
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (sel ? theme::header.brighter (0.12f) : theme::panel2);
    g.fillRoundedRectangle (r, 3.0f);
    if (sel) { g.setColour (theme::accentA); g.drawRoundedRectangle (r, 3.0f, 1.2f); }
    // trigger LED
    g.setColour (theme::accentA.withAlpha (0.15f + 0.85f * flash));
    g.fillEllipse (8.0f, r.getCentreY() - 4.0f, 8.0f, 8.0f);
    g.setColour (theme::textDim);
    g.setFont (theme::font (11.0f));
    g.drawText (String (ch + 1), 20, 0, 16, getHeight(), Justification::centred);
    g.setColour (theme::text);
    g.setFont (theme::font (12.5f, sel));
    g.drawText (owner.proc.channelName (ch), 38, 0, getWidth() - 100, getHeight(), Justification::centredLeft);
}

void MicroSection::ChannelRow::resized()
{
    auto r = getLocalBounds().reduced (3);
    mute.setBounds (r.removeFromRight (26));
    r.removeFromRight (3);
    play.setBounds (r.removeFromRight (26));
}

void MicroSection::ChannelRow::mouseDown (const MouseEvent&) { owner.selectChannel (ch); }

MicroSection::MicroSection (TwinboxProcessor& p) : proc (p), patch (p), grid (p, false)
{
    // header: patterns A-L
    for (int i = 0; i < numMicroPatterns; ++i)
    {
        auto* b = patternButtons.add (new TextButton (String::charToString ((juce_wchar) ('A' + i))));
        b->setColour (TextButton::buttonOnColourId, theme::accentA);
        b->setTooltip ("Select pattern " + b->getButtonText() + " (switches at the end of the playing pattern)");
        b->onClick = [this, i]
        {
            auto* prm = proc.apvts.getParameter (aPattern);
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i));
            prm->endChangeGesture();
        };
        addAndMakeVisible (b);
    }
    storeL.setTooltip ("Store the current 8 drum patches as the LEFT morph target");
    storeR.setTooltip ("Store the current 8 drum patches as the RIGHT morph target");
    storeL.setColour (TextButton::buttonOnColourId, theme::accentA.withAlpha (0.55f));
    storeR.setColour (TextButton::buttonOnColourId, theme::accentA.withAlpha (0.55f));
    storeL.onClick = [this] { proc.storeMorph (false); };
    storeR.onClick = [this] { proc.storeMorph (true); };
    morph.attach (proc.apvts, aMorph);
    level.attach (proc.apvts, aLevel);
    for (auto* c : std::initializer_list<Component*> { &storeL, &storeR, &morph, &level }) addAndMakeVisible (c);

    for (int i = 0; i < numMicroChannels; ++i) addAndMakeVisible (rows.add (new ChannelRow (*this, i)));

    nameEdit.setEditable (false, true, false);
    nameEdit.setColour (Label::backgroundColourId, theme::panel2);
    nameEdit.setColour (Label::outlineColourId, theme::outline);
    nameEdit.setTooltip ("Double-click to rename the selected channel");
    nameEdit.onTextChange = [this] { proc.setChannelName (selected, nameEdit.getText()); repaint(); for (auto* r : rows) r->repaint(); };
    copyBtn.onClick = [this] { proc.copyChannel (selected); };
    pasteBtn.onClick = [this] { proc.pasteChannel (selected); };
    copyBtn.setTooltip ("Copy drum patch"); pasteBtn.setTooltip ("Paste drum patch");
    addAndMakeVisible (nameEdit); addAndMakeVisible (copyBtn); addAndMakeVisible (pasteBtn);

    addAndMakeVisible (patch);
    patch.setExpandable ([this] { openPatchWindow(); });

    addAndMakeVisible (patPanel);
    patPanel.setExpandable ([this] { openPatternWindow(); });
    addAndMakeVisible (grid);
    lenBox.setSliderStyle (Slider::IncDecButtons);
    lenBox.setRange (1, numSteps, 1);
    lenBox.setTextBoxStyle (Slider::TextBoxLeft, false, 34, 22);
    lenBox.setTooltip ("Pattern length (steps)");
    lenBox.onValueChange = [this]
    {
        proc.micro.pattern ((int) proc.apvts.getRawParameterValue (aPattern)->load()).length.store ((int) lenBox.getValue());
    };
    addAndMakeVisible (lenBox);
    swing.attach (proc.apvts, aSwing);
    addAndMakeVisible (swing);
    fillRate.setCaption ("Fill Rate");
    fillRate.attach (proc.apvts, aFillRate, { "M", "2", "3", "4", "5", "6", "7", "8" });
    addAndMakeVisible (fillRate);
    fill.attach (proc.apvts, aFill);
    fill.setTooltip ("Fill: play the steps marked as Fill");
    addAndMakeVisible (fill);
    clearBtn.onClick = [this]
    {
        auto& P = proc.micro.pattern ((int) proc.apvts.getRawParameterValue (aPattern)->load());
        for (auto& s : P.steps[(size_t) selected]) s.store (0);
        grid.repaint();
    };
    clearBtn.setTooltip ("Clear the selected channel in this pattern");
    copyPat.onClick = [this] { patClip = (int) proc.apvts.getRawParameterValue (aPattern)->load(); };
    pastePat.onClick = [this]
    {
        if (patClip < 0) return;
        auto& src = proc.micro.pattern (patClip);
        auto& dst = proc.micro.pattern ((int) proc.apvts.getRawParameterValue (aPattern)->load());
        for (int c = 0; c < numMicroChannels; ++c)
            for (int s = 0; s < numSteps; ++s) dst.steps[(size_t) c][(size_t) s].store (src.steps[(size_t) c][(size_t) s].load());
        dst.length.store (src.length.load());
        refreshPatternControls();
    };
    for (auto* c : { &clearBtn, &copyPat, &pastePat }) addAndMakeVisible (c);
    for (int i = 0; i < 4; ++i)
    {
        auto* b = pageBtns.add (new TextButton (String (i * 16 + 1) + "-" + String ((i + 1) * 16)));
        b->setColour (TextButton::buttonOnColourId, theme::header.brighter (0.3f));
        b->onClick = [this, i] { grid.setPage (i); };
        addAndMakeVisible (b);
    }
    browserBtn.setTooltip ("Genre preset browser (128 kits + patterns)");
    browserBtn.onClick = [this] { if (onOpenBrowser) onOpenBrowser(); };
    addAndMakeVisible (browserBtn);

    selectChannel (0);
    startTimerHz (30);
}

MicroSection::~MicroSection() { stopTimer(); }

void MicroSection::selectChannel (int ch)
{
    selected = jlimit (0, numMicroChannels - 1, ch);
    patch.setChannel (selected);
    grid.setChannel (selected);
    nameEdit.setText (proc.channelName (selected), dontSendNotification);
    for (auto* r : rows) r->repaint();
    if (patchWindow != nullptr)
        if (auto* c = dynamic_cast<MicroPatchWindowContent*> (patchWindow->getContentComponent())) c->setChannel (selected);
}

void MicroSection::refreshPatternControls()
{
    const int pat = (int) proc.apvts.getRawParameterValue (aPattern)->load();
    lenBox.setValue (proc.micro.pattern (pat).length.load(), dontSendNotification);
    grid.repaint();
    if (patternWindow != nullptr) patternWindow->repaint();
}

void MicroSection::openPatternWindow()
{
    if (patternWindow != nullptr) { patternWindow->toFront (true); return; }
    auto* g = new MicroPatternWindowContent (proc, [this] (int ch) { selectChannel (ch); });
    patternWindow = std::make_unique<ExpandWindow> ("Section A - Pattern Editor (all channels)   |   click: on/off   right-click trigger: accent   drag: paint",
                                                    g, 1280, 900, [this] { patternWindow.reset(); });
}

void MicroSection::openPatchWindow()
{
    if (patchWindow != nullptr) { patchWindow->toFront (true); return; }
    auto* c = new MicroPatchWindowContent (proc, selected, [this] (int ch) { selectChannel (ch); });
    patchWindow = std::make_unique<ExpandWindow> ("Section A - Drum Patch Editor", c, 1300, 640, [this] { patchWindow.reset(); });
}

void MicroSection::timerCallback()
{
    const int pat = (int) proc.apvts.getRawParameterValue (aPattern)->load();
    const int playing = proc.micro.playingPattern();
    for (int i = 0; i < patternButtons.size(); ++i)
    {
        auto* b = patternButtons[i];
        b->setToggleState (i == pat, dontSendNotification);
        b->setColour (TextButton::buttonColourId, (proc.isPlaying() && i == playing && i != pat) ? theme::accentA.withAlpha (0.35f) : theme::panel2);
    }
    for (int i = 0; i < pageBtns.size(); ++i)
    {
        pageBtns[i]->setToggleState (i == grid.getPage(), dontSendNotification);
        pageBtns[i]->setAlpha (i * 16 < proc.micro.pattern (pat).length.load() ? 1.0f : 0.4f);
    }
    if (pat != lastPattern) { lastPattern = pat; refreshPatternControls(); }
    else if ((int) lenBox.getValue() != proc.micro.pattern (pat).length.load()) refreshPatternControls();

    storeL.setToggleState (proc.hasMorph (false), dontSendNotification);
    storeR.setToggleState (proc.hasMorph (true), dontSendNotification);
    fill.setColour (TextButton::buttonColourId, proc.micro.fillActiveNow() && proc.isPlaying() ? theme::accentG.withAlpha (0.6f) : theme::panel2);
    for (auto* r : rows)
    {
        const int h = proc.micro.hitCount (r->ch);
        if (h != r->lastHits) { r->lastHits = h; r->flash = 1.0f; r->repaint(); }
        else if (r->flash > 0.01f) { r->flash *= 0.8f; r->repaint(); }
    }
    repaint (0, 0, getWidth(), 44);
}

void MicroSection::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (theme::panel.darker (0.25f));
    g.fillRoundedRectangle (r, 5.0f);
    // section title
    g.setColour (theme::accentA);
    g.fillRoundedRectangle (10.0f, 10.0f, 22.0f, 22.0f, 3.0f);
    g.setColour (Colours::black);
    g.setFont (theme::font (15.0f, true));
    g.drawText ("A", 10, 10, 22, 22, Justification::centred);
    g.setColour (theme::text);
    g.setFont (theme::font (14.0f, true));
    g.drawText ("DRUM SYNTH", 40, 10, 120, 22, Justification::centredLeft);
    g.setColour (theme::textDim);
    g.setFont (theme::font (11.0f));
    g.drawText ("8-channel synthetic drums", 40, 26, 160, 14, Justification::centredLeft);
    g.drawText ("PATTERN", 200, 4, 80, 12, Justification::centredLeft);
    g.drawText ("MORPH", storeL.getX(), 4, 80, 12, Justification::centredLeft);
    // level meter
    drawMeter (g, Rectangle<float> ((float) getWidth() - 16.0f, 10.0f, 6.0f, 30.0f), proc.meter (0), theme::accentA);
}

void MicroSection::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto header = r.removeFromTop (40);
    header.removeFromLeft (190);
    auto pats = header.removeFromLeft (12 * 30).withTrimmedTop (12);
    for (auto* b : patternButtons) b->setBounds (pats.removeFromLeft (30).reduced (1, 2));
    header.removeFromLeft (14);
    browserBtn.setBounds (header.removeFromLeft (90).withTrimmedTop (12).reduced (1, 2));
    header.removeFromRight (16);
    level.setBounds (header.removeFromRight (64).expanded (0, 4));
    header.removeFromRight (16);
    storeR.setBounds (header.removeFromRight (70).withTrimmedTop (14).reduced (2, 2));
    morph.setBounds (header.removeFromRight (64).expanded (0, 4));
    storeL.setBounds (header.removeFromRight (70).withTrimmedTop (14).reduced (2, 2));

    r.removeFromTop (6);
    auto left = r.removeFromLeft (184);
    r.removeFromLeft (6);
    const int rowH = jmin (28, (left.getHeight() - 70) / numMicroChannels);
    for (auto* row : rows) row->setBounds (left.removeFromTop (rowH).reduced (0, 1));
    left.removeFromTop (6);
    nameEdit.setBounds (left.removeFromTop (24));
    left.removeFromTop (4);
    auto cp = left.removeFromTop (24);
    copyBtn.setBounds (cp.removeFromLeft (cp.getWidth() / 2).reduced (0, 0).withTrimmedRight (2));
    pasteBtn.setBounds (cp.withTrimmedLeft (2));

    auto patArea = r.removeFromBottom (jmax (130, r.getHeight() * 2 / 5));
    r.removeFromBottom (6);
    patch.setBounds (r);

    patPanel.setBounds (patArea);
    auto pc = patPanel.getBounds().withTrimmedTop (Panel::headerH).reduced (6, 4);
    auto ctl = pc.removeFromLeft (330);
    auto c1 = ctl.removeFromLeft (110);
    lenBox.setBounds (c1.removeFromTop (26).reduced (0, 1));
    c1.removeFromTop (4);
    fill.setBounds (c1.removeFromTop (26).reduced (0, 1));
    c1.removeFromTop (4);
    clearBtn.setBounds (c1.removeFromTop (24).reduced (0, 1));
    swing.setBounds (ctl.removeFromLeft (70));
    auto c3 = ctl.reduced (4, 0);
    fillRate.setBounds (c3.removeFromTop (c3.getHeight() - 28));
    auto cpp = c3.withTrimmedTop (4);
    copyPat.setBounds (cpp.removeFromLeft (cpp.getWidth() / 2).withTrimmedRight (2));
    pastePat.setBounds (cpp.withTrimmedLeft (2));
    pc.removeFromLeft (8);
    auto pages = pc.removeFromBottom (20);
    pages.removeFromLeft (56);
    for (auto* b : pageBtns) b->setBounds (pages.removeFromLeft (64).reduced (1, 0));
    pc.removeFromBottom (3);
    grid.setBounds (pc);
}
