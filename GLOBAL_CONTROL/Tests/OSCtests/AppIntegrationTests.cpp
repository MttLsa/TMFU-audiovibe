// End-to-end test of the GLOBAL CONTROL application logic (AppController + BridgeConnection, the code the
// GUI runs) against the real gc_bridge.js over real UDP on 127.0.0.1. The Live Object Model behind the
// bridge is the Node test fixture (mock_live.js: 10 hypothetical "Pro-C 2" devices), NOT Ableton Live.
//   GlobalControlIntegrationTests <path to Tests/OSCtests>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_osc/juce_osc.h>

#include "../../GlobalControlApp/Core/AppController.h"

#include <cmath>
#include <iostream>

using namespace gc;

static int g_fail = 0, g_checks = 0;
#define CHECK(c) do { ++g_checks; if (!(c)) { ++g_fail; std::cout << "    FAIL line " << __LINE__ << ": " << #c << std::endl; } } while (0)

static constexpr int kApp = 47320, kBridge = 47321, kCtl = 47322;
static juce::File g_dir;

struct MockBridge {
    juce::ChildProcess proc;
    bool start (const juce::String& projectId, int idOffset)
    {
        juce::StringArray cmd { "node", g_dir.getChildFile ("mock_bridge_server.js").getFullPathName(),
                                "--bridge-port", juce::String (kBridge), "--app-port", juce::String (kApp),
                                "--control-port", juce::String (kCtl), "--project-id", projectId, "--id-offset", juce::String (idOffset) };
        return proc.start (cmd, 0);
    }
    void stop() { proc.kill(); proc.waitForProcessToFinish (2000); }
    ~MockBridge() { if (proc.isRunning()) stop(); }
};

static bool waitFor (const std::function<bool()>& pred, int timeoutMs = 8000)
{
    const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
    while (juce::Time::getMillisecondCounter() < end) {
        if (pred()) return true;
        juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
    }
    return pred();
}

static void control (const juce::String& addr, std::initializer_list<juce::OSCArgument> args)
{
    juce::OSCSender s;
    s.connect ("127.0.0.1", kCtl);
    juce::OSCMessage m { juce::OSCAddressPattern (addr) };
    for (const auto& a : args) m.addArgument (a);
    s.send (m);
}

static double thresholdDb (const MacroInstance& in) { return in.display.getDoubleValue(); }   // "-12.00 dB"

static bool allOk (const Macro& m, int n)
{
    return (int) m.instances.size() == n && m.countStatus (InstanceStatus::Ok) == n && m.controllableCount() == n && m.scale.has_value();
}

static int indexOfParam (const AppController& app, const juce::String& name)
{
    if (auto* ps = app.selectedPluginParameters()) for (const auto& p : *ps) if (p.originalName == name) return p.index;
    return -1;
}

static void run()
{
    juce::TemporaryFile tmpDir;
    const auto presetRoot = tmpDir.getFile();
    presetRoot.createDirectory();
    const juce::String projectId = "gcp-test0000000000000001";

    MockBridge mock;
    CHECK (mock.start (projectId, 0));

    auto app = std::make_unique<AppController> (presetRoot);
    CHECK (app->start (kApp, kBridge));

    std::cout << "  connection, scan, device browser" << std::endl;
    CHECK (waitFor ([&] { return app->connectionState() == BridgeConnection::State::Connected && app->getSnapshot().valid; }));
    CHECK (app->getProjectId() == projectId);
    const auto kinds = app->pluginKinds ({}, true);
    CHECK (! kinds.isEmpty() && kinds[0].name == "Pro-C 2" && kinds[0].instances == 10);
    CHECK (app->getSelectedPlugin() == kinds[0].key);
    CHECK (waitFor ([&] { return app->selectedPluginParameters() != nullptr; }));
    CHECK (indexOfParam (*app, "Threshold") >= 0);
    CHECK (app->instanceRows ({}, 0).size() == 10);
    CHECK (app->instanceRows ("Vocal", 0).size() == 2);

    std::cout << "  macro 1: Threshold on 10 instances, RELATIVE -2 dB" << std::endl;
    CHECK (app->assignParameters ({ "Threshold" }, 0).wasOk());
    CHECK (waitFor ([&] { return allOk (app->getMacro (0), 10) && ! app->getMacro (0).fresh; }));
    const auto& m0 = app->getMacro (0);
    CHECK (m0.scale && m0.scale->kind() == ScaleKind::Linear && m0.scale->unit() == "dB");
    std::map<juce::String, double> before;
    for (const auto& in : m0.instances) before[in.deviceId] = thresholdDb (in);
    CHECK (before.size() == 10);
    app->setMacroMode (0, MacroMode::Relative);
    auto st = app->getMacro (0).settings;
    st.relativeSpan = 6; app->setMacroSettings (0, st);
    app->setMacroKnob (0, 0.5 - (2.0 / 6.0) / 2.0);                   // -2 dB
    CHECK (app->getMacro (0).valueText == "-2.00 dB");
    CHECK (waitFor ([&] {
        for (const auto& in : app->getMacro (0).instances) if (std::abs (thresholdDb (in) - (before[in.deviceId] - 2)) > 0.02) return false;
        return true; }));
    for (const auto& in : app->getMacro (0).instances) CHECK (in.lastSource == "gc");

    std::cout << "  feedback from the plug-in GUI (external) and relative rebase" << std::endl;
    const int thrIdx = indexOfParam (*app, "Threshold");
    control ("/mock/external", { juce::OSCArgument (3), juce::OSCArgument (thrIdx), juce::OSCArgument (0.5f) });   // Bass -> -30 dB
    const auto bassId = [&] { for (const auto& d : app->getSnapshot().devices) if (d.trackName == "Bass" && ! d.isBridge) return d.id; return juce::String(); }();
    auto bass = [&]() -> const MacroInstance& { for (const auto& in : app->getMacro (0).instances) if (in.deviceId == bassId) return in; return app->getMacro (0).instances[0]; };
    CHECK (waitFor ([&] { return bass().lastSource == "external" && std::abs (thresholdDb (bass()) + 30) < 0.02; }));
    CHECK (std::abs (app->getMacro (0).scale->toDisplay (bass().startRaw).value_or (0) + 28) < 0.05);   // start rebased: -30 = start - 2
    app->setMacroKnob (0, 0.5);                                         // back to 0: others return home, Bass continues from -28
    CHECK (waitFor ([&] { return std::abs (thresholdDb (bass()) + 28) < 0.02; }));
    CHECK (! app->getMacro (0).outOfSync);

    std::cout << "  ABSOLUTE -12 dB on all, exclusion" << std::endl;
    app->setMacroMode (0, MacroMode::Absolute);
    st = app->getMacro (0).settings;
    CHECK (std::abs (st.rangeMin + 60) < 1e-9 && std::abs (st.rangeMax) < 1e-9);
    const auto vocalIds = [&] { juce::StringArray v; for (const auto& d : app->getSnapshot().devices) if (d.trackName == "Vocal") v.add (d.id); return v; }();
    app->setInstanceExcluded (vocalIds[0], true);
    const double vocal0 = [&] { for (const auto& in : app->getMacro (0).instances) if (in.deviceId == vocalIds[0]) return thresholdDb (in); return 0.0; }();
    app->setMacroKnob (0, MacroEngine::knobForDisplay (st, -12));
    CHECK (app->getMacro (0).valueText == "-12.00 dB");
    CHECK (waitFor ([&] {
        for (const auto& in : app->getMacro (0).instances) {
            if (in.deviceId == vocalIds[0]) { if (std::abs (thresholdDb (in) - vocal0) > 0.01) return false; }
            else if (std::abs (thresholdDb (in) + 12) > 0.02) return false;
        }
        return true; }));
    app->setInstanceExcluded (vocalIds[0], false);
    CHECK (waitFor ([&] { return app->getMacro (0).mixed; }));          // excluded instance kept its own value

    std::cout << "  multi-parameter assignment, groups" << std::endl;
    app->selectAllInstances (false);
    for (const auto& d : app->getSnapshot().devices) if (d.trackName == "Kick" || d.trackName == "Snare") app->setInstanceSelected (d.id, true);
    CHECK (app->createGroup ("Drums").wasOk());
    CHECK (app->renameGroup (0, "Drum comps").wasOk());
    CHECK (app->assignParameters ({ "Attack", "Release" }, 1).wasOk());
    CHECK (waitFor ([&] { return allOk (app->getMacro (1), 3) && allOk (app->getMacro (2), 3); }));
    CHECK (app->getMacro (1).groupName == "Drum comps");
    CHECK (app->getMacro (1).scale->kind() == ScaleKind::Logarithmic && app->getMacro (1).scale->unit() == "ms");
    app->setMacroMode (1, MacroMode::Relative);
    st = app->getMacro (1).settings; st.relativeSpan = 2; app->setMacroSettings (1, st);
    std::vector<double> att0;
    for (const auto& in : app->getMacro (1).instances) att0.push_back (in.display.getDoubleValue());
    app->setMacroKnob (1, 0.75);                                          // +1 octave = x2
    CHECK (waitFor ([&] {
        size_t k = 0;
        for (const auto& in : app->getMacro (1).instances) if (std::abs (in.display.getDoubleValue() / att0[k++] - 2.0) > 0.02) return false;
        return true; }));
    CHECK (app->assignParameters ({ "Style" }, 3).wasOk());               // enumerated
    CHECK (waitFor ([&] { return allOk (app->getMacro (3), 3); }));
    CHECK (app->getMacro (3).scale->kind() == ScaleKind::Enumerated);
    CHECK (app->formatMacroValue (3, 1.0) == "Pumping");
    CHECK (app->assignParameters ({ "Nope" }, 4).failed());

    std::cout << "  presets: save / rename / duplicate / load (project + global)" << std::endl;
    auto& store = app->getPresetStore();
    CHECK (store.save (PresetScope::Project, projectId, "Mix A", app->toPresetVar(), false).wasOk());
    CHECK (store.save (PresetScope::Project, projectId, "Mix A", app->toPresetVar(), false).failed());   // no silent overwrite
    CHECK (store.save (PresetScope::Global, {}, "Comp template", app->toPresetVar(), false).wasOk());
    auto list = store.list (PresetScope::Project, projectId);
    CHECK (list.size() == 1);
    CHECK (store.rename (list[0], "Mix B").wasOk());
    list = store.list (PresetScope::Project, projectId);
    juce::String dupName;
    CHECK (list.size() == 1 && list[0].name == "Mix B" && store.duplicate (list[0], dupName).wasOk() && dupName == "Mix B 2");
    list = store.list (PresetScope::Project, projectId);
    CHECK (list.size() == 2 && store.remove (list[1]).wasOk());
    app->clearMacro (0); app->clearMacro (1);
    juce::var pv;
    CHECK (store.load (store.list (PresetScope::Project, projectId)[0], pv).wasOk());
    auto report = app->applyPresetVar (pv, PresetScope::Project);
    CHECK (report.size() >= 4 && report[0].contains ("10/10 instances restored"));
    CHECK (waitFor ([&] { return allOk (app->getMacro (0), 10) && allOk (app->getMacro (1), 3); }));
    CHECK (app->getMacro (1).settings.mode == MacroMode::Relative && app->getGroups().size() == 1 && app->getGroups()[0].deviceIds.size() == 3);
    juce::var gv;
    CHECK (store.load (store.list (PresetScope::Global, {})[0], gv).wasOk());
    report = app->applyPresetVar (gv, PresetScope::Global);
    CHECK (waitFor ([&] { return allOk (app->getMacro (1), 10); }));      // template: all instances of the plug-in
    // restore the project preset for the reconnection test
    report = app->applyPresetVar (pv, PresetScope::Project);
    CHECK (waitFor ([&] { return allOk (app->getMacro (0), 10) && allOk (app->getMacro (1), 3); }));
    waitFor ([] { return false; }, 1800);                                 // let the session autosave run

    std::cout << "  structure change: a device is deleted in Live" << std::endl;
    control ("/mock/remove", { juce::OSCArgument (8) });                  // Synth's Pro-C 2
    CHECK (waitFor ([&] { return app->getMacro (0).countStatus (InstanceStatus::Missing) == 1 && app->getMacro (0).countStatus (InstanceStatus::Ok) == 9; }));
    bool missingRow = false;
    for (const auto& r : app->instanceRows ({}, 0)) missingRow = missingRow || (r.missing && r.trackName == "Synth");
    CHECK (missingRow);
    waitFor ([] { return false; }, 1800);

    std::cout << "  project closed and reopened: new bridge session, new Live ids, mappings restored" << std::endl;
    mock.stop();
    CHECK (waitFor ([&] { return app->connectionState() != BridgeConnection::State::Connected; }, 7000));
    MockBridge reopened;
    CHECK (reopened.start (projectId, 5000));                             // same Set (project id), different runtime ids
    CHECK (waitFor ([&] { return app->connectionState() == BridgeConnection::State::Connected && allOk (app->getMacro (0), 10) && allOk (app->getMacro (1), 3); }, 12000));
    for (const auto& in : app->getMacro (0).instances) CHECK (in.deviceId.getIntValue() > 5000);
    app->setMacroKnob (0, MacroEngine::knobForDisplay (app->getMacro (0).settings, -20));
    CHECK (waitFor ([&] { for (const auto& in : app->getMacro (0).instances) if (std::abs (thresholdDb (in) + 20) > 0.02) return false; return true; }));

    std::cout << "  a different Live Set: no mapping is carried over" << std::endl;
    reopened.stop();
    CHECK (waitFor ([&] { return app->connectionState() != BridgeConnection::State::Connected; }, 7000));
    MockBridge other;
    CHECK (other.start ("gcp-other000000000000002", 9000));
    CHECK (waitFor ([&] { return app->connectionState() == BridgeConnection::State::Connected && app->getProjectId() == "gcp-other000000000000002" && app->getSnapshot().valid; }, 12000));
    waitFor ([] { return false; }, 500);
    CHECK (! app->getMacro (0).assigned && ! app->getMacro (1).assigned);

    const auto stats = app->getBridge().getStats();
    std::cout << "  packets in " << stats.packetsIn << ", out " << stats.packetsOut << ", parameter writes " << stats.setsSent
              << ", duplicates dropped " << stats.duplicates << std::endl;
    app.reset();
    other.stop();
    presetRoot.deleteRecursively();
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    g_dir = argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[1])) : juce::File::getCurrentWorkingDirectory();
    run();
    std::cout << g_checks << " checks, " << g_fail << " failed" << std::endl;
    return g_fail ? 1 : 0;
}
