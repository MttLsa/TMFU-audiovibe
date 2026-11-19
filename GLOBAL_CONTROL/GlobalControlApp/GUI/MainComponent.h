#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

#include "MacroKnob.h"
#include "Panels.h"

namespace gc::ui {

class MainComponent : public juce::Component, private juce::ChangeListener, private juce::Timer {
public:
    explicit MainComponent (AppController&);
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    void selectMacro (int);
    void showSetup (bool);

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void refreshAll();
    void openInspector (int);

    AppController& app;
    HeaderBar header;
    PresetBar presetBar;
    DeviceBrowser devices;
    InstancePanel instances;
    ParameterPanel parameters;
    juce::OwnedArray<MacroKnob> knobs;
    StatusBar status;
    SetupPanel setup;
    juce::TooltipWindow tooltips { this, 600 };
    int selectedMacro = 0;
    bool setupDismissed = false, wasConnected = false;
    juce::uint32 startedAt = juce::Time::getMillisecondCounter();
    juce::Rectangle<int> macroArea;
};

} // namespace gc::ui
