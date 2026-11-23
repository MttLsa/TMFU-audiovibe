#pragma once
// Sections of the main window:
//   A HeaderBar          connection status, project, last scan, instance count, SCAN PROJECT
//   B DeviceBrowser      plug-ins / devices of the project, search
//   C InstancePanel      instances of the selected plug-in: selection, search, filter, groups, exclusion
//   D ParameterPanel     parameters exposed by Live, multi-selection, assignment to macros
//   F PresetBar          project / global presets
//     SetupPanel         first-run guide shown while the bridge is not connected
//     StatusBar          last event + log
#include <juce_gui_basics/juce_gui_basics.h>

#include "../Core/AppController.h"
#include "../Core/BridgeInstaller.h"

namespace gc::ui {

class HeaderBar : public juce::Component {
public:
    explicit HeaderBar (AppController&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void refresh();
    std::function<void()> onSetup;
private:
    AppController& app;
    juce::TextButton scan { "SCAN PROJECT" }, setup { "SETUP" };
    juce::Rectangle<int> projectArea;
};

class DeviceBrowser : public juce::Component, private juce::ListBoxModel {
public:
    explicit DeviceBrowser (AppController&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
private:
    int getNumRows() override;
    void paintListBoxItem (int, juce::Graphics&, int, int, bool) override;
    void selectedRowsChanged (int) override;
    AppController& app;
    juce::TextEditor search;
    juce::ToggleButton pluginsOnly { "Plug-ins only" };
    juce::ListBox list;
    juce::Array<PluginKind> rows;
    bool updating = false;
};

class InstancePanel : public juce::Component, private juce::TableListBoxModel {
public:
    explicit InstancePanel (AppController&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
private:
    int getNumRows() override;
    void paintRowBackground (juce::Graphics&, int, int, int, bool) override;
    void paintCell (juce::Graphics&, int, int, int, int, bool) override;
    void selectedRowsChanged (int) override;
    void cellClicked (int row, int col, const juce::MouseEvent&) override;
    void groupMenu();
    void askName (const juce::String& title, const juce::String& initial, std::function<void (juce::String)>);
    AppController& app;
    juce::TextEditor search;
    juce::ComboBox filter;
    juce::TextButton all { "All" }, none { "None" }, group { "Groups" }, exclude { "Exclude" }, include { "Include" };
    juce::TableListBox table;
    juce::Array<InstanceRow> rows;
    bool updating = false;
};

class ParameterPanel : public juce::Component, private juce::ListBoxModel {
public:
    explicit ParameterPanel (AppController&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    void setTargetMacro (int);
    int getTargetMacro() const { return target.getSelectedId() - 1; }
private:
    int getNumRows() override;
    void paintListBoxItem (int, juce::Graphics&, int, int, bool) override;
    void listBoxItemDoubleClicked (int, const juce::MouseEvent&) override;
    void assign();
    AppController& app;
    juce::TextEditor search;
    juce::ListBox list;
    juce::ComboBox target;
    juce::TextButton assignBtn { "ASSIGN" };
    juce::Label hint;
    juce::Array<ParamInfo> rows;
};

class PresetBar : public juce::Component {
public:
    explicit PresetBar (AppController&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
private:
    PresetScope scope() const { return projectBtn.getToggleState() ? PresetScope::Project : PresetScope::Global; }
    const PresetInfo* current() const;
    void save(); void load(); void rename(); void duplicate(); void remove();
    void showReport (const juce::String& title, const juce::StringArray&);
    AppController& app;
    juce::TextButton projectBtn { "PROJECT" }, globalBtn { "GLOBAL" };
    juce::ComboBox presets;
    juce::TextButton saveBtn { "Save" }, loadBtn { "Load" }, renameBtn { "Rename" }, dupBtn { "Duplicate" }, delBtn { "Delete" };
    juce::Array<PresetInfo> items;
};

class SetupPanel : public juce::Component {
public:
    explicit SetupPanel (AppController&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    std::function<void()> onClose;
private:
    void installBridge (bool overwrite);
    AppController& app;
    BridgeEnvironment env;
    juce::TextButton install { "Install bridge in User Library" }, reveal { "Show folder" }, close { "Close" };
    juce::String installMessage;
    double lastInspect = 0;
};

class StatusBar : public juce::Component {
public:
    explicit StatusBar (AppController&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh() { repaint(); }
private:
    AppController& app;
    juce::TextButton logBtn { "Log" };
};

} // namespace gc::ui
