#pragma once
#include "LookAndFeel.h"
#include "../PluginProcessor.h"

// -------------------------------------------------------------------------------------------------
//  REMIX decks, in a large window:
//   TAPE  - reel-to-reel machine. Drag the RIGHT reel up/down (up = pull faster, down = slow down / stop),
//           shake the LEFT reel (wobble / drag), click between the reels = a random action.
//   VINYL - turntable. Grab the record and move it (scratch: speed follows your hand, holding still = stop),
//           START/STOP (brake, stays stopped), 33 / 45 and the pitch fader (speed + pitch) like a real deck.
// -------------------------------------------------------------------------------------------------
class DeckView : public juce::Component, private juce::Timer
{
public:
    DeckView (GroovulationProcessor& p, int type);
    ~DeckView() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
private:
    void timerCallback() override;
    void trigger (int action);
    void renderBackdrop();
    void drawTape (juce::Graphics&);
    void drawVinyl (juce::Graphics&);
    void drawReel (juce::Graphics&, juce::Point<float> c, float r, float angle, float pack);   // hub = Groovulation logo

    GroovulationProcessor& proc;
    const int type;
    juce::Rectangle<float> deckArea;
    juce::Point<float> reelL, reelR, platter;
    float reelRadius = 100, platterR = 200;
    float angleL = 0, angleR = 0, recordAngle = 0, packL = 0.75f;
    double lastTime = 0;
    enum Grip { none, rightReel, leftReel, record, fader } grip = none;
    juce::Point<float> pressPos, lastPos;
    double lastMoveTime = 0, lastAngleTime = 0;
    float lastAngle = 0;
    float shownSpeed = 1.0f;
    double logoPhase = 0.0;
    juce::OwnedArray<juce::TextButton> buttons;
    juce::Rectangle<float> startStopBtn, rpm33, rpm45, faderArea;
    float vinylRate() const;
    juce::Image backdrop;   // neutral background, rendered on resize
};

class DeckWindow : public juce::DocumentWindow
{
public:
    DeckWindow (GroovulationProcessor& p, int type, std::function<void()> onClose);
    void closeButtonPressed() override;
private:
    GroovulationProcessor& proc;
    std::function<void()> onCloseFn;
};
