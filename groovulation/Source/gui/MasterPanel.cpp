#include "MasterPanel.h"

using namespace juce;

MasterPanel::MasterPanel (GroovulationProcessor& p) : proc (p)
{
    for (auto* c : std::initializer_list<Component*> { &pMaster, &pSwing, &pCut, &pBr, &pDeck }) addAndMakeVisible (c);
    auto& s = proc.apvts;
    master.attach (s, "master"); swing.attach (s, "swing"); hp.attach (s, "mHp"); bp.attach (s, "mBp"); lp.attach (s, "mLp"); mres.attach (s, "mRes");
    brInterval.attach (s, "brInterval"); brOffset.attach (s, "brOffset"); brGrid.attach (s, "brGrid"); brChance.attach (s, "brChance");
    brGate.attach (s, "brGate"); brVar.attach (s, "brVar"); brDecay.attach (s, "brDecay"); brPitch.attach (s, "brPitch");
    brPDecay.attach (s, "brPDecay"); brVol.attach (s, "brVol"); deckCtl.attach (s, "deckCtl");
    swingOn.attach (s, "swingOn"); brOn.attach (s, "brOn"); brNoTrip.attach (s, "brNoTrip");
    swingGrid.attach (s, "swingGrid"); brMode.attach (s, "brMode");
    for (auto* k : { &master, &swing, &hp, &bp, &lp, &mres, &brInterval, &brOffset, &brGrid, &brChance, &brGate, &brVar, &brDecay,
                     &brPitch, &brPDecay, &brVol, &deckCtl })
        addAndMakeVisible (k);
    for (auto* c : std::initializer_list<Component*> { &swingOn, &brOn, &brNoTrip, &swingGrid, &brMode, &tapeBtn, &vinylBtn }) addAndMakeVisible (c);
    hp.slider.setTooltip ("High-pass: turn right to close (mirror of LP)");
    lp.slider.setTooltip ("Low-pass: turn left to close (mirror of HP)");
    bp.slider.setTooltip ("Band-pass centre: fully left = off");
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
    pMaster.setBounds (r.removeFromTop (H * 17 / 100).withTrimmedBottom (6));
    pSwing.setBounds (r.removeFromTop (H * 18 / 100).withTrimmedBottom (6));
    pCut.setBounds (r.removeFromTop (H * 18 / 100).withTrimmedBottom (6));
    pBr.setBounds (r.removeFromTop (H * 32 / 100).withTrimmedBottom (6));
    pDeck.setBounds (r);
    auto in = [] (Panel& p) { return p.getBounds().withTrimmedTop (Panel::headerH + 2).reduced (6, 3); };
    {
        auto a = in (pMaster); a.removeFromRight (46);
        master.setBounds (a.removeFromLeft (a.getWidth() / 2));
        mres.setBounds (a);
    }
    {
        auto a = in (pSwing);
        auto left = a.removeFromLeft (a.getWidth() / 2);
        swingOn.setBounds (left.removeFromTop (left.getHeight() / 2).withSizeKeepingCentre (left.getWidth() - 30, 26));
        swingGrid.setBounds (left.withSizeKeepingCentre (left.getWidth() - 20, 24));
        swing.setBounds (a);
    }
    layoutRow (in (pCut), { &hp, &bp, &lp });
    {
        auto a = in (pBr);
        auto top = a.removeFromTop (26);
        brOn.setBounds (top.removeFromLeft (60).reduced (2, 1));
        brNoTrip.setBounds (top.removeFromRight (110).reduced (2, 1));
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
