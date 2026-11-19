#include "AppController.h"

#include <cmath>

namespace gc {

using juce::String;

static const uint32_t kPalette[kNumMacros] = { 0xffe8a33d, 0xff3fc1d9, 0xffd65db1, 0xff8fd14f,
                                                0xfff07c3a, 0xff5b8def, 0xffe5534b, 0xff3ec9a7 };

const char* toString (InstanceStatus s)
{
    switch (s) {
        case InstanceStatus::Pending: return "resolving";
        case InstanceStatus::Ok: return "ok";
        case InstanceStatus::Missing: return "not found";
        case InstanceStatus::Ambiguous: return "ambiguous";
        case InstanceStatus::ParamMissing: return "parameter not exposed";
        case InstanceStatus::Locked: return "locked";
    }
    return "?";
}

int Macro::countStatus (InstanceStatus s) const
{
    int n = 0;
    for (const auto& i : instances) if (i.status == s) ++n;
    return n;
}

int Macro::controllableCount() const
{
    int n = 0;
    for (const auto& i : instances) if (i.controllable()) ++n;
    return n;
}

String Macro::summary() const
{
    if (! assigned) return "not assigned";
    String s = pluginName + " / " + paramName + " / " + String (countStatus (InstanceStatus::Ok)) + " inst.";
    const int bad = (int) instances.size() - countStatus (InstanceStatus::Ok) - countStatus (InstanceStatus::Pending);
    if (bad > 0) s << " (" << bad << " unresolved)";
    return s;
}

// ------------------------------------------------------------------------------------------------- Live process
namespace {
class LiveWatcher : public juce::Thread {
public:
    explicit LiveWatcher (std::atomic<bool>& f) : juce::Thread ("GC Live watcher"), flag (f) {}
    void run() override
    {
        while (! threadShouldExit()) {
           #if JUCE_MAC
            juce::ChildProcess p;
            if (p.start ("/usr/bin/pgrep -x Live", juce::ChildProcess::wantStdOut)) {
                const auto out = p.readAllProcessOutput();
                flag = out.trim().isNotEmpty();
            }
           #endif
            wait (3000);
        }
    }
private:
    std::atomic<bool>& flag;
};
}

// ------------------------------------------------------------------------------------------------- lifecycle
AppController::AppController (juce::File presetRoot) : presets (std::move (presetRoot))
{
    for (int i = 0; i < kNumMacros; ++i) {
        macros[(size_t) i].settings.name = "Macro " + std::to_string (i + 1);
        macros[(size_t) i].settings.colour = kPalette[i];
    }
    bridge.addListener (this);
}

AppController::~AppController()
{
    stopTimer();
    if (liveWatcher) liveWatcher->stopThread (4000);
    persistSession();
    bridge.removeListener (this);
    bridge.stop();
}

bool AppController::start (int appPort, int bridgePort)
{
    if (! bridge.start (appPort, bridgePort, &startError)) { addLog ("ERROR " + startError); return false; }
    liveWatcher = std::make_unique<LiveWatcher> (liveRunning);
    liveWatcher->startThread (juce::Thread::Priority::low);
    startTimer (100);
    addLog ("Listening on 127.0.0.1:" + String (appPort) + ", bridge expected on port " + String (bridgePort));
    return true;
}

void AppController::addLog (const String& m)
{
    log.add (juce::Time::getCurrentTime().toString (false, true, true, true) + "  " + m);
    if (log.size() > 300) log.removeRange (0, log.size() - 300);
}

void AppController::changed (bool persist)
{
    if (persist && stateProjectId.isNotEmpty()) persistDue = juce::Time::getMillisecondCounterHiRes() + 1500;
    sendChangeMessage();
    if (onStateChanged) onStateChanged();
}

void AppController::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (persistDue > 0 && now >= persistDue) { persistDue = 0; persistSession(); }
    if (rescanQueued && bridge.getState() == BridgeConnection::State::Connected && ! bridge.isScanning()) {
        rescanQueued = false;
        scanProject();
    }
}

String AppController::getProjectName() const
{
    const auto f = presets.getRoot().getChildFile ("project_names.json");
    const auto v = juce::JSON::parse (f.loadFileAsString());
    const auto n = v[juce::Identifier (getProjectId().isEmpty() ? "_" : getProjectId())].toString();
    return n;
}

void AppController::setProjectName (const String& name)
{
    if (getProjectId().isEmpty()) return;
    const auto f = presets.getRoot().getChildFile ("project_names.json");
    auto v = juce::JSON::parse (f.loadFileAsString());
    if (! v.isObject()) v = juce::var (new juce::DynamicObject());
    v.getDynamicObject()->setProperty (juce::Identifier (getProjectId()), name.trim());
    f.getParentDirectory().createDirectory();
    f.replaceWithText (juce::JSON::toString (v));
    changed (false);
}

// ------------------------------------------------------------------------------------------------- bridge events
void AppController::connectionChanged (BridgeConnection::State s)
{
    addLog (String ("Bridge ") + toString (s) + (s == BridgeConnection::State::Connected ? " - Live " + bridge.getLiveVersion() : String()));
    changed (false);
}

void AppController::projectChanged (const String& pid)
{
    addLog ("Live Set " + (pid.isEmpty() ? String ("(id pending)") : pid) + " - scanning");
    paramCache.clear(); paramRequested.clear(); observed.clear(); curveRequested.clear(); curveKeyByPid.clear();
    for (auto& m : macros) for (auto& in : m.instances) { in.paramId = 0; in.hasValue = false; }
    byParam.clear();
    lastScanNewSession = true;
    bool anyAssigned = false;
    for (const auto& m : macros) anyAssigned = anyAssigned || m.assigned;
    if (pid.isNotEmpty() && pid != stateProjectId) {
        if (stateProjectId.isEmpty() && anyAssigned) stateProjectId = pid;     // id arrived after the first assignments
        else {
            if (stateProjectId.isNotEmpty()) presets.saveSession (stateProjectId, toPresetVar());   // keep the previous Set's state
            persistDue = 0;
            sessionRestorePending = true;
        }
    }
    scanProject();
    changed (false);
}

void AppController::scanProject()
{
    if (bridge.getState() != BridgeConnection::State::Connected) { addLog ("Scan requested but the bridge is not connected"); return; }
    if (bridge.isScanning()) { rescanQueued = true; return; }
    bridge.requestScan();
    changed (false);
}

void AppController::structureChanged()
{
    addLog ("Project structure changed - rescanning");
    rescanQueued = true;
}

void AppController::parameterRemoved (int64_t) { rescanQueued = true; }

void AppController::bridgeError (const String& code, const String& msg)
{
    addLog ("Bridge " + code + ": " + msg);
    if (code == "E_NOT_FOUND") rescanQueued = true;
    changed (false);
}

std::vector<const DeviceInfo*> AppController::devicesOf (const String& key) const
{
    std::vector<const DeviceInfo*> v;
    for (const auto& d : snapshot.devices)
        if (! d.isBridge && d.pluginKey() == key) v.push_back (&d);
    return v;
}

void AppController::scanCompleted (const ProjectSnapshot& snap)
{
    snapshot = snap;
    snapshotFps = snapshot.fingerprints();
    addLog ("Scan: " + String (snap.tracks.size()) + " tracks, " + String (snap.countInstances()) + " devices (" + String (snap.durationMs) + " ms)");

    if (sessionRestorePending) {
        sessionRestorePending = false;
        const auto pid = snapshot.projectId.isNotEmpty() ? snapshot.projectId : getProjectId();
        const auto saved = presets.loadSession (pid);
        for (auto& m : macros) { const auto c = m.settings.colour; m = Macro(); m.settings.colour = c; }
        for (int i = 0; i < kNumMacros; ++i) macros[(size_t) i].settings.name = "Macro " + std::to_string (i + 1);
        groups.clear(); excluded.clear(); excludedFps.clear(); selected.clear();
        stateProjectId = pid;
        if (! saved.isVoid()) {
            addLog ("Restoring the saved session of this Live Set");
            for (const auto& line : applyPresetVar (saved, PresetScope::Project)) addLog ("  " + line);
        }
    } else {
        resolveAfterScan (lastScanNewSession);
    }
    lastScanNewSession = false;

    if (selectedPlugin.isEmpty() || devicesOf (selectedPlugin).empty()) {   // pick the plug-in with most instances
        juce::Array<PluginKind> kinds = pluginKinds ({}, true);
        if (! kinds.isEmpty()) selectPlugin (kinds.getFirst().key);
    } else selectPlugin (selectedPlugin);
    changed (true);
}

void AppController::resolveAfterScan (bool newSession)
{
    // 1. macros
    for (auto& m : macros) {
        if (! m.assigned) continue;
        std::vector<DeviceFingerprint> saved;
        std::vector<size_t> idx;
        for (size_t k = 0; k < m.instances.size(); ++k) {
            auto& in = m.instances[k];
            const DeviceInfo* d = (! newSession && in.deviceId.isNotEmpty()) ? snapshot.findDevice (in.deviceId) : nullptr;
            if (d != nullptr && d->pluginKey() == m.pluginKey()) {          // same session: Live id is authoritative
                for (size_t j = 0; j < snapshotFps.size(); ++j) if (snapshot.devices[(int) j].id == d->id) in.fp = snapshotFps[j];
                if (in.status != InstanceStatus::Ok) in.status = InstanceStatus::Pending;
                continue;
            }
            saved.push_back (in.fp); idx.push_back (k);
        }
        if (! saved.empty()) {
            const auto res = InstanceReconciler::reconcile (saved, snapshotFps);
            for (size_t r = 0; r < res.size(); ++r) {
                auto& in = m.instances[idx[r]];
                in.paramId = 0; in.hasValue = false; in.note = res[r].reason;
                if (res[r].candidateIndex >= 0) {
                    in.deviceId = snapshot.devices[res[r].candidateIndex].id;
                    in.fp = snapshotFps[(size_t) res[r].candidateIndex];
                    in.status = InstanceStatus::Pending;
                    if (res[r].status == MatchStatus::Relocated) addLog (m.paramName + ": instance " + res[r].reason);
                } else {
                    in.deviceId = {};
                    in.status = res[r].status == MatchStatus::Ambiguous ? InstanceStatus::Ambiguous : InstanceStatus::Missing;
                    addLog (m.paramName + ": " + String (in.fp.trackName) + " / " + String (in.fp.locationKey) + " - " + toString (in.status));
                }
            }
        }
        resolveMacroParams (m);
    }

    remapGroupsAndExclusions();
}

void AppController::remapGroupsAndExclusions()
{
    // groups and exclusions: re-identify their members in the current scan
    auto remap = [this] (const std::vector<DeviceFingerprint>& fps) {
        juce::StringArray ids;
        const auto res = InstanceReconciler::reconcile (fps, snapshotFps);
        for (const auto& r : res) if (r.candidateIndex >= 0) ids.add (snapshot.devices[r.candidateIndex].id);
        return ids;
    };
    for (auto& g : groups) g.deviceIds = remap (g.members);
    excluded.clear();
    for (const auto& id : remap (excludedFps)) excluded.insert (id);
    std::set<String> stillThere;
    for (const auto& id : selected) if (snapshot.findDevice (id)) stillThere.insert (id);
    selected = stillThere;
    rebuildIndex();
}

void AppController::resolveMacroParams (Macro& m)
{
    for (auto& in : m.instances) {
        if (in.deviceId.isEmpty() || (in.status != InstanceStatus::Pending && in.status != InstanceStatus::Ok && in.status != InstanceStatus::ParamMissing && in.status != InstanceStatus::Locked))
            continue;
        const auto it = paramCache.find (in.deviceId);
        if (it == paramCache.end()) {
            in.status = InstanceStatus::Pending;
            if (paramRequested.insert (in.deviceId).second) bridge.requestParameters (in.deviceId);
            continue;
        }
        const ParamInfo* found = nullptr;
        for (const auto& p : it->second)
            if (p.originalName == m.paramName && (found == nullptr || p.index == m.paramIndex)) found = &p;
        if (found == nullptr) {
            in.status = InstanceStatus::ParamMissing; in.paramId = 0;
            in.note = "\"" + m.paramName + "\" is not exposed by Live on this instance (use Configure on the device)";
            continue;
        }
        const bool wasValue = in.hasValue && in.paramId == found->id;
        in.paramId = found->id;
        if (! wasValue) { in.currentRaw = found->value; in.display = found->display; in.hasValue = true; }
        if (! in.hasStart) { in.startRaw = in.currentRaw; in.hasStart = true; }
        in.status = found->controllable() ? InstanceStatus::Ok : InstanceStatus::Locked;
        if (in.status == InstanceStatus::Locked) in.note = "parameter " + found->statusText();
        else in.note = {};
    }

    if (! m.scale) {
        const auto key = scaleKey (m.pluginKey(), m.paramName);
        if (auto s = scaleCache.find (key); s != scaleCache.end()) m.scale = s->second;
        else
            for (const auto& in : m.instances)
                if (in.paramId != 0) {
                    if (curveRequested.insert (in.paramId).second) { curveKeyByPid[in.paramId] = key; bridge.requestCurve (in.paramId); }
                    break;
                }
    }
    finishAssignmentIfReady (m);
}

void AppController::finishAssignmentIfReady (Macro& m)
{
    if (! m.scale) { m.valueText = m.assigned ? String ("learning scale...") : String ("-"); return; }
    if (m.fresh) {
        bool anyValue = false;
        for (const auto& in : m.instances) anyValue = anyValue || in.controllable();
        if (! anyValue) return;
        m.fresh = false;
        const auto keepName = m.settings.name; const auto keepColour = m.settings.colour; const auto keepMode = m.settings.mode;
        MacroEngine::applyDefaults (m.settings, *m.scale);
        m.settings.name = keepName; m.settings.colour = keepColour; m.settings.mode = keepMode;
        for (const auto& in : m.instances)
            if (in.controllable()) {
                m.knob = m.settings.mode == MacroMode::Relative ? 0.5 : MacroEngine::knobForRaw (m.settings, *m.scale, in.currentRaw);
                break;
            }
    }
    updateMacroDisplay (m);
}

void AppController::rebuildIndex()
{
    byParam.clear();
    std::set<int64_t> needed;
    for (int mi = 0; mi < kNumMacros; ++mi)
        for (int ii = 0; ii < (int) macros[(size_t) mi].instances.size(); ++ii) {
            const auto pid = macros[(size_t) mi].instances[(size_t) ii].paramId;
            if (pid != 0) { byParam.insert ({ pid, { mi, ii } }); needed.insert (pid); }
        }
    juce::Array<int64_t> on, off;
    for (auto p : needed) if (! observed.count (p)) on.add (p);
    for (auto p : observed) if (! needed.count (p)) off.add (p);
    if (! on.isEmpty()) bridge.observe (on, true);
    if (! off.isEmpty()) bridge.observe (off, false);
    observed = needed;
}

void AppController::deviceParametersReceived (const String& deviceId, const juce::Array<ParamInfo>& params)
{
    paramCache[deviceId] = params;
    paramRequested.erase (deviceId);
    for (auto& m : macros)
        if (m.assigned)
            for (const auto& in : m.instances)
                if (in.deviceId == deviceId) { resolveMacroParams (m); break; }
    rebuildIndex();
    changed (false);
}

void AppController::curveReceived (const CurveData& c)
{
    const auto it = curveKeyByPid.find (c.paramId);
    if (it == curveKeyByPid.end()) return;
    scaleCache[it->second] = c.scale;
    for (auto& m : macros)
        if (m.assigned && scaleKey (m.pluginKey(), m.paramName) == it->second) { m.scale = c.scale; finishAssignmentIfReady (m); }
    addLog ("Scale of " + it->second.fromFirstOccurrenceOf ("|", false, false).replace ("#", " / ") + ": " + toString (c.scale.kind())
            + (c.scale.unit().empty() ? String() : " (" + String (c.scale.unit()) + ")"));
    changed (false);
}

void AppController::parameterValue (int64_t pid, double v, const String& display, const String& source, int)
{
    // parameter browser cache
    for (auto& [dev, list] : paramCache)
        for (auto& p : list)
            if (p.id == pid) { p.value = v; p.display = display; }

    const auto range = byParam.equal_range (pid);
    bool persist = false;
    std::set<int> touched;
    for (auto it = range.first; it != range.second; ++it) {
        auto& m = macros[(size_t) it->second.first];
        auto& in = m.instances[(size_t) it->second.second];
        in.currentRaw = v; in.display = display; in.hasValue = true; in.lastSource = source;
        if (! in.hasStart) { in.startRaw = v; in.hasStart = true; }
        if ((source == "external" || source == "automation") && m.settings.mode == MacroMode::Relative && m.scale) {
            in.startRaw = MacroEngine::rebaseStart (m.settings, *m.scale, v, m.knob);
            persist = true;
        }
        touched.insert (it->second.first);
    }
    for (int mi : touched) {
        auto& m = macros[(size_t) mi];
        if (m.fresh) finishAssignmentIfReady (m);
        // follow the knob only for changes made outside GLOBAL CONTROL (never fight the user's gesture)
        if (source != "gc" && m.settings.mode == MacroMode::Absolute && m.scale) {
            updateMacroDisplay (m);
            if (! m.mixed)
                for (const auto& in : m.instances)
                    if (in.controllable()) { m.knob = MacroEngine::knobForRaw (m.settings, *m.scale, in.currentRaw); break; }
        }
        updateMacroDisplay (m);
    }
    changed (persist);
}

// ------------------------------------------------------------------------------------------------- formatting
static String fmtNumber (double v, int precision, const String& unit)
{
    if (! std::isfinite (v)) return v < 0 ? "-inf " + unit : "inf " + unit;
    String s = precision >= 0 ? String (v, precision) : String (v, 3);
    if (unit.isNotEmpty() && ! unit.startsWith (":")) s << " ";
    return s + unit;
}

String AppController::formatMacroValue (int i, double knob) const
{
    const auto& m = macros[(size_t) i];
    if (! m.assigned) return "-";
    if (! m.scale) return "...";
    const auto& s = *m.scale;
    const String unit (s.unit());
    if (m.settings.mode == MacroMode::Relative) {
        const double a = MacroEngine::relativeAmount (m.settings, knob);
        if (s.kind() == ScaleKind::Discrete || s.kind() == ScaleKind::Enumerated) return (a >= 0 ? "+" : "") + String ((int) std::round (a)) + " step";
        if (s.kind() == ScaleKind::Unknown) return (a >= 0 ? "+" : "") + String (a / juce::jmax (1e-12, 2.0 * m.settings.relativeSpan) * 100.0, 1) + " %";
        if (MacroEngine::multiplicative (s)) return String (juce::CharPointer_UTF8 ("\xc3\x97")) + String (std::pow (2.0, a), 2);
        return (a >= 0 ? "+" : "") + fmtNumber (a, juce::jmax (0, m.settings.precision), unit);
    }
    const auto out = MacroEngine::compute (m.settings, s, { { 0, 0 } }, knob);
    if (out.empty()) return "-";
    if (s.kind() == ScaleKind::Enumerated) {
        const auto idx = (size_t) std::round (out[0].raw - s.minRaw());
        return idx < s.labels().size() ? String (s.labels()[idx]) : String ((int) out[0].raw);
    }
    if (s.kind() == ScaleKind::Unknown) return String (out[0].raw, 3);
    const auto d = s.toDisplay (out[0].raw);
    return d ? fmtNumber (*d, m.settings.precision, unit) : String ("-");
}

void AppController::updateMacroDisplay (Macro& m)
{
    if (! m.assigned) { m.valueText = "-"; m.mixed = false; m.outOfSync = false; return; }
    if (! m.scale) { m.valueText = "learning scale..."; return; }
    const int idx = (int) (&m - macros.data());
    m.mixed = false; m.outOfSync = false;
    std::optional<double> first;
    const double tol = [&] { const auto r = m.scale->displayRange(); return r ? (r->second - r->first) * 0.002 : 1e-6; }();
    std::vector<InstanceTarget> targets;
    for (const auto& in : m.instances) {
        if (! in.controllable() || excluded.count (in.deviceId)) continue;
        const double cmp = m.scale->kind() == ScaleKind::Unknown || m.scale->kind() == ScaleKind::Enumerated ? in.currentRaw : m.scale->toDisplay (in.currentRaw).value_or (in.currentRaw);
        if (! first) first = cmp;
        else if (m.settings.mode == MacroMode::Absolute && std::abs (cmp - *first) > juce::jmax (tol, 1e-6)) m.mixed = true;
        targets.push_back ({ in.paramId, in.startRaw });
    }
    if (m.settings.mode == MacroMode::Relative && ! targets.empty()) {
        const auto out = MacroEngine::compute (m.settings, *m.scale, targets, m.knob);
        size_t k = 0;
        for (const auto& in : m.instances) {
            if (! in.controllable() || excluded.count (in.deviceId)) continue;
            if (std::abs (out[k].raw - in.currentRaw) > (m.scale->maxRaw() - m.scale->minRaw()) * 0.003) m.outOfSync = true;
            ++k;
        }
    }
    m.valueText = formatMacroValue (idx, m.knob);
    if (m.settings.mode == MacroMode::Absolute && m.mixed) m.valueText = "mixed";
}

// ------------------------------------------------------------------------------------------------- browser & instances
juce::Array<PluginKind> AppController::pluginKinds (const String& search, bool pluginsOnly) const
{
    std::map<String, PluginKind> kinds;
    for (const auto& d : snapshot.devices) {
        if (d.isBridge || d.canHaveChains) continue;
        if (pluginsOnly && ! d.isPlugin) continue;
        auto& k = kinds[d.pluginKey()];
        k.key = d.pluginKey(); k.className = d.className; k.name = d.classDisplayName; k.format = d.format; k.isPlugin = d.isPlugin;
        ++k.instances;
    }
    juce::Array<PluginKind> out;
    for (auto& [key, k] : kinds)
        if (search.isEmpty() || k.name.containsIgnoreCase (search) || k.format.containsIgnoreCase (search)) out.add (k);
    std::sort (out.begin(), out.end(), [] (const PluginKind& a, const PluginKind& b) {
        if (a.isPlugin != b.isPlugin) return a.isPlugin;
        if (a.instances != b.instances) return a.instances > b.instances;
        return a.name.compareNatural (b.name) < 0; });
    return out;
}

void AppController::selectPlugin (const String& key)
{
    selectedPlugin = key;
    const auto devs = devicesOf (key);
    for (const auto* d : devs)                       // the parameter browser shows the first instance
        if (! paramCache.count (d->id)) { if (paramRequested.insert (d->id).second) bridge.requestParameters (d->id); break; }
        else break;
    changed (false);
}

const juce::Array<ParamInfo>* AppController::selectedPluginParameters() const
{
    const auto devs = devicesOf (selectedPlugin);
    if (devs.empty()) return nullptr;
    const auto it = paramCache.find (devs.front()->id);
    return it == paramCache.end() ? nullptr : &it->second;
}

bool AppController::parametersLoading() const
{
    return ! devicesOf (selectedPlugin).empty() && selectedPluginParameters() == nullptr;
}

juce::Array<InstanceRow> AppController::instanceRows (const String& search, int filter) const
{
    juce::Array<InstanceRow> rows;
    auto macrosOf = [this] (const String& id) {
        String s;
        for (int i = 0; i < kNumMacros; ++i)
            for (const auto& in : macros[(size_t) i].instances)
                if (in.deviceId == id && in.status == InstanceStatus::Ok) { s << (s.isEmpty() ? "" : " ") << "M" << (i + 1); break; }
        return s;
    };
    auto groupsOf = [this] (const String& id) {
        String s;
        for (const auto& g : groups) if (g.deviceIds.contains (id)) s << (s.isEmpty() ? "" : ", ") << g.name;
        return s;
    };
    for (const auto& d : snapshot.devices) {
        if (d.isBridge || d.canHaveChains) continue;
        if (selectedPlugin.isNotEmpty() && d.pluginKey() != selectedPlugin) continue;
        InstanceRow r;
        r.device = &d;
        r.label = d.name != d.classDisplayName ? d.name + "  (" + d.classDisplayName + ")" : d.name;
        r.trackName = d.trackName + (d.rackPath.isNotEmpty() ? juce::String (juce::CharPointer_UTF8 ("  \xe2\x80\xba ")) + d.rackPath.trimCharactersAtEnd (" >") : String());
        r.instanceId = "#" + d.id + "  " + d.key;
        r.paramCount = paramCache.count (d.id) ? paramCache.at (d.id).size() : d.paramCount;
        r.selected = selected.count (d.id) > 0;
        r.excluded = excluded.count (d.id) > 0;
        r.macros = macrosOf (d.id);
        r.groupNames = groupsOf (d.id);
        r.connection = bridge.getState() != BridgeConnection::State::Connected ? "offline" : d.isActive ? "online" : "device off";
        rows.add (r);
    }
    // saved instances that the current project does not contain (never silently re-mapped)
    std::set<String> seen;
    for (int i = 0; i < kNumMacros; ++i) {
        const auto& m = macros[(size_t) i];
        if (! m.assigned || (selectedPlugin.isNotEmpty() && m.pluginKey() != selectedPlugin)) continue;
        for (const auto& in : m.instances) {
            if (in.status != InstanceStatus::Missing && in.status != InstanceStatus::Ambiguous) continue;
            const String k = String (in.fp.trackName) + "|" + String (in.fp.locationKey);
            if (! seen.insert (k).second) continue;
            InstanceRow r;
            r.label = String (in.fp.deviceName);
            r.trackName = String (in.fp.trackName);
            r.instanceId = String (in.fp.locationKey);
            r.connection = toString (in.status);
            r.macros = "M" + String (i + 1) + " (unresolved)";
            r.missing = true;
            rows.add (r);
        }
    }
    juce::Array<InstanceRow> out;
    for (const auto& r : rows) {
        if (search.isNotEmpty() && ! (r.label.containsIgnoreCase (search) || r.trackName.containsIgnoreCase (search)
                                      || r.instanceId.containsIgnoreCase (search) || r.groupNames.containsIgnoreCase (search))) continue;
        const bool keep = filter == 0 || (filter == 1 && r.selected) || (filter == 2 && r.macros.isNotEmpty())
                       || (filter == 3 && r.macros.isEmpty()) || (filter == 4 && r.excluded);
        if (keep) out.add (r);
    }
    return out;
}

void AppController::setInstanceSelected (const String& id, bool on)
{
    if (on) selected.insert (id); else selected.erase (id);
    changed (false);
}

void AppController::selectAllInstances (bool on)
{
    for (const auto* d : devicesOf (selectedPlugin)) {
        if (on && ! excluded.count (d->id)) selected.insert (d->id);
        if (! on) selected.erase (d->id);
    }
    changed (false);
}

juce::StringArray AppController::selectedInstances() const
{
    juce::StringArray s;
    for (const auto* d : devicesOf (selectedPlugin)) if (selected.count (d->id)) s.add (d->id);
    return s;
}

void AppController::setInstanceExcluded (const String& id, bool on)
{
    if (on) { excluded.insert (id); selected.erase (id); }
    else excluded.erase (id);
    excludedFps.clear();
    for (size_t j = 0; j < snapshotFps.size(); ++j)
        if (excluded.count (snapshot.devices[(int) j].id)) excludedFps.push_back (snapshotFps[j]);
    for (auto& m : macros) updateMacroDisplay (m);
    addLog (String (on ? "Excluded" : "Included") + " instance #" + id);
    changed (true);
}

juce::Result AppController::createGroup (const String& nameIn)
{
    const auto name = nameIn.trim();
    if (name.isEmpty()) return juce::Result::fail ("empty group name");
    for (const auto& g : groups) if (g.name == name) return juce::Result::fail ("a group named \"" + name + "\" already exists");
    InstanceGroup g;
    g.name = name; g.pluginKey = selectedPlugin;
    for (size_t j = 0; j < snapshotFps.size(); ++j) {
        const auto& d = snapshot.devices.getReference ((int) j);
        if (d.pluginKey() == selectedPlugin && selected.count (d.id)) { g.deviceIds.add (d.id); g.members.push_back (snapshotFps[j]); }
    }
    if (g.deviceIds.isEmpty()) return juce::Result::fail ("select the instances to group first");
    groups.push_back (g);
    addLog ("Group \"" + name + "\": " + String (g.deviceIds.size()) + " instances");
    changed (true);
    return juce::Result::ok();
}

juce::Result AppController::renameGroup (int i, const String& nameIn)
{
    const auto name = nameIn.trim();
    if (i < 0 || i >= (int) groups.size() || name.isEmpty()) return juce::Result::fail ("invalid group");
    for (int k = 0; k < (int) groups.size(); ++k) if (k != i && groups[(size_t) k].name == name) return juce::Result::fail ("name already used");
    for (auto& m : macros) if (m.groupName == groups[(size_t) i].name) m.groupName = name;
    groups[(size_t) i].name = name;
    changed (true);
    return juce::Result::ok();
}

void AppController::deleteGroup (int i)
{
    if (i < 0 || i >= (int) groups.size()) return;
    for (auto& m : macros) if (m.groupName == groups[(size_t) i].name) m.groupName = {};
    groups.erase (groups.begin() + i);
    changed (true);
}

void AppController::selectGroup (int i)
{
    if (i < 0 || i >= (int) groups.size()) return;
    const auto& g = groups[(size_t) i];
    if (g.pluginKey.isNotEmpty()) selectPlugin (g.pluginKey);
    for (const auto* d : devicesOf (selectedPlugin)) selected.erase (d->id);
    for (const auto& id : g.deviceIds) if (! excluded.count (id)) selected.insert (id);
    changed (false);
}

// ------------------------------------------------------------------------------------------------- macros
juce::Result AppController::assignParameters (const juce::StringArray& names, int firstSlot)
{
    if (names.isEmpty()) return juce::Result::fail ("select one or more parameters");
    const auto devs = devicesOf (selectedPlugin);
    if (devs.empty()) return juce::Result::fail ("select a plug-in in the device browser");
    std::vector<const DeviceInfo*> targets;
    for (const auto* d : devs) if (selected.count (d->id) && ! excluded.count (d->id)) targets.push_back (d);
    if (targets.empty()) for (const auto* d : devs) if (! excluded.count (d->id)) targets.push_back (d);
    if (targets.empty()) return juce::Result::fail ("all instances are excluded");
    const auto* params = selectedPluginParameters();
    if (params == nullptr) return juce::Result::fail ("parameters are still loading");

    String groupName;
    for (const auto& g : groups) {
        if (g.pluginKey != selectedPlugin || g.deviceIds.size() != (int) targets.size()) continue;
        bool same = true;
        for (const auto* d : targets) same = same && g.deviceIds.contains (d->id);
        if (same) { groupName = g.name; break; }
    }

    int slot = firstSlot;
    for (const auto& pname : names) {
        if (slot >= kNumMacros) return juce::Result::fail ("not enough macro slots after Macro " + String (firstSlot + 1));
        const ParamInfo* p = nullptr;
        for (const auto& x : *params) if (x.originalName == pname) { p = &x; break; }
        if (p == nullptr) return juce::Result::fail ("\"" + pname + "\" is not exposed by Live");
        auto& m = macros[(size_t) slot];
        const auto colour = m.settings.colour;
        const auto mode = m.assigned ? m.settings.mode : MacroMode::Absolute;
        m = Macro();
        m.settings.colour = colour; m.settings.mode = mode; m.settings.name = p->name.toStdString();
        m.assigned = true; m.fresh = true;
        m.className = devs.front()->className; m.pluginName = devs.front()->classDisplayName;
        m.paramName = p->originalName; m.paramIndex = p->index; m.groupName = groupName;
        for (const auto* d : targets) {
            MacroInstance in;
            in.deviceId = d->id;
            for (size_t j = 0; j < snapshotFps.size(); ++j) if (snapshot.devices[(int) j].id == d->id) in.fp = snapshotFps[j];
            m.instances.push_back (in);
        }
        resolveMacroParams (m);
        addLog ("Macro " + String (slot + 1) + " = " + m.pluginName + " / " + p->name + " on " + String ((int) targets.size()) + " instances");
        ++slot;
    }
    rebuildIndex();
    changed (true);
    return juce::Result::ok();
}

void AppController::clearMacro (int i)
{
    auto& m = macros[(size_t) i];
    const auto c = m.settings.colour;
    m = Macro();
    m.settings.colour = c; m.settings.name = "Macro " + std::to_string (i + 1);
    rebuildIndex();
    changed (true);
}

void AppController::setMacroKnob (int i, double knob)
{
    auto& m = macros[(size_t) i];
    m.knob = juce::jlimit (0.0, 1.0, knob);
    if (! m.assigned || ! m.scale) { changed (false); return; }
    std::vector<InstanceTarget> targets;
    for (const auto& in : m.instances)
        if (in.controllable() && in.hasStart && ! excluded.count (in.deviceId)) targets.push_back ({ in.paramId, in.startRaw });
    for (const auto& o : MacroEngine::compute (m.settings, *m.scale, targets, m.knob)) bridge.queueSet (o.paramId, o.raw);
    m.valueText = formatMacroValue (i, m.knob);
    m.mixed = false; m.outOfSync = false;
    changed (true);
}

void AppController::setMacroSettings (int i, const MacroSettings& s)
{
    auto& m = macros[(size_t) i];
    const bool modeChanged = s.mode != m.settings.mode;
    const auto newMode = s.mode;
    m.settings = s;
    if (modeChanged) { m.settings.mode = newMode == MacroMode::Absolute ? MacroMode::Relative : MacroMode::Absolute; setMacroMode (i, newMode); return; }
    updateMacroDisplay (m);
    changed (true);
}

void AppController::setMacroMode (int i, MacroMode mode)
{
    auto& m = macros[(size_t) i];
    if (m.settings.mode == mode) return;
    m.settings.mode = mode;
    if (mode == MacroMode::Relative) { captureRelative (i); return; }
    for (const auto& in : m.instances)
        if (in.controllable() && m.scale) { m.knob = MacroEngine::knobForRaw (m.settings, *m.scale, in.currentRaw); break; }
    updateMacroDisplay (m);
    changed (true);
}

void AppController::captureRelative (int i)
{
    auto& m = macros[(size_t) i];
    for (auto& in : m.instances) if (in.hasValue) { in.startRaw = in.currentRaw; in.hasStart = true; }
    m.knob = 0.5;
    updateMacroDisplay (m);
    changed (true);
}

void AppController::resetMacro (int i)
{
    auto& m = macros[(size_t) i];
    if (! m.assigned) return;
    double k = m.settings.defaultKnob;
    if (m.settings.mode == MacroMode::Relative) k = 0.5;
    else if (m.scale) {
        for (const auto& in : m.instances) {
            const auto it = paramCache.find (in.deviceId);
            if (it == paramCache.end()) continue;
            for (const auto& p : it->second)
                if (p.id == in.paramId && p.hasDefault) { k = MacroEngine::knobForRaw (m.settings, *m.scale, p.defaultValue); break; }
            break;
        }
    }
    setMacroKnob (i, k);
}

// ------------------------------------------------------------------------------------------------- presets
juce::var AppController::macroToVar (const Macro& m, int slot) const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("slot", slot);
    o->setProperty ("name", String (m.settings.name));
    o->setProperty ("colour", String::toHexString ((juce::int64) m.settings.colour));
    o->setProperty ("mode", m.settings.mode == MacroMode::Absolute ? "absolute" : "relative");
    o->setProperty ("rangeMin", m.settings.rangeMin);
    o->setProperty ("rangeMax", m.settings.rangeMax);
    o->setProperty ("relativeSpan", m.settings.relativeSpan);
    o->setProperty ("invert", m.settings.invert);
    o->setProperty ("precision", m.settings.precision);
    o->setProperty ("fineFactor", m.settings.fineFactor);
    o->setProperty ("defaultKnob", m.settings.defaultKnob);
    o->setProperty ("knob", m.knob);
    o->setProperty ("assigned", m.assigned);
    if (m.assigned) {
        auto* pl = new juce::DynamicObject();
        pl->setProperty ("className", m.className);
        pl->setProperty ("classDisplayName", m.pluginName);
        o->setProperty ("plugin", juce::var (pl));
        auto* pa = new juce::DynamicObject();
        pa->setProperty ("name", m.paramName);
        pa->setProperty ("index", m.paramIndex);
        o->setProperty ("parameter", juce::var (pa));
        o->setProperty ("group", m.groupName);
        juce::Array<juce::var> inst;
        for (const auto& in : m.instances) {
            auto* io = new juce::DynamicObject();
            io->setProperty ("fingerprint", fingerprintToVar (in.fp));
            io->setProperty ("startRaw", in.startRaw);
            io->setProperty ("hasStart", in.hasStart);
            inst.add (juce::var (io));
        }
        o->setProperty ("instances", inst);
    }
    return juce::var (o);
}

juce::var AppController::toPresetVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("format", kPresetFormat);
    o->setProperty ("version", kPresetVersion);
    o->setProperty ("app", "GLOBAL CONTROL " JUCE_STRINGIFY (GC_VERSION));
    o->setProperty ("savedAt", juce::Time::getCurrentTime().toISO8601 (true));
    o->setProperty ("projectId", stateProjectId);
    o->setProperty ("projectName", getProjectName());
    juce::Array<juce::var> ms;
    for (int i = 0; i < kNumMacros; ++i) ms.add (macroToVar (macros[(size_t) i], i));
    o->setProperty ("macros", ms);
    juce::Array<juce::var> gs;
    for (const auto& g : groups) {
        auto* go = new juce::DynamicObject();
        go->setProperty ("name", g.name);
        go->setProperty ("pluginKey", g.pluginKey);
        juce::Array<juce::var> mem;
        for (const auto& f : g.members) mem.add (fingerprintToVar (f));
        go->setProperty ("members", mem);
        gs.add (juce::var (go));
    }
    o->setProperty ("groups", gs);
    juce::Array<juce::var> ex;
    for (const auto& f : excludedFps) ex.add (fingerprintToVar (f));
    o->setProperty ("excluded", ex);
    return juce::var (o);
}

juce::StringArray AppController::applyPresetVar (const juce::var& v, PresetScope scope)
{
    juce::StringArray report;
    if (auto r = PresetStore::validate (v); r.failed()) { report.add ("Invalid preset: " + r.getErrorMessage()); return report; }
    const bool project = scope == PresetScope::Project;
    if (project && v["projectId"].toString().isNotEmpty() && v["projectId"].toString() != getProjectId())
        report.add ("Note: preset saved for another Live Set (" + v["projectId"].toString() + "): instances are matched by fingerprint");

    groups.clear(); excludedFps.clear(); excluded.clear();
    if (project) {
        if (auto* ex = v["excluded"].getArray()) for (const auto& f : *ex) excludedFps.push_back (fingerprintFromVar (f));
        if (auto* gs = v["groups"].getArray())
            for (const auto& g : *gs) {
                InstanceGroup ig; ig.name = g["name"].toString(); ig.pluginKey = g["pluginKey"].toString();
                if (auto* mem = g["members"].getArray()) for (const auto& f : *mem) ig.members.push_back (fingerprintFromVar (f));
                groups.push_back (ig);
            }
    }

    for (int i = 0; i < kNumMacros; ++i) { const auto c = macros[(size_t) i].settings.colour; macros[(size_t) i] = Macro(); macros[(size_t) i].settings.colour = c; macros[(size_t) i].settings.name = "Macro " + std::to_string (i + 1); }

    if (auto* ms = v["macros"].getArray()) {
        for (const auto& mv : *ms) {
            const int slot = (int) mv["slot"];
            if (slot < 0 || slot >= kNumMacros) continue;
            auto& m = macros[(size_t) slot];
            m.settings.name = mv["name"].toString().toStdString();
            m.settings.colour = (uint32_t) mv["colour"].toString().getHexValue64();
            m.settings.mode = mv["mode"].toString() == "relative" ? MacroMode::Relative : MacroMode::Absolute;
            m.settings.rangeMin = mv["rangeMin"]; m.settings.rangeMax = mv["rangeMax"];
            m.settings.relativeSpan = mv["relativeSpan"]; m.settings.invert = (bool) mv["invert"];
            m.settings.precision = mv["precision"]; m.settings.fineFactor = mv["fineFactor"];
            m.settings.defaultKnob = mv["defaultKnob"];
            m.knob = mv["knob"];
            if (! (bool) mv["assigned"]) continue;
            m.assigned = true;
            m.className = mv["plugin"]["className"].toString(); m.pluginName = mv["plugin"]["classDisplayName"].toString();
            m.paramName = mv["parameter"]["name"].toString(); m.paramIndex = mv["parameter"]["index"];
            m.groupName = mv["group"].toString();

            if (project) {
                std::vector<DeviceFingerprint> saved;
                std::vector<std::pair<double, bool>> starts;
                if (auto* inst = mv["instances"].getArray())
                    for (const auto& iv : *inst) { saved.push_back (fingerprintFromVar (iv["fingerprint"])); starts.push_back ({ (double) iv["startRaw"], (bool) iv["hasStart"] }); }
                const auto res = InstanceReconciler::reconcile (saved, snapshotFps);
                int ok = 0; juce::StringArray bad;
                for (size_t k = 0; k < res.size(); ++k) {
                    MacroInstance in; in.fp = saved[k];
                    in.startRaw = starts[k].first; in.hasStart = starts[k].second && m.settings.mode == MacroMode::Relative;
                    in.note = res[k].reason;
                    if (res[k].candidateIndex >= 0) {
                        in.deviceId = snapshot.devices[res[k].candidateIndex].id; in.fp = snapshotFps[(size_t) res[k].candidateIndex];
                        in.status = InstanceStatus::Pending; ++ok;
                    } else {
                        in.status = res[k].status == MatchStatus::Ambiguous ? InstanceStatus::Ambiguous : InstanceStatus::Missing;
                        bad.add (String (saved[k].trackName) + " " + String (saved[k].locationKey) + " (" + toString (in.status) + ")");
                    }
                    m.instances.push_back (in);
                }
                String line = "M" + String (slot + 1) + " " + m.pluginName + " / " + m.paramName + ": " + String (ok) + "/" + String ((int) res.size()) + " instances restored";
                if (! bad.isEmpty()) line << " - not restored: " << bad.joinIntoString (", ");
                report.add (line);
            } else {
                for (size_t j = 0; j < snapshotFps.size(); ++j) {
                    const auto& d = snapshot.devices.getReference ((int) j);
                    if (d.isBridge || d.pluginKey() != m.pluginKey()) continue;
                    MacroInstance in; in.deviceId = d.id; in.fp = snapshotFps[j]; in.status = InstanceStatus::Pending;
                    m.instances.push_back (in);
                }
                if (m.settings.mode == MacroMode::Relative) m.knob = 0.5;
                report.add ("M" + String (slot + 1) + " " + m.pluginName + " / " + m.paramName + ": applied to all "
                            + String ((int) m.instances.size()) + " instances in this Live Set");
            }
            if (m.settings.mode == MacroMode::Absolute) m.fresh = false;
        }
    }
    remapGroupsAndExclusions();
    for (auto& m : macros) if (m.assigned) resolveMacroParams (m);
    rebuildIndex();
    changed (true);
    return report;
}

void AppController::persistSession()
{
    if (stateProjectId.isEmpty() || stateProjectId != getProjectId()) return;
    presets.saveSession (stateProjectId, toPresetVar());
}

} // namespace gc
