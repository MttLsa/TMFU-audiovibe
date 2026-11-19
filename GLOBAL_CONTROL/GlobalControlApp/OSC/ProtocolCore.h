#pragma once
// Transport-independent pieces of the GLOBAL CONTROL OSC protocol (v1) used by the app.
// The JUCE OSCSender/OSCReceiver glue (Milestone 6/7) feeds these classes; they hold no sockets and take
// the current time as a parameter so they can be unit-tested deterministically.
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace gc::proto {

constexpr int kVersion = 1;
constexpr int kAppPort = 47120;      // app listens on 127.0.0.1 only
constexpr int kBridgePort = 47121;   // bridge [udpreceive]
constexpr double kHeartbeatMs = 1000, kTimeoutMs = 4000, kHelloRetryMs = 1000;

/** Envelope check for bridge -> app messages: proto, bridgeSession, seq. */
struct Envelope { int proto = 0; std::string session; int64_t seq = 0; };

/** Drops duplicated packets (same session + seq) and restarts when the bridge session changes. */
class SequenceFilter {
public:
    explicit SequenceFilter (size_t window = 1024) : window_ (window) {}
    /** true when the packet must be processed. */
    bool accept (const std::string& session, int64_t seq);
    const std::string& session() const noexcept { return session_; }
private:
    size_t window_;
    std::string session_;
    std::unordered_set<int64_t> seen_;
    std::deque<int64_t> order_;
};

/** Latest-wins coalescing of outgoing parameter writes with a minimum interval between flushes.
    A knob moved at 1 kHz on 10 instances never produces more than (1000/minIntervalMs) packets/s. */
class SetCoalescer {
public:
    explicit SetCoalescer (double minIntervalMs = 16.0) : minInterval_ (minIntervalMs) {}
    void push (int64_t paramId, double raw) { pending_[paramId] = raw; }
    /** Returns the batch to send now (possibly empty). */
    std::vector<std::pair<int64_t, double>> poll (double nowMs);
    bool empty() const noexcept { return pending_.empty(); }
private:
    double minInterval_, last_ = -1e300;
    std::map<int64_t, double> pending_;
};

/** Reassembles chunked JSON transfers (/global/device/list, /global/device/parameters, /global/parameter/curve). */
class ChunkAssembler {
public:
    /** Adds one URI-decoded chunk; returns the full document when complete. */
    std::optional<std::string> add (const std::string& key, int part, int total, const std::string& chunk, double nowMs);
    /** Drops transfers older than timeoutMs, returns their keys (the caller re-requests them). */
    std::vector<std::string> expire (double nowMs, double timeoutMs = 3000);
private:
    struct Transfer { int total = 0; std::map<int, std::string> parts; double started = 0; };
    std::map<std::string, Transfer> open_;
};

std::string uriDecode (const std::string&);
std::string uriEncode (const std::string&);

/** Connection state machine: hello / heartbeat / timeout / reconnect. */
class ConnectionMonitor {
public:
    enum class State { Disconnected, Handshaking, Connected, Lost };
    /** What the app must do after tick(). */
    enum class Action { None, SendHello, SendHeartbeat };

    Action tick (double nowMs);
    void onHelloReply (const std::string& bridgeSession, const std::string& nonce, const std::string& projectId, double nowMs);
    void onBridgeTraffic (const std::string& bridgeSession, double nowMs);   // heartbeat or any valid message

    State state() const noexcept { return state_; }
    const std::string& nonce() const noexcept { return nonce_; }
    const std::string& projectId() const noexcept { return projectId_; }
    /** true once when the bridge restarted or a different Live Set was opened: rescan + reconcile. */
    bool takeProjectChanged() { const bool c = projectChanged_; projectChanged_ = false; return c; }

private:
    State state_ = State::Disconnected;
    std::string bridgeSession_, nonce_, projectId_;
    double lastRx_ = -1e300, lastHello_ = -1e300, lastHb_ = -1e300;
    bool projectChanged_ = false;
};

const char* toString (ConnectionMonitor::State);

} // namespace gc::proto
