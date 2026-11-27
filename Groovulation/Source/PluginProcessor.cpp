#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Scales.h"

using namespace juce;

namespace
{
    enum P { pOn, pSolo, pGain, pPan, pAtk, pDec, pSus, pRel, pPitch,
             pHpOn, pHpCut, pHpRes, pBpOn, pBpCut, pBpRes, pLpOn, pLpCut, pLpRes, pDirt,
             pChOn, pChMode, pChRate, pChDepth, pChMix, pWidth, pMono, pLmOn, pLmStart, pLmEnd,
             pDtime, pDfb, pDmix, pDpp, pRsize, pRdecay, pRdamp, pRpre, pRmix,
             pL1rate, pL1shape, pL1depth, pL1target, pL2rate, pL2shape, pL2depth, pL2target, pL3rate, pL3shape, pL3depth, pL3target,
             pGrid, pSteps, pSlice, pSstart, pSend, pFade, pVola, pKeysrc, pSlLoop, pCount };
    static_assert (pCount <= 64, "raise the size of GroovulationProcessor::mp");
    const char* const pNames[pCount] { "on", "solo", "gain", "pan", "atk", "dec", "sus", "rel", "pitch",
        "hpOn", "hpCut", "hpRes", "bpOn", "bpCut", "bpRes", "lpOn", "lpCut", "lpRes", "dirt",
        "chOn", "chMode", "chRate", "chDepth", "chMix", "width", "mono", "lmOn", "lmStart", "lmEnd",
        "dtime", "dfb", "dmix", "dpp", "rsize", "rdecay", "rdamp", "rpre", "rmix",
        "lfo1rate", "lfo1shape", "lfo1depth", "lfo1target", "lfo2rate", "lfo2shape", "lfo2depth", "lfo2target",
        "lfo3rate", "lfo3shape", "lfo3depth", "lfo3target",
        "grid", "steps", "slice", "sstart", "send", "fade", "vola", "keysrc", "slloop" };

}

AudioProcessor::BusesProperties GroovulationProcessor::makeBuses()
{
    {
        auto b = BusesProperties().withOutput ("Main", AudioChannelSet::stereo(), true);
        for (int m = 0; m < lb::numModules; ++m) b = b.withOutput (lb::moduleName (m), AudioChannelSet::stereo(), false);
        return b;
    }
}

GroovulationProcessor::GroovulationProcessor()
    : AudioProcessor (makeBuses()), apvts (*this, &undoManager, "PARAMS", lb::createLayout())
{
    for (int m = 0; m < lb::numModules; ++m)
    {
        for (int k = 0; k < pCount; ++k)
        {
            mp[(size_t) m][(size_t) k] = apvts.getRawParameterValue (lb::pid (m, pNames[k]));
            jassert (mp[(size_t) m][(size_t) k] != nullptr);
        }
        volPoints[(size_t) m] = { { 0.0f, volUnityY }, { 1.0f, volUnityY } };
        volShared[(size_t) m] = std::make_shared<VolEnvelope> (volPoints[(size_t) m]);
        engines[(size_t) m].setVolEnvelope (volShared[(size_t) m]);
        engines[(size_t) m].setPercussive (lb::isPercussive (m));
        similarAnchor[(size_t) m] = -1;
        applied[(size_t) m] = 0.0f;
    }
    library->addChangeListener (this);
    startTimer (400);   // first loops (random) unless the host restores a state first
}

GroovulationProcessor::~GroovulationProcessor()
{
    alive.reset();
    library->removeChangeListener (this);
    loader.removeAllJobs (true, 4000);
}

bool GroovulationProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet() != AudioChannelSet::stereo()) return false;
    for (int i = 1; i < l.outputBuses.size(); ++i)
        if (! l.outputBuses[i].isDisabled() && l.outputBuses[i] != AudioChannelSet::stereo()) return false;
    for (auto& in : l.inputBuses) if (! in.isDisabled()) return false;
    return true;
}

void GroovulationProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const bool rateChanged = std::abs (sampleRate - sr) > 0.5;
    sr = sampleRate; maxBlock = jmax (16, samplesPerBlock);
    for (auto& e : engines) e.prepare (sr, maxBlock);
    modBuf.setSize (2, maxBlock);
    masterFilter.prepare (sr);
    beatRepeat.prepare (sr);
    deck.prepare (sr);
    setLatencySamples (0);
    // loops are stored at the playback rate: reload them when the rate changes
    if (rateChanged)
        MessageManager::callAsync ([safe = std::weak_ptr<bool> (alive), this]
        {
            if (safe.expired()) return;
            for (int m = 0; m < lb::numModules; ++m) loadPath (m, paths[(size_t) m]);
        });
    else
        for (int m = 0; m < lb::numModules; ++m)
            if (auto l = std::atomic_load (&shown[(size_t) m])) engines[(size_t) m].setLoop (l);
}

//==============================================================================
ModuleParams GroovulationProcessor::readParams (int m) const
{
    const auto& a = mp[(size_t) m];
    auto v = [&a] (int k) { return a[(size_t) k]->load(); };
    ModuleParams p;
    p.on = v (pOn) > 0.5f; p.gainDb = v (pGain); p.pan = v (pPan);
    p.atkMs = v (pAtk); p.decMs = v (pDec); p.sus = v (pSus); p.relMs = v (pRel);
    p.pitch = v (pPitch); p.tempo = 1.0f;
    p.filt.on[0] = v (pHpOn) > 0.5f; p.filt.freq[0] = v (pHpCut); p.filt.res[0] = v (pHpRes);
    p.filt.on[1] = v (pBpOn) > 0.5f; p.filt.freq[1] = v (pBpCut); p.filt.res[1] = v (pBpRes);
    p.filt.on[2] = v (pLpOn) > 0.5f; p.filt.freq[2] = v (pLpCut); p.filt.res[2] = v (pLpRes);
    p.dirt = v (pDirt);
    p.chOn = v (pChOn) > 0.5f; p.chMode = (int) v (pChMode); p.chRate = v (pChRate); p.chDepth = v (pChDepth); p.chMix = v (pChMix);
    p.width = v (pWidth); p.mono = v (pMono) > 0.5f;
    p.lmOn = v (pLmOn) > 0.5f; p.lmStart = v (pLmStart); p.lmEnd = v (pLmEnd);
    p.dtime = (int) v (pDtime); p.dfb = v (pDfb); p.dmix = v (pDmix); p.dpp = v (pDpp) > 0.5f;
    p.rsize = v (pRsize); p.rdecay = v (pRdecay); p.rdamp = v (pRdamp); p.rpre = v (pRpre); p.rmix = v (pRmix);
    p.lfoRate[0] = (int) v (pL1rate); p.lfoShape[0] = (int) v (pL1shape); p.lfoDepth[0] = v (pL1depth); p.lfoTarget[0] = (int) v (pL1target);
    p.lfoRate[1] = (int) v (pL2rate); p.lfoShape[1] = (int) v (pL2shape); p.lfoDepth[1] = v (pL2depth); p.lfoTarget[1] = (int) v (pL2target);
    p.lfoRate[2] = (int) v (pL3rate); p.lfoShape[2] = (int) v (pL3shape); p.lfoDepth[2] = v (pL3depth); p.lfoTarget[2] = (int) v (pL3target);
    p.grid = (int) v (pGrid); p.steps = (int) v (pSteps);
    p.slice = v (pSlice) > 0.5f; p.sStart = v (pSstart); p.sEnd = v (pSend); p.fade = v (pFade) > 0.5f; p.volA = v (pVola) > 0.5f;
    p.slLoop = v (pSlLoop) > 0.5f;
    return p;
}

float GroovulationProcessor::computeSemitones (int m, const LoopData* l, const ModuleParams& p, bool raw) const
{
    const bool scaleOn = apvts.getRawParameterValue ("scaleOn")->load() > 0.5f;
    const bool tonal = lb::isTonal (m) || (m == lb::custom && l != nullptr && l->info.voiced >= 0.45f);
    if (raw || ! scaleOn || ! tonal || l == nullptr) return p.pitch;   // MANUAL / RAW: original pitch + knob
    const bool byName = mp[(size_t) m][pKeysrc]->load() > 0.5f;
    int root; std::array<float, 12> chroma {};
    float tune = 0.0f;
    if (byName && l->info.nameRoot >= 0)
    {
        root = l->info.nameRoot;
        const auto msk = scales::mask (root, l->info.nameMinor ? 1 : 0);
        for (int i = 0; i < 12; ++i) chroma[(size_t) i] = msk[(size_t) i] ? (i == root ? 2.0f : 1.0f) : 0.0f;
        float s = 0; for (auto c : chroma) s += c; for (auto& c : chroma) c /= s;
    }
    else
    {
        if (l->info.keyRoot < 0) return p.pitch;
        root = l->info.keyRoot; chroma = l->info.chroma; tune = l->info.tuneCents;
    }
    const int sRoot = (int) apvts.getRawParameterValue ("scaleRoot")->load();
    const int sType = (int) apvts.getRawParameterValue ("scaleType")->load();
    const int shift = scales::bestShift (chroma, root, sRoot, sType);
    // manual PITCH still works: the result is moved to the nearest scale note
    const int total = shift + (int) std::lround (p.pitch);
    const int snap = scales::snapMove (((root + total) % 12 + 12) % 12, scales::mask (sRoot, sType));
    return (float) (total + snap) - tune / 100.0f;
}

void GroovulationProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    midi.clear();
    const int n = buffer.getNumSamples();
    for (int c = 0; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    if (n > modBuf.getNumSamples()) modBuf.setSize (2, n, false, false, true);

    // ---------------- transport
    double ppq = 0, bpm = apvts.getRawParameterValue ("bpm")->load();
    bool playing = false, hostHasPpq = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition(); q && pos->getIsPlaying()) { ppq = *q; playing = true; hostHasPpq = true; }
        }
    if (! hostHasPpq && apvts.getRawParameterValue ("run")->load() > 0.5f)
    {
        if (! wasPlaying) internalPpq = 0.0;
        ppq = internalPpq; playing = true;
    }
    const double pps = bpm / 60.0 / sr;
    const bool jumped = playing && (! wasPlaying || std::abs (ppq - expectedPpq) > 0.02);
    expectedPpq = ppq + pps * n;
    internalPpq = ppq + pps * n;
    wasPlaying = playing;
    bpmNow = bpm; playingNow = playing;

    BlockContext ctx;
    ctx.playing = playing; ctx.jumped = jumped; ctx.ppq = ppq; ctx.ppqPerSample = pps; ctx.bpm = bpm;
    ctx.raw = apvts.getRawParameterValue ("raw")->load() > 0.5f;
    ctx.swing = apvts.getRawParameterValue ("swingOn")->load() > 0.5f ? apvts.getRawParameterValue ("swing")->load() : 0.0f;
    // turntable: pitch fader and 33/45 change tempo AND pitch of everything (like a real record)
    const int deckType = (int) apvts.getRawParameterValue ("deck")->load();
    if (deckType == 2)
        ctx.rateMul = (apvts.getRawParameterValue ("vinyl45")->load() > 0.5f ? 45.0 / (100.0 / 3.0) : 1.0)
                    * (1.0 + apvts.getRawParameterValue ("vinylPitch")->load() / 100.0);
    if ((lastDeckType == 2 && deckType != 2) || resync.exchange (false)) ctx.jumped = playing;   // back in sync with the host
    lastDeckType = deckType;
    ctx.swingGrid = (int) apvts.getRawParameterValue ("swingGrid")->load();

    bool anySolo = false;
    for (int m = 0; m < lb::numModules; ++m) anySolo |= mp[(size_t) m][pSolo]->load() > 0.5f;

    auto mainBus = getBusBuffer (buffer, false, 0);
    float* mainL = mainBus.getWritePointer (0);
    float* mainR = mainBus.getWritePointer (1);
    for (int m = 0; m < lb::numModules; ++m)
    {
        if (auto nl = std::atomic_exchange (&pending[(size_t) m], LoopPtr())) engines[(size_t) m].setLoop (nl);
        if (clearRequest[(size_t) m].exchange (false)) engines[(size_t) m].setLoop (nullptr);
        auto& e = engines[(size_t) m];
        ModuleParams p = readParams (m);
        if (anySolo && mp[(size_t) m][pSolo]->load() < 0.5f) p.on = false;
        ctx.semitones = computeSemitones (m, e.getLoop().get(), p, ctx.raw);
        applied[(size_t) m] = ctx.semitones;
        float* l = modBuf.getWritePointer (0); float* r = modBuf.getWritePointer (1);
        e.process (l, r, n, ctx, p);
        // multi-out: a module with its own (enabled) output bus is routed there instead of the main mix
        const int bus = m + 1;
        if (bus < getBusCount (false) && getBus (false, bus) != nullptr && getBus (false, bus)->isEnabled())
        {
            auto aux = getBusBuffer (buffer, false, bus);
            if (aux.getNumChannels() >= 2) { aux.copyFrom (0, 0, l, n); aux.copyFrom (1, 0, r, n); continue; }
        }
        FloatVectorOperations::add (mainL, l, n);
        FloatVectorOperations::add (mainR, r, n);
    }

    // ---------------- master: cutoff HP -> BP -> LP (independent, each with ON and resonance).
    // Per-sample smoothing (~3 ms) and per-sample coefficients (was: one step per host block): the sound follows
    // the knobs immediately, also on fast movements and with large host buffers.
    {
        auto rv = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
        fx::TriFilterParams fp;
        fp.on[0] = rv ("mHpOn") > 0.5f; fp.freq[0] = rv ("mHp");    fp.res[0] = rv ("mHpRes");
        fp.on[1] = rv ("mBpOn") > 0.5f; fp.freq[1] = rv ("mBpCut"); fp.res[1] = rv ("mBpRes");
        fp.on[2] = rv ("mLpOn") > 0.5f; fp.freq[2] = rv ("mLp");    fp.res[2] = rv ("mLpRes");
        masterFilter.process (mainL, mainR, n, fp);
    }

    // ---------------- remix: beat repeat, tape / vinyl deck
    if (apvts.getRawParameterValue ("brOn")->load() > 0.5f)
    {
        fx::BeatRepeatParams b;
        b.intervalBeats = lb::brIntervalBeats ((int) apvts.getRawParameterValue ("brInterval")->load());
        b.offsetBeats = apvts.getRawParameterValue ("brOffset")->load() * 0.25;
        if (b.offsetBeats >= b.intervalBeats) b.offsetBeats = std::fmod (b.offsetBeats, b.intervalBeats);
        b.gridBeats = lb::brGridBeats ((int) apvts.getRawParameterValue ("brGrid")->load());
        b.gateBeats = apvts.getRawParameterValue ("brGate")->load() * 0.25;
        b.variation = apvts.getRawParameterValue ("brVar")->load();
        b.chance = apvts.getRawParameterValue ("brChance")->load();
        b.decay = apvts.getRawParameterValue ("brDecay")->load();
        b.pitch = apvts.getRawParameterValue ("brPitch")->load();
        b.pitchDecay = apvts.getRawParameterValue ("brPDecay")->load();
        b.mode = (int) apvts.getRawParameterValue ("brMode")->load();
        b.volume = fx::dbToGain (apvts.getRawParameterValue ("brVol")->load());
        beatRepeat.process (mainL, mainR, n, ppq, pps, playing, b);
    }
    else beatRepeat.reset();
    deck.process (mainL, mainR, n, deckType, apvts.getRawParameterValue ("deckCtl")->load(), bpm,
                  apvts.getRawParameterValue ("vinylMotor")->load() > 0.5f);

    // master: -6 dB mix-bus headroom (12 modules summed) + gain, then a soft safety clipper (transparent below -1 dBFS)
    const float mg = fx::dbToGain (apvts.getRawParameterValue ("master")->load()) * 0.5f;
    mainBus.applyGain (mg);
    for (int c = 0; c < 2; ++c)
    {
        auto* d = mainBus.getWritePointer (c);
        for (int i = 0; i < n; ++i)
        {
            const float x = d[i], a = std::abs (x);
            if (a > 0.89f) d[i] = std::copysign (0.89f + 0.11f * std::tanh ((a - 0.89f) / 0.11f), x);
        }
    }
    float pk = 0.0f;
    for (int c = 0; c < 2; ++c) pk = jmax (pk, mainBus.getMagnitude (c, 0, n));
    masterPeak = jmax (pk, masterPeak.load() * 0.9f);
}

//==============================================================================
std::vector<int> GroovulationProcessor::candidatesFor (int m) const
{
    auto d = library->db();
    return library->candidates (*d, m, bpmNow.load());
}

void GroovulationProcessor::loadAsync (int m, const LoopEntry& e)
{
    const int gen = ++loadGen[(size_t) m];
    paths[(size_t) m] = e.path;
    const double rate = sr;
    loader.addJob ([this, m, e, gen, rate]
    {
        auto l = LoopLibrary::load (e, rate);
        if (l == nullptr) return;
        MessageManager::callAsync ([this, m, l, gen, safe = std::weak_ptr<bool> (alive)]
        {
            if (safe.expired() || gen != loadGen[(size_t) m]) return;
            std::atomic_store (&shown[(size_t) m], l);
            std::atomic_store (&pending[(size_t) m], l);
            uiState.setProperty ("loopsChanged", (int) uiState.getProperty ("loopsChanged", 0) + 1, nullptr);
        });
    });
}

void GroovulationProcessor::loadPath (int m, const String& path)
{
    if (path.isEmpty()) return;
    auto d = library->db();
    if (const int i = d->indexOf (path); i >= 0 && m != lb::custom)
    {
        // a loop saved in a session (or by an older version) that now belongs to another module is not reloaded here
        if (d->entries[(size_t) i].info.type != m) { randomLoop (m); return; }
        loadAsync (m, d->entries[(size_t) i]);
        return;
    }
    if (! File (path).existsAsFile()) { if (m != lb::custom) randomLoop (m); return; }
    // file outside the scanned folders (CUSTOM, moved files): analyse it in the background, then load
    const int gen = ++loadGen[(size_t) m];
    paths[(size_t) m] = path;
    loader.addJob ([this, m, path, gen, safe = std::weak_ptr<bool> (alive)]
    {
        AudioFormatManager fm; fm.registerBasicFormats();
        LoopEntry e; e.path = path; e.name = File (path).getFileNameWithoutExtension();
        e.info = analysis::analyse (fm, File (path), path);
        if (! e.info.ok) return;
        MessageManager::callAsync ([this, m, e, gen, safe]
        {
            if (safe.expired() || gen != loadGen[(size_t) m]) return;
            if (m != lb::custom && e.info.type != m) { randomLoop (m); return; }   // not a loop for this module
            loadAsync (m, e);
        });
    });
}

void GroovulationProcessor::loadCustomFile (const File& f)
{
    if (f.existsAsFile()) loadPath (lb::custom, f.getFullPathName());
}

void GroovulationProcessor::clearCustom()
{
    ++loadGen[(size_t) lb::custom];
    paths[(size_t) lb::custom] = {};
    std::atomic_store (&shown[(size_t) lb::custom], LoopPtr());
    clearRequest[(size_t) lb::custom] = true;
}

void GroovulationProcessor::selectLoop (int m, int index)
{
    auto d = library->db();
    if (index < 0 || index >= (int) d->entries.size()) return;
    loadAsync (m, d->entries[(size_t) index]);
}

void GroovulationProcessor::stepLoop (int m, int dir)
{
    if (m == lb::custom) return;   // CUSTOM holds the user's own file
    auto d = library->db();
    const auto c = library->candidates (*d, m, bpmNow.load());
    if (c.empty()) return;
    const int cur = d->indexOf (paths[(size_t) m]);
    auto it = std::find (c.begin(), c.end(), cur);
    int pos = it == c.end() ? (dir > 0 ? -1 : 0) : (int) (it - c.begin());
    pos = (pos + dir + (int) c.size()) % (int) c.size();
    similarAnchor[(size_t) m] = -1;
    selectLoop (m, c[(size_t) pos]);
}

void GroovulationProcessor::randomLoop (int m)
{
    if (m == lb::custom) return;   // CUSTOM holds the user's own file
    auto d = library->db();
    const auto c = library->candidates (*d, m, bpmNow.load());
    if (c.empty()) return;
    const int cur = d->indexOf (paths[(size_t) m]);
    int pick = c[(size_t) Random::getSystemRandom().nextInt ((int) c.size())];
    if (pick == cur && c.size() > 1) pick = c[(size_t) ((std::find (c.begin(), c.end(), pick) - c.begin() + 1) % (int) c.size())];
    similarAnchor[(size_t) m] = -1;
    selectLoop (m, pick);
}

void GroovulationProcessor::similarLoop (int m)
{
    if (m == lb::custom) return;   // CUSTOM holds the user's own file
    auto d = library->db();
    const auto c = library->candidates (*d, m, bpmNow.load());
    const int cur = d->indexOf (paths[(size_t) m]);
    if (c.empty() || cur < 0) { randomLoop (m); return; }
    auto& visited = similarVisited[(size_t) m];
    // the anchor is the loop that was loaded when SIMILAR was first pressed; each press goes one step further away
    if (similarAnchor[(size_t) m] < 0 || d->indexOf (paths[(size_t) m]) != cur || visited.count (cur) == 0)
    {
        if (similarAnchor[(size_t) m] < 0 || visited.count (cur) == 0) { similarAnchor[(size_t) m] = cur; visited.clear(); visited.insert (cur); }
    }
    int next = LoopLibrary::nearest (*d, similarAnchor[(size_t) m], visited, c);
    if (next < 0) { visited.clear(); visited.insert (similarAnchor[(size_t) m]); next = LoopLibrary::nearest (*d, similarAnchor[(size_t) m], visited, c); }
    if (next < 0) return;
    visited.insert (next);
    selectLoop (m, next);
}

void GroovulationProcessor::randomAll()
{
    for (int m = 0; m < lb::numModules; ++m) randomLoop (m);
}

void GroovulationProcessor::setVolEnvelope (int m, const VolEnvelope& e)
{
    volPoints[(size_t) m] = e;
    std::sort (volPoints[(size_t) m].begin(), volPoints[(size_t) m].end(), [] (auto& a, auto& b) { return a.x < b.x; });
    auto sp = std::make_shared<VolEnvelope> (volPoints[(size_t) m]);
    engines[(size_t) m].setVolEnvelope (sp);   // shared_ptr swap; read once per block on the audio thread
    volShared[(size_t) m] = sp;
}

void GroovulationProcessor::changeListenerCallback (ChangeBroadcaster*)
{
    // the library finished a scan: fill empty modules with random loops
    bool anyEmpty = false;
    for (auto& p : paths) anyEmpty |= p.isEmpty();
    if (anyEmpty && (stateRestored || started))
        for (int m = 0; m < lb::numModules; ++m) if (paths[(size_t) m].isEmpty() && m != lb::custom) randomLoop (m);
    // after a (re)scan: a module whose loop now belongs to another module gets a fitting one
    auto d = library->db();
    for (int m = 0; m < lb::numModules; ++m)
    {
        if (m == lb::custom || paths[(size_t) m].isEmpty()) continue;
        const int i = d->indexOf (paths[(size_t) m]);
        if (i >= 0 && d->entries[(size_t) i].info.type != m) randomLoop (m);
    }
    uiState.setProperty ("dbChanged", (int) uiState.getProperty ("dbChanged", 0) + 1, nullptr);
}

void GroovulationProcessor::timerCallback()
{
    // SLICE off -> SLICE LOOP off too (also when SLICE is switched off by automation / undo)
    for (int m = 0; m < lb::numModules; ++m)
        if (mp[(size_t) m][pSlice]->load() < 0.5f && mp[(size_t) m][pSlLoop]->load() > 0.5f)
            if (auto* prm = apvts.getParameter (lb::pid (m, "slloop")))
            {
                prm->beginChangeGesture(); prm->setValueNotifyingHost (0.0f); prm->endChangeGesture();
            }
    if (! started)
    {
        started = true;
        if (! stateRestored) randomAll();
        startTimer (500);
    }
    updatePools();
}

// LFO -> Loop: each module that uses it gets a pool of up to 8 similar loops (max 8 bars) preloaded in RAM,
// so the engine can switch loops instantly (quantised to the grid, crossfaded) without touching the disk.
void GroovulationProcessor::updatePools()
{
    auto d = library->db();
    for (int m = 0; m < lb::numModules; ++m)
    {
        const bool wants = readParams (m).usesLoopLfo() && m != lb::custom;
        const String key = wants ? paths[(size_t) m] + "@" + String (sr) : String();
        if (key == poolKey[(size_t) m]) continue;
        poolKey[(size_t) m] = key;
        if (! wants) { engines[(size_t) m].setPool (nullptr); continue; }
        const int cur = d->indexOf (paths[(size_t) m]);
        if (cur < 0) continue;
        std::vector<int> pool { cur };
        std::vector<int> cand;
        for (int i : library->candidates (*d, m, bpmNow.load())) if (d->entries[(size_t) i].info.beats <= 32) cand.push_back (i);
        std::set<int> used { cur };
        while ((int) pool.size() < 8)
        {
            const int nx = LoopLibrary::nearest (*d, cur, used, cand);
            if (nx < 0) break;
            used.insert (nx); pool.push_back (nx);
        }
        std::vector<LoopEntry> entries;
        for (int i : pool) entries.push_back (d->entries[(size_t) i]);
        const double rate = sr;
        loader.addJob ([this, m, entries, rate, key, safe = std::weak_ptr<bool> (alive)]
        {
            auto pl = std::make_shared<LoopPool>();
            for (auto& e : entries) if (auto l = LoopLibrary::load (e, rate)) pl->push_back (l);
            MessageManager::callAsync ([this, m, pl, key, safe]
            {
                if (safe.expired() || poolKey[(size_t) m] != key) return;
                engines[(size_t) m].setPool (pl);
            });
        });
    }
}

//==============================================================================
void GroovulationProcessor::setSnapshotName (const String& nm)
{
    snapName = File::createLegalFileName (nm.trim()).isEmpty() ? String ("Untitled") : File::createLegalFileName (nm.trim());
}

void GroovulationProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    ValueTree mods ("MODULES");
    for (int m = 0; m < lb::numModules; ++m)
    {
        ValueTree t ("M");
        t.setProperty ("index", m, nullptr);
        t.setProperty ("path", paths[(size_t) m], nullptr);
        String pts;
        for (auto& p : volPoints[(size_t) m]) pts << String (p.x, 5) << "," << String (p.y, 5) << ";";
        t.setProperty ("vol", pts, nullptr);
        mods.appendChild (t, nullptr);
    }
    state.removeChild (state.getChildWithName ("MODULES"), nullptr);
    state.appendChild (mods, nullptr);
    state.setProperty ("snapshot", snapName, nullptr);
    state.removeChild (state.getChildWithName ("UI"), nullptr);
    ValueTree ui ("UI");
    ui.setProperty ("scale", uiState.getProperty ("scale", 0.75), nullptr);
    ui.setProperty ("selected", uiState.getProperty ("selected", 0), nullptr);
    state.appendChild (ui, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void GroovulationProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<XmlElement> xml (getXmlFromBinary (data, size));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = ValueTree::fromXml (*xml);
    stateRestored = true;
    snapName = state.getProperty ("snapshot", "Untitled").toString();
    if (auto ui = state.getChildWithName ("UI"); ui.isValid())
    {
        uiState.setProperty ("scale", ui.getProperty ("scale", 0.75), nullptr);
        uiState.setProperty ("selected", ui.getProperty ("selected", 0), nullptr);
    }
    auto mods = state.getChildWithName ("MODULES");
    std::array<String, lb::numModules> wanted;
    for (int i = 0; i < mods.getNumChildren(); ++i)
    {
        auto t = mods.getChild (i);
        const int m = t.getProperty ("index", -1);
        if (m < 0 || m >= lb::numModules) continue;
        wanted[(size_t) m] = t.getProperty ("path", "").toString();
        VolEnvelope e;
        StringArray pts; pts.addTokens (t.getProperty ("vol", "").toString(), ";", ""); pts.removeEmptyStrings();
        for (auto& s : pts) e.push_back ({ s.upToFirstOccurrenceOf (",", false, false).getFloatValue(), s.fromFirstOccurrenceOf (",", false, false).getFloatValue() });
        if (e.size() < 2) e = { { 0.0f, volUnityY }, { 1.0f, volUnityY } };
        setVolEnvelope (m, e);
    }
    state.removeChild (mods, nullptr);
    state.removeChild (state.getChildWithName ("UI"), nullptr);
    apvts.replaceState (state);
    // loops: by path (re-analysed if needed); missing files -> random loop of the right type
    auto apply = [this, wanted]
    {
        for (int m = 0; m < lb::numModules; ++m)
        {
            if (wanted[(size_t) m].isNotEmpty()) loadPath (m, wanted[(size_t) m]);
            else if (m == lb::custom) clearCustom();
            else randomLoop (m);
        }
    };
    if (MessageManager::getInstance()->isThisTheMessageThread()) apply();
    else MessageManager::callAsync ([apply, safe = std::weak_ptr<bool> (alive)] { if (! safe.expired()) apply(); });
}

bool GroovulationProcessor::saveSnapshot (const String& name)
{
    setSnapshotName (name);
    MemoryBlock mb; getStateInformation (mb);
    LoopLibrary::snapshotFolder().createDirectory();
    std::unique_ptr<XmlElement> xml (getXmlFromBinary (mb.getData(), (int) mb.getSize()));
    return xml != nullptr && xml->writeTo (LoopLibrary::snapshotFolder().getChildFile (snapName + ".groovulation"));
}

bool GroovulationProcessor::loadSnapshot (const File& f)
{
    auto xml = XmlDocument::parse (f);
    if (xml == nullptr) return false;
    MemoryBlock mb; copyXmlToBinary (*xml, mb);
    setStateInformation (mb.getData(), (int) mb.getSize());
    setSnapshotName (f.getFileNameWithoutExtension());
    return true;
}

//==============================================================================
GroovulationProcessor::PrintResult GroovulationProcessor::printModules (std::function<void (float)> progress)
{
    PrintResult res;
    printing = true;
    const double bpm = bpmNow.load(), rate = sr;
    const auto folder = LoopLibrary::printFolder().getChildFile (snapName);
    folder.createDirectory();
    res.folder = folder;
    BlockContext base;
    base.playing = true; base.bpm = bpm; base.ppqPerSample = bpm / 60.0 / rate;
    base.raw = apvts.getRawParameterValue ("raw")->load() > 0.5f;
    base.swing = apvts.getRawParameterValue ("swingOn")->load() > 0.5f ? apvts.getRawParameterValue ("swing")->load() : 0.0f;
    base.swingGrid = (int) apvts.getRawParameterValue ("swingGrid")->load();
    bool anySolo = false;
    for (int m = 0; m < lb::numModules; ++m) anySolo |= mp[(size_t) m][pSolo]->load() > 0.5f;
    ignoreUnused (anySolo);

    for (int m = 0; m < lb::numModules; ++m)
    {
        auto loop = currentLoop (m);
        if (progress) progress ((float) m / lb::numModules);
        if (loop == nullptr) continue;
        ModuleParams p = readParams (m);
        p.on = true;   // print every loaded module, also muted ones
        BlockContext ctx = base;
        ctx.semitones = computeSemitones (m, loop.get(), p, ctx.raw);
        ModuleEngine e;
        e.setPercussive (lb::isPercussive (m));
        constexpr int bs = 512;
        e.prepare (rate, bs);
        e.setLoop (loop);
        e.setVolEnvelope (std::make_shared<VolEnvelope> (volPoints[(size_t) m]));
        const double region = ModuleEngine::regionBeats (*loop, p);
        const double uPerPpq = ctx.raw ? loop->bpm / bpm * p.tempo : p.tempo;   // (q = ppq * tempo from a fresh start)
        const double cyclePpq = region / jmax (1e-6, uPerPpq);
        const int cycle = (int) std::lround (cyclePpq * 60.0 / bpm * rate);
        if (cycle <= 0) continue;
        AudioBuffer<float> out (2, cycle);
        std::vector<float> l (bs), r (bs);
        // first cycle = pre-roll (fills delay / reverb tails), the second one is written -> seamless loop
        const int total = cycle * 2;
        for (int pos = 0; pos < total; pos += bs)
        {
            const int nn = jmin (bs, total - pos);
            ctx.ppq = pos * ctx.ppqPerSample;
            ctx.jumped = pos == 0;
            e.process (l.data(), r.data(), nn, ctx, p);
            for (int i = 0; i < nn; ++i)
                if (pos + i >= cycle) { out.setSample (0, pos + i - cycle, l[(size_t) i]); out.setSample (1, pos + i - cycle, r[(size_t) i]); }
        }
        const auto file = folder.getChildFile (File::createLegalFileName (lb::moduleName (m) + " - " + snapName) + ".wav");
        file.deleteFile();
        WavAudioFormat wav;
        if (auto stream = file.createOutputStream())
        {
            std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (stream.get(), rate, 2, 24, {}, 0));
            if (w != nullptr)
            {
                stream.release();
                w->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
                res.files.add (file);
            }
        }
    }
    if (progress) progress (1.0f);
    res.ok = ! res.files.isEmpty();
    res.message = res.ok ? String (res.files.size()) + " loops printed to " + folder.getFullPathName() : String ("No loops to print");
    printing = false;
    return res;
}

GroovulationProcessor::PrintResult GroovulationProcessor::exportOriginals()
{
    // EXPORT: the original, unprocessed files of the loaded loops -> PRINT/<snapshot>/<Module> - <snapshot> (original).<ext>
    PrintResult res;
    const auto folder = LoopLibrary::printFolder().getChildFile (snapName);
    folder.createDirectory();
    res.folder = folder;
    for (int m = 0; m < lb::numModules; ++m)
    {
        const File src (paths[(size_t) m]);
        if (paths[(size_t) m].isEmpty() || ! src.existsAsFile()) continue;
        const auto dst = folder.getChildFile (File::createLegalFileName (lb::moduleName (m) + " - " + snapName + " (original)") + src.getFileExtension());
        dst.deleteFile();
        if (src.copyFileTo (dst)) res.files.add (dst);
    }
    res.ok = ! res.files.isEmpty();
    res.message = res.ok ? String (res.files.size()) + " original loops exported to " + folder.getFullPathName() : String ("No loops to export");
    return res;
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GroovulationProcessor(); }
