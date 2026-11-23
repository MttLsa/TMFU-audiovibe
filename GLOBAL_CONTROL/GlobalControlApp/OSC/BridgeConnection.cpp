#include "BridgeConnection.h"

namespace gc {

using juce::OSCArgument;
using juce::String;

const char* toString (BridgeConnection::State s)
{
    switch (s) {
        case BridgeConnection::State::Offline: return "offline";
        case BridgeConnection::State::Searching: return "searching";
        case BridgeConnection::State::Connected: return "connected";
        case BridgeConnection::State::Lost: return "lost";
    }
    return "?";
}

static double nowMs() { return juce::Time::getMillisecondCounterHiRes(); }

static double num (const juce::OSCMessage& m, int i)
{
    if (i >= m.size()) return 0;
    const auto& a = m[i];
    if (a.isInt32()) return a.getInt32();
    if (a.isFloat32()) return a.getFloat32();
    if (a.isString()) return a.getString().getDoubleValue();
    return 0;
}

static String str (const juce::OSCMessage& m, int i)
{
    if (i >= m.size()) return {};
    const auto& a = m[i];
    if (a.isString()) return a.getString();
    if (a.isInt32()) return String (a.getInt32());
    if (a.isFloat32()) return String (a.getFloat32());
    return {};
}

static String uri (const String& s) { return String::fromUTF8 (proto::uriDecode (s.toStdString()).c_str()); }

BridgeConnection::BridgeConnection()
{
    appSession = String::toHexString (juce::Random::getSystemRandom().nextInt64()).paddedLeft ('0', 16);
}

BridgeConnection::~BridgeConnection() { stop(); }

bool BridgeConnection::start (int appPort, int bridgePort, String* error)
{
    stop();
    if (! rxSocket.bindToPort (appPort, "127.0.0.1")) {
        if (error) *error = "UDP port " + String (appPort) + " on 127.0.0.1 is busy (is another GLOBAL CONTROL running?)";
        return false;
    }
    if (! receiver.connectToSocket (rxSocket) || ! sender.connect ("127.0.0.1", bridgePort)) {
        if (error) *error = "cannot open the OSC sockets";
        return false;
    }
    receiver.addListener (this);
    running = true;
    monitor = proto::ConnectionMonitor();
    setState (State::Searching);
    startTimer (16);
    return true;
}

void BridgeConnection::stop()
{
    if (! running) return;
    stopTimer();
    receiver.removeListener (this);
    receiver.disconnect();
    sender.disconnect();
    rxSocket.shutdown();
    running = false;
    setState (State::Offline);
}

void BridgeConnection::setState (State s)
{
    if (s == state) return;
    state = s;
    listeners.call ([s] (Listener& l) { l.connectionChanged (s); });
}

bool BridgeConnection::sendArgs (const String& address, const juce::Array<OSCArgument>& payload)
{
    if (! running) return false;
    juce::OSCMessage m { juce::OSCAddressPattern (address) };
    m.addInt32 (proto::kVersion);
    m.addString (appSession);
    m.addInt32 ((juce::int32) ++seq);
    m.addString (address == "/global/hello" ? String() : String (monitor.nonce()));
    for (const auto& a : payload) m.addArgument (a);
    ++stats.packetsOut;
    return sender.send (m);
}

bool BridgeConnection::sendMessage (const String& address, std::initializer_list<OSCArgument> payload)
{
    juce::Array<OSCArgument> a;
    for (const auto& x : payload) a.add (x);
    return sendArgs (address, a);
}

void BridgeConnection::sendHello()
{
    sendMessage ("/global/hello", { OSCArgument (String ("GLOBAL CONTROL")), OSCArgument (String (JUCE_STRINGIFY (GC_VERSION))) });
    lastHelloSeq = seq;
}

void BridgeConnection::requestScan()
{
    if (state != State::Connected || scanReq != 0) return;
    sendMessage ("/global/scan/request", { OSCArgument ((juce::int32) (seq + 1)) });
    scanReq = seq;
    scanStartedMs = nowMs();
    pendingDocs["list:" + String (scanReq)] = { nowMs(), 0 };
}

void BridgeConnection::requestParameters (const String& deviceId)
{
    if (state != State::Connected) return;
    sendMessage ("/global/device/parameters", { OSCArgument ((juce::int32) deviceId.getIntValue()) });
    auto& p = pendingDocs["params:" + deviceId];
    p.first = nowMs();
}

void BridgeConnection::requestCurve (int64_t pid, int samples)
{
    if (state != State::Connected) return;
    sendMessage ("/global/parameter/describe", { OSCArgument ((juce::int32) pid), OSCArgument ((juce::int32) samples) });
    auto& p = pendingDocs["curve:" + String (pid)];
    p.first = nowMs();
}

void BridgeConnection::queueSet (int64_t pid, double raw) { coalescer.push (pid, raw); }

void BridgeConnection::observe (const juce::Array<int64_t>& ids, bool on)
{
    for (int start = 0; start < ids.size(); start += 200) {
        juce::Array<OSCArgument> a { OSCArgument ((juce::int32) (on ? 1 : 0)), OSCArgument ((juce::int32) juce::jmin (200, ids.size() - start)) };
        for (int i = start; i < juce::jmin (ids.size(), start + 200); ++i) a.add (OSCArgument ((juce::int32) ids[i]));
        sendArgs ("/global/parameter/observe", a);
    }
}

void BridgeConnection::get (const juce::Array<int64_t>& ids)
{
    for (int start = 0; start < ids.size(); start += 200) {
        juce::Array<OSCArgument> a { OSCArgument ((juce::int32) juce::jmin (200, ids.size() - start)) };
        for (int i = start; i < juce::jmin (ids.size(), start + 200); ++i) a.add (OSCArgument ((juce::int32) ids[i]));
        sendArgs ("/global/parameter/get", a);
    }
}

void BridgeConnection::setProjectId (const String& id)
{
    sendMessage ("/global/project/setid", { OSCArgument (id) });
}

void BridgeConnection::timerCallback()
{
    const double t = nowMs();
    switch (monitor.tick (t)) {
        case proto::ConnectionMonitor::Action::SendHello: sendHello(); break;
        case proto::ConnectionMonitor::Action::SendHeartbeat: sendMessage ("/global/heartbeat", {}); break;
        case proto::ConnectionMonitor::Action::None: break;
    }
    const auto ms = monitor.state();
    if (ms == proto::ConnectionMonitor::State::Connected) setState (State::Connected);
    else if (ms == proto::ConnectionMonitor::State::Lost) { setState (State::Lost); scanReq = 0; }
    else setState (State::Searching);

    if (state == State::Connected) {
        auto batch = coalescer.poll (t);
        for (size_t start = 0; start < batch.size(); start += 200) {
            const auto n = std::min<size_t> (200, batch.size() - start);
            juce::Array<OSCArgument> a { OSCArgument ((juce::int32) (seq + 1)), OSCArgument ((juce::int32) n) };
            for (size_t i = start; i < start + n; ++i) {
                a.add (OSCArgument ((juce::int32) batch[i].first));
                a.add (OSCArgument ((float) batch[i].second));
            }
            sendArgs ("/global/parameter/set", a);
            stats.setsSent += (int64_t) n;
        }
    }

    for (const auto& key : chunks.expire (t, 3000)) pendingDocs[String (key)].first = 0;   // force a retry below

    for (auto it = pendingDocs.begin(); it != pendingDocs.end();) {
        auto& [sentAt, retries] = it->second;
        const auto key = it->first;
        if (t - sentAt < (key.startsWith ("list:") ? 15000.0 : 3000.0)) { ++it; continue; }
        if (state != State::Connected || retries >= 2) {
            listeners.call ([key] (Listener& l) { l.bridgeError ("E_TIMEOUT", "no complete answer for " + key); });
            if (key.startsWith ("list:")) scanReq = 0;
            it = pendingDocs.erase (it);
            continue;
        }
        ++retries;
        const auto kind = key.upToFirstOccurrenceOf (":", false, false), id = key.fromFirstOccurrenceOf (":", false, false);
        it = pendingDocs.erase (it);
        if (kind == "list") { scanReq = 0; requestScan(); }
        else if (kind == "params") requestParameters (id);
        else if (kind == "curve") requestCurve (id.getLargeIntValue());
        if (pendingDocs.count (key)) pendingDocs[key].second = retries;
        else if (kind == "list" && scanReq) pendingDocs["list:" + String (scanReq)].second = retries;
    }
}

void BridgeConnection::oscMessageReceived (const juce::OSCMessage& m)
{
    ++stats.packetsIn;
    if (m.size() < 3 || (int) num (m, 0) != proto::kVersion || ! m[1].isString()) { ++stats.errors; return; }
    const String address = m.getAddressPattern().toString();
    const String session = m[1].getString();
    const auto mseq = (int64_t) num (m, 2);
    if (! seqFilter.accept (session.toStdString(), mseq)) { ++stats.duplicates; return; }
    const double t = nowMs();
    lastTrafficMs = t;

    if (address == "/global/hello") {
        const auto reqSeq = (int64_t) num (m, 3);
        const auto nonce = str (m, 4);
        if (nonce.isEmpty() || reqSeq != lastHelloSeq) {        // unsolicited announce: bridge just loaded
            monitor.onBridgeTraffic (session.toStdString(), t);
            sendHello();
            return;
        }
        liveVersion = str (m, 6); bridgeVersion = str (m, 7);
        monitor.onHelloReply (session.toStdString(), nonce.toStdString(), str (m, 5).toStdString(), t);
        setState (State::Connected);
        if (monitor.takeProjectChanged()) {
            const auto pid = getProjectId();
            listeners.call ([pid] (Listener& l) { l.projectChanged (pid); });
        }
        return;
    }

    monitor.onBridgeTraffic (session.toStdString(), t);
    if (monitor.state() != proto::ConnectionMonitor::State::Connected) return;   // stale session: timer re-handshakes

    if (address == "/global/heartbeat") {
        const auto pid = str (m, 5);
        if (pid.isNotEmpty() && pid != getProjectId()) sendHello();   // project id became known / changed
        return;
    }
    if (address == "/global/device/list" || address == "/global/device/parameters" || address == "/global/parameter/curve") {
        handleChunk (address, m);
        return;
    }
    if (address == "/global/scan/complete") {
        if ((int64_t) num (m, 3) == scanReq) scanReq = 0;
        return;
    }
    if (address == "/global/parameter/value") {
        const auto pid = str (m, 3).getLargeIntValue();
        const double v = num (m, 4);
        const auto disp = uri (str (m, 5)), src = str (m, 6);
        const int txn = (int) num (m, 7);
        listeners.call ([&] (Listener& l) { l.parameterValue (pid, v, disp, src, txn); });
        return;
    }
    if (address == "/global/instance/status") {
        const auto kind = str (m, 3);
        if (kind == "structure_changed") listeners.call ([] (Listener& l) { l.structureChanged(); });
        else if (kind == "param_removed") { const auto pid = str (m, 4).getLargeIntValue(); listeners.call ([pid] (Listener& l) { l.parameterRemoved (pid); }); }
        return;
    }
    if (address == "/global/error") {
        const auto code = str (m, 4), msg = uri (str (m, 5));
        ++stats.errors;
        if (code == "E_AUTH") sendHello();
        listeners.call ([&] (Listener& l) { l.bridgeError (code, msg); });
        return;
    }
}

void BridgeConnection::handleChunk (const String& address, const juce::OSCMessage& m)
{
    if (m.size() < 8) return;
    const String kind = address == "/global/device/list" ? "list" : address == "/global/device/parameters" ? "params" : "curve";
    const String key = str (m, 4);
    const auto doc = chunks.add ((kind + ":" + key).toStdString(), (int) num (m, 5), (int) num (m, 6),
                                 proto::uriDecode (str (m, 7).toStdString()), nowMs());
    if (! doc) return;
    const auto parsed = juce::JSON::parse (String::fromUTF8 (doc->c_str()));
    if (kind == "list") pendingDocs.erase ("list:" + String (scanReq));
    pendingDocs.erase (kind + ":" + key);
    onDocument (kind, key, parsed);
}

void BridgeConnection::onDocument (const String& kind, const String& key, const juce::var& doc)
{
    if (kind == "list") {
        const auto snap = ProjectSnapshot::fromJson (doc);
        scanReq = 0;
        auto s2 = snap;
        s2.durationMs = (int) (nowMs() - scanStartedMs);
        listeners.call ([&s2] (Listener& l) { l.scanCompleted (s2); });
    } else if (kind == "params") {
        const auto params = parseParameters (doc);
        listeners.call ([&] (Listener& l) { l.deviceParametersReceived (key, params); });
    } else {
        CurveData c;
        c.paramId = key.getLargeIntValue();
        if (auto* arr = doc["samples"].getArray())
            for (const auto& smp : *arr) c.samples.push_back ({ (double) smp[0], smp[1].toString().toStdString() });
        c.scale = ParameterScale::fromSamples ((double) doc["min"], (double) doc["max"], (int) doc["isQuantized"] != 0, c.samples);
        listeners.call ([&c] (Listener& l) { l.curveReceived (c); });
    }
}

} // namespace gc
