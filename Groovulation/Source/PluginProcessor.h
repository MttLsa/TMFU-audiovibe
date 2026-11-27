#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Params.h"
#include "dsp/LoopLibrary.h"
#include "dsp/ModuleEngine.h"
#include "dsp/Fx.h"

class GroovulationProcessor : public juce::AudioProcessor, private juce::ChangeListener, private juce::Timer
{
public:
    GroovulationProcessor();
    ~GroovulationProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Groovulation"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::UndoManager undoManager { 30000, 60 };   // UNDO / REDO of every parameter (one step per gesture)
    juce::AudioProcessorValueTreeState apvts;
    juce::SharedResourcePointer<LoopLibrary> library;
    juce::ValueTree uiState { "UI" };

    // ---------------- loop selection (message thread)
    void selectLoop (int module, int dbIndex);
    void stepLoop (int module, int direction);     // LOOP SELECT ◀ ▶
    void randomLoop (int module);
    void similarLoop (int module);                 // SIMILAR SELECT
    void randomAll();
    LoopPtr currentLoop (int module) const { return std::atomic_load (&shown[(size_t) module]); }
    juce::String loopPath (int module) const { return paths[(size_t) module]; }
    std::vector<int> candidatesFor (int module) const;

    // ---------------- volume automation (message thread)
    VolEnvelope volEnvelope (int module) const { return volPoints[(size_t) module]; }
    void setVolEnvelope (int module, const VolEnvelope& e);

    // ---------------- scale info for the UI
    float appliedSemitones (int module) const { return applied[(size_t) module].load(); }
    float moduleLevel (int module) const { return engines[(size_t) module].level.load(); }
    int reseeks (int module) const { return engines[(size_t) module].reseeks.load(); }
    float modulePlayPos (int module) const { return engines[(size_t) module].playPos.load(); }
    float masterLevel() const { return masterPeak.load(); }
    double currentBpm() const { return bpmNow.load(); }
    bool isTransportRunning() const { return playingNow.load(); }

    // ---------------- CUSTOM module (floppy / drag & drop)
    void loadCustomFile (const juce::File& f);
    void clearCustom();
    void requestResync() { resync = true; }

    // ---------------- snapshot / print / export
    juce::String snapshotName() const { return snapName; }
    void setSnapshotName (const juce::String& n);
    bool saveSnapshot (const juce::String& name);
    bool loadSnapshot (const juce::File& f);
    struct PrintResult { bool ok = false; juce::Array<juce::File> files; juce::File folder; juce::String message; };
    // renders every loaded module exactly as it leaves the module (1 loop cycle, tails wrapped) into
    // <root>/PRINT/<snapshot>/<Module> - <snapshot>.wav  (call from a background thread)
    PrintResult printModules (std::function<void (float)> progress = {});
    // EXPORT: copies the original (unprocessed) files: <Module> - <snapshot> (original).<ext>
    PrintResult exportOriginals();
    std::atomic<bool> printing { false };

    fx::Deck deck;   // tape / vinyl (UI talks to it directly via atomics)

private:
    static BusesProperties makeBuses();
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void loadAsync (int module, const LoopEntry& e);
    void loadPath (int module, const juce::String& path);
    void updatePools();
    ModuleParams readParams (int module) const;
    float computeSemitones (int module, const LoopData* l, const ModuleParams& p, bool raw) const;

    std::array<ModuleEngine, lb::numModules> engines;
    std::array<LoopPtr, lb::numModules> pending, shown;
    std::array<juce::String, lb::numModules> paths;
    std::array<VolEnvelope, lb::numModules> volPoints;
    std::array<VolPtr, lb::numModules> volShared;
    std::array<std::set<int>, lb::numModules> similarVisited;
    std::array<int, lb::numModules> similarAnchor;
    std::array<std::atomic<float>, lb::numModules> applied;
    std::array<int, lb::numModules> loadGen {};
    juce::ThreadPool loader { 2 };
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);

    double sr = 44100.0;
    int maxBlock = 512;
    double internalPpq = 0.0, expectedPpq = -1.0;
    bool wasPlaying = false;
    std::atomic<double> bpmNow { 120.0 };
    std::atomic<bool> playingNow { false };
    std::atomic<float> masterPeak { 0.0f };
    std::atomic<bool> resync { false };
    int lastDeckType = 0;
    bool started = false;
    std::array<juce::String, lb::numModules> poolKey;
    std::array<std::atomic<bool>, lb::numModules> clearRequest {};
    juce::String snapName { "Untitled" };
    bool stateRestored = false;

    juce::AudioBuffer<float> modBuf;
    fx::TriFilter masterFilter;
    fx::BeatRepeat beatRepeat;

    std::array<std::array<std::atomic<float>*, 64>, lb::numModules> mp {};   // raw parameter pointers per module
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GroovulationProcessor)
};
