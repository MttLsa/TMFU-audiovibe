#include "MasterPanel.h"

using namespace juce;

MasterPanel::MasterPanel (GroovulationProcessor& p) : proc (p)
{
    for (auto* c : std::initializer_list<Component*> { &pMaster, &pSwing, &pCut, &pBr, &pDeck }) addAndMakeVisible (c);
    auto& s = proc.apvts;
    master.attach (s, "master"); swing.attach (s, "swing"); hp.attach (s, "mHp"); bp.attach (s, "mBpCut"); lp.attach (s, "mLp");
    hpRes.attach (s, "mHpRes"); bpRes.attach (s, "mBpRes"); lpRes.attach (s, "mLpRes");
    hpOn.attach (s, "mHpOn"); bpOn.attach (s, "mBpOn"); lpOn.attach (s, "mLpOn");
    hp.setCaption ("HP Freq"); bp.setCaption ("BP Freq"); lp.setCaption ("LP Freq");
    brInterval.attach (s, "brInterval"); brOffset.attach (s, "brOffset"); brGrid.attach (s, "brGrid"); brChance.attach (s, "brChance");
    brGate.attach (s, "brGate"); brVar.attach (s, "brVar"); brDecay.attach (s, "brDecay"); brPitch.attach (s, "brPitch");
    brPDecay.attach (s, "brPDecay"); brVol.attach (s, "brVol"); deckCtl.attach (s, "deckCtl");
    swingOn.attach (s, "swingOn"); brOn.attach (s, "brOn");
    swingGrid.attach (s, "swingGrid"); brMode.attach (s, "brMode");
    swingGrid.setInverted (true); brMode.setInverted (true);
    for (auto* k : { &master, &swing, &hp, &bp, &lp, &hpRes, &bpRes, &lpRes, &brInterval, &brOffset, &brGrid, &brChance, &brGate, &brVar, &brDecay,
                     &brPitch, &brPDecay, &brVol, &deckCtl })
        addAndMakeVisible (k);
    for (auto* c : std::initializer_list<Component*> { &swingOn, &brOn, &hpOn, &bpOn, &lpOn, &swingGrid, &brMode, &tapeBtn, &vinylBtn }) addAndMakeVisible (c);
    hp.slider.setTooltip ("Passa-alto: a sinistra aperto, girando a destra chiude (speculare al LP)");
    lp.slider.setTooltip ("Passa-basso: a destra aperto, girando a sinistra chiude (speculare all'HP)");
    bp.slider.setTooltip ("Passa-banda: frequenza centrale");
    hpOn.setTooltip ("Attiva / disattiva il passa-alto del master");
    bpOn.setTooltip ("Attiva / disattiva il passa-banda del master");
    lpOn.setTooltip ("Attiva / disattiva il passa-basso del master");
    swingOn.setTooltip ("Global swing, live: drums are re-timed slice by slice (groove), the other modules with a smooth time warp");
    deckCtl.slider.setTooltip ("HAND: a hand on the tape / record for MIDI controllers and automation. Centre = off, left = slow down, stop, reverse, right = push faster (x3)");
    tapeBtn.onClick = [this] { if (onOpenDeck) onOpenDeck (1); };
    vinylBtn.onClick = [this] { if (onOpenDeck) onOpenDeck (2); };
    startTimerHz (20);
}

void MasterPanel::timerCallback()
{
    const float lv = proc.masterLevel();
    if (std::abs (lv - level) > 0.003f) { level = lv; repaint(); }
}

void MasterPanel::paint (Graphics& g)
{
    auto m = pMaster.getBounds().withTrimmedTop (Panel::headerH + 6).reduced (8, 4);
    auto meter = m.removeFromRight (40).toFloat();
    // stereo meter (shared level)
    drawMeter (g, meter.removeFromLeft (14).reduced (0, 2), level, theme::lcdBg2.darker (0.3f));
    meter.removeFromLeft (6);
    drawMeter (g, meter.removeFromLeft (14).reduced (0, 2), level, theme::lcdBg2.darker (0.3f));
}

void MasterPanel::resized()
{
    auto r = getLocalBounds();
    const int H = r.getHeight();
    pMaster.setBounds (r.removeFromTop (H * 15 / 100).withTrimmedBottom (6));
    pSwing.setBounds (r.removeFromTop (H * 16 / 100).withTrimmedBottom (6));
    pCut.setBounds (r.removeFromTop (H * 22 / 100).withTrimmedBottom (6));
    pBr.setBounds (r.removeFromTop (H * 32 / 100).withTrimmedBottom (6));
    pDeck.setBounds (r);
    auto in = [] (Panel& p) { return p.getBounds().withTrimmedTop (Panel::headerH + 2).reduced (6, 3); };
    {
        auto a = in (pMaster); a.removeFromRight (46);
        master.setBounds (a);
    }
    {
        auto a = in (pSwing);
        auto left = a.removeFromLeft (a.getWidth() / 2);
        swingOn.setBounds (left.removeFromTop (left.getHeight() / 2).withSizeKeepingCentre (left.getWidth() - 30, 26));
        swingGrid.setBounds (left.withSizeKeepingCentre (left.getWidth() - 20, 24));
        swing.setBounds (a);
    }
    {
        auto a = in (pCut);
        const int cw = a.getWidth() / 3;
        Toggle* tg[3] { &hpOn, &bpOn, &lpOn };
        Knob* fk[3] { &hp, &bp, &lp };
        Knob* rk[3] { &hpRes, &bpRes, &lpRes };
        for (int k = 0; k < 3; ++k)
        {
            auto col = (k < 2 ? a.removeFromLeft (cw) : a).reduced (3, 0);
            tg[k]->setBounds (col.removeFromTop (22).reduced (6, 1));
            layoutRow (col, { fk[k], rk[k] });
        }
    }
    {
        auto a = in (pBr);
        auto top = a.removeFromTop (26);
        brOn.setBounds (top.removeFromLeft (60).reduced (2, 1));
        top.removeFromRight (60);
        brMode.setBounds (top.reduced (8, 1));
        layoutRow (a.removeFromTop (a.getHeight() / 2), { &brInterval, &brOffset, &brGrid, &brChance, &brGate });
        layoutRow (a, { &brVar, &brDecay, &brPitch, &brPDecay, &brVol });
    }
    {
        auto a = in (pDeck);
        auto btns = a.removeFromLeft (a.getWidth() / 2);
        tapeBtn.setBounds (btns.removeFromTop (btns.getHeight() / 2).reduced (4, 4));
        vinylBtn.setBounds (btns.reduced (4, 4));
        layoutRow (a, { &deckCtl });
    }
}
