#pragma once
// BridgeInstaller: puts GLOBAL_CONTROL_BRIDGE.amxd (+ gc_bridge.js) into the Ableton User Library and
// checks the prerequisites. Never overwrites a user's file without explicit confirmation.
#include <juce_core/juce_core.h>

namespace gc {

struct BridgeEnvironment {
    juce::Array<juce::File> liveApps;      // /Applications/Ableton Live*.app
    bool maxForLiveFound = false;          // Max bundled inside Live (Suite) or a standalone Max
    juce::String maxForLiveDetail;
    juce::File userLibrary;                // ~/Music/Ableton/User Library (default location)
    bool userLibraryExists = false;
    juce::File targetDir;                  // .../Presets/MIDI Effects/Max MIDI Effect
    juce::File sourceDir;                  // where the app ships the bridge files
    bool sourceComplete = false;           // .amxd present in the app
    bool installed = false;                // .amxd present in the User Library
    bool upToDate = false;                 // installed files identical to the shipped ones
};

class BridgeInstaller {
public:
    static constexpr const char* kDeviceFile = "GLOBAL_CONTROL_BRIDGE.amxd";
    static constexpr const char* kScriptFile = "gc_bridge.js";

    static BridgeEnvironment inspect();

    enum class Outcome { Installed, AlreadyUpToDate, NeedsConfirmation, Failed };
    /** overwrite=false never replaces an existing, different file (returns NeedsConfirmation). */
    static Outcome install (const BridgeEnvironment&, bool overwrite, juce::String& message);

    static juce::File findSourceDir();
};

} // namespace gc
