#pragma once
#include "LookAndFeel.h"
#include "../PluginProcessor.h"

// -------------------------------------------------------------------------------------------------
//  REMIX decks, in a large window:
//   TAPE  - reel-to-reel machine. Drag the RIGHT reel up/down (up = pull faster, down = slow down / stop),
//           shake the LEFT reel (wobble / drag), click between the reels = a random hand action.
//   VINYL - turntable. Grab the record and move it (scratch: speed follows your hand, holding still = stop),
//           START/STOP button (brake / spin up), click outside the record = a random hand action.
//  The hands follow the mouse; during automatic actions both hands play the deck.
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
    static void drawHand (juce::Graphics&, juce::Point<float> at, float scale, bool left, float grab, float rotation);

    GroovulationProcessor& proc;
    const int type;
    juce::Rectangle<float> deckArea;
    juce::Point<float> reelL, reelR, platter;
    float reelRadius = 100, platterR = 200;
    float angleL = 0, angleR = 0, recordAngle = 0, packL = 0.75f;
    double lastTime = 0;
    // hands: current and target positions (screen), grab amount
    juce::Point<float> handL, handR, handLT, handRT;
    float grabL = 0, grabR = 0;
    enum Grip { none, rightReel, leftReel, record } grip = none;
    juce::Point<float> pressPos, lastPos;
    double lastMoveTime = 0, lastAngleTime = 0;
    float lastAngle = 0;
    float shownSpeed = 1.0f;
    double logoPhase = 0.0;
    juce::OwnedArray<juce::TextButton> buttons;
    juce::Rectangle<float> startStopBtn;
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
