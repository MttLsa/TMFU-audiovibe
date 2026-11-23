#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

#include "../Core/AppController.h"

namespace gc::ui {

/** One of the 8 macro controls: name, mode badge, large circular knob, numeric display, assignment summary.
    Drag (vertical or horizontal) to move, Shift = fine adjustment, wheel, double-click = reset,
    right-click = menu (mode, capture, settings, clear). Positions coming back from Live are animated. */
class MacroKnob : public juce::Component, private juce::Timer {
public:
    MacroKnob (AppController&, int index);

    std::function<void (int)> onSelect;          // the macro became the assignment target
    std::function<void (int)> onOpenSettings;
    void setSelected (bool);
    void refresh();                              // called by the main window when the state changed

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void showMenu();
    void moveBy (double delta, bool fine);
    juce::Rectangle<float> knobArea() const;

    AppController& app;
    const int index;
    bool selected = false, dragging = false, hover = false;
    double shown = 0.5;                          // animated knob position
    float dragStartY = 0, dragStartX = 0;
    double dragStartValue = 0;
    juce::Rectangle<int> gearArea, nameArea, displayArea;
};

} // namespace gc::ui
