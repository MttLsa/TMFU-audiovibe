#include "PluginProcessor.h"
#include "PluginEditor.h"

// =============================================================================================== parameter tables
namespace pid
{
const char* targetParamId (int t)
{
    static const char* ids[numTargets] = { "", input, drive, cutoff, reso, lowFreq, lowGain, highFreq, highGain,
                                           mix, output, lfoRate, lfoDepth, envDepth };
    return (t > 0 && t < numTargets) ? ids[t] : "";
}

juce::StringArray circuitNames() { return { "Op-Amp", "Diode", "Asymmetric", "Fuzz", "Rectifier", "Tube", "Hard Clip", "Wavefolder" }; }
juce::StringArray filterNames()  { return { "Low-Pass 12", "Low-Pass 24", "High-Pass 12", "High-Pass 24", "Band-Pass", "Notch", "Peak" }; }
juce::StringArray lfoShapeNames() { return { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Sample & Hold", "Smooth Random" }; }
juce::StringArray lfoDivNames()  { return { "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2", "1 Bar", "2 Bars", "4 Bars" }; }
juce::StringArray envModeNames() { return { "Follower", "Generator" }; }
juce::StringArray targetNames()
{
    return { "None", "Input", "Drive", "Cutoff", "Resonance", "Low Freq", "Low Gain", "High Freq", "High Gain",
             "Mix", "Output", "LFO Rate", "LFO Depth", "Env Depth" };
}

double lfoDivBeats (int i)
{
    static const double b[] = { 0.125, 1.0 / 6.0, 0.25, 0.375, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 2.0, 4.0, 8.0, 16.0 };
    return b[juce::jlimit (0, 13, i)];
}
} // namespace pid

// =============================================================================================== factory presets
namespace
{
struct PresetValue
{
    const char* id;
    float value;
};

struct Preset
{
    const char* name;
    std::vector<PresetValue> values;
};

const std::vector<Preset>& factoryPresets()
{
    using namespace pid;
    // choice values are indices: circuit 0 Op-Amp 1 Diode 2 Asymmetric 3 Fuzz 4 Rectifier 5 Tube 6 Hard Clip 7 Wavefolder
    // filter 0 LP12 1 LP24 2 HP12 3 HP24 4 BP 5 Notch 6 Peak; targets as pid::Target
    static const std::vector<Preset> p = {
        { "Init", {} },
        { "Warm Console", { { circuit, 5 }, { drive, 25 }, { cutoff, 16000 }, { lowFreq, 100 }, { lowGain, 2 },
                            { highFreq, 8000 }, { highGain, -1.5f } } },
        { "Mid Push", { { circuit, 0 }, { drive, 55 }, { cutoff, 9000 }, { reso, 10 }, { lowGain, -2 }, { highGain, 1 },
                        { output, -2 } } },
        { "Diode Crunch", { { circuit, 1 }, { drive, 60 }, { filterType, 1 }, { cutoff, 7000 }, { reso, 20 }, { output, -3 } } },
        { "Asym Grit", { { circuit, 2 }, { drive, 65 }, { filterType, 2 }, { cutoff, 120 }, { highFreq, 5000 },
                         { highGain, 3 }, { output, -5 } } },
        { "Fuzz Wall", { { circuit, 3 }, { drive, 80 }, { filterType, 1 }, { cutoff, 4500 }, { reso, 25 }, { lowFreq, 90 },
                         { lowGain, 4 }, { output, -5 } } },
        { "Octave Rectifier", { { circuit, 4 }, { drive, 60 }, { filterType, 4 }, { cutoff, 1200 }, { reso, 35 }, { mix, 70 }, { output, 3 } } },
        { "Hard Edge", { { circuit, 6 }, { drive, 70 }, { cutoff, 8000 }, { highGain, -3 }, { output, -3 } } },
        { "Folded Bells", { { circuit, 7 }, { drive, 55 }, { filterType, 6 }, { cutoff, 2500 }, { reso, 40 }, { mix, 80 },
                            { output, -4 } } },
        { "Auto-Wah Fuzz", { { circuit, 3 }, { drive, 60 }, { filterType, 4 }, { cutoff, 500 }, { reso, 60 },
                             { envMode, 0 }, { envTarget, tCutoff }, { envDepth, 45 }, { envAttack, 5 }, { envRelease, 120 },
                             { envThresh, -40 }, { output, 3 } } },
        { "Tempo Sweep", { { circuit, 1 }, { drive, 45 }, { filterType, 1 }, { cutoff, 1500 }, { reso, 55 },
                           { lfoShape, 0 }, { lfoSync, 1 }, { lfoDiv, 11 }, { lfoDepth, 35 }, { lfoTarget, tCutoff }, { output, -5 } } },
        { "Moving Notch", { { circuit, 0 }, { drive, 40 }, { filterType, 5 }, { cutoff, 800 }, { reso, 30 },
                            { lfoShape, 1 }, { lfoRate, 0.3f }, { lfoDepth, 30 }, { lfoTarget, tCutoff } } },
        { "Transient Bite", { { circuit, 5 }, { drive, 30 }, { cutoff, 14000 }, { envMode, 1 }, { envAttack, 1 },
                              { envRelease, 80 }, { envThresh, -24 }, { envTarget, tDrive }, { envDepth, 60 }, { output, -4 } } },
        { "Wobble Bass", { { circuit, 2 }, { drive, 50 }, { filterType, 1 }, { cutoff, 400 }, { reso, 65 }, { lowGain, 3 },
                           { lfoShape, 0 }, { lfoSync, 1 }, { lfoDiv, 5 }, { lfoDepth, 40 }, { lfoTarget, tCutoff }, { output, -6 } } },
        { "Telephone", { { circuit, 6 }, { drive, 45 }, { filterType, 4 }, { cutoff, 1800 }, { reso, 20 }, { lowFreq, 300 },
                         { lowGain, -12 }, { highFreq, 4000 }, { highGain, -10 }, { output, 6 } } },
        { "S&H Scramble", { { circuit, 4 }, { drive, 45 }, { cutoff, 2000 }, { reso, 70 }, { lfoShape, 5 }, { lfoSync, 1 },
                            { lfoDiv, 2 }, { lfoDepth, 40 }, { lfoTarget, tCutoff }, { mix, 75 }, { output, -2 } } }
    };
    return p;
}

juce::NormalisableRange<float> logRange (float lo, float hi)
{
    return { lo, hi,
             [] (float s, float e, float t) { return s * std::pow (e / s, t); },
             [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
             [] (float s, float e, float v) { return juce::jlimit (s, e, v); } };
}

juce::String hzText (float v, int)
{
    return v < 1000.0f ? juce::String (juce::roundToInt (v)) + " Hz" : juce::String (v / 1000.0f, 2) + " kHz";
}

float hzValue (const juce::String& t)
{
    const float v = t.getFloatValue();
    return t.containsIgnoreCase ("k") ? v * 1000.0f : v;
}

juce::String msText (float v, int)
{
    return v < 1000.0f ? juce::String (v, v < 10.0f ? 1 : 0) + " ms" : juce::String (v / 1000.0f, 2) + " s";
}

float msValue (const juce::String& t)
{
    const float v = t.getFloatValue();
    const auto s = t.trim();
    return (! s.containsIgnoreCase ("ms") && s.endsWithIgnoreCase ("s")) ? v * 1000.0f : v;
}

juce::String dbText (float v, int) { return juce::String (v, 1) + " dB"; }
juce::String pctText (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; }
float plainValue (const juce::String& t) { return t.getFloatValue(); }
} // namespace

// =============================================================================================== layout
juce::AudioProcessorValueTreeState::ParameterLayout HeatsinkProcessor::createLayout()
{
    using namespace pid;
    using APF = juce::AudioParameterFloat;
    using APC = juce::AudioParameterChoice;
    using APB = juce::AudioParameterBool;
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    auto flt = [&] (const char* id, const char* name, juce::NormalisableRange<float> r, float def,
                    juce::String (*toText) (float, int), float (*fromText) (const juce::String&))
    {
        l.add (std::make_unique<APF> (juce::ParameterID { id, 1 }, name, r, def,
                                      juce::AudioParameterFloatAttributes()
                                          .withStringFromValueFunction (toText)
                                          .withValueFromStringFunction (fromText)));
    };

    flt (input, "Input", { -24.0f, 24.0f, 0.01f }, 0.0f, dbText, plainValue);
    flt (drive, "Drive", { 0.0f, 100.0f, 0.01f }, 35.0f, pctText, plainValue);
    l.add (std::make_unique<APC> (juce::ParameterID { circuit, 1 }, "Circuit", circuitNames(), 0));
    l.add (std::make_unique<APB> (juce::ParameterID { filterOn, 1 }, "Filter On", true));
    l.add (std::make_unique<APC> (juce::ParameterID { filterType, 1 }, "Filter Type", filterNames(), 0));
    flt (cutoff, "Cutoff", logRange (20.0f, 20000.0f), 20000.0f, hzText, hzValue);
    flt (reso, "Resonance", { 0.0f, 100.0f, 0.01f }, 0.0f, pctText, plainValue);
    flt (lowFreq, "Low Freq", logRange (30.0f, 1000.0f), 120.0f, hzText, hzValue);
    flt (lowGain, "Low Gain", { -18.0f, 18.0f, 0.01f }, 0.0f, dbText, plainValue);
    flt (highFreq, "High Freq", logRange (1000.0f, 16000.0f), 6000.0f, hzText, hzValue);
    flt (highGain, "High Gain", { -18.0f, 18.0f, 0.01f }, 0.0f, dbText, plainValue);
    flt (mix, "Mix", { 0.0f, 100.0f, 0.01f }, 100.0f, pctText, plainValue);
    flt (output, "Output", { -24.0f, 12.0f, 0.01f }, 0.0f, dbText, plainValue);

    l.add (std::make_unique<APC> (juce::ParameterID { lfoShape, 1 }, "LFO Shape", lfoShapeNames(), 0));
    flt (lfoRate, "LFO Rate", logRange (0.01f, 20.0f), 1.0f,
         [] (float v, int) { return juce::String (v, v < 1.0f ? 3 : 2) + " Hz"; }, plainValue);
    l.add (std::make_unique<APB> (juce::ParameterID { lfoSync, 1 }, "LFO Sync", false));
    l.add (std::make_unique<APC> (juce::ParameterID { lfoDiv, 1 }, "LFO Division", lfoDivNames(), 8));
    flt (lfoDepth, "LFO Depth", { -100.0f, 100.0f, 0.01f }, 0.0f, pctText, plainValue);
    l.add (std::make_unique<APC> (juce::ParameterID { lfoTarget, 1 }, "LFO Target", targetNames(), tCutoff));

    l.add (std::make_unique<APC> (juce::ParameterID { envMode, 1 }, "Env Mode", envModeNames(), 0));
    flt (envAttack, "Env Attack", logRange (0.1f, 500.0f), 5.0f, msText, msValue);
    flt (envRelease, "Env Release", logRange (5.0f, 5000.0f), 150.0f, msText, msValue);
    flt (envThresh, "Env Threshold", { -60.0f, -6.0f, 0.01f }, -30.0f, dbText, plainValue);
    flt (envDepth, "Env Depth", { -100.0f, 100.0f, 0.01f }, 0.0f, pctText, plainValue);
    l.add (std::make_unique<APC> (juce::ParameterID { envTarget, 1 }, "Env Target", targetNames(), tCutoff));
    return l;
}

// =============================================================================================== processor
HeatsinkProcessor::HeatsinkProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HEATSINK", createLayout())
{
    using namespace pid;
    pInput = param (input);       pDrive = param (drive);         pCircuit = param (circuit);
    pFilterOn = param (filterOn); pFilterType = param (filterType); pCutoff = param (cutoff);
    pReso = param (reso);         pLowFreq = param (lowFreq);     pLowGain = param (lowGain);
    pHighFreq = param (highFreq); pHighGain = param (highGain);   pMix = param (mix);
    pOutput = param (output);     pLfoShape = param (lfoShape);   pLfoRate = param (lfoRate);
    pLfoSync = param (lfoSync);   pLfoDiv = param (lfoDiv);       pLfoDepth = param (lfoDepth);
    pLfoTarget = param (lfoTarget); pEnvMode = param (envMode);   pEnvAttack = param (envAttack);
    pEnvRelease = param (envRelease); pEnvThresh = param (envThresh); pEnvDepth = param (envDepth);
    pEnvTarget = param (envTarget);

    for (int t = 1; t < numTargets; ++t)
        modRange[(size_t) t] = apvts.getParameterRange (targetParamId (t));
}

bool HeatsinkProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void HeatsinkProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fs = sampleRate;
    maxBlock = juce::jmax (controlStep, samplesPerBlock);
    const int numCh = juce::jmax (1, juce::jmin (2, getTotalNumOutputChannels()));

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) numCh, (size_t) osFactorLog2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampler->initProcessing ((size_t) maxBlock);
    latency = (int) std::lround (oversampler->getLatencyInSamples());
    setLatencySamples (latency);

    const double osRate = fs * osFactor;
    driver.prepare (osRate);
    filter.prepare (osRate);
    eq.prepare (fs);
    env.prepare (fs);

    for (auto* v : { &aIn, &aDrive, &aCut, &aReso, &aMix, &aOut, &aFOn })
        v->assign ((size_t) maxBlock, 0.0f);
    const size_t segs = (size_t) (maxBlock / controlStep + 2);
    for (auto* v : { &segLowF, &segLowG, &segHighF, &segHighG })
        v->assign (segs, 0.0f);

    dryBuf.setSize (2, maxBlock);
    dryLine.setSize (2, juce::jmax (1, latency));

    for (auto* s : { &smIn, &smDrive, &smCutLog, &smReso, &smMix, &smOut, &smFOn })
        s->prepare (fs, 4.0f);
    smFOn.prepare (fs, 8.0f);
    for (auto* s : { &smLowF, &smLowG, &smHighF, &smHighG })
        s->prepare (fs / controlStep, 4.0f);

    reset();
}

void HeatsinkProcessor::reset()
{
    if (oversampler != nullptr) oversampler->reset();
    driver.setCircuit ((int) pCircuit->load());
    filter.setType ((int) pFilterType->load());
    driver.reset();
    filter.reset();
    eq.reset();
    env.reset();
    lfo.reset();
    dryLine.clear();
    dryWrite = 0;
    lastLfo = lastEnv = 0.0f;

    smIn.reset (hs::dbToGain (pInput->load()));
    smDrive.reset (pDrive->load() * 0.01f);
    smCutLog.reset (std::log2 (pCutoff->load()));
    smReso.reset (pReso->load() * 0.01f);
    smMix.reset (pMix->load() * 0.01f);
    smOut.reset (hs::dbToGain (pOutput->load()));
    smFOn.reset (pFilterOn->load() > 0.5f ? 1.0f : 0.0f);
    smLowF.reset (std::log2 (pLowFreq->load()));
    smLowG.reset (pLowGain->load());
    smHighF.reset (std::log2 (pHighFreq->load()));
    smHighG.reset (pHighGain->load());
    eq.set (pLowFreq->load(), pLowGain->load(), pHighFreq->load(), pHighGain->load());
}

float HeatsinkProcessor::modulated (int t, float base, int lfoT, int envT, float lfoAmt, float envAmt) const noexcept
{
    if (t != lfoT && t != envT) return base;
    const auto& r = modRange[(size_t) t];
    float n = r.convertTo0to1 (base);
    if (t == lfoT) n += lfoAmt;
    if (t == envT) n += envAmt;
    return r.convertFrom0to1 (hs::clampf (n, 0.0f, 1.0f));
}

void HeatsinkProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    for (int c = numIn; c < numOut; ++c) buffer.clear (c, 0, buffer.getNumSamples());
    const int numCh = juce::jmin (2, numIn, numOut, buffer.getNumChannels());
    if (numCh <= 0 || oversampler == nullptr) return;

    HostTime ht;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) ht.bpm = juce::jlimit (20.0, 999.0, *bpm);
            if (auto ppq = pos->getPpqPosition()) { ht.ppq = *ppq; ht.hasPpq = true; }
            ht.playing = pos->getIsPlaying();
        }

    const int total = buffer.getNumSamples();
    for (int pos = 0; pos < total; pos += maxBlock)
    {
        HostTime chunkTime = ht;
        chunkTime.ppq += (double) pos / fs * ht.bpm / 60.0;
        processChunk (buffer, numCh, pos, juce::jmin (maxBlock, total - pos), chunkTime);
    }
}

void HeatsinkProcessor::processChunk (juce::AudioBuffer<float>& buffer, int numCh, int start, int len, const HostTime& ht)
{
    using namespace pid;
    float* ch[2] = { buffer.getWritePointer (0, start), numCh > 1 ? buffer.getWritePointer (1, start) : nullptr };

    // ---- discrete parameters
    driver.setCircuit ((int) pCircuit->load());
    filter.setType ((int) pFilterType->load());
    driver.beginBlock();
    filter.beginBlock();
    const int lfoShapeV = (int) pLfoShape->load();
    const bool sync = pLfoSync->load() > 0.5f;
    const double divBeats = lfoDivBeats ((int) pLfoDiv->load());
    const int lfoT = (int) pLfoTarget->load();
    const int envT = (int) pEnvTarget->load();
    const int envModeV = (int) pEnvMode->load();
    const float fOnTarget = pFilterOn->load() > 0.5f ? 1.0f : 0.0f;

    // ---- dry path, delayed to match the oversampling latency
    for (int c = 0; c < numCh; ++c)
    {
        float* d = dryBuf.getWritePointer (c);
        if (latency == 0)
        {
            juce::FloatVectorOperations::copy (d, ch[c], len);
            continue;
        }
        float* line = dryLine.getWritePointer (c);
        int w = dryWrite;
        for (int i = 0; i < len; ++i)
        {
            d[i] = line[w];
            line[w] = ch[c][i];
            if (++w >= latency) w = 0;
        }
    }
    if (latency > 0) dryWrite = (dryWrite + len) % latency;

    // ---- modulation + parameter smoothing (control rate, per-sample ramps)
    int seg = 0;
    for (int s0 = 0; s0 < len; s0 += controlStep, ++seg)
    {
        const int n = juce::jmin (controlStep, len - s0);

        float peak = 0.0f;
        for (int c = 0; c < numCh; ++c)
            for (int i = 0; i < n; ++i) peak = std::max (peak, std::abs (ch[c][s0 + i]));

        const float e = env.process (peak, n, envModeV, pEnvThresh->load(), pEnvAttack->load(), pEnvRelease->load());

        // modulator depths and LFO rate may themselves be modulated (by the other source, or by the previous LFO value)
        const float envDepthBase = pEnvDepth->load();
        const float lfoDepthBase = pLfoDepth->load();
        const float preLfoAmt = lastLfo * lfoDepthBase * 0.01f;
        const float preEnvAmt = e * envDepthBase * 0.01f;
        const float lfoDepthEff = modulated (tLfoDepth, lfoDepthBase, lfoT, envT, preLfoAmt, preEnvAmt) * 0.01f;
        const float rateEff = modulated (tLfoRate, pLfoRate->load(), lfoT, envT, preLfoAmt, preEnvAmt);

        if (sync && ht.playing && ht.hasPpq)
            lfo.setPhase ((ht.ppq + (double) (s0 + n) / fs * ht.bpm / 60.0) / divBeats);
        else
            lfo.advance ((sync ? ht.bpm / 60.0 / divBeats : (double) rateEff) * (double) n / fs);

        const float lv = lfo.value (lfoShapeV);
        const float lfoAmt = lv * lfoDepthEff;
        const float envDepthEff = modulated (tEnvDepth, envDepthBase, lfoT, envT, lfoAmt, preEnvAmt) * 0.01f;
        const float envAmt = e * envDepthEff;
        lastLfo = lv;
        lastEnv = e;

        auto mod = [&] (int t, std::atomic<float>* p) { return modulated (t, p->load(), lfoT, envT, lfoAmt, envAmt); };
        const float tIn = hs::dbToGain (mod (tInput, pInput));
        const float tDrv = mod (tDrive, pDrive) * 0.01f;
        const float tCut = std::log2 (mod (tCutoff, pCutoff));
        const float tRes = mod (tReso, pReso) * 0.01f;
        const float tMx = mod (tMix, pMix) * 0.01f;
        const float tOut = hs::dbToGain (mod (tOutput, pOutput));

        for (int i = 0; i < n; ++i)
        {
            const int k = s0 + i;
            aIn[(size_t) k] = smIn.next (tIn);
            aDrive[(size_t) k] = smDrive.next (tDrv);
            aCut[(size_t) k] = std::exp2 (smCutLog.next (tCut));
            aReso[(size_t) k] = smReso.next (tRes);
            aMix[(size_t) k] = smMix.next (tMx);
            aOut[(size_t) k] = smOut.next (tOut);
            aFOn[(size_t) k] = smFOn.next (fOnTarget);
        }

        segLowF[(size_t) seg] = std::exp2 (smLowF.next (std::log2 (mod (tLowFreq, pLowFreq))));
        segLowG[(size_t) seg] = smLowG.next (mod (tLowGain, pLowGain));
        segHighF[(size_t) seg] = std::exp2 (smHighF.next (std::log2 (mod (tHighFreq, pHighFreq))));
        segHighG[(size_t) seg] = smHighG.next (mod (tHighGain, pHighGain));
    }
    lfoMeter.store (lastLfo);
    envMeter.store (lastEnv);

    // ---- input gain
    for (int c = 0; c < numCh; ++c)
        juce::FloatVectorOperations::multiply (ch[c], aIn.data(), len);

    // ---- drive + filter at 4x
    juce::dsp::AudioBlock<float> block (ch, (size_t) numCh, (size_t) len);
    auto up = oversampler->processSamplesUp (block);
    float* u[2] = { up.getChannelPointer (0), numCh > 1 ? up.getChannelPointer (1) : nullptr };
    const float osRate = (float) (fs * osFactor);
    const int cCur = driver.current(), cPrev = driver.previous();

    for (int j = 0; j < len; ++j)
    {
        const float dn = aDrive[(size_t) j];
        const float gCur = hs::DriveStage::gainFor (cCur, dn);
        const float gPrev = driver.isFading() ? hs::DriveStage::gainFor (cPrev, dn) : gCur;
        const float g = hs::tptG (aCut[(size_t) j], osRate);
        const float rs = aReso[(size_t) j];
        const float fon = aFOn[(size_t) j];

        for (int k = 0; k < osFactor; ++k)
        {
            const int idx = j * osFactor + k;
            for (int c = 0; c < numCh; ++c)
            {
                const float y = driver.process (c, u[c][idx], gCur, gPrev);
                const float f = filter.process (c, y, g, rs);
                u[c][idx] = y + (f - y) * fon;
            }
            driver.advance();
            filter.advance();
        }
    }
    oversampler->processSamplesDown (block);

    // ---- EQ, wet/dry, output
    seg = 0;
    for (int s0 = 0; s0 < len; s0 += controlStep, ++seg)
    {
        const int n = juce::jmin (controlStep, len - s0);
        eq.set (segLowF[(size_t) seg], segLowG[(size_t) seg], segHighF[(size_t) seg], segHighG[(size_t) seg]);
        for (int c = 0; c < numCh; ++c)
        {
            const float* d = dryBuf.getReadPointer (c);
            for (int i = 0; i < n; ++i)
            {
                const int k = s0 + i;
                const float w = eq.process (c, ch[c][k]);
                float y = (d[k] + (w - d[k]) * aMix[(size_t) k]) * aOut[(size_t) k];
                if (! std::isfinite (y)) y = 0.0f;
                ch[c][k] = y;
            }
        }
    }
}

// =============================================================================================== programs / state
int HeatsinkProcessor::getNumPrograms() { return (int) factoryPresets().size(); }

const juce::String HeatsinkProcessor::getProgramName (int index)
{
    const auto& p = factoryPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? juce::String (p[(size_t) index].name) : juce::String();
}

void HeatsinkProcessor::setCurrentProgram (int index)
{
    const auto& presets = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    currentProgram = index;

    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (const auto& v : presets[(size_t) index].values)
        if (auto* rp = apvts.getParameter (v.id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (v.value));
}

void HeatsinkProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void HeatsinkProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentProgram = juce::jlimit (0, getNumPrograms() - 1, (int) state.getProperty ("program", 0));
            apvts.replaceState (state);
        }
}

juce::AudioProcessorEditor* HeatsinkProcessor::createEditor() { return new HeatsinkEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HeatsinkProcessor(); }
