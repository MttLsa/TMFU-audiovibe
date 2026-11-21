#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace params;

namespace
{
    // the preset library must exist before the parameter layout (the "Synth Model" lists come from it)
    juce::AudioProcessorValueTreeState::ParameterLayout makeLayout() { PresetLibrary::get(); return createLayout(); }
    bool presetControlled (int i)
    {
        const auto& L = layout();
        if (i >= L.lfoBase[0]) return false;                      // LFOs: the user's
        if (i >= L.eqBase[0]) return false;                       // EQ: the user's
        if (i >= L.ofLpOn && i <= L.ofHpRes) return false;        // output filters: the user's
        if (i == L.gMaster || i == L.gBpm) return false;
        return true;
    }
}

FivesynthProcessor::FivesynthProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SYNTHZILLA", makeLayout())   // state tag kept from SynthZilla: saved projects reload
{
    const int n = numParams();
    raw.resize ((size_t) n); prm.resize ((size_t) n); snap.assign ((size_t) n, 0.0f);
    for (int i = 0; i < n; ++i)
    {
        raw[(size_t) i] = apvts.getRawParameterValue (def (i).id);
        prm[(size_t) i] = apvts.getParameter (def (i).id);
    }
    for (int m = 0; m < numModules; ++m) apvts.addParameterListener (def (mod (m, mModel)).id, this);
    const auto& all = PresetLibrary::get().all();
    int start = 0;
    for (int i = 0; i < (int) all.size(); ++i) if (all[(size_t) i].name == "Organ 2") start = i;
    if (! all.empty()) loadPreset (start);
}

FivesynthProcessor::~FivesynthProcessor()
{
    for (int m = 0; m < numModules; ++m) apvts.removeParameterListener (def (mod (m, mModel)).id, this);
}

bool FivesynthProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void FivesynthProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    smoothed.assign ((size_t) numParams(), 0.0f);
    smoothReset = true;
    engine.prepare (sampleRate, juce::jmax (32, samplesPerBlock));
    chorus.prepare (sampleRate);
    reverb.setSampleRate (sampleRate);
    reverb.reset();
    limiter.prepare (sampleRate);
    revBuf.setSize (2, juce::jmax (32, samplesPerBlock));
    chunkMidi.ensureSize (2048);
    for (auto& b : eq) for (auto& q : b) q.reset();
    for (auto& f : outF) for (auto& s : f) s.reset();
    for (auto& k : eqKey) for (auto& x : k) x = -12345.0f;
    keyboard.reset();
}

const juce::String FivesynthProcessor::getProgramName (int i)
{
    const auto& all = PresetLibrary::get().all();
    return juce::isPositiveAndBelow (i, (int) all.size()) ? all[(size_t) i].name : juce::String ("Init");
}

void FivesynthProcessor::setParam (int i, float value)
{
    auto* p = prm[(size_t) i];
    p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (def (i).mn, def (i).mx, value)));
}

void FivesynthProcessor::applyModuleValues (int slot, const PresetModule& pm)
{
    for (int s = 0; s < numModSuffix; ++s)
    {
        if (s == mOn || s == mModel) continue;
        const int i = mod (slot, s);
        float value = def (i).def;
        const juce::String key = def (i).id.fromFirstOccurrenceOf ("_", false, false);
        if (const auto* var = pm.values.getVarPointer (key))
        {
            if (var->isString()) { const int c = def (i).choices.indexOf (var->toString(), true); if (c >= 0) value = (float) c; }
            else value = (float) (double) *var;
        }
        if (s == mLevel && ! pm.values.contains (key)) value = 0.8f;
        setParam (i, value);
    }
}

void FivesynthProcessor::loadPreset (int index)
{
    const auto& all = PresetLibrary::get().all();
    if (! juce::isPositiveAndBelow (index, (int) all.size())) return;
    const auto& pr = all[(size_t) index];
    loading = true;
    smoothReset = true;
    for (int i = 0; i < numParams(); ++i)
        if (presetControlled (i)) setParam (i, def (i).def);
    for (int m = 0; m < numModules; ++m) setParam (mod (m, mOn), 0.0f);
    int maxSlot = 1;
    for (auto& pm : pr.modules)
    {
        setParam (mod (pm.slot, mOn), 1.0f);
        const int mi = PresetLibrary::get().modelIndex (pm.model);
        if (mi >= 0) setParam (mod (pm.slot, mModel), (float) mi);
        applyModuleValues (pm.slot, pm);
        maxSlot = juce::jmax (maxSlot, pm.slot);
    }
    for (auto& nv : pr.globals)
    {
        const int i = indexOf (nv.name.toString());
        if (i < 0) continue;
        float value = def (i).def;
        if (nv.value.isString()) { const int c = def (i).choices.indexOf (nv.value.toString(), true); if (c >= 0) value = (float) c; }
        else value = (float) (double) nv.value;
        setParam (i, value);
    }
    setParam (layout().gSlots, (float) juce::jmax ((int) raw[(size_t) layout().gSlots]->load(), maxSlot + 1, 2));
    currentPreset.store (index);
    resetPending.store (true);
    loading = false;
}

// "Change synth" of a module: copy the module of the first preset that uses that instrument (its signature sound)
void FivesynthProcessor::loadModelPatch (int module, int modelIndex)
{
    const auto* md = PresetLibrary::get().model (modelIndex);
    if (md == nullptr) return;
    loading = true;
    smoothReset = true;
    for (auto& pr : PresetLibrary::get().all())
        for (auto& pm : pr.modules)
            if (pm.model == md->name)
            {
                applyModuleValues (module, pm);
                loading = false;
                return;
            }
    PresetModule empty;
    applyModuleValues (module, empty);
    setParam (mod (module, mEngine), md->digital ? 1.0f : 0.0f);
    loading = false;
}

void FivesynthProcessor::parameterChanged (const juce::String& id, float)
{
    if (loading) return;
    for (int m = 0; m < numModules; ++m)
        if (id == def (mod (m, mModel)).id) pendingModel.fetch_or (1 << m);
    triggerAsyncUpdate();
}

void FivesynthProcessor::handleAsyncUpdate()
{
    const int mask = pendingModel.exchange (0);
    for (int m = 0; m < numModules; ++m)
        if (mask & (1 << m)) loadModelPatch (m, (int) raw[(size_t) mod (m, mModel)]->load());
}

void FivesynthProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    keyboard.processNextMidiBuffer (midi, 0, n, true);
    if (resetPending.exchange (false)) engine.allNotesOff();
    buffer.clear();

    double bpm = raw[(size_t) layout().gBpm]->load(), ppq = -1;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            playing = pos->getIsPlaying();
            if (auto p = pos->getPpqPosition()) ppq = *p;
        }
    if (ppq < 0) ppq = internalPpq;

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    constexpr int chunk = 32;
    for (int s = 0; s < n; s += chunk)
    {
        const int len = juce::jmin (chunk, n - s);
        chunkMidi.clear();
        for (const auto meta : midi)
            if (meta.samplePosition >= s && meta.samplePosition < s + len) chunkMidi.addEvent (meta.getMessage(), meta.samplePosition - s);
        renderChunk (L + s, R + s, len, chunkMidi, bpm, ppq + (double) s / sr * bpm / 60.0, playing);
    }
    internalPpq = ppq + (double) n / sr * bpm / 60.0;
}

void FivesynthProcessor::renderChunk (float* L, float* R, int n, const juce::MidiBuffer& midi, double bpm, double ppq, bool playing)
{
    const auto& lay = layout();
    const int np = numParams();
    // parameter snapshot; continuous (float) parameters are smoothed with a one-pole filter (~20 ms) so that moving a
    // knob or fader changes the sound without steps; switches, menus and integer values change at once
    const bool jump = smoothReset.exchange (false);
    const float a = 1.0f - std::exp (-(float) n / (0.02f * (float) sr));
    for (int i = 0; i < np; ++i)
    {
        const float target = raw[(size_t) i]->load();
        auto& sm = smoothed[(size_t) i];
        if (jump || def (i).kind != params::F) sm = target;
        else sm += (target - sm) * a;
        snap[(size_t) i] = sm;
    }

    // ------------------------------------------------------------ 8 LFOs -> any parameter
    for (int l = 0; l < numLfos; ++l)
    {
        const int b = lay.lfoBase[l];
        const int shape = (int) snap[(size_t) (b + lShape)];
        double cyclesPerSec;
        const bool sync = snap[(size_t) (b + lSync)] > 0.5f;
        if (sync) cyclesPerSec = bpm / 60.0 / lfoDivBeats ((int) snap[(size_t) (b + lDiv)]);
        else cyclesPerSec = snap[(size_t) (b + lRate)];
        const double prev = lfoPhase[l];
        if (sync && playing) lfoPhase[l] = ppq / lfoDivBeats ((int) snap[(size_t) (b + lDiv)]);
        else lfoPhase[l] += cyclesPerSec * n / sr;
        const bool wrapped = std::floor (lfoPhase[l]) != std::floor (prev);
        if (wrapped) { lfoSh[l] = rnd.nextFloat() * 2.0f - 1.0f; lfoRnd[l][0] = lfoRnd[l][1]; lfoRnd[l][1] = rnd.nextFloat() * 2.0f - 1.0f; }
        double ph = lfoPhase[l] + snap[(size_t) (b + lPhase)] / 360.0;
        const float p = (float) (ph - std::floor (ph));
        const float fr = (float) (lfoPhase[l] - std::floor (lfoPhase[l]));
        float val;
        switch (shape)
        {
            case 0: val = dsp::triangle (p); break;
            case 1: val = dsp::sine (p); break;
            case 2: val = 2.0f * p - 1.0f; break;
            case 3: val = 1.0f - 2.0f * p; break;
            case 4: val = p < 0.5f ? 1.0f : -1.0f; break;
            case 5: val = lfoSh[l]; break;
            default: val = lfoRnd[l][0] + (lfoRnd[l][1] - lfoRnd[l][0]) * (0.5f - 0.5f * std::cos (fr * 3.14159265f)); break;
        }
        lfoVals[(size_t) l].store (val);
        const int t = (int) snap[(size_t) (b + lTarget)] - 1;
        if (t >= 0 && t < lay.numTargets)
        {
            auto* pr = prm[(size_t) t];
            const float norm = juce::jlimit (0.0f, 1.0f, pr->convertTo0to1 (snap[(size_t) t]) + snap[(size_t) (b + lDepth)] * val * 0.5f);
            snap[(size_t) t] = pr->convertFrom0to1 (norm);
        }
    }

    engine.render (L, R, n, midi, snap.data(), bpm);

    // ------------------------------------------------------------ global chain: drive, chorus, reverb, EQ, output filters
    const float* v = snap.data();
    if (v[lay.gDrive] > 0.001f)
    {
        const float k = 1.0f + v[lay.gDrive] * 9.0f, comp = 1.0f / std::pow (k, 0.55f);
        for (int i = 0; i < n; ++i) { L[i] = dsp::fastTanh (L[i] * k) * comp * 1.2f; R[i] = dsp::fastTanh (R[i] * k) * comp * 1.2f; }
    }
    chorus.process (L, R, n, (int) v[lay.gChoMode], v[lay.gChoMix]);
    if (v[lay.gRevMix] > 0.001f)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.3f + 0.68f * v[lay.gRevSize]; rp.damping = v[lay.gRevDamp]; rp.wetLevel = 1.0f; rp.dryLevel = 0.0f; rp.width = 1.0f;
        reverb.setParameters (rp);
        revBuf.copyFrom (0, 0, L, n); revBuf.copyFrom (1, 0, R, n);
        reverb.processStereo (revBuf.getWritePointer (0), revBuf.getWritePointer (1), n);
        const float wet = v[lay.gRevMix] * 0.5f, dry = 1.0f - 0.3f * v[lay.gRevMix];
        for (int i = 0; i < n; ++i) { L[i] = L[i] * dry + revBuf.getSample (0, i) * wet; R[i] = R[i] * dry + revBuf.getSample (1, i) * wet; }
    }
    for (int e = 0; e < numEqBands; ++e)
    {
        const int b = lay.eqBase[e];
        if (v[b + eqOn] < 0.5f) continue;
        const float key[5] = { 1, v[b + eqType], v[b + eqFreq], v[b + eqGain], v[b + eqQ] };
        if (std::memcmp (key, eqKey[e], sizeof (key)) != 0)
        {
            for (auto& q : eq[e]) q.set ((int) key[1], key[2], key[3], key[4], (float) sr);
            std::memcpy (eqKey[e], key, sizeof (key));
        }
        const int type = (int) key[1];
        if ((type == 1 || type == 2 || type == 3) && std::abs (key[3]) < 0.01f) continue;
        for (int i = 0; i < n; ++i) { L[i] = eq[e][0].process (L[i]); R[i] = eq[e][1].process (R[i]); }
    }
    const int ofOn[3] = { lay.ofLpOn, lay.ofBpOn, lay.ofHpOn };
    for (int f = 0; f < 3; ++f)
    {
        if (v[ofOn[f]] < 0.5f) continue;
        const float g = dsp::cutoffG (v[ofOn[f] + 1], (float) sr), res = v[ofOn[f] + 2];
        const int mode = f == 0 ? 0 : f == 1 ? 1 : 2;
        for (int i = 0; i < n; ++i) { L[i] = outF[f][0].process (L[i], g, res, mode); R[i] = outF[f][1].process (R[i], g, res, mode); }
    }

    const float target = juce::Decibels::decibelsToGain (v[lay.gMaster] + v[lay.gTrim]);
    float pk = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        gainSm += (target - gainSm) * 0.002f;
        L[i] *= gainSm; R[i] *= gainSm;
        pk = juce::jmax (pk, std::abs (L[i]), std::abs (R[i]));
    }
    peakPre.store (juce::jmax (peakPre.load(), pk));
    limiter.process (L, R, n);
    outMeter.store (juce::jmax (pk, outMeter.load() * 0.97f));
}

juce::AudioProcessorEditor* FivesynthProcessor::createEditor() { return new FivesynthEditor (*this); }

void FivesynthProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset.load(), nullptr);
    state.removeChild (state.getChildWithName ("UI"), nullptr);
    state.appendChild (uiState.createCopy(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void FivesynthProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            loading = true;
            smoothReset = true;
            currentPreset.store ((int) vt.getProperty ("preset", -1));
            auto ui = vt.getChildWithName ("UI");
            if (ui.isValid()) { uiState.copyPropertiesFrom (ui, nullptr); vt.removeChild (ui, nullptr); }
            apvts.replaceState (vt);
            loading = false;
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FivesynthProcessor(); }
