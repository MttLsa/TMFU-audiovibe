#include "LimeEngine.h"
#include <algorithm>

using namespace params;

namespace
{
    const char* partSuffixes[] = { lm::smp, lm::smpOpen, lm::tune, lm::dcy, lm::dcyOpen, lm::link, lm::fmode, lm::cut, lm::res,
                                   lm::lowcut, lm::fmFreq, lm::fmAmt, lm::zap, lm::zapDcy, lm::lvl, lm::pan, lm::send, lm::mute };
    enum { iSmp, iSmpOpen, iTune, iDcy, iDcyOpen, iLink, iFmode, iCut, iRes, iLowcut, iFmFreq, iFmAmt, iZap, iZapDcy, iLvl, iPan, iSend, iMute, iCount };

    int slotToPart (int slot)
    {
        static const int m[] { kick, snare, hat, hat, tie, pod };
        return m[slot];
    }
}

LimeEngine::LimeEngine()
{
    loadDefaultPatterns();
}

std::atomic<float>* LimeEngine::raw (const juce::String& id)
{
    return state->getRawParameterValue (id);
}

void LimeEngine::bind (juce::AudioProcessorValueTreeState& apvts)
{
    state = &apvts;
    for (int pt = 0; pt < numLimeParts; ++pt)
        for (int i = 0; i < iCount; ++i)
            pr[(size_t) pt].p[i] = apvts.getRawParameterValue (b (pt, partSuffixes[i]));   // nullptr if part lacks it

    pPattern = raw (bPattern); pLen = raw (bLen); pTieLen = raw (bTieLen); pPodLen = raw (bPodLen); pModLen = raw (bModLen);
    pTieStr = raw (bTieStr); pPodStr = raw (bPodStr); pModStr = raw (bModStr); pShuffle = raw (bShuffle); pAccent = raw (bAccent);
    pGain = raw (bGain); pPitch = raw (bPitch); pDirt = raw (bDirt); pNorm = raw (bNorm);
    pLfoRate = raw (bLfoRate); pLfoFM = raw (bLfoFM); pLfoDM = raw (bLfoDM);
    pDlyTime = raw (bDlyTime); pDlyFb = raw (bDlyFb); pDlyDamp = raw (bDlyDamp); pDlyLvl = raw (bDlyLvl);
    pFltCut = raw (bFltCut); pFltRes = raw (bFltRes); pModDest = raw (bModDest); pModAmt = raw (bModAmt);
}

void LimeEngine::prepare (double sampleRate)
{
    sr = sampleRate;
    const size_t dlen = (size_t) (sr * 4.5);
    dlyL.assign (dlen, 0.0f); dlyR.assign (dlen, 0.0f);
    dlyW = 0; dlyTimeSm = 0.0f;
    fltL.reset(); fltR.reset();
    for (auto& s : voices) for (auto& v : s) v.active = false;
    busDL.assign (8192, 0.f); busDR.assign (8192, 0.f); busSL.assign (8192, 0.f); busSR.assign (8192, 0.f);
}

SamplePtr LimeEngine::sampleFor (int slot) const
{
    if (auto u = std::atomic_load (&user[(size_t) slot])) return u;
    const int part = slotToPart (slot);
    auto* p = slot == slotHatOpen ? pr[(size_t) part].p[iSmpOpen] : pr[(size_t) part].p[iSmp];
    return SampleBank::get()[p != nullptr ? (int) p->load() : 0];
}

void LimeEngine::setUserSample (int slot, SamplePtr s)
{
    std::atomic_store (&user[(size_t) slot], s);
    if (s == nullptr) userPaths[(size_t) slot].clear();
}

void LimeEngine::audition (int slot, float vel)
{
    if (slot >= 0 && slot < numSlots) auditionQ[(size_t) slot].store (juce::jlimit (1, 1000, (int) (vel * 1000.0f)));
}

void LimeEngine::readParams()
{
    for (int pt = 0; pt < numLimeParts; ++pt)
    {
        auto& r = pr[(size_t) pt].p;
        auto g = [&] (int i, float def) { return r[i] != nullptr ? r[i]->load() : def; };
        auto& p = pp[(size_t) pt];
        p.smp = (int) g (iSmp, 0); p.smpOpen = (int) g (iSmpOpen, 0);
        p.tune = g (iTune, 0); p.dcy = g (iDcy, 50); p.dcyOpen = g (iDcyOpen, 50); p.link = g (iLink, 1) > 0.5f;
        p.fmode = (int) g (iFmode, 0); p.cut = g (iCut, pt == hat ? 20.f : 20000.f); p.res = g (iRes, 0);
        p.lowcut = g (iLowcut, 0) > 0.5f; p.fmFreq = g (iFmFreq, 100); p.fmAmt = g (iFmAmt, 0);
        p.zap = g (iZap, 0); p.zapDcy = g (iZapDcy, 30);
        p.lvl = g (iLvl, 0); p.pan = g (iPan, 0); p.send = g (iSend, 0); p.mute = g (iMute, 0) > 0.5f;
    }
}

void LimeEngine::trigger (int slot, float vel)
{
    const int part = slotToPart (slot);
    const auto& p = pp[(size_t) part];
    if (p.mute) return;

    // Hat "Link": closed & open behave like one monophonic hi-hat (each chokes the other)
    if (part == hat && p.link)
    {
        const int other = slot == slotHatClosed ? slotHatOpen : slotHatClosed;
        for (auto& v : voices[(size_t) other]) if (v.active) v.fading = true;
    }
    // retrigger: fade out the previous voice of this slot (declick), start a new one
    for (auto& v : voices[(size_t) slot]) if (v.active) v.fading = true;

    auto& v = voices[(size_t) slot][(size_t) nextVoice[(size_t) slot]];
    nextVoice[(size_t) slot] = (nextVoice[(size_t) slot] + 1) % voicesPerSlot;

    v.smp = sampleFor (slot);
    if (v.smp == nullptr || v.smp->data.empty()) return;
    v.active = true; v.fading = false; v.fade = 1.0f;
    v.pos = 0.0; v.t = 0.0; v.fmPhase = 0.0;
    v.slot = slot;
    v.flt.reset(); v.hp.reset();

    // per-trigger values, with Mod-lane sample & hold
    const float m = modSmooth;    // -1 … +1 scaled by amount
    const int dest = (int) pModDest->load();
    v.tuneSt   = p.tune;
    v.decayPct = slot == slotHatOpen ? p.dcyOpen : p.dcy;
    v.cutHz    = p.cut;
    v.fmAmt    = p.fmAmt;
    v.zap      = p.zap;
    switch (dest)
    {
        case mdKickTune:   if (part == kick)  v.tuneSt += m * 12.0f; break;
        case mdKickDecay:  if (part == kick)  v.decayPct += m * 50.0f; break;
        case mdSnareTune:  if (part == snare) v.tuneSt += m * 12.0f; break;
        case mdSnareDecay: if (part == snare) v.decayPct += m * 50.0f; break;
        case mdSnareCut:   if (part == snare) v.cutHz *= std::pow (2.0f, m * 4.0f); break;
        case mdHatTune:    if (part == hat)   v.tuneSt += m * 12.0f; break;
        case mdHatDecay:   if (part == hat)   v.decayPct += m * 50.0f; break;
        case mdTieTune:    if (part == tie)   v.tuneSt += m * 12.0f; break;
        case mdTieFM:      if (part == tie)   v.fmAmt += m * 100.0f; break;
        case mdPodTune:    if (part == pod)   v.tuneSt += m * 12.0f; break;
        case mdPodZap:     if (part == pod)   v.zap += m * 100.0f; break;
        default: break;
    }
    v.decayPct = juce::jlimit (0.0f, 100.0f, v.decayPct);
    v.fmAmt = juce::jlimit (0.0f, 100.0f, v.fmAmt);
    v.zap = juce::jlimit (-100.0f, 100.0f, v.zap);
    v.gain = vel;
    hits[(size_t) slot].fetch_add (1);
}

void LimeEngine::renderVoice (Voice& v, float* dL, float* dR, float* sL, float* sR, int n)
{
    if (! v.active) return;
    const int part = slotToPart (v.slot);
    const auto& p = pp[(size_t) part];
    const auto& data = v.smp->data;
    const double len = (double) data.size();
    const double baseRate = v.smp->sourceRate / sr * std::pow (2.0, (v.tuneSt + pPitch->load()) / 12.0);

    // decay: 0..99 % -> exponential time constant 8 ms … 2.5 s; 100 % -> full sample (no envelope)
    const bool fullLen = v.decayPct >= 99.5f;
    const double tau = 0.008 * std::pow (312.0, v.decayPct / 100.0);

    // filters
    const double q = 0.5 + p.res * 0.01 * 9.5;
    int fmode = 0;
    if (part == snare) fmode = p.fmode;
    if (part == hat)   fmode = 2;
    v.flt.set (part == hat ? p.cut : v.cutHz, sr, q);
    const bool useFilter = part == hat ? p.cut > 25.0f : v.cutHz < 19500.0f;
    if (part == kick && p.lowcut) v.hp.set (90.0, sr, 0.7);

    const double fmHz = p.fmFreq;
    const double fmDepth = v.fmAmt * 0.01 * 2.0;     // ±2 octaves
    const double zapTau = 0.004 * std::pow (100.0, p.zapDcy * 0.01);
    const double zapOct = v.zap * 0.01 * 4.0;         // ±4 octaves

    const float lvl = juce::Decibels::decibelsToGain (p.lvl, -60.0f) * v.gain;
    const float panN = (p.pan * 0.01f + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
    const float gl = std::cos (panN) * 1.41421356f * lvl, gr = std::sin (panN) * 1.41421356f * lvl;
    float send = p.send * 0.01f;
    if ((int) pModDest->load() == mdDelaySend) send = juce::jlimit (0.0f, 1.0f, send + modSmooth);

    for (int i = 0; i < n; ++i)
    {
        double rate = baseRate;
        if (part == tie && fmDepth > 0.0)
        {
            v.fmPhase += fmHz / sr;
            rate *= std::pow (2.0, fmDepth * std::sin (2.0 * juce::MathConstants<double>::pi * v.fmPhase));
        }
        if (part == pod && std::abs (zapOct) > 1.0e-9)
            rate *= std::pow (2.0, zapOct * std::exp (-v.t / zapTau));

        const int i0 = (int) v.pos;
        if (i0 >= (int) len - 1) { v.active = false; break; }
        const float fr = (float) (v.pos - i0);
        float x = data[(size_t) i0] + fr * (data[(size_t) i0 + 1] - data[(size_t) i0]);

        const float env = fullLen ? 1.0f : (float) std::exp (-v.t / tau);
        x *= env;
        if (useFilter) x = v.flt.process (x, fmode);
        if (part == kick && p.lowcut) x = v.hp.process (x, 2);
        if (v.fading)
        {
            v.fade *= 0.992f;
            x *= v.fade;
            if (v.fade < 1.0e-3f) { v.active = false; break; }
        }

        dL[i] += x * gl;  dR[i] += x * gr;
        sL[i] += x * gl * send;  sR[i] += x * gr * send;

        v.pos += rate;
        v.t += 1.0 / sr;
        if (! fullLen && env < 1.0e-4f) { v.active = false; break; }
    }
}

void LimeEngine::process (float* L, float* R, int numSamples, const BlockTime& bt, const std::vector<TriggerEvent>& midiEvents)
{
    readParams();
    if (bt.bpm > 1.0) bpm = bt.bpm;

    if ((int) busDL.size() < numSamples)
    {
        busDL.assign ((size_t) numSamples, 0.f); busDR.assign ((size_t) numSamples, 0.f);
        busSL.assign ((size_t) numSamples, 0.f); busSR.assign ((size_t) numSamples, 0.f);
    }
    std::fill (busDL.begin(), busDL.begin() + numSamples, 0.f);
    std::fill (busDR.begin(), busDR.begin() + numSamples, 0.f);
    std::fill (busSL.begin(), busSL.begin() + numSamples, 0.f);
    std::fill (busSR.begin(), busSR.begin() + numSamples, 0.f);

    // ---------------- events
    struct Ev { int offset; int slot; float vel; float mod; bool isMod; };
    std::array<Ev, 512> ev;
    int nev = 0;
    auto push = [&] (Ev e) { if (nev < (int) ev.size()) ev[(size_t) nev++] = e; };

    for (auto& e : midiEvents) push ({ e.offset, e.target, e.velocity, 0.f, false });
    for (int s = 0; s < numSlots; ++s)
        if (const int a = auditionQ[(size_t) s].exchange (0); a > 0) push ({ 0, s, (float) a / 1000.0f, 0.f, false });

    const int wanted = juce::jlimit (0, NP - 1, (int) pPattern->load());
    const int mainLen = juce::jlimit (1, NS, (int) pLen->load());
    const double shuffle = pShuffle->load() * 0.01;
    const float acc = pAccent->load() * 0.01f;
    auto velFor = [acc] (uint8_t f) { return (f & accent) ? 1.0f : 1.0f - 0.6f * acc; };

    if (! bt.playing)
    {
        currentPattern.store (wanted);
        for (auto& s : laneSteps) s.store (-1);
    }
    else
    {
        // main lane: Kick / Snare / Hat
        forEachOnset (bt, 0.25, shuffle, [&] (long long k, double ppq)
        {
            if (k % mainLen == 0 || bt.justStarted) currentPattern.store (wanted);
            const int step = (int) (k % mainLen);
            laneSteps[0].store (step);
            auto& pat = patterns[(size_t) currentPattern.load()];
            const int off = bt.toSample (ppq);
            for (int lane = laneKick; lane <= laneHat; ++lane)
            {
                const uint8_t f = pat.steps[(size_t) lane][(size_t) step].load();
                if (! (f & trig)) continue;
                int slot = lane == laneKick ? slotKick : (lane == laneSnare ? slotSnare : ((f & open) ? slotHatOpen : slotHatClosed));
                push ({ off, slot, velFor (f), 0.f, false });
            }
        });

        // Tie / Pod / Mod lanes: own length, optional stretch to the main length
        auto lane = [&] (int laneIdx, std::atomic<float>* lenP, std::atomic<float>* strP, int uiIdx, bool isMod)
        {
            const int len = juce::jlimit (1, NS, (int) lenP->load());
            const bool stretch = strP->load() > 0.5f;
            const double stepPpq = stretch ? 0.25 * (double) mainLen / (double) len : 0.25;
            forEachOnset (bt, stepPpq, stretch ? 0.0 : shuffle, [&] (long long k, double ppq)
            {
                const int step = (int) (k % len);
                laneSteps[(size_t) uiIdx].store (step);
                auto& pat = patterns[(size_t) currentPattern.load()];
                const int off = bt.toSample (ppq);
                if (isMod)
                    push ({ off, -1, 0.f, pat.mod[(size_t) step].load(), true });
                else
                {
                    const uint8_t f = pat.steps[(size_t) laneIdx][(size_t) step].load();
                    if (f & trig) push ({ off, laneIdx == laneTie ? slotTie : slotPod, velFor (f), 0.f, false });
                }
            });
        };
        lane (laneTie, pTieLen, pTieStr, 1, false);
        lane (lanePod, pPodLen, pPodStr, 2, false);
        lane (-1, pModLen, pModStr, 3, true);
    }

    // mod events first at equal offsets so triggers on the same step see the new value
    std::stable_sort (ev.begin(), ev.begin() + nev, [] (const Ev& x, const Ev& y)
    {
        return x.offset != y.offset ? x.offset < y.offset : (x.isMod && ! y.isMod);
    });

    const float modAmt = pModAmt->load() * 0.01f;
    int pos = 0, ei = 0;
    while (pos < numSamples)
    {
        while (ei < nev && ev[(size_t) ei].offset <= pos)
        {
            const auto& e = ev[(size_t) ei++];
            if (e.isMod) { modHeld = e.mod; modNow.store (e.mod); modSmooth = (modHeld * 2.0f - 1.0f) * modAmt; }
            else if (e.slot >= 0 && e.slot < numSlots) trigger (e.slot, e.vel);
        }
        modSmooth = (modHeld * 2.0f - 1.0f) * modAmt;
        const int next = ei < nev ? juce::jmin (numSamples, ev[(size_t) ei].offset) : numSamples;
        const int n = juce::jmax (1, next - pos);
        for (auto& slotVoices : voices)
            for (auto& v : slotVoices)
                renderVoice (v, busDL.data() + pos, busDR.data() + pos, busSL.data() + pos, busSR.data() + pos, n);
        pos += n;
    }

    // ---------------- FX chain: Delay -> Filter -> Dirt -> Norm -> Gain
    const int dest = (int) pModDest->load();
    const double lfoHz = pLfoRate->load();
    const float lfoFM = pLfoFM->load() * 0.01f, lfoDM = pLfoDM->load() * 0.01f;

    const double beats = delayTimeBeats ((int) pDlyTime->load());
    const float baseDly = (float) juce::jlimit (16.0, (double) dlyL.size() - 4.0, beats * 60.0 / juce::jmax (20.0, bpm) * sr);
    float fb = pDlyFb->load() * 0.01f;
    if (dest == mdDelayFb) fb = juce::jlimit (0.0f, 0.95f, fb + modSmooth * 0.5f);
    const float damp = 0.05f + 0.94f * (1.0f - pDlyDamp->load() * 0.01f);
    const float dlyLvl = juce::Decibels::decibelsToGain (pDlyLvl->load(), -60.0f);
    if (dlyTimeSm <= 0.0f) dlyTimeSm = baseDly;

    float cutBase = pFltCut->load();
    if (dest == mdFilter) cutBase *= std::pow (2.0f, modSmooth * 4.0f);
    const double fq = 0.6 + pFltRes->load() * 0.01 * 9.0;
    const float dirt = pDirt->load() * 0.01f;
    const bool norm = pNorm->load() > 0.5f;
    const float gain = juce::Decibels::decibelsToGain (pGain->load(), -60.0f);
    const int dsz = (int) dlyL.size();
    const double lfoInc = lfoHz / sr;

    for (int i = 0; i < numSamples; ++i)
    {
        lfoPhase += lfoInc;
        if (lfoPhase >= 1.0) lfoPhase -= 1.0;
        const float lfo = (float) std::sin (2.0 * juce::MathConstants<double>::pi * lfoPhase);

        // delay with LFO time modulation (DM)
        const float target = baseDly * (1.0f + 0.25f * lfoDM * lfo);
        dlyTimeSm += 0.0005f * (target - dlyTimeSm);
        float rp = (float) dlyW - dlyTimeSm;
        while (rp < 0) rp += (float) dsz;
        const int r0 = (int) rp; const float rf = rp - (float) r0; const int r1 = (r0 + 1) % dsz;
        const float yL = dlyL[(size_t) r0] + rf * (dlyL[(size_t) r1] - dlyL[(size_t) r0]);
        const float yR = dlyR[(size_t) r0] + rf * (dlyR[(size_t) r1] - dlyR[(size_t) r0]);
        dampL += damp * (yL - dampL);
        dampR += damp * (yR - dampR);
        // ping-pong cross feedback
        dlyL[(size_t) dlyW] = busSL[(size_t) i] + dampR * fb;
        dlyR[(size_t) dlyW] = busSR[(size_t) i] + dampL * fb;
        dlyW = (dlyW + 1) % dsz;

        float l = busDL[(size_t) i] + yL * dlyLvl;
        float r = busDR[(size_t) i] + yR * dlyLvl;

        // filter (LFO FM = cutoff modulation)
        if ((i & 15) == 0)
        {
            const double c = juce::jlimit (30.0, 20000.0, (double) cutBase * std::pow (2.0, 3.0 * lfoFM * lfo));
            fltL.set (c, sr, fq); fltR.set (c, sr, fq);
        }
        if (cutBase < 19500.0f || lfoFM > 0.0f)
        {
            l = fltL.process (l, 0);
            r = fltR.process (r, 0);
        }

        // Dirt: asymmetric saturation
        if (dirt > 0.001f)
        {
            const float d = 1.0f + dirt * 14.0f, bias = 0.15f * dirt;
            const float comp = 1.0f / (1.0f + dirt * 2.0f);
            l = (std::tanh (l * d + bias) - std::tanh (bias)) * comp * 1.6f;
            r = (std::tanh (r * d + bias) - std::tanh (bias)) * comp * 1.6f;
        }
        // Norm: loudness maximiser (fast compressor + soft clip)
        if (norm)
        {
            const float pk = juce::jmax (std::abs (l), std::abs (r));
            normEnv = pk > normEnv ? normEnv + 0.3f * (pk - normEnv) : normEnv * 0.9997f;
            const float g = normEnv > 0.35f ? 0.35f / normEnv : 1.0f;
            l = std::tanh (l * g * 2.4f) * 0.9f;
            r = std::tanh (r * g * 2.4f) * 0.9f;
        }
        L[i] += l * gain;
        R[i] += r * gain;
    }
}

//==============================================================================
void LimeEngine::loadDefaultPatterns()
{
    for (auto& p : patterns) p.clear();
    auto set = [this] (int pat, int lane, const char* s)
    {
        // '-' off, 'x' hit, 'X' accent, 'o' open hat, 'O' accented open hat
        for (int i = 0; i < NS && s[i]; ++i)
        {
            uint8_t v = 0;
            switch (s[i]) { case 'x': v = trig; break; case 'X': v = trig | accent; break;
                            case 'o': v = trig | open; break; case 'O': v = trig | open | accent; break; default: break; }
            patterns[(size_t) pat].steps[(size_t) lane][(size_t) i].store (v);
        }
    };
    auto setMod = [this] (int pat, std::initializer_list<float> vals)
    {
        int i = 0;
        for (float v : vals) if (i < NS) patterns[(size_t) pat].mod[(size_t) i++].store (v);
    };
    set (0, laneKick,  "X---x---X---x---");
    set (0, laneSnare, "----X-------X---");
    set (0, laneHat,   "x-o-x-o-x-o-x-ox");
    set (0, laneTie,   "--x----x--x-----");
    set (0, lanePod,   "------x-------x-");
    setMod (0, { 0.5f, 0.7f, 0.3f, 0.9f, 0.5f, 0.2f, 0.8f, 0.6f, 0.5f, 0.7f, 0.3f, 0.9f, 0.5f, 0.2f, 0.8f, 0.6f });

    set (1, laneKick,  "X--x--X---x--x--");
    set (1, laneSnare, "----X-------X-x-");
    set (1, laneHat,   "xxoxxxoxxxoxxxox");
    set (1, laneTie,   "x--x--x--x--x---");
    set (1, lanePod,   "--------X-------");

    set (2, laneKick,  "X-------X-x-----");
    set (2, laneSnare, "----X-------X---");
    set (2, laneHat,   "--o---o---o---o-");
    set (2, laneTie,   "-x-x-x-x-x-x-x-x");
    set (2, lanePod,   "X---------------");
}

juce::ValueTree LimeEngine::saveState() const
{
    juce::ValueTree vt ("LIME_PATTERNS");
    for (int p = 0; p < NP; ++p)
    {
        juce::ValueTree pt ("P");
        juce::String s;
        for (int l = 0; l < numTrigLanes; ++l)
            for (int st = 0; st < NS; ++st)
                s << juce::String::charToString ((juce::juce_wchar) ('0' + patterns[(size_t) p].steps[(size_t) l][(size_t) st].load()));
        pt.setProperty ("steps", s, nullptr);
        juce::String m;
        for (int st = 0; st < NS; ++st) m << juce::String (patterns[(size_t) p].mod[(size_t) st].load(), 3) << (st < NS - 1 ? "," : "");
        pt.setProperty ("mod", m, nullptr);
        vt.appendChild (pt, nullptr);
    }
    for (int s = 0; s < numSlots; ++s)
        vt.setProperty ("user" + juce::String (s), userPaths[(size_t) s], nullptr);
    return vt;
}

void LimeEngine::loadState (const juce::ValueTree& vt)
{
    if (! vt.isValid()) return;
    for (int p = 0; p < NP && p < vt.getNumChildren(); ++p)
    {
        auto pt = vt.getChild (p);
        const juce::String s = pt.getProperty ("steps").toString();
        for (int l = 0; l < numTrigLanes; ++l)
            for (int st = 0; st < NS; ++st)
            {
                const int idx = l * NS + st;
                patterns[(size_t) p].steps[(size_t) l][(size_t) st].store ((uint8_t) (idx < s.length() ? juce::jlimit (0, 7, (int) (s[idx] - '0')) : 0));
            }
        juce::StringArray m;
        m.addTokens (pt.getProperty ("mod").toString(), ",", {});
        for (int st = 0; st < NS; ++st)
            patterns[(size_t) p].mod[(size_t) st].store (st < m.size() ? juce::jlimit (0.0f, 1.0f, m[st].getFloatValue()) : 0.5f);
    }
    for (int s = 0; s < numSlots; ++s)
        userPaths[(size_t) s] = vt.getProperty ("user" + juce::String (s)).toString();
}
