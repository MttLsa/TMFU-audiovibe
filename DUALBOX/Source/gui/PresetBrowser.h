#pragma once
#include "LookAndFeel.h"
#include "../PluginProcessor.h"
#include "../Presets.h"

// Genre preset browser (Section A: kit + pattern into the current slot / Section B: kit + patterns A-D)
class PresetBrowser : public juce::Component, private juce::ListBoxModel
{
public:
    PresetBrowser (TwinboxProcessor& p, int section);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    int getNumRows() override { return (int) filtered.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override { load (row); }
    void selectGenre (int g);
    void load (int row);

    TwinboxProcessor& proc;
    int section;
    juce::OwnedArray<juce::TextButton> genreBtns;
    juce::ListBox list;
    juce::ToggleButton kitTgl { "Kit (sounds)" }, patTgl { "Pattern(s)" }, tempoTgl { "Set tempo" };
    juce::TextButton loadBtn { "LOAD" };
    std::vector<int> filtered;
    int genre = 0;
    juce::String lastLoaded;
};
