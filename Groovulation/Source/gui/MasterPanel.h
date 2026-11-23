#pragma once
#include "LookAndFeel.h"
#include "../PluginProcessor.h"

// Master section: MASTER, GLOBAL SWING (ON / amount, live), CUTOFF (HP | BP | LP), REMIX (Beat Repeat, Tape, Vinyl)
class MasterPanel : public juce::Component, private juce::Timer
{
public:
    explicit MasterPanel (GroovulationProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    std::function<void (int)> onOpenDeck;   // 1 = tape, 2 = vinyl
private:
    void timerCallback() override;
    GroovulationProcessor& proc;
    Panel pMaster { "Master", theme::master }, pSwing { "Global Swing", theme::master }, pCut { "Cutoff", theme::master },
          pBr { "Remix  /  Beat Repeat", theme::master }, pDeck { "Remix  /  Tape & Vinyl", theme::master };
    Knob master { "Master", theme::master }, swing { "Amount", theme::master }, hp { "HP", theme::master }, bp { "BP", theme::master },
         lp { "LP", theme::master }, mres { "Res", theme::master },
         brInterval { "Interval", theme::master }, brOffset { "Offset", theme::master }, brGrid { "Grid", theme::master },
         brChance { "Chance", theme::master }, brGate { "Length", theme::master }, brVar { "Variation", theme::master },
         brDecay { "Decay", theme::master }, brPitch { "Pitch", theme::master }, brPDecay { "P.Decay", theme::master },
         brVol { "Volume", theme::master }, deckCtl { "Hand", theme::master };
    Toggle swingOn { "ON", theme::red }, brOn { "ON", theme::red }, brNoTrip { "NO TRIPLETS", theme::red };
    Segmented swingGrid { theme::master }, brMode { theme::master };
    juce::TextButton tapeBtn { "TAPE MACHINE" }, vinylBtn { "VINYL" };
    float level = 0.0f;
};
