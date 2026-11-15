#pragma once
#include "PluginProcessor.h"
#include "gui/LookAndFeel.h"
#include "gui/ModuleViews.h"
#include "gui/MasterPanel.h"
#include "gui/DeckWindows.h"

// Panel shown after EXPORT: the printed loops can be dragged into Ableton Live / Logic in one go
// (the DAW creates one audio track per file).
class ExportPanel : public juce::Component
{
public:
    ExportPanel();
    void setFiles (const juce::Array<juce::File>& f, const juce::File& folder);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDrag (const juce::MouseEvent&) override;
    std::function<void()> onClose;
private:
    juce::Array<juce::File> files;
    juce::File folder;
    juce::TextButton reveal { "SHOW IN FINDER" }, close { "CLOSE" };
    juce::Rectangle<int> dragArea;
};

class MainView : public juce::Component, private juce::Timer
{
public:
    explicit MainView (GroovulationProcessor& p);
    ~MainView() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    static constexpr int designW = 1640, designH = 1000;
    void selectModule (int m);
private:
    void timerCallback() override;
    void showMenu();
    void startPrint (bool exportAfter);
    void openDeck (int type);
    void askText (const juce::String& title, const juce::String& initial, std::function<void (juce::String)> done);
    void drawWoofer (juce::Graphics&, juce::Rectangle<float>);

    GroovulationProcessor& proc;
    std::vector<std::unique_ptr<ModuleStrip>> strips;
    ModuleEditor editor;
    MasterPanel master;
    juce::TextButton menu { "MENU" }, randomAll { "RANDOM ALL" }, print { "PRINT" }, exportBtn { "EXPORT" };
    Toggle scaleOn { "SCALE", theme::amber }, raw { "RAW", theme::red }, run { "RUN", theme::lcdDim };
    juce::ComboBox scaleRoot, scaleType;
    juce::Slider bpm;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rootAtt, typeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmAtt;
    ExportPanel exportPanel;
    std::unique_ptr<DeckWindow> deckWindow;
    std::unique_ptr<juce::FileChooser> chooser;
    std::atomic<float> printProgress { -1.0f };
    juce::String status;
    double phase = 0.0;
    float level = 0.0f;
    juce::Rectangle<float> logoArea, lcdArea;
};

class GroovulationEditor : public juce::AudioProcessorEditor
{
public:
    explicit GroovulationEditor (GroovulationProcessor&);
    ~GroovulationEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    GroovLookAndFeel lnf;
    GroovulationProcessor& proc;
    MainView view;
    juce::TooltipWindow tooltips { this, 600 };
    float scale = 0.75f;
};
