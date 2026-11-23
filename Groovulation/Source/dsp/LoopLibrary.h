#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include "Analysis.h"
#include "../Params.h"
#include <atomic>
#include <memory>
#include <set>

// One analysed file of the database
struct LoopEntry
{
    juce::String path, name;
    juce::int64 size = 0, mtime = 0;
    analysis::Result info;          // (features etc.)
    std::vector<float> vec;         // normalised + weighted descriptor (similarity space)
    int family = 0;                 // cluster id inside its type ("sonorità e carattere simile")
};

struct LoopDatabase
{
    std::vector<LoopEntry> entries;
    std::array<std::vector<int>, lb::numModules> byType;   // indices sorted by name
    int indexOf (const juce::String& path) const;
};
using DbPtr = std::shared_ptr<const LoopDatabase>;

// A loop loaded for playback (resampled to the host rate, always 2 channels)
struct LoopData
{
    juce::String path, name;
    int type = 0;
    juce::AudioBuffer<float> audio;
    double sampleRate = 44100.0;
    double bpm = 120.0;
    int beats = 4;
    std::vector<float> onsets;      // fractions of the loop
    std::vector<float> overview;    // waveform overview (peak per bin, 0..1), 1024 bins
    analysis::Result info;          // key, tuning, chroma, f0 ...
    int numSamples() const { return audio.getNumSamples(); }
};
using LoopPtr = std::shared_ptr<const LoopData>;

// -------------------------------------------------------------------------------------------------
//  Folders:  <root>/Loops/<01 Kick ... 09 Melodic 03 Synth>   factory loops (installed on first run)
//            <root>/PRINT/<snapshot>/                           printed loops
//            <root>/Snapshots/                                  saved snapshots
//            <root>/Database/                                   loop database + "similar loops" registry
//  <root> = macOS /Users/Shared/Groovulation, Linux ~/Documents/Groovulation
//  Shared by all plug-in instances (juce::SharedResourcePointer).
// -------------------------------------------------------------------------------------------------
class LoopLibrary : public juce::ChangeBroadcaster, private juce::Thread
{
public:
    LoopLibrary();
    ~LoopLibrary() override;

    static juce::File rootFolder();
    static juce::File factoryFolder() { return rootFolder().getChildFile ("Loops"); }
    static juce::File printFolder()   { return rootFolder().getChildFile ("PRINT"); }
    static juce::File snapshotFolder(){ return rootFolder().getChildFile ("Snapshots"); }
    static juce::File databaseFolder(){ return rootFolder().getChildFile ("Database"); }

    DbPtr db() const { return std::atomic_load (&database); }

    // custom folders (persisted in <root>/settings.xml, shared by all instances)
    juce::StringArray userFolders() const;
    void addUserFolder (const juce::File& f);
    void removeUserFolder (const juce::String& path);

    void startScan (bool fullRescan = false);
    bool isScanning() const { return scanning.load(); }
    float scanProgress() const { return progress.load(); }
    juce::String scanStatus() const { const juce::ScopedLock sl (statusLock); return status; }

    // loops of a module type that fit (1 .. 32 bars at the project tempo)
    std::vector<int> candidates (const LoopDatabase& d, int type, double hostBpm) const;
    // nearest loop of the same type in the similarity space, excluding 'exclude'
    static int nearest (const LoopDatabase& d, int index, const std::set<int>& exclude, const std::vector<int>& pool);

    // audio classifier for loops whose names say nothing: k-nearest-neighbours over the loops that ARE named
    // (z-scored descriptors), so it learns from the factory library and from every named user loop.
    static int classifyByNeighbours (const std::vector<LoopEntry>& labelled, const analysis::Result& r, int k = 3);

    // reads + resamples a loop for playback (any thread)
    static LoopPtr load (const LoopEntry& e, double targetRate);

    // the plug-in's own short file name parser etc.
    juce::AudioFormatManager& formats() { return fm; }

    void installFactoryIfMissing();

private:
    void run() override;
    void finalise (LoopDatabase& d) const;
    void saveDatabase (const LoopDatabase& d) const;
    DbPtr loadDatabaseFile() const;

    DbPtr database;
    juce::AudioFormatManager fm;
    std::atomic<bool> scanning { false }, rescanAll { false };
    std::atomic<float> progress { 0.0f };
    juce::CriticalSection statusLock;
    juce::String status;
    std::unique_ptr<juce::PropertiesFile> settings;
    mutable juce::CriticalSection settingsLock;
};
