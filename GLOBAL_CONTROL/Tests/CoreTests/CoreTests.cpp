// Unit tests for the GLOBAL CONTROL core (no JUCE, no Live). Build & run:
//   cmake -S GLOBAL_CONTROL -B build-gc && cmake --build build-gc && ctest --test-dir build-gc
#include "../../GlobalControlApp/Core/ParameterScale.h"
#include "../../GlobalControlApp/MacroEngine/MacroEngine.h"
#include "../../GlobalControlApp/MappingEngine/InstanceReconciler.h"
#include "../../GlobalControlApp/OSC/ProtocolCore.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

static int g_fail = 0, g_checks = 0;
#define CHECK(c) do { ++g_checks; if (!(c)) { ++g_fail; std::printf ("    FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR(a, b, eps) CHECK (std::abs ((a) - (b)) <= (eps))

using namespace gc;

static std::vector<ScaleSample> sample (int n, const std::function<std::string (double)>& f, double lo = 0, double hi = 1)
{
    std::vector<ScaleSample> s;
    for (int i = 0; i < n; ++i) { const double x = lo + (hi - lo) * i / (n - 1); s.push_back ({ x, f (x) }); }
    return s;
}
static std::string fmt (double v, const char* unit, int dec = 2)
{
    char b[64]; std::snprintf (b, sizeof b, "%.*f %s", dec, v, unit); return b;
}

// Example scales shaped like real plug-in parameters (values are NOT taken from any specific plug-in).
static ParameterScale dbScale()  { return ParameterScale::fromSamples (0, 1, false, sample (65, [] (double x) { return fmt (-60 + 60 * x, "dB"); })); }
static ParameterScale msScale()  { return ParameterScale::fromSamples (0, 1, false, sample (65, [] (double x) { const double ms = 0.005 * std::pow (250 / 0.005, x); return ms >= 1000 ? fmt (ms / 1000, "s") : fmt (ms, "ms", 3); })); }
static ParameterScale hzScale()  { return ParameterScale::fromSamples (0, 1, false, sample (65, [] (double x) { const double hz = 20 * std::pow (1000.0, x); return hz >= 1000 ? fmt (hz / 1000, "kHz", 3) : fmt (hz, "Hz", 1); })); }
static ParameterScale ratioScale() { return ParameterScale::fromSamples (0, 1, false, sample (65, [] (double x) { return fmt (1 + 99 * x * x * x, ":1"); })); }

static void testParse()
{
    auto p = parseDisplay ("-12.0 dB"); CHECK (p.value && *p.value == -12.0 && p.unit == "dB");
    p = parseDisplay ("1.2 kHz");   CHECK (p.value && std::abs (*p.value - 1200) < 1e-9 && p.unit == "Hz");
    p = parseDisplay ("1.20 s");    CHECK (p.value && std::abs (*p.value - 1200) < 1e-9 && p.unit == "ms");
    p = parseDisplay ("-inf dB");   CHECK (p.infinite && *p.value < 0 && p.unit == "dB");
    p = parseDisplay ("Clean");     CHECK (! p.value && p.unit == "Clean");
    p = parseDisplay ("4.00:1");    CHECK (p.value && *p.value == 4.0 && p.unit == ":1");
    p = parseDisplay ("+3,5 dB");   CHECK (p.value && *p.value == 3.5);
    p = parseDisplay ("1e-3 s");    CHECK (p.value && std::abs (*p.value - 1.0) < 1e-12);
}

static void testScales()
{
    auto db = dbScale();
    CHECK (db.kind() == ScaleKind::Linear && db.unit() == "dB");
    NEAR (*db.toRaw (-12), 0.8, 1e-3);          // display strings are rounded to 0.01 dB
    NEAR (*db.toDisplay (0.8), -12, 0.01);
    NEAR (*db.toRaw (+6), 1.0, 0);          // clamped
    auto ms = msScale();
    CHECK (ms.kind() == ScaleKind::Logarithmic && ms.unit() == "ms");
    const double r = *ms.toRaw (10.0);
    NEAR (0.005 * std::pow (250 / 0.005, r), 10.0, 0.05);   // log-domain interpolation stays accurate
    auto hz = hzScale();
    CHECK (hz.kind() == ScaleKind::Logarithmic && hz.unit() == "Hz");
    NEAR (20 * std::pow (1000.0, *hz.toRaw (1000)), 1000, 2);
    CHECK (ratioScale().kind() == ScaleKind::Curved);

    std::vector<ScaleSample> en { { 0, "Clean" }, { 1, "Classic" }, { 2, "Opto" } };
    auto e = ParameterScale::fromSamples (0, 2, true, en);
    CHECK (e.kind() == ScaleKind::Enumerated && e.labels().size() == 3 && ! e.toRaw (1));
    std::vector<ScaleSample> st { { 0, "1x" }, { 1, "2x" }, { 2, "4x" }, { 3, "8x" } };
    auto d = ParameterScale::fromSamples (0, 3, true, st);
    CHECK (d.kind() == ScaleKind::Discrete && *d.toRaw (3.9) == 2 && *d.toDisplay (3) == 8);

    std::vector<ScaleSample> weird { { 0, "A" }, { 0.5, "B" }, { 1, "A" } };
    CHECK (ParameterScale::fromSamples (0, 1, false, weird).kind() == ScaleKind::Unknown);
    auto dec = ParameterScale::fromSamples (0, 1, false, sample (33, [] (double x) { return fmt (100 - 100 * x, "%"); }));
    CHECK (dec.kind() == ScaleKind::Linear && ! dec.increasing());
    NEAR (*dec.toRaw (25), 0.75, 1e-9);
}

static void testAbsolute()
{
    auto db = dbScale();
    MacroSettings m; m.mode = MacroMode::Absolute; m.rangeMin = -60; m.rangeMax = 0;
    std::vector<InstanceTarget> inst { { 1, 0.8 }, { 2, 0.86 }, { 3, 0.75 } };
    auto out = MacroEngine::compute (m, db, inst, MacroEngine::knobForDisplay (m, -12));
    CHECK (out.size() == 3);
    for (auto& o : out) { NEAR (*db.toDisplay (o.raw), -12, 1e-6); CHECK (! o.clamped); }
    m.invert = true;
    out = MacroEngine::compute (m, db, inst, 0.0);
    NEAR (*db.toDisplay (out[0].raw), 0, 1e-9);
    m.invert = false; m.rangeMin = -80;                                         // beyond the plug-in range
    out = MacroEngine::compute (m, db, inst, 0.0);
    CHECK (out[0].clamped); NEAR (out[0].raw, 0.0, 0);
    auto ms = msScale();                                                        // log knob for log params
    MacroSettings a; a.rangeMin = 1; a.rangeMax = 100; a.precision = 3;
    out = MacroEngine::compute (a, ms, inst, 0.5);
    NEAR (*ms.toDisplay (out[0].raw), 10.0, 0.05);
    NEAR (MacroEngine::knobForRaw (a, ms, out[0].raw), 0.5, 0.01);              // knob <-> value round trip (log knob)
    NEAR (MacroEngine::knobForRaw (m, db, *db.toRaw (-30)), 0.625, 1e-6);       // range -80..0 dB
}

static void testRelative()
{
    // the example of the brief: Kick -12, Snare -8, Bass -15, Vocal -10 dB, global -2 dB
    auto db = dbScale();
    std::vector<InstanceTarget> inst;
    for (double v : { -12.0, -8.0, -15.0, -10.0 }) inst.push_back ({ (int64_t) inst.size() + 1, *db.toRaw (v) });
    MacroSettings m; m.mode = MacroMode::Relative; m.relativeSpan = 6;   // knob ends = +/-6 dB
    const double knob = 0.5 + (-2.0 / 6.0) / 2.0;
    NEAR (MacroEngine::relativeAmount (m, knob), -2.0, 1e-12);
    auto out = MacroEngine::compute (m, db, inst, knob);
    const double expect[] = { -14, -10, -17, -12 };
    for (size_t i = 0; i < 4; ++i) NEAR (*db.toDisplay (out[i].raw), expect[i], 1e-6);
    out = MacroEngine::compute (m, db, inst, 0.5);                       // back to centre = original values
    for (size_t i = 0; i < 4; ++i) NEAR (out[i].raw, inst[i].startRaw, 1e-9);

    // clamping is per instance and never accumulates
    std::vector<InstanceTarget> edge { { 1, *db.toRaw (-1) }, { 2, *db.toRaw (-30) } };
    out = MacroEngine::compute (m, db, edge, 1.0);                       // +6 dB
    CHECK (out[0].clamped); NEAR (*db.toDisplay (out[0].raw), 0, 1e-9);
    NEAR (*db.toDisplay (out[1].raw), -24, 1e-6);
    out = MacroEngine::compute (m, db, edge, 0.5);
    NEAR (*db.toDisplay (out[0].raw), -1, 1e-6);

    // logarithmic: multiplicative, ratios preserved (10 ms & 40 ms, +1 octave -> 20 & 80 ms)
    auto ms = msScale();
    std::vector<InstanceTarget> att { { 1, *ms.toRaw (10) }, { 2, *ms.toRaw (40) } };
    MacroSettings r; r.mode = MacroMode::Relative; r.relativeSpan = 2;   // +/-2 octaves
    out = MacroEngine::compute (r, ms, att, 0.75);
    const double a = *ms.toDisplay (out[0].raw), b = *ms.toDisplay (out[1].raw);
    NEAR (a, 20, 0.1); NEAR (b, 80, 0.4); NEAR (b / a, 4.0, 0.02);

    // discrete / enumerated: integer steps, clamped
    std::vector<ScaleSample> en { { 0, "Clean" }, { 1, "Classic" }, { 2, "Opto" }, { 3, "Vocal" } };
    auto e = ParameterScale::fromSamples (0, 3, true, en);
    MacroSettings s; s.mode = MacroMode::Relative; s.relativeSpan = 3;
    out = MacroEngine::compute (s, e, { { 1, 0 }, { 2, 2 } }, 0.5 + 1.0 / 6.0);   // +1 step
    CHECK (out[0].raw == 1 && out[1].raw == 3);
    out = MacroEngine::compute (s, e, { { 1, 3 } }, 1.0);
    CHECK (out[0].raw == 3 && out[0].clamped);

    // unknown scale: normalized and flagged approximate
    std::vector<ScaleSample> weird { { 0, "A" }, { 0.5, "B" }, { 1, "A" } };
    auto u = ParameterScale::fromSamples (0, 1, false, weird);
    out = MacroEngine::compute (r, u, { { 1, 0.5 } }, 1.0);
    CHECK (out[0].approximate); NEAR (out[0].raw, 1.0, 1e-12);

    // rebase after an external change: the next knob move continues from the new value
    {
        MacroSettings rm; rm.mode = MacroMode::Relative; rm.relativeSpan = 6;
        const double k = 0.5 + (-2.0 / 6.0) / 2.0;                               // knob at -2 dB
        const double start = MacroEngine::rebaseStart (rm, db, *db.toRaw (-20), k); // user set -20 dB in the GUI
        NEAR (*db.toDisplay (start), -18, 1e-6);
        auto o = MacroEngine::compute (rm, db, { { 1, start } }, k);
        NEAR (*db.toDisplay (o[0].raw), -20, 1e-6);                                // no jump
        MacroSettings lm; lm.mode = MacroMode::Relative; lm.relativeSpan = 2;
        const double s2 = MacroEngine::rebaseStart (lm, ms, *ms.toRaw (40), 0.75); // +1 octave
        NEAR (*ms.toDisplay (s2), 20, 0.1);
        NEAR (MacroEngine::rebaseStart (s, e, 3, 0.5 + 1.0 / 6.0), 2, 0);
        MacroSettings d; MacroEngine::applyDefaults (d, db);
        CHECK (d.rangeMin == -60 && d.rangeMax == 0 && d.relativeSpan == 20 && d.precision == 2);
        MacroSettings dl; MacroEngine::applyDefaults (dl, ms);
        CHECK (dl.relativeSpan == 2);
    }

    MacroSettings f; f.fineFactor = 0.1;
    NEAR (MacroEngine::drag (f, 0.5, 0.2, true), 0.52, 1e-12);
    NEAR (MacroEngine::drag (f, 0.5, 0.8, false), 1.0, 0);
}

static DeviceFingerprint fp (const std::string& loc, const std::string& track, int tIdx, const std::string& name = "Pro-C 2")
{
    DeviceFingerprint d;
    d.className = "PluginDevice"; d.classDisplayName = "Pro-C 2"; d.paramNamesHash = "abcd1234";
    d.deviceName = name; d.trackName = track; d.locationKey = loc; d.trackIndex = tIdx;
    return d;
}

static void testReconcile()
{
    std::vector<DeviceFingerprint> saved { fp ("t0/d0", "Kick", 0), fp ("t0/d2", "Kick", 0), fp ("t1/d0", "Snare", 1), fp ("t3/d0", "Vocal", 3, "Vox Comp") };
    InstanceReconciler::assignOrdinals (saved);

    // same project reopened: everything exact
    auto scanned = saved;
    auto r = InstanceReconciler::reconcile (saved, scanned);
    for (int i = 0; i < 4; ++i) CHECK (r[(size_t) i].status == MatchStatus::Exact && r[(size_t) i].candidateIndex == i);

    // a track inserted before Snare (indices shift), the Vocal device renamed "Vox Comp" moved to another slot
    scanned = { fp ("t0/d0", "Kick", 0), fp ("t0/d2", "Kick", 0), fp ("t2/d0", "Snare", 2), fp ("t4/d1", "Vocal", 4, "Vox Comp") };
    InstanceReconciler::assignOrdinals (scanned);
    r = InstanceReconciler::reconcile (saved, scanned);
    CHECK (r[2].status == MatchStatus::Relocated && r[2].candidateIndex == 2);
    CHECK (r[3].status == MatchStatus::Relocated && r[3].candidateIndex == 3);

    // a different plug-in is never matched
    auto other = scanned; other[2].classDisplayName = "Pro-Q 3";
    r = InstanceReconciler::reconcile (saved, other);
    CHECK (r[2].status == MatchStatus::Missing && r[2].candidateIndex == -1);
    // the same plug-in with a different Configure set (other param hash) is still recognized
    auto conf = saved; conf[1].paramNamesHash = "ffff0000";
    r = InstanceReconciler::reconcile (saved, conf);
    CHECK (r[1].status == MatchStatus::Exact && r[1].candidateIndex == 1);

    // two identical candidates on a renamed track: ambiguous, not silently mapped
    std::vector<DeviceFingerprint> s1 { fp ("t5/d0", "Synth", 5) };
    InstanceReconciler::assignOrdinals (s1);
    std::vector<DeviceFingerprint> c1 { fp ("t7/d0", "Synth", 7), fp ("t8/d0", "Synth", 8) };
    c1[1].ordinalOnTrack = 0; c1[0].ordinalOnTrack = 0;
    r = InstanceReconciler::reconcile (s1, c1);
    CHECK (r[0].status == MatchStatus::Ambiguous && r[0].candidateIndex == -1);

    // track deleted: missing
    r = InstanceReconciler::reconcile ({ fp ("t9/d0", "Gone", 9) }, scanned);
    CHECK (r[0].status == MatchStatus::Missing);
}

static void testProtocol()
{
    using namespace gc::proto;
    SequenceFilter f;
    CHECK (f.accept ("a", 1) && f.accept ("a", 2) && ! f.accept ("a", 2) && f.accept ("b", 2));

    SetCoalescer c (16);
    for (int i = 0; i < 100; ++i) { c.push (10, i / 100.0); c.push (11, i / 50.0); }
    auto b = c.poll (0);
    CHECK (b.size() == 2 && b[0].second == 0.99 && b[1].second == 1.98);
    c.push (10, 0.5);
    CHECK (c.poll (5).empty() && c.poll (16).size() == 1);

    ChunkAssembler ca;
    CHECK (! ca.add ("s1", 1, 2, "ld\"}", 0));
    auto doc = ca.add ("s1", 0, 2, "{\"a\":\"wor", 1);
    CHECK (doc && *doc == "{\"a\":\"world\"}");
    ca.add ("s2", 0, 3, "x", 0);
    CHECK (ca.expire (5000).size() == 1);
    CHECK (uriDecode ("%7B%22a%22%3A1%7D") == "{\"a\":1}" && uriDecode (uriEncode ("Pro-C 2 / Kick, ü")) == "Pro-C 2 / Kick, ü");

    ConnectionMonitor m;
    CHECK (m.tick (0) == ConnectionMonitor::Action::SendHello && m.state() == ConnectionMonitor::State::Handshaking);
    CHECK (m.tick (500) == ConnectionMonitor::Action::None);
    m.onHelloReply ("B1", "nonce", "gcp-1", 600);
    CHECK (m.state() == ConnectionMonitor::State::Connected && m.takeProjectChanged() && ! m.takeProjectChanged());
    CHECK (m.tick (700) == ConnectionMonitor::Action::SendHeartbeat);
    m.onBridgeTraffic ("B1", 3000);
    CHECK (m.tick (3500) == ConnectionMonitor::Action::None || m.state() == ConnectionMonitor::State::Connected);
    m.tick (8000);                                                           // silence > 4 s
    CHECK (m.state() == ConnectionMonitor::State::Lost);
    m.onHelloReply ("B2", "n2", "gcp-2", 8100);                              // Live Set reopened / other project
    CHECK (m.state() == ConnectionMonitor::State::Connected && m.takeProjectChanged());
    m.onBridgeTraffic ("B3", 8200);                                          // bridge reloaded underneath us
    CHECK (m.state() == ConnectionMonitor::State::Lost && m.tick (8201) == ConnectionMonitor::Action::SendHello);
}

int main()
{
    struct { const char* name; void (*fn)(); } tests[] = {
        { "display parsing", testParse }, { "scale detection & inversion", testScales }, { "absolute mode", testAbsolute },
        { "relative mode", testRelative }, { "instance reconciliation", testReconcile }, { "protocol core", testProtocol } };
    for (auto& t : tests) { const int before = g_fail; t.fn(); std::printf ("  %s  %s\n", g_fail == before ? "ok  " : "FAIL", t.name); }
    std::printf ("%d checks, %d failed\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
