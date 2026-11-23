#include "ProtocolCore.h"

#include <cctype>

namespace gc::proto {

bool SequenceFilter::accept (const std::string& session, int64_t seq)
{
    if (session != session_) { session_ = session; seen_.clear(); order_.clear(); }
    if (! seen_.insert (seq).second) return false;
    order_.push_back (seq);
    if (order_.size() > window_) { seen_.erase (order_.front()); order_.pop_front(); }
    return true;
}

std::vector<std::pair<int64_t, double>> SetCoalescer::poll (double now)
{
    std::vector<std::pair<int64_t, double>> out;
    if (pending_.empty() || now - last_ < minInterval_) return out;
    out.assign (pending_.begin(), pending_.end());
    pending_.clear();
    last_ = now;
    return out;
}

std::optional<std::string> ChunkAssembler::add (const std::string& key, int part, int total, const std::string& chunk, double now)
{
    if (total <= 0 || part < 0 || part >= total) return std::nullopt;
    auto& t = open_[key];
    if (t.total != total) { t = Transfer{}; t.total = total; t.started = now; }
    t.parts[part] = chunk;
    if ((int) t.parts.size() < total) return std::nullopt;
    std::string doc;
    for (auto& [i, c] : t.parts) doc += c;
    open_.erase (key);
    return doc;
}

std::vector<std::string> ChunkAssembler::expire (double now, double timeoutMs)
{
    std::vector<std::string> dead;
    for (auto it = open_.begin(); it != open_.end();) {
        if (now - it->second.started > timeoutMs) { dead.push_back (it->first); it = open_.erase (it); }
        else ++it;
    }
    return dead;
}

static int hexv (char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    c = (char) std::tolower ((unsigned char) c);
    return (c >= 'a' && c <= 'f') ? c - 'a' + 10 : -1;
}

std::string uriDecode (const std::string& s)
{
    std::string o;
    o.reserve (s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size() && hexv (s[i + 1]) >= 0 && hexv (s[i + 2]) >= 0) {
            o += (char) (hexv (s[i + 1]) * 16 + hexv (s[i + 2])); i += 2;
        } else o += s[i];
    }
    return o;
}

std::string uriEncode (const std::string& s)
{
    static const char* hex = "0123456789ABCDEF";
    std::string o;
    for (unsigned char c : s) {
        if (std::isalnum (c) || c == '-' || c == '_' || c == '.' || c == '!' || c == '~' || c == '*' || c == '\'' || c == '(' || c == ')') o += (char) c;
        else { o += '%'; o += hex[c >> 4]; o += hex[c & 15]; }
    }
    return o;
}

ConnectionMonitor::Action ConnectionMonitor::tick (double now)
{
    if ((state_ == State::Connected) && now - lastRx_ > kTimeoutMs) state_ = State::Lost;
    if (state_ != State::Connected) {
        if (now - lastHello_ >= kHelloRetryMs) {
            lastHello_ = now;
            if (state_ == State::Disconnected) state_ = State::Handshaking;
            return Action::SendHello;
        }
        return Action::None;
    }
    if (now - lastHb_ >= kHeartbeatMs) { lastHb_ = now; return Action::SendHeartbeat; }
    return Action::None;
}

void ConnectionMonitor::onHelloReply (const std::string& session, const std::string& nonce, const std::string& projectId, double now)
{
    if (! bridgeSession_.empty() && (session != bridgeSession_ || projectId != projectId_)) projectChanged_ = true;
    if (bridgeSession_.empty()) projectChanged_ = true;     // first connection: initial scan
    bridgeSession_ = session; nonce_ = nonce; projectId_ = projectId;
    state_ = State::Connected; lastRx_ = now;
}

void ConnectionMonitor::onBridgeTraffic (const std::string& session, double now)
{
    if (state_ == State::Connected && session != bridgeSession_) {   // bridge reloaded (Set reopened): new handshake
        state_ = State::Lost;
        lastHello_ = -1e300;
        return;
    }
    if (session == bridgeSession_) lastRx_ = now;
}

const char* toString (ConnectionMonitor::State s)
{
    switch (s) {
        case ConnectionMonitor::State::Disconnected: return "disconnected";
        case ConnectionMonitor::State::Handshaking: return "handshaking";
        case ConnectionMonitor::State::Connected: return "connected";
        case ConnectionMonitor::State::Lost: return "lost";
    }
    return "?";
}

} // namespace gc::proto
