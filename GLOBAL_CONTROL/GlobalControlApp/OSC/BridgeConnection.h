#pragma once
// BridgeConnection: the app side of the OSC protocol v1, on top of juce_osc.
//  - receives on a DatagramSocket bound to 127.0.0.1:47120 (never reachable from the network)
//  - sends to 127.0.0.1:47121 ([udpreceive] of GLOBAL_CONTROL_BRIDGE.amxd)
//  - handshake/nonce, heartbeat, timeout and automatic reconnection (ConnectionMonitor)
//  - duplicate filtering (SequenceFilter), chunk reassembly with retry (ChunkAssembler)
//  - latest-wins coalescing of parameter writes, one batch every 16 ms at most (SetCoalescer)
// Network I/O runs on juce_osc's receiver thread; every callback below is delivered on the message thread.
#include <juce_events/juce_events.h>
#include <juce_osc/juce_osc.h>

#include "../Core/ParameterScale.h"
#include "../Models/ProjectModel.h"
#include "ProtocolCore.h"

namespace gc {

struct CurveData {
    int64_t paramId = 0;
    ParameterScale scale;
    std::vector<ScaleSample> samples;
};

class BridgeConnection : private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>,
                         private juce::Timer {
public:
    enum class State { Offline, Searching, Connected, Lost };

    struct Listener {
        virtual ~Listener() = default;
        virtual void connectionChanged (State) {}
        /** First connection, bridge reloaded or another Live Set opened: rescan & reconcile. */
        virtual void projectChanged (const juce::String& /*projectId*/) {}
        virtual void scanCompleted (const ProjectSnapshot&) {}
        virtual void deviceParametersReceived (const juce::String& /*deviceId*/, const juce::Array<ParamInfo>&) {}
        virtual void curveReceived (const CurveData&) {}
        virtual void parameterValue (int64_t /*pid*/, double /*value*/, const juce::String& /*display*/, const juce::String& /*source*/, int /*txn*/) {}
        virtual void structureChanged() {}
        virtual void parameterRemoved (int64_t) {}
        virtual void bridgeError (const juce::String& /*code*/, const juce::String& /*message*/) {}
    };

    BridgeConnection();
    ~BridgeConnection() override;

    /** Opens the sockets. Returns false (with a reason) if the app port is busy. */
    bool start (int appPort = proto::kAppPort, int bridgePort = proto::kBridgePort, juce::String* error = nullptr);
    void stop();

    void addListener (Listener* l) { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

    State getState() const noexcept { return state; }
    juce::String getProjectId() const { return monitor.projectId(); }
    juce::String getLiveVersion() const { return liveVersion; }
    juce::String getBridgeVersion() const { return bridgeVersion; }
    double getLastBridgeTrafficMs() const noexcept { return lastTrafficMs; }
    bool isScanning() const noexcept { return scanReq != 0; }

    // requests (no-ops while not connected)
    void requestScan();
    void requestParameters (const juce::String& deviceId);
    void requestCurve (int64_t paramId, int samples = 65);
    void queueSet (int64_t paramId, double raw);           // coalesced, sent by the timer
    void observe (const juce::Array<int64_t>& ids, bool on);
    void get (const juce::Array<int64_t>& ids);
    void setProjectId (const juce::String& id);

    struct Stats { int64_t packetsIn = 0, packetsOut = 0, duplicates = 0, setsSent = 0, errors = 0; };
    const Stats& getStats() const noexcept { return stats; }

private:
    void oscMessageReceived (const juce::OSCMessage&) override;
    void timerCallback() override;

    bool sendMessage (const juce::String& address, std::initializer_list<juce::OSCArgument> payload);
    bool sendArgs (const juce::String& address, const juce::Array<juce::OSCArgument>& payload);
    void sendHello();
    void setState (State);
    void handleChunk (const juce::String& kind, const juce::OSCMessage&);
    void onDocument (const juce::String& kind, const juce::String& key, const juce::var& doc);

    juce::DatagramSocket rxSocket { false };
    juce::OSCReceiver receiver;
    juce::OSCSender sender;
    bool running = false;

    juce::String appSession;
    int64_t seq = 0, lastHelloSeq = 0;
    proto::SequenceFilter seqFilter;
    proto::ChunkAssembler chunks;
    proto::ConnectionMonitor monitor;
    proto::SetCoalescer coalescer { 16.0 };
    State state = State::Offline;
    juce::String liveVersion, bridgeVersion;
    double lastTrafficMs = 0;
    int64_t scanReq = 0;
    double scanStartedMs = 0;
    std::map<juce::String, std::pair<double, int>> pendingDocs;   // "kind:key" -> (sentAt, retries)
    Stats stats;
    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BridgeConnection)
};

const char* toString (BridgeConnection::State);

} // namespace gc
