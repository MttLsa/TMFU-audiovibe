#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace params;

namespace
{
    // MIDI note map (one section at a time, chosen with the MIDI A / MIDI B buttons)
    //   Section A: 36..43 -> channels 1..8 (repeats every 8 keys)
    //   Section B: 36..41 -> Kick, Snare, Hat closed, Hat open, Tie, Pod (repeats every 6 keys)
    constexpr int midiNoteBase = 36;
}

TwinboxProcessor::TwinboxProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TWINBOX", params::createLayout())
{
    mod.init (apvts);
    micro.bind (mod);
    lime.bind (mod);
    library.installFactoryIfMissing();
    library.reloadAll();

    static const char* names[] { "Kick", "Snare", "CH", "OH", "Clap", "Tom", "Rim", "Bell" };
    for (int i = 0; i < numMicroChannels; ++i) microNames[(size_t) i] = names[i];

    pXfade = apvts.getRawParameterValue (gXfade);
    pCutoff = apvts.getRawParameterValue (gCutoff);
    pMode = apvts.getRawParameterValue (gMode);
    pRes = apvts.getRawParameterValue (gRes);
    pMaster = apvts.getRawParameterValue (gMaster);
    pTempo = apvts.getRawParameterValue (gTempo);
    pRun = apvts.getRawParameterValue (gRun);
    pMorph = apvts.getRawParameterValue (aMorph);
    pNormA = apvts.getRawParameterValue (aNorm);
    pNormB = apvts.getRawParameterValue (bNorm);
    pMidiTarget = apvts.getRawParameterValue (gMidiTarget);

    for (int ch = 0; ch < numMicroChannels; ++ch)
        for (auto* s : mt::patchSuffixes())
            morphParams.push_back (apvts.getParameter (a (ch, s)));

    uiState.setProperty ("globalExpanded", true, nullptr);
    startTimerHz (30);
}

TwinboxProcessor::~TwinboxProcessor() { stopTimer(); }

bool TwinboxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void TwinboxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    micro.prepare (sampleRate);
    lime.prepare (sampleRate, samplesPerBlock);
    bufA.setSize (2, samplesPerBlock);
    bufB.setSize (2, samplesPerBlock);
    gFilt[0].reset(); gFilt[1].reset();
    normA.prepare (sampleRate); normB.prepare (sampleRate); safety.prepare (sampleRate);
    midiA.reserve (256); midiB.reserve (256);
    wasPlaying = false;
}

void TwinboxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (bufA.getNumSamples() < n) { bufA.setSize (2, n, false, false, true); bufB.setSize (2, n, false, false, true); }
    bufA.clear(); bufB.clear();

    // ------------------------------------------------------------ transport
    BlockTime bt;
    bt.numSamples = n;
    bt.bpm = pTempo->load();
    bool hostPlay = false;
    double hostPpq = 0.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bt.bpm = *b;
            hostPlay = pos->getIsPlaying();
            if (auto p = pos->getPpqPosition()) hostPpq = *p;
        }
    hostPlaying.store (hostPlay);
    bt.ppqPerSample = bt.bpm / (60.0 * sr);

    if (hostPlay)
    {
        bt.playing = true;
        bt.ppqStart = hostPpq;
    }
    else if (pRun->load() > 0.5f)
    {
        if (! wasPlaying) internalPpq = 0.0;
        bt.playing = true;
        bt.ppqStart = internalPpq;
        internalPpq += bt.ppqPerSample * n;
    }
    bt.justStarted = bt.playing && ! wasPlaying;
    wasPlaying = bt.playing;
    transportPlaying.store (bt.playing);

    // ------------------------------------------------------------ LFOs -> modulated parameters for both engines
    mod.process (bt, sr, n);

    // ------------------------------------------------------------ MIDI
    midiA.clear(); midiB.clear();
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (! m.isNoteOn()) continue;
        const int note = m.getNoteNumber();
        const float vel = m.getFloatVelocity();
        // the MIDI keyboard / pads play ONE section at a time (MIDI A / MIDI B buttons): from C1 (note 36) upwards,
        // one sound per key; the row repeats every 8 (A) / 6 (B) keys so a 16-pad grid (Push, MPC...) covers it twice
        if (note < midiNoteBase) continue;
        const int idx = note - midiNoteBase;
        if (pMidiTarget->load() < 0.5f)
            midiA.push_back ({ meta.samplePosition, idx % numMicroChannels, vel });
        else
            midiB.push_back ({ meta.samplePosition, idx % LimeEngine::numTargets, vel });
    }

    // ------------------------------------------------------------ engines
    micro.process (bufA.getWritePointer (0), bufA.getWritePointer (1), n, bt, midiA);
    lime.process  (bufB.getWritePointer (0), bufB.getWritePointer (1), n, bt, midiB);
    normA.process (bufA.getWritePointer (0), bufA.getWritePointer (1), n, pNormA->load() > 0.5f);
    normB.process (bufB.getWritePointer (0), bufB.getWritePointer (1), n, pNormB->load() > 0.5f);

    // ------------------------------------------------------------ A/B mix -> cutoff filter -> master
    const float xTarget = pXfade->load() * 0.01f;
    const float cutTarget = pCutoff->load();
    const int mode = (int) pMode->load();                    // 0 HP, 1 BP, 2 LP
    const int svfMode = mode == 0 ? 2 : (mode == 1 ? 1 : 0);
    const double q = 0.5 + std::pow (pRes->load() * 0.01, 1.5) * 14.5;
    const float master = juce::Decibels::decibelsToGain (pMaster->load(), -60.0f);
    const float bpMakeup = svfMode == 1 ? (float) juce::jmax (1.0, std::sqrt (q)) : 1.0f;   // keep BP loudness usable at high Q
    const bool bypassFilter = (mode == 2 && cutTarget >= 19900.0f && pRes->load() < 0.5f)
                           || (mode == 0 && cutTarget <= 20.5f && pRes->load() < 0.5f);

    auto* aL = bufA.getReadPointer (0); auto* aR = bufA.getReadPointer (1);
    auto* bL = bufB.getReadPointer (0); auto* bR = bufB.getReadPointer (1);
    auto* oL = buffer.getWritePointer (0); auto* oR = buffer.getWritePointer (1);
    float pkA = 0, pkB = 0, pkO = 0;

    for (int i = 0; i < n; ++i)
    {
        xfadeSm += 0.002f * (xTarget - xfadeSm);
        cutSm += 0.004f * (cutTarget - cutSm);
        if ((i & 15) == 0)
        {
            gFilt[0].set (cutSm, sr, q);
            gFilt[1].g = gFilt[0].g; gFilt[1].k = gFilt[0].k;
            gFilt[1].a1 = gFilt[0].a1; gFilt[1].a2 = gFilt[0].a2; gFilt[1].a3 = gFilt[0].a3;
        }
        // linear crossfade: 0 % = only A, 50 % = 50 % A + 50 % B, 100 % = only B
        float l = aL[i] * (1.0f - xfadeSm) + bL[i] * xfadeSm;
        float r = aR[i] * (1.0f - xfadeSm) + bR[i] * xfadeSm;
        if (! bypassFilter)
        {
            l = gFilt[0].process (l, svfMode);
            r = gFilt[1].process (r, svfMode);
            l *= bpMakeup; r *= bpMakeup;
        }
        l *= master; r *= master;
        oL[i] = l; oR[i] = r;
    }
    safety.process (oL, oR, n);                              // the output never clips (ceiling -0.3 dBFS)
    for (int i = 0; i < n; ++i)
    {
        const float l = oL[i], r = oR[i];
        pkA = juce::jmax (pkA, std::abs (aL[i]), std::abs (aR[i]));
        pkB = juce::jmax (pkB, std::abs (bL[i]), std::abs (bR[i]));
        pkO = juce::jmax (pkO, std::abs (l), std::abs (r));
    }
    auto decayMeter = [] (std::atomic<float>& m, float pk) { m.store (juce::jmax (pk, m.load() * 0.92f)); };
    decayMeter (meters[0], pkA);
    decayMeter (meters[1], pkB);
    decayMeter (meters[2], pkO);
    midi.clear();
}

//==============================================================================
//  Morph / copy-paste (message thread)
//==============================================================================
void TwinboxProcessor::storeMorph (bool right)
{
    auto& dst = right ? morphR : morphL;
    dst.clear();
    for (auto* p : morphParams) dst.push_back (p->getValue());
    (right ? morphHasR : morphHasL) = true;
    lastMorph = -1.0f;
}

void TwinboxProcessor::applyMorph (float m)
{
    if (! (morphHasL && morphHasR)) return;
    for (size_t i = 0; i < morphParams.size(); ++i)
    {
        auto* p = morphParams[i];
        float v = morphL[i] + (morphR[i] - morphL[i]) * m;
        if (dynamic_cast<juce::AudioParameterChoice*> (p) != nullptr || dynamic_cast<juce::AudioParameterBool*> (p) != nullptr)
            v = m < 0.5f ? morphL[i] : morphR[i];
        if (std::abs (p->getValue() - v) > 1.0e-6f)
            p->setValueNotifyingHost (v);
    }
}

void TwinboxProcessor::timerCallback()
{
    const float m = pMorph->load();
    if (std::abs (m - lastMorph) > 1.0e-4f)
    {
        if (lastMorph >= 0.0f) applyMorph (m);
        lastMorph = m;
    }
}

void TwinboxProcessor::copyChannel (int ch)
{
    const auto& sfx = mt::patchSuffixes();
    clip.clear();
    for (auto* s : sfx) clip.push_back (apvts.getParameter (a (ch, s))->getValue());
    hasClip = true;
}

void TwinboxProcessor::pasteChannel (int ch)
{
    if (! hasClip) return;
    const auto& sfx = mt::patchSuffixes();
    for (size_t i = 0; i < sfx.size(); ++i)
    {
        auto* p = apvts.getParameter (a (ch, sfx[i]));
        p->beginChangeGesture();
        p->setValueNotifyingHost (clip[i]);
        p->endChangeGesture();
    }
}

void TwinboxProcessor::importLegacy (const juce::File& ens, std::function<void (LegacyImporter::Result)> onDone,
                                     std::function<void (float)> progress)
{
    importPool.addJob ([this, ens, onDone, progress]
    {
        auto r = LegacyImporter::run (ens, progress);
        juce::MessageManager::callAsync ([this, r, onDone]
        {
            for (int m = 0; m < SampleLibrary::numModules; ++m)
                if (library.source (m) == SampleLibrary::legacy) library.setSource (m, SampleLibrary::legacy);
            if (onDone) onDone (r);
        });
    });
}

//==============================================================================
//  State
//==============================================================================
void TwinboxProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("TWINBOX_STATE");
    root.appendChild (apvts.copyState(), nullptr);
    root.appendChild (micro.saveState(), nullptr);
    root.appendChild (lime.saveState(), nullptr);
    root.appendChild (library.saveState(), nullptr);

    juce::ValueTree names ("NAMES");
    for (int i = 0; i < numMicroChannels; ++i) names.setProperty ("n" + juce::String (i), microNames[(size_t) i], nullptr);
    root.appendChild (names, nullptr);

    juce::ValueTree morph ("MORPH");
    auto toStr = [] (const std::vector<float>& v) { juce::StringArray s; for (auto x : v) s.add (juce::String (x, 5)); return s.joinIntoString (","); };
    if (morphHasL) morph.setProperty ("L", toStr (morphL), nullptr);
    if (morphHasR) morph.setProperty ("R", toStr (morphR), nullptr);
    root.appendChild (morph, nullptr);
    root.appendChild (uiState.createCopy(), nullptr);

    if (auto xml = root.createXml()) copyXmlToBinary (*xml, destData);
}

void TwinboxProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("TWINBOX_STATE")) return;

    if (auto s = root.getChildWithName (apvts.state.getType()); s.isValid()) apvts.replaceState (s);
    micro.loadState (root.getChildWithName ("MICRO_PATTERNS"));
    lime.loadState (root.getChildWithName ("LIME_PATTERNS"));
    library.loadState (root.getChildWithName ("SAMPLE_FOLDERS"));

    if (auto names = root.getChildWithName ("NAMES"); names.isValid())
        for (int i = 0; i < numMicroChannels; ++i)
            microNames[(size_t) i] = names.getProperty ("n" + juce::String (i), microNames[(size_t) i]).toString();

    auto fromStr = [this] (const juce::String& s, std::vector<float>& v)
    {
        juce::StringArray t; t.addTokens (s, ",", {});
        if (t.size() != (int) morphParams.size()) return false;
        v.clear(); for (auto& x : t) v.push_back (x.getFloatValue());
        return true;
    };
    if (auto m = root.getChildWithName ("MORPH"); m.isValid())
    {
        morphHasL = fromStr (m.getProperty ("L").toString(), morphL);
        morphHasR = fromStr (m.getProperty ("R").toString(), morphR);
    }
    lastMorph = pMorph->load();
    if (auto ui = root.getChildWithName ("UI"); ui.isValid()) uiState.copyPropertiesFrom (ui, nullptr);
}

juce::AudioProcessorEditor* TwinboxProcessor::createEditor() { return new TwinboxEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TwinboxProcessor(); }
