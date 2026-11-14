#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Params.h"
#include "dsp/MicroEngine.h"
#include "dsp/LimeEngine.h"

class TwinboxProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    TwinboxProcessor();
    ~TwinboxProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ------------------------------------------------------------------ public API for the editor
    juce::AudioProcessorValueTreeState apvts;
    SampleLibrary library;
    MicroEngine micro;
    LimeEngine  lime { library };

    juce::String channelName (int ch) const { return microNames[(size_t) ch]; }
    void setChannelName (int ch, const juce::String& n) { microNames[(size_t) ch] = n; }

    // Morph (Section A): store the current drum patches as the Left / Right morph targets
    void storeMorph (bool right);
    bool hasMorph (bool right) const { return right ? morphHasR : morphHasL; }

    // Copy / paste of a drum patch (Section A)
    void copyChannel (int ch);
    void pasteChannel (int ch);

    // Legacy import from the user's own Limelite .ens (runs on a background thread)
    void importLegacy (const juce::File& ens, std::function<void (LegacyImporter::Result)> onDone,
                       std::function<void (float)> progress = {});

    float meter (int which) const { return meters[(size_t) which].load(); }   // 0 = A, 1 = B, 2 = Out
    bool isPlaying() const { return transportPlaying.load(); }
    bool hostIsPlaying() const { return hostPlaying.load(); }

    juce::ValueTree uiState { "UI" };   // editor persistent state (e.g. GLOBAL panel expanded)

private:
    void timerCallback() override;      // morph interpolation on the message thread
    void applyMorph (float m);

    std::array<juce::String, params::numMicroChannels> microNames;
    juce::ThreadPool importPool { 1 };

    // transport
    double sr = 44100.0;
    double internalPpq = 0.0;
    bool   wasPlaying = false;
    std::atomic<bool> transportPlaying { false }, hostPlaying { false };

    // global filter
    TptSvf gFilt[2];
    float  cutSm = 20000.0f;
    float  xfadeSm = 0.5f;
    juce::AudioBuffer<float> bufA, bufB;
    std::vector<TriggerEvent> midiA, midiB;
    std::array<std::atomic<float>, 3> meters {};

    // morph
    std::vector<float> morphL, morphR, clip;
    bool morphHasL = false, morphHasR = false, hasClip = false;
    float lastMorph = -1.0f;
    std::vector<juce::RangedAudioParameter*> morphParams;   // ordered [ch][suffix]

    std::atomic<float>* pXfade {}, *pCutoff {}, *pMode {}, *pRes {}, *pMaster {}, *pTempo {}, *pRun {}, *pMorph {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TwinboxProcessor)
};
