#pragma once
// Data received from GLOBAL_CONTROL_BRIDGE (see Protocol/OSCSpecification/GC_OSC_PROTOCOL.md).
// Everything here comes from the Live Object Model at runtime: no static plug-in data.
#include <juce_core/juce_core.h>

#include "../MappingEngine/InstanceReconciler.h"

namespace gc {

struct TrackInfo {
    juce::String id, key, kind, name;   // kind: track | return | master
    int index = 0, colour = 0;
    bool isMidi = false, isGroup = false, isBridgeTrack = false;
};

struct DeviceInfo {
    juce::String id, key, trackId, trackName, trackKey, trackKind, rackPath;
    juce::String name, className, classDisplayName, format, paramNamesHash;
    int position = 0, depth = 0, type = 0, paramCount = 0, trackIndex = -1;
    bool isPlugin = false, isActive = true, canHaveChains = false, isBridge = false;

    /** Devices of the same kind ("the same plug-in"): class + plug-in name. */
    juce::String pluginKey() const { return className + "|" + classDisplayName; }
    juce::String location() const;          // "Kick · slot 2" / "Drum Bus · Rack > slot 1"
    DeviceFingerprint fingerprint (const juce::String& projectId) const;
};

struct ParamInfo {
    int64_t id = 0;
    int index = 0;
    juce::String name, originalName, display;
    double value = 0, min = 0, max = 1, defaultValue = 0;
    bool hasDefault = false, quantized = false, enabled = true;
    int state = 0, automationState = 0;
    juce::StringArray items;

    /** Can GLOBAL CONTROL write it? (exposed, enabled, active) */
    bool controllable() const { return enabled && state != 2; }
    juce::String statusText() const;
};

struct ProjectSnapshot {
    juce::String projectId;
    juce::Array<TrackInfo> tracks;
    juce::Array<DeviceInfo> devices;
    juce::Time scannedAt;
    int durationMs = 0;
    bool valid = false;

    static ProjectSnapshot fromJson (const juce::var&);
    const DeviceInfo* findDevice (const juce::String& id) const;
    /** Fingerprints in scan order, ordinals assigned (input of InstanceReconciler). */
    std::vector<DeviceFingerprint> fingerprints() const;
    int countInstances() const;              // third-party plug-in instances
};

juce::Array<ParamInfo> parseParameters (const juce::var& doc);

juce::var fingerprintToVar (const DeviceFingerprint&);
DeviceFingerprint fingerprintFromVar (const juce::var&);

} // namespace gc
