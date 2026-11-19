#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace prm;

MicrovibeProcessor::MicrovibeProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "MICROVIBE", createLayout())
{
    raw.resize ((size_t) count());
    base.resize ((size_t) count());
    for (int i = 0; i < count(); ++i) raw[(size_t) i] = apvts.getRawParameterValue (def (i).id);
    const auto& all = PresetLibrary::get().all();
    if (! all.empty()) loadPreset (0);
}

bool MicrovibeProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    const auto in = l.getMainInputChannelSet();
    if (in != juce::AudioChannelSet::stereo() && in != juce::AudioChannelSet::mono() && ! in.isDisabled()) return false;
    if (l.inputBuses.size() > 1)
    {
        const auto sc = l.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::stereo() && sc != juce::AudioChannelSet::mono()) return false;
    }
    return true;
}

void MicrovibeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock);
    scratch.setSize (2, juce::jmax (1, samplesPerBlock));
}

const juce::String MicrovibeProcessor::getProgramName (int i)
{
    const auto& all = PresetLibrary::get().all();
    return juce::isPositiveAndBelow (i, (int) all.size()) ? all[(size_t) i].name : juce::String ("Init");
}

void MicrovibeProcessor::setParam (int index, float realValue)
{
    if (auto* p = apvts.getParameter (def (index).id))
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
}

void MicrovibeProcessor::resetSharedData()
{
    auto& sh = engine.shared;
    for (int p = 0; p < NPAT; ++p) for (int l = 0; l < NLANE; ++l) for (int s = 0; s < mv::kMaxSteps; ++s)
    { sh.gate[p][l][s] = (uint8_t) ((s % 4) == 0); sh.value[p][l][s] = (s % 4) == 0 ? 0.8f : 0.5f; }
    for (int k = 0; k < NLFO; ++k) for (int i = 0; i < 16; ++i)
        sh.lfoShape[k][i] = 0.5f + 0.5f * std::sin (mv::kTwoPi * (float) i / 16.0f + (float) k);
    for (auto& m : sh.snapMask) m = 0;
}

void MicrovibeProcessor::loadPreset (int index)
{
    const auto& all = PresetLibrary::get().all();
    if (! juce::isPositiveAndBelow (index, (int) all.size())) return;
    const auto& pr = all[(size_t) index];
    const int outIdx = ids().out;
    for (int i = 0; i < count(); ++i)
    {
        if (i == outIdx) continue;     // the output volume belongs to the user
        float value = def (i).def;
        if (const auto* var = pr.values.getVarPointer (def (i).id))
        {
            if (var->isString() && var->toString().startsWithChar ('@'))
                value = (float) destIndexOf (indexOf (var->toString().substring (1)));   // matrix destination by parameter id
            else if (var->isString())
            {
                const int c = def (i).choices.indexOf (var->toString(), true);
                if (c >= 0) value = (float) c;
            }
            else value = (float) (double) *var;
        }
        setParam (i, value);
    }
    resetSharedData();
    auto& sh = engine.shared;
    if (auto* lanes = pr.lanes.getArray())
        for (int l = 0; l < juce::jmin (NLANE, lanes->size()); ++l)
        {
            const auto& ln = (*lanes)[l];
            const juce::String g = ln["g"].toString();
            const auto* vals = ln["v"].getArray();
            for (int p = 0; p < NPAT; ++p)
                for (int s = 0; s < mv::kMaxSteps; ++s)
                {
                    if (g.isNotEmpty()) sh.gate[p][l][s] = (uint8_t) (s < g.length() && g[s] == 'x');
                    if (vals != nullptr && vals->size() > 0) sh.value[p][l][s] = (float) (double) (*vals)[s % vals->size()];
                }
        }
    if (auto* obj = pr.lfoShapes.getDynamicObject())
        for (int k = 0; k < NLFO; ++k)
            if (auto* a = obj->getProperty (juce::String (k + 1)).getArray())
                for (int i = 0; i < juce::jmin (16, a->size()); ++i) sh.lfoShape[k][i] = (float) (double) (*a)[i];
    if (auto* snaps = pr.snaps.getArray())
        for (int k = 0; k < juce::jmin (NSNAP, snaps->size()); ++k)
        {
            uint64_t m = 0;
            if (auto* degs = (*snaps)[k].getArray())
                for (auto& d : *degs) { const int x = (int) d; if (x >= 0 && x < 64) m |= (uint64_t) 1 << x; }
            sh.snapMask[k] = m;
        }
    currentPreset.store (index);
    engine.resetState();
    if (onPresetChanged) onPresetChanged();
}

int MicrovibeProcessor::assignModulation (int source, int paramIndex)
{
    const int dst = destIndexOf (paramIndex);
    if (dst <= 0 || source <= 0 || source >= NUM_SOURCES) return -1;
    const auto& I = ids();
    for (int k = 0; k < NSLOT; ++k)
        if ((int) getParam (I.mxSrc[k]) == source && (int) getParam (I.mxDst[k]) == dst) return k;
    for (int k = 0; k < NSLOT; ++k)
        if ((int) getParam (I.mxSrc[k]) == 0 || (int) getParam (I.mxDst[k]) == 0)
        {
            setParam (I.mxSrc[k], (float) source);
            setParam (I.mxVia[k], 0);
            setParam (I.mxDst[k], (float) dst);
            setParam (I.mxCurve[k], 0);
            setParam (I.mxAmt[k], 0.3f);
            return k;
        }
    return -1;
}

void MicrovibeProcessor::clearModulation (int slot)
{
    const auto& I = ids();
    if (! juce::isPositiveAndBelow (slot, NSLOT)) return;
    setParam (I.mxSrc[slot], 0); setParam (I.mxVia[slot], 0); setParam (I.mxDst[slot], 0); setParam (I.mxAmt[slot], 0); setParam (I.mxCurve[slot], 0);
}

bool MicrovibeProcessor::loadScale (const juce::File& scl, const juce::File& kbm, juce::String& error)
{
    const auto s = scl.loadFileAsString(), k = kbm.existsAsFile() ? kbm.loadFileAsString() : juce::String();
    if (! engine.tuning.loadUser (s, k, error)) return false;
    userSclText = s; userKbmText = k;
    setParam (ids().tuScale, (float) (tuning::builtinNames().size() - 1));
    return true;
}

void MicrovibeProcessor::randomizeLane (int p, int l)
{
    juce::Random r;
    for (int s = 0; s < mv::kMaxSteps; ++s)
    {
        engine.shared.gate[p][l][s] = (uint8_t) (r.nextFloat() < (s % 4 == 0 ? 0.6f : 0.3f));
        engine.shared.value[p][l][s] = 0.2f + 0.8f * r.nextFloat();
    }
}

void MicrovibeProcessor::clearLane (int p, int l)
{
    for (int s = 0; s < mv::kMaxSteps; ++s) { engine.shared.gate[p][l][s] = 0; engine.shared.value[p][l][s] = 0.5f; }
}

void MicrovibeProcessor::copyPattern (int from, int to)
{
    for (int l = 0; l < NLANE; ++l) for (int s = 0; s < mv::kMaxSteps; ++s)
    {
        engine.shared.gate[to][l][s] = engine.shared.gate[from][l][s].load();
        engine.shared.value[to][l][s] = engine.shared.value[from][l][s].load();
    }
}

void MicrovibeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    for (size_t i = 0; i < raw.size(); ++i) base[i] = raw[i]->load (std::memory_order_relaxed);

    auto mainIn = getBusBuffer (buffer, true, 0);
    auto mainOut = getBusBuffer (buffer, false, 0);
    if (scratch.getNumSamples() < n) scratch.setSize (2, n, false, false, true);
    // main input -> L/R (mono input is duplicated)
    const int inCh = mainIn.getNumChannels();
    float* L = mainOut.getWritePointer (0);
    float* R = mainOut.getNumChannels() > 1 ? mainOut.getWritePointer (1) : scratch.getWritePointer (1);
    if (inCh == 0) { juce::FloatVectorOperations::clear (L, n); juce::FloatVectorOperations::clear (R, n); }
    else if (inCh == 1) juce::FloatVectorOperations::copy (R, mainIn.getReadPointer (0), n);

    const float* scL = nullptr; const float* scR = nullptr;
    if (getBusCount (true) > 1 && getBus (true, 1) != nullptr && getBus (true, 1)->isEnabled())
    {
        auto sc = getBusBuffer (buffer, true, 1);
        if (sc.getNumChannels() > 0) { scL = sc.getReadPointer (0); scR = sc.getReadPointer (sc.getNumChannels() > 1 ? 1 : 0); }
    }

    mv::Transport tr;
    tr.forceRun = isStandalone();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            tr.hasHost = ! isStandalone();
            if (auto b = pos->getBpm()) tr.bpm = *b; else tr.bpm = base[(size_t) ids().bpm];
            if (auto q = pos->getPpqPosition()) tr.ppq = *q;
            tr.hostRunning = pos->getIsPlaying();
            if (isStandalone()) tr.bpm = base[(size_t) ids().bpm];
        }

    engine.process (L, R, scL, scR, n, midi, tr, base.data());
    midi.clear();
}

tuning::Scale MicrovibeProcessor::guiScale() const
{
    const int idx = (int) raw[(size_t) ids().tuScale]->load();
    if (idx == tuning::builtinNames().size() - 1 && userSclText.isNotEmpty())
    {
        tuning::Scale s; juce::String err;
        if (tuning::parseScl (userSclText, s, err)) return s;
    }
    return tuning::builtin (idx);
}

void MicrovibeProcessor::toggleDroneDegree (int snap, int degree)
{
    if (! juce::isPositiveAndBelow (snap, NSNAP) || ! juce::isPositiveAndBelow (degree, 64)) return;
    engine.shared.snapMask[snap] = engine.shared.snapMask[snap].load() ^ ((uint64_t) 1 << degree);
}

juce::AudioProcessorEditor* MicrovibeProcessor::createEditor() { return new MicrovibeEditor (*this); }

// ------------------------------------------------------------------------------------------- state
juce::ValueTree MicrovibeProcessor::sharedToTree() const
{
    juce::ValueTree t ("SHARED");
    const auto& sh = engine.shared;
    for (int p = 0; p < NPAT; ++p)
        for (int l = 0; l < NLANE; ++l)
        {
            juce::String g; juce::String v;
            for (int s = 0; s < mv::kMaxSteps; ++s)
            {
                g << (sh.gate[p][l][s].load() ? 'x' : '.');
                v << juce::String (sh.value[p][l][s].load(), 3) << (s + 1 < mv::kMaxSteps ? " " : "");
            }
            juce::ValueTree lane ("LANE");
            lane.setProperty ("p", p, nullptr); lane.setProperty ("l", l, nullptr);
            lane.setProperty ("g", g, nullptr); lane.setProperty ("v", v, nullptr);
            t.appendChild (lane, nullptr);
        }
    for (int k = 0; k < NLFO; ++k)
    {
        juce::String v;
        for (int i = 0; i < 16; ++i) v << juce::String (sh.lfoShape[k][i].load(), 3) << (i < 15 ? " " : "");
        juce::ValueTree s ("LFOSHAPE"); s.setProperty ("k", k, nullptr); s.setProperty ("v", v, nullptr);
        t.appendChild (s, nullptr);
    }
    for (int k = 0; k < NSNAP; ++k) t.setProperty ("snap" + juce::String (k + 1), juce::String::toHexString ((juce::int64) sh.snapMask[k].load()), nullptr);
    t.setProperty ("scl", userSclText, nullptr);
    t.setProperty ("kbm", userKbmText, nullptr);
    return t;
}

void MicrovibeProcessor::sharedFromTree (const juce::ValueTree& t)
{
    auto& sh = engine.shared;
    for (const auto& c : t)
    {
        if (c.hasType ("LANE"))
        {
            const int p = c["p"], l = c["l"];
            if (! juce::isPositiveAndBelow (p, NPAT) || ! juce::isPositiveAndBelow (l, NLANE)) continue;
            const juce::String g = c["g"].toString();
            auto v = juce::StringArray::fromTokens (c["v"].toString(), " ", "");
            for (int s = 0; s < mv::kMaxSteps; ++s)
            {
                sh.gate[p][l][s] = (uint8_t) (s < g.length() && g[s] == 'x');
                if (s < v.size()) sh.value[p][l][s] = v[s].getFloatValue();
            }
        }
        else if (c.hasType ("LFOSHAPE"))
        {
            const int k = c["k"];
            if (! juce::isPositiveAndBelow (k, NLFO)) continue;
            auto v = juce::StringArray::fromTokens (c["v"].toString(), " ", "");
            for (int i = 0; i < juce::jmin (16, v.size()); ++i) sh.lfoShape[k][i] = v[i].getFloatValue();
        }
    }
    for (int k = 0; k < NSNAP; ++k)
        sh.snapMask[k] = (uint64_t) t.getProperty ("snap" + juce::String (k + 1), "0").toString().getHexValue64();
    userSclText = t["scl"].toString(); userKbmText = t["kbm"].toString();
    if (userSclText.isNotEmpty()) { juce::String err; engine.tuning.loadUser (userSclText, userKbmText, err); }
}

void MicrovibeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset.load(), nullptr);
    state.removeChild (state.getChildWithName ("SHARED"), nullptr);
    state.appendChild (sharedToTree(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void MicrovibeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            currentPreset.store ((int) vt.getProperty ("preset", -1));
            const auto shared = vt.getChildWithName ("SHARED");
            if (shared.isValid()) sharedFromTree (shared);
            vt.removeChild (shared, nullptr);
            apvts.replaceState (vt);
            engine.resetState();
            if (onPresetChanged) onPresetChanged();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MicrovibeProcessor(); }
