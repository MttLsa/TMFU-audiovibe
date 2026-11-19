#pragma once
// InstanceReconciler: re-associates saved instances (preset / previous session) with the devices found by
// a new scan. Live runtime ids are NOT stable across sessions, so identity is reconstructed from a
// fingerprint. Hard constraints (must match, otherwise the candidate is rejected):
//   - className (PluginDevice / AuPluginDevice / native class)
//   - classDisplayName (the plug-in, e.g. "Pro-C 2")
//   - paramNamesHash  (the exact list of exposed parameter names: same plug-in, same Configure set)
// Soft evidence (scored): bridge project id, location key (t3/d1/c0/d0), track name, device name,
// ordinal among same-plug-in devices on the same track, track index.
// Rules: one-to-one; a saved instance whose best score is tied with another candidate is AMBIGUOUS and
// is NOT mapped; below the minimum evidence it is MISSING. Nothing is ever mapped silently on a guess.
#include <cstdint>
#include <string>
#include <vector>

namespace gc {

struct DeviceFingerprint {
    std::string runtimeId;        // Live id at the time (informative only)
    std::string projectId;        // id stored by the bridge inside the Live Set
    std::string className, classDisplayName, paramNamesHash;
    std::string deviceName;       // user-visible name (may be renamed)
    std::string trackName;
    std::string locationKey;      // e.g. "t3/d1" or "t4/d0/c1/d0"
    int trackIndex = -1;
    int ordinalOnTrack = 0;       // n-th device of this plug-in on the same track (0-based)
};

enum class MatchStatus { Exact, Relocated, Ambiguous, Missing };
const char* toString (MatchStatus);

struct MatchResult {
    int savedIndex = -1;
    int candidateIndex = -1;      // -1 for Ambiguous / Missing
    MatchStatus status = MatchStatus::Missing;
    int score = 0;
    std::string reason;
};

class InstanceReconciler {
public:
    static constexpr int kMinScore = 3;

    /** Computes ordinalOnTrack for a scanned list (in scan order). */
    static void assignOrdinals (std::vector<DeviceFingerprint>&);

    static std::vector<MatchResult> reconcile (const std::vector<DeviceFingerprint>& saved,
                                               const std::vector<DeviceFingerprint>& scanned);

    static int score (const DeviceFingerprint& saved, const DeviceFingerprint& candidate);
};

} // namespace gc
