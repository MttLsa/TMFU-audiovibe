#include "InstanceReconciler.h"

#include <algorithm>
#include <map>
#include <tuple>

namespace gc {

const char* toString (MatchStatus s)
{
    switch (s) {
        case MatchStatus::Exact: return "exact";
        case MatchStatus::Relocated: return "relocated";
        case MatchStatus::Ambiguous: return "ambiguous";
        case MatchStatus::Missing: break;
    }
    return "missing";
}

void InstanceReconciler::assignOrdinals (std::vector<DeviceFingerprint>& v)
{
    std::map<std::tuple<std::string, std::string, std::string>, int> counter;
    for (auto& d : v) {
        const auto trackPart = d.locationKey.substr (0, d.locationKey.find ('/'));
        d.ordinalOnTrack = counter[{ trackPart, d.className, d.classDisplayName }]++;
    }
}

int InstanceReconciler::score (const DeviceFingerprint& s, const DeviceFingerprint& c)
{
    if (s.className != c.className || s.classDisplayName != c.classDisplayName)
        return -1;
    int sc = 0;
    if (! s.paramNamesHash.empty() && s.paramNamesHash == c.paramNamesHash) sc += 1;   // same Configure set
    if (! s.locationKey.empty() && s.locationKey == c.locationKey) sc += 4;
    if (! s.trackName.empty() && s.trackName == c.trackName) sc += 3;
    if (s.deviceName == c.deviceName && s.deviceName != s.classDisplayName) sc += 2;   // a custom name is strong evidence
    if (s.trackName == c.trackName && s.ordinalOnTrack == c.ordinalOnTrack) sc += 2;
    if (s.trackIndex >= 0 && s.trackIndex == c.trackIndex) sc += 1;
    return sc;
}

std::vector<MatchResult> InstanceReconciler::reconcile (const std::vector<DeviceFingerprint>& saved,
                                                        const std::vector<DeviceFingerprint>& scanned)
{
    struct Pair { int s, c, score; };
    std::vector<Pair> pairs;
    std::vector<std::vector<int>> perSaved (saved.size());
    for (int i = 0; i < (int) saved.size(); ++i)
        for (int j = 0; j < (int) scanned.size(); ++j) {
            const int sc = score (saved[(size_t) i], scanned[(size_t) j]);
            if (sc >= kMinScore) pairs.push_back ({ i, j, sc });
        }
    std::stable_sort (pairs.begin(), pairs.end(), [] (const Pair& a, const Pair& b) { return a.score > b.score; });

    std::vector<MatchResult> res (saved.size());
    for (int i = 0; i < (int) saved.size(); ++i) res[(size_t) i].savedIndex = i;
    std::vector<bool> savedDone (saved.size(), false), candTaken (scanned.size(), false);

    // Greedy by descending score, but a saved instance with two equally good FREE candidates is ambiguous,
    // and a candidate wanted equally by two saved instances is ambiguous for both.
    for (size_t k = 0; k < pairs.size(); ++k) {
        const auto& p = pairs[k];
        if (savedDone[(size_t) p.s] || candTaken[(size_t) p.c]) continue;
        bool tie = false;
        for (const auto& q : pairs) {
            if (q.score != p.score) continue;
            if (q.s == p.s && q.c != p.c && ! candTaken[(size_t) q.c]) tie = true;
            if (q.c == p.c && q.s != p.s && ! savedDone[(size_t) q.s]) tie = true;
        }
        auto& r = res[(size_t) p.s];
        if (tie) {
            r.status = MatchStatus::Ambiguous; r.score = p.score;
            r.reason = "several devices match equally well - choose manually";
            savedDone[(size_t) p.s] = true;
            continue;
        }
        r.candidateIndex = p.c; r.score = p.score;
        const auto& s = saved[(size_t) p.s]; const auto& c = scanned[(size_t) p.c];
        r.status = (s.locationKey == c.locationKey && s.trackName == c.trackName) ? MatchStatus::Exact : MatchStatus::Relocated;
        r.reason = r.status == MatchStatus::Exact ? "same plug-in, same place" : "same plug-in, moved/renamed: " + s.locationKey + " -> " + c.locationKey;
        savedDone[(size_t) p.s] = true; candTaken[(size_t) p.c] = true;
    }
    for (auto& r : res)
        if (r.status == MatchStatus::Missing) r.reason = "no device with the same plug-in fingerprint and enough evidence";
    return res;
}

} // namespace gc
