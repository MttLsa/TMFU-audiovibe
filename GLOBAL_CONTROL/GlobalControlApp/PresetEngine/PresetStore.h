#pragma once
// PresetStore: versioned JSON presets on disk.
//   <root>/Global/<name>.gcpreset                 global presets (mapping templates, any project)
//   <root>/Projects/<projectId>/<name>.gcpreset   project presets (bound to one Live Set)
//   <root>/Sessions/<projectId>.json              automatic session state, restored on reconnection
// Default root: ~/Library/Application Support/GLOBAL CONTROL (macOS).
#include <juce_core/juce_core.h>

namespace gc {

constexpr const char* kPresetFormat = "GLOBAL_CONTROL_PRESET";
constexpr int kPresetVersion = 1;

enum class PresetScope { Global, Project };

struct PresetInfo {
    juce::String name;
    PresetScope scope = PresetScope::Global;
    juce::File file;
    juce::Time modified;
};

class PresetStore {
public:
    explicit PresetStore (juce::File root = defaultRoot());
    static juce::File defaultRoot();

    juce::Array<PresetInfo> list (PresetScope, const juce::String& projectId) const;
    juce::Result save (PresetScope, const juce::String& projectId, const juce::String& name, const juce::var& data, bool overwrite);
    juce::Result load (const PresetInfo&, juce::var& out) const;
    juce::Result rename (const PresetInfo&, const juce::String& newName);
    juce::Result remove (const PresetInfo&);
    juce::Result duplicate (const PresetInfo&, juce::String& newNameOut);
    bool exists (PresetScope, const juce::String& projectId, const juce::String& name) const;

    void saveSession (const juce::String& projectId, const juce::var& data);
    juce::var loadSession (const juce::String& projectId) const;

    /** Validates format/version; migrates older versions when needed. */
    static juce::Result validate (const juce::var&);
    static juce::String sanitize (const juce::String& name);

    const juce::File& getRoot() const noexcept { return root; }

private:
    juce::File dirFor (PresetScope, const juce::String& projectId) const;
    juce::File fileFor (PresetScope, const juce::String& projectId, const juce::String& name) const;
    juce::File root;
};

} // namespace gc
