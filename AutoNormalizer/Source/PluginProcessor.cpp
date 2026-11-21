#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout AutoNormalizerProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto dB = AudioParameterFloatAttributes().withLabel ("dB");
    auto sec = AudioParameterFloatAttributes().withLabel ("s");

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "target", 2 }, "Target Loudness",
                                                        NormalisableRange<float> (-40.0f, -5.0f, 0.1f), -14.0f,
                                                        AudioParameterFloatAttributes().withLabel ("LUFS")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "measure", 1 }, "Measure Time",
                                                        NormalisableRange<float> (0.4f, 10.0f, 0.1f, 0.5f), 2.0f, sec));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "gate", 1 }, "Signal Gate",
                                                        NormalisableRange<float> (-90.0f, -20.0f, 1.0f), -50.0f, dB));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "auto", 1 }, "Auto Detect Preset Change", true));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "sens", 1 }, "Detect Sensitivity",
                                                        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f,
                                                        AudioParameterFloatAttributes().withLabel ("%")));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "midipc", 1 }, "Trigger On MIDI Program Change", true));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "maxboost", 1 }, "Max Boost",
                                                        NormalisableRange<float> (0.0f, 48.0f, 0.5f), 24.0f, dB));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "safety", 1 }, "Safe Gain While Measuring", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "freeze", 1 }, "Freeze Gain", false));
    // Automatable/MIDI-learnable trigger: every off->on transition starts a new normalization.
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "trigger", 1 }, "Normalize Now", false));

    return { p.begin(), p.end() };
}

AutoNormalizerProcessor::AutoNormalizerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AutoNormalizer", createLayout())
{
    pTarget = apvts.getRawParameterValue ("target");
    pMeasure = apvts.getRawParameterValue ("measure");
    pGate = apvts.getRawParameterValue ("gate");
    pAuto = apvts.getRawParameterValue ("auto");
    pSens = apvts.getRawParameterValue ("sens");
    pMidiPC = apvts.getRawParameterValue ("midipc");
    pMaxBoost = apvts.getRawParameterValue ("maxboost");
    pSafety = apvts.getRawParameterValue ("safety");
    pFreeze = apvts.getRawParameterValue ("freeze");
    pTrigger = apvts.getRawParameterValue ("trigger");
}

bool AutoNormalizerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

an::Settings AutoNormalizerProcessor::readSettings() const
{
    an::Settings s;
    s.targetLufs = pTarget->load();
    s.measureSeconds = pMeasure->load();
    s.gateDb = pGate->load();
    s.autoDetect = pAuto->load() > 0.5f;
    s.sensitivity = pSens->load() / 100.0f;
    s.maxBoostDb = pMaxBoost->load();
    s.safetyCeiling = pSafety->load() > 0.5f;
    s.frozen = pFreeze->load() > 0.5f;
    return s;
}

void AutoNormalizerProcessor::prepareToPlay (double sampleRate, int)
{
    engine.setSettings (readSettings());
    engine.prepare (sampleRate, juce::jmax (1, getTotalNumInputChannels()));
    setLatencySamples (engine.getLatencySamples()); // limiter look-ahead (1.5 ms)
}

void AutoNormalizerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    engine.setSettings (readSettings());

    // Trigger 1: MIDI Program Change (the same message that switches the preset of the previous plugin)
    if (pMidiPC->load() > 0.5f)
        for (const auto meta : midi)
            if (meta.getMessage().isProgramChange())
            {
                engine.requestNormalize (an::TriggerSource::Midi);
                break;
            }

    // Trigger 2: "Normalize Now" parameter (host automation / MIDI learn), rising edge
    const bool trig = pTrigger->load() > 0.5f;
    if (trig && ! lastTriggerParam)
        engine.requestNormalize (an::TriggerSource::Manual);
    lastTriggerParam = trig;

    // Trigger 3 (automatic detection) runs inside the engine.
    engine.process (buffer.getArrayOfWritePointers(), getTotalNumOutputChannels(), buffer.getNumSamples());
}

void AutoNormalizerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    const auto snap = engine.getSnapshot();
    juce::ValueTree eng ("Engine");
    eng.setProperty ("normalizedOnce", snap.normalizedOnce, nullptr);
    eng.setProperty ("gainDb", snap.gainDb, nullptr);
    eng.setProperty ("hasRef", snap.hasReference, nullptr);
    eng.setProperty ("refLevel", snap.refLevelDb, nullptr);
    juce::StringArray shape;
    for (float v : snap.refShape)
        shape.add (juce::String (v, 4));
    eng.setProperty ("refShape", shape.joinIntoString (" "), nullptr);

    state.removeChild (state.getChildWithName ("Engine"), nullptr);
    state.appendChild (eng, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AutoNormalizerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    const auto eng = state.getChildWithName ("Engine");
    state.removeChild (eng, nullptr);
    apvts.replaceState (state);

    if (eng.isValid())
    {
        an::NormalizerEngine::Snapshot snap;
        snap.normalizedOnce = (bool) eng.getProperty ("normalizedOnce", false);
        snap.gainDb = (float) eng.getProperty ("gainDb", 0.0f);
        snap.hasReference = (bool) eng.getProperty ("hasRef", false);
        snap.refLevelDb = (float) eng.getProperty ("refLevel", -100.0f);
        auto tokens = juce::StringArray::fromTokens (eng.getProperty ("refShape").toString(), " ", "");
        for (int b = 0; b < an::NormalizerEngine::kNumBands && b < tokens.size(); ++b)
            snap.refShape[(size_t) b] = tokens[b].getFloatValue();
        engine.restoreSnapshot (snap);
    }

    lastTriggerParam = pTrigger->load() > 0.5f; // restoring a session must not start a normalization
}

juce::AudioProcessorEditor* AutoNormalizerProcessor::createEditor()
{
    return new AutoNormalizerEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AutoNormalizerProcessor();
}
