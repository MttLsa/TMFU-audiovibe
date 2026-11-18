#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace params;

TwinsynthProcessor::TwinsynthProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TWINSYNTH", createLayout())
{
    for (int i = 0; i < NUM; ++i) raw[(size_t) i] = apvts.getRawParameterValue (def (i).id);
    // start on a house classic (the library is dance-oriented)
    const auto& all = PresetLibrary::get().all();
    int start = 0;
    for (int i = 0; i < (int) all.size(); ++i) if (all[(size_t) i].name == "Organ 2") start = i;
    if (! all.empty()) loadPreset (start);
}

bool TwinsynthProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void TwinsynthProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock);
    chorus.prepare (sampleRate);
    delay.prepare (sampleRate);
    reverb.setSampleRate (sampleRate);
    reverb.reset();
    limiter.prepare (sampleRate);
    revBuf.setSize (2, samplesPerBlock);
    keyboard.reset();
}

const juce::String TwinsynthProcessor::getProgramName (int i)
{
    const auto& all = PresetLibrary::get().all();
    return juce::isPositiveAndBelow (i, (int) all.size()) ? all[(size_t) i].name : juce::String ("Init");
}

void TwinsynthProcessor::loadPreset (int index)
{
    const auto& all = PresetLibrary::get().all();
    if (! juce::isPositiveAndBelow (index, (int) all.size())) return;
    const auto& pr = all[(size_t) index];
    for (int i = 0; i < NUM; ++i)
    {
        auto* prm = apvts.getParameter (def (i).id);
        float value = def (i).def;
        if (const auto* var = pr.values.getVarPointer (def (i).id))
        {
            if (var->isString())
            {
                const int c = choicesOf (i).indexOf (var->toString(), true);
                if (c >= 0) value = (float) c;
            }
            else value = (float) (double) *var;
        }
        if (juce::String (def (i).id) == "g_master") continue;   // the master volume is the user's
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
    }
    currentPreset.store (index);
    resetPending.store (true);
    if (onPresetChanged) onPresetChanged();
}

void TwinsynthProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    keyboard.processNextMidiBuffer (midi, 0, n, true);
    for (int i = 0; i < NUM; ++i) snap[i] = raw[(size_t) i]->load();
    if (resetPending.exchange (false)) engine.allNotesOff();

    buffer.clear();
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    engine.render (L, R, n, midi, snap);

    // drive
    if (snap[gDrive] > 0.001f)
    {
        const float k = 1.0f + snap[gDrive] * 9.0f, comp = 1.0f / std::pow (k, 0.55f);
        for (int i = 0; i < n; ++i) { L[i] = dsp::fastTanh (L[i] * k) * comp * 1.2f; R[i] = dsp::fastTanh (R[i] * k) * comp * 1.2f; }
    }
    chorus.process (L, R, n, (int) snap[gChoMode], snap[gChoMix]);

    // tempo-synced delay
    double bpm = snap[gBpm];
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm = *b;
    static const float divBeats[] = { 0.25f, 1.0f / 3.0f, 0.5f, 0.75f, 2.0f / 3.0f, 1.0f, 1.5f, 2.0f };
    const float secs = (float) (60.0 / juce::jmax (30.0, bpm)) * divBeats[juce::jlimit (0, 7, (int) snap[gDlyDiv])];
    delay.process (L, R, n, secs, snap[gDlyFb], snap[gDlyMix], snap[gDlyPing] > 0.5f);

    if (snap[gRevMix] > 0.001f)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.3f + 0.68f * snap[gRevSize]; rp.damping = snap[gRevDamp]; rp.wetLevel = 1.0f; rp.dryLevel = 0.0f; rp.width = 1.0f;
        reverb.setParameters (rp);
        revBuf.setSize (2, n, false, false, true);
        revBuf.copyFrom (0, 0, L, n); revBuf.copyFrom (1, 0, R, n);
        reverb.processStereo (revBuf.getWritePointer (0), revBuf.getWritePointer (1), n);
        const float wet = snap[gRevMix] * 0.5f, dry = 1.0f - 0.3f * snap[gRevMix];
        for (int i = 0; i < n; ++i) { L[i] = L[i] * dry + revBuf.getSample (0, i) * wet; R[i] = R[i] * dry + revBuf.getSample (1, i) * wet; }
    }

    const float target = juce::Decibels::decibelsToGain (snap[gMaster] + snap[gTrim]);
    float pk = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        gainSm += (target - gainSm) * 0.002f;
        L[i] *= gainSm; R[i] *= gainSm;
        pk = juce::jmax (pk, std::abs (L[i]), std::abs (R[i]));
    }
    limiter.process (L, R, n);
    outMeter.store (juce::jmax (pk, outMeter.load() * 0.9f));
}

juce::AudioProcessorEditor* TwinsynthProcessor::createEditor() { return new TwinsynthEditor (*this); }

void TwinsynthProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void TwinsynthProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            currentPreset.store ((int) vt.getProperty ("preset", -1));
            apvts.replaceState (vt);
            if (onPresetChanged) onPresetChanged();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TwinsynthProcessor(); }
