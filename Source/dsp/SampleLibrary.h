#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include <juce_data_structures/juce_data_structures.h>
#include <memory>
#include <vector>

// One audio sample in memory (mono or stereo, de-interleaved).
struct Sample
{
    juce::String name;
    double sourceRate = 44100.0;
    std::vector<float> L, R;              // R empty for mono
    int length() const noexcept { return (int) L.size(); }
    bool stereo() const noexcept { return ! R.empty(); }
};
using SamplePtr = std::shared_ptr<const Sample>;

// All samples of one folder, as seen by a sampler module's WAV selector.
struct SampleSet
{
    juce::File folder;
    std::vector<SamplePtr> samples;      // main samples (Hat: closed)
    std::vector<SamplePtr> openPartner;  // Hat only: "open/<same name>" partner or nullptr
    juce::StringArray names;
    int size() const noexcept { return (int) samples.size(); }
};
using SampleSetPtr = std::shared_ptr<const SampleSet>;

// -------------------------------------------------------------------------------------------------
//  Folder based sample management for the Section B samplers.
//
//  <root>/Twinbox Kick, Twinbox Snare, Twinbox Hat, Twinbox Tie, Twinbox Pod   (factory, installed on first run)
//  <root>/Legacy Kick,  Legacy Snare,  Legacy Hat,  Legacy Tie,  Legacy Pod     (filled by the Limelite importer)
//  or any user folder per module.
//  <root> = macOS /Users/Shared/Twinbox/Samples, Windows C:\Users\Public\Documents\Twinbox\Samples,
//           Linux ~/Documents/Twinbox/Samples
// -------------------------------------------------------------------------------------------------
class SampleLibrary : public juce::ChangeBroadcaster
{
public:
    enum Module { kick = 0, snare, hat, tie, pod, numModules };
    enum Source { twinbox = 0, legacy, user, numSources };

    SampleLibrary();
    ~SampleLibrary() override;

    static juce::File rootFolder();
    static juce::String moduleName (int m);
    static juce::File folderFor (int module, Source s);
    static bool hasLegacy();
    static juce::StringArray namesIn (const juce::File& folder);   // same order as the WAV selector

    void installFactoryIfMissing();       // extracts the embedded factory archive (first run)

    // audio-thread safe accessor
    SampleSetPtr set (int module) const { return std::atomic_load (&sets[(size_t) module]); }

    Source source (int module) const { return sources[(size_t) module]; }
    juce::File userFolder (int module) const { return userFolders[(size_t) module]; }
    juce::File currentFolder (int module) const;
    void setSource (int module, Source s);                 // reloads asynchronously
    void setUserFolder (int module, const juce::File& f);  // also switches source to "user"
    void reloadAll();
    bool isLoading() const { return pending.load() > 0; }

    int indexOf (int module, const juce::String& sampleName) const;

    juce::ValueTree saveState() const;
    void loadState (const juce::ValueTree& vt);

private:
    void reload (int module);
    static SamplePtr loadFile (juce::AudioFormatManager& fm, const juce::File& f);

    std::array<SampleSetPtr, numModules> sets;
    std::array<Source, numModules> sources {};
    std::array<juce::File, numModules> userFolders;
    std::array<int, numModules> generation {};
    std::atomic<int> pending { 0 };
    juce::ThreadPool pool { 1 };
    juce::CriticalSection lock;
};

// -------------------------------------------------------------------------------------------------
//  Imports the original sample maps from a user's own "Limelite 1.3.ens" into the Legacy folders.
//  Nothing from the original ensemble is shipped with Twinbox.
// -------------------------------------------------------------------------------------------------
struct LegacyImporter
{
    struct Result { bool ok = false; int written = 0; juce::String message; };
    static Result run (const juce::File& ensFile, std::function<void (float)> progress = {});
};
