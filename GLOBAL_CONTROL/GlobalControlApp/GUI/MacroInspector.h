#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

#include "../Core/AppController.h"

namespace gc::ui {

/** Settings of one macro (shown in a call-out): name, colour, Absolute/Relative, range, inversion,
    precision, fine adjustment, reset / capture / clear and the state of every controlled instance. */
class MacroInspector : public juce::Component,
                       private juce::ChangeListener,
                       private juce::ListBoxModel {
public:
    MacroInspector (AppController&, int index);
    ~MacroInspector() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void load();
    void push();
    void configureRanges();

    AppController& app;
    const int index;
    bool loading = false;

    juce::TextEditor name;
    juce::OwnedArray<juce::TextButton> swatches;
    juce::TextButton absBtn { "ABSOLUTE" }, relBtn { "RELATIVE" };
    juce::Label minLabel, maxLabel, spanLabel, precLabel, fineLabel, info;
    juce::Slider minSlider, maxSlider, spanSlider, precSlider, fineSlider;
    juce::ToggleButton invert { "Invert" };
    juce::TextButton resetBtn { "Reset" }, captureBtn { "Capture" }, clearBtn { "Clear" };
    juce::ListBox instances;
};

} // namespace gc::ui
