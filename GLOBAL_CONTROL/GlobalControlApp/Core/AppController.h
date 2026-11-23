#pragma once
// AppController: the application state and logic behind the GUI.
//  Device browser  : plug-ins found by the last scan, grouped by kind (class + plug-in name)
//  Instance manager: selection, search/filter, exclusion, named groups of instances
//  Parameter browser: parameters really exposed by Live for the selected plug-in
//  Macro engine    : 8 independent macros (plug-in / parameter / instances / mode / range ...)
//  Feedback        : values coming back from Live, origin-aware, no feedback loop
//  Presets & reconnection: versioned JSON, automatic session state per Live Set, reconciliation
// Everything runs on the message thread; network I/O is asynchronous (BridgeConnection).
#include <juce_events/juce_events.h>

#include <array>
#include <optional>
#include <set>

#include "../MacroEngine/MacroEngine.h"
#include "../OSC/BridgeConnection.h"
#include "../PresetEngine/PresetStore.h"

namespace gc {

constexpr int kNumMacros = 8;

enum class InstanceStatus { Pending, Ok, Missing, Ambiguous, ParamMissing, Locked };
const char* toString (InstanceStatus);

struct MacroInstance {
    juce::String deviceId;                 // Live id in the current session ("" when unresolved)
    DeviceFingerprint fp;                  // persistent identity
    int64_t paramId = 0;
    double startRaw = 0, currentRaw = 0;
    bool hasStart = false, hasValue = false;
    juce::String display, lastSource, note;
    InstanceStatus status = InstanceStatus::Pending;
    bool controllable() const { return status == InstanceStatus::Ok && paramId != 0 && hasValue; }
};

struct Macro {
    MacroSettings settings;
    bool assigned = false;
    juce::String className, pluginName;    // plug-in (class_name / class_display_name)
    juce::String paramName;                // original_name of the DeviceParameter
    int paramIndex = -1;
    juce::String groupName;
    std::vector<MacroInstance> instances;
    std::optional<ParameterScale> scale;
    double knob = 0.5;
    bool mixed = false;                    // absolute: instances currently differ
    bool outOfSync = false;                // relative: values in Live differ from start + variation
    bool fresh = false;                    // just assigned: apply default range when the scale is known
    juce::String valueText;

    juce::String pluginKey() const { return className + "|" + pluginName; }
    int countStatus (InstanceStatus s) const;
    int controllableCount() const;
    juce::String summary() const;          // "Pro-C 2 / Threshold / 10 inst."
};

struct InstanceGroup {
    juce::String name;
    juce::String pluginKey;
    juce::StringArray deviceIds;           // current session
    std::vector<DeviceFingerprint> members;
};

struct InstanceRow {                       // one line of the instance manager
    const DeviceInfo* device = nullptr;    // nullptr for a saved instance not found in the project
    juce::String label, trackName, instanceId, groupNames, macros, connection;
    int paramCount = 0;
    bool selected = false, excluded = false, missing = false;
};

struct PluginKind {                        // one line of the device browser
    juce::String key, className, name, format;
    int instances = 0;
    bool isPlugin = false;
};

class AppController : public juce::ChangeBroadcaster,
                      private BridgeConnection::Listener,
                      private juce::Timer {
public:
    explicit AppController (juce::File presetRoot = PresetStore::defaultRoot());
    ~AppController() override;

    bool start (int appPort = proto::kAppPort, int bridgePort = proto::kBridgePort);
    juce::String getStartError() const { return startError; }
    BridgeConnection& getBridge() { return bridge; }

    // ------------------------------------------------------------------ connection / project
    BridgeConnection::State connectionState() const { return bridge.getState(); }
    bool isLiveRunning() const { return liveRunning.load(); }
    const ProjectSnapshot& getSnapshot() const { return snapshot; }
    juce::String getProjectId() const { return bridge.getProjectId(); }
    juce::String getProjectName() const;
    void setProjectName (const juce::String&);
    void scanProject();
    bool isScanning() const { return bridge.isScanning(); }

    // ------------------------------------------------------------------ device browser
    juce::Array<PluginKind> pluginKinds (const juce::String& search, bool pluginsOnly) const;
    void selectPlugin (const juce::String& pluginKey);
    juce::String getSelectedPlugin() const { return selectedPlugin; }

    // ------------------------------------------------------------------ instance manager
    juce::Array<InstanceRow> instanceRows (const juce::String& search, int filter) const;   // filter: 0 all, 1 selected, 2 mapped, 3 unmapped, 4 excluded
    void setInstanceSelected (const juce::String& deviceId, bool);
    void selectAllInstances (bool);
    void setInstanceExcluded (const juce::String& deviceId, bool);
    bool isExcluded (const juce::String& deviceId) const { return excluded.count (deviceId) > 0; }
    juce::StringArray selectedInstances() const;
    const std::vector<InstanceGroup>& getGroups() const { return groups; }
    juce::Result createGroup (const juce::String& name);
    juce::Result renameGroup (int index, const juce::String& name);
    void deleteGroup (int index);
    void selectGroup (int index);

    // ------------------------------------------------------------------ parameter browser
    /** Parameters of the selected plug-in (from its first instance), empty while loading. */
    const juce::Array<ParamInfo>* selectedPluginParameters() const;
    bool parametersLoading() const;

    // ------------------------------------------------------------------ macros
    const Macro& getMacro (int i) const { return macros[(size_t) i]; }
    /** Assigns the given parameters (original names) to macro slots starting at firstSlot. */
    juce::Result assignParameters (const juce::StringArray& paramNames, int firstSlot);
    void clearMacro (int i);
    void setMacroKnob (int i, double knob01);
    void setMacroSettings (int i, const MacroSettings&);
    void setMacroMode (int i, MacroMode);
    void resetMacro (int i);
    void captureRelative (int i);          // relative: current values become the new starting point
    juce::String formatMacroValue (int i, double knob01) const;

    // ------------------------------------------------------------------ presets
    PresetStore& getPresetStore() { return presets; }
    juce::var toPresetVar() const;
    /** Applies a preset; returns a human-readable report of what was / was not restored. */
    juce::StringArray applyPresetVar (const juce::var&, PresetScope);

    // ------------------------------------------------------------------ log
    const juce::StringArray& getLog() const { return log; }
    juce::String lastMessage() const { return log.isEmpty() ? juce::String() : log[log.size() - 1]; }
    void addLog (const juce::String&);

    /** Called after each change from the bridge; tests use it to wait for state. */
    std::function<void()> onStateChanged;

private:
    // BridgeConnection::Listener
    void connectionChanged (BridgeConnection::State) override;
    void projectChanged (const juce::String&) override;
    void scanCompleted (const ProjectSnapshot&) override;
    void deviceParametersReceived (const juce::String&, const juce::Array<ParamInfo>&) override;
    void curveReceived (const CurveData&) override;
    void parameterValue (int64_t, double, const juce::String&, const juce::String&, int) override;
    void structureChanged() override;
    void parameterRemoved (int64_t) override;
    void bridgeError (const juce::String&, const juce::String&) override;
    void timerCallback() override;

    void changed (bool persist = true);
    void resolveAfterScan (bool newSession);
    void remapGroupsAndExclusions();
    void resolveMacroParams (Macro&);
    void requestMissingData();
    void rebuildIndex();
    void updateMacroDisplay (Macro&);
    void finishAssignmentIfReady (Macro&);
    juce::String scaleKey (const juce::String& pluginKey, const juce::String& param) const { return pluginKey + "#" + param; }
    std::vector<const DeviceInfo*> devicesOf (const juce::String& pluginKey) const;
    juce::var macroToVar (const Macro&, int slot) const;
    void persistSession();

    BridgeConnection bridge;
    PresetStore presets;
    juce::String startError;
    ProjectSnapshot snapshot;
    std::vector<DeviceFingerprint> snapshotFps;                    // aligned with snapshot.devices
    juce::String stateProjectId;           // project the current macros/groups belong to
    bool sessionRestorePending = false, lastScanNewSession = true;
    std::map<juce::String, juce::Array<ParamInfo>> paramCache;     // deviceId -> parameters
    std::set<juce::String> paramRequested;
    std::map<juce::String, ParameterScale> scaleCache;             // pluginKey#param -> scale
    std::set<int64_t> curveRequested;
    std::map<int64_t, juce::String> curveKeyByPid;
    std::multimap<int64_t, std::pair<int, int>> byParam;           // pid -> (macro, instance)
    std::set<int64_t> observed;

    juce::String selectedPlugin;
    std::set<juce::String> selected, excluded;
    std::vector<DeviceFingerprint> excludedFps;
    std::vector<InstanceGroup> groups;
    std::array<Macro, kNumMacros> macros;
    juce::StringArray log;
    double persistDue = 0;
    bool rescanQueued = false;

    std::atomic<bool> liveRunning { false };
    std::unique_ptr<juce::Thread> liveWatcher;
};

} // namespace gc
