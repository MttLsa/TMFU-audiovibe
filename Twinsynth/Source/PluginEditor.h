#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Skins.h"

namespace ui
{
    const juce::Colour bg     { 0xff17181b };
    const juce::Colour panel  { 0xff222429 };
    const juce::Colour panel2 { 0xff2b2e34 };
    const juce::Colour line   { 0xff3a3e46 };
    const juce::Colour text   { 0xffdcdfe4 };
    const juce::Colour dim    { 0xff8b919c };
    const juce::Colour colA   { 0xffff8a3d };   // Section A — analog, warm orange
    const juce::Colour colD   { 0xff35d0ff };   // Section D — digital, cyan
    const juce::Colour colG   { 0xffe8e8e8 };
    inline juce::Font font (float h, bool bold = false) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
}

class TwinLook : public juce::LookAndFeel_V4
{
public:
    TwinLook();
    void setSkin (const skins::Skin& s);
    const skins::Skin* skin = nullptr;
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return ui::font (11.5f); }
    juce::Font getPopupMenuFont() override { return ui::font (13.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return ui::font (juce::jmin (12.5f, h * 0.6f), true); }
};

// One parameter control (knob, combo or toggle) with its caption.
class Control : public juce::Component
{
public:
    Control (juce::AudioProcessorValueTreeState& s, int pid, const juce::String& label, juce::Colour accent);
    void setSkin (const skins::Skin& s);
    void resized() override;
    void paint (juce::Graphics&) override;
    int pid;
    bool wide = false;
private:
    juce::String caption;
    juce::Colour accent;
    juce::Slider knob;
    juce::ComboBox combo;
    juce::TextButton toggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sa;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ca;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ba;
};

struct GroupDef { juce::String title; std::vector<std::pair<int, juce::String>> items; int cols = 0; int page = -1; };

class SectionPanel : public juce::Component, private juce::AudioProcessorValueTreeState::Listener, private juce::AsyncUpdater
{
public:
    SectionPanel (juce::AudioProcessorValueTreeState& s, const juce::String& title, const juce::String& subtitle,
                  juce::Colour accent, std::vector<GroupDef> groups, int onPid, int pagePid);
    ~SectionPanel() override;
    void setSkin (const juce::String& key, const juce::String& model, const juce::String& lcdText);
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
private:
    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { resized(); repaint(); }
    juce::AudioProcessorValueTreeState& state;
    juce::String title, subtitle;
    juce::Colour accent;
    TwinLook look;
    const skins::Skin* skin = nullptr;
    juce::String model, lcdText;
    bool digital = false, compact = false;
    std::vector<GroupDef> groups;
    std::vector<std::vector<std::unique_ptr<Control>>> controls;
    std::vector<juce::Rectangle<int>> groupBounds;
    std::unique_ptr<Control> onButton;
    int onPid, pagePid;
};

struct FilterListModel : public juce::ListBoxModel
{
    juce::StringArray items; int selected = 0; std::function<void()> onChange;
    int getNumRows() override { return items.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool sel) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override { selected = row; if (onChange) onChange(); }
};

class PresetBrowser : public juce::Component, private juce::TableListBoxModel
{
public:
    explicit PresetBrowser (TwinsynthProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    void showSelected();
    std::function<void()> onClose;
private:
    // results table
    int getNumRows() override;
    void paintRowBackground (juce::Graphics&, int row, int w, int h, bool selected) override;
    void paintCell (juce::Graphics&, int row, int col, int w, int h, bool selected) override;
    void selectedRowsChanged (int row) override;
    void cellDoubleClicked (int, int, const juce::MouseEvent&) override { if (onClose) onClose(); }
    void apply();

    TwinsynthProcessor& proc;
    juce::TableListBox table { "presets", this };
    FilterListModel decadeModel, genreModel;
    juce::ListBox decadeList, genreList;
    juce::TextButton secAll { "ALL" }, secA { "A" }, secD { "D" }, secAD { "A+D" }, danceOnly { "DANCE / HOUSE / TECHNO" }, close { "CLOSE" };
    juce::TextEditor search;
    int secSel = 0;
    std::vector<int> rows;
    juce::Label info;
};

class TwinsynthEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TwinsynthEditor (TwinsynthProcessor&);
    ~TwinsynthEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void updateHeader();
    void step (int d);
    TwinsynthProcessor& proc;
    TwinLook look;
    juce::Component content;
    std::unique_ptr<SectionPanel> panelA, panelD, panelG;
    juce::MidiKeyboardComponent keys;
    PresetBrowser browser;
    juce::TextButton prev { "<" }, next { ">" }, browse { "BROWSER" };
    juce::Label presetName, presetInfo;
    int shownPreset = -2;
    static constexpr int W = 1360, H = 756;
};
