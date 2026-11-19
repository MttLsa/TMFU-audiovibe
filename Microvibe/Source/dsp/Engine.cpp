#include "Engine.h"

namespace mv
{
    using namespace prm;

    Engine::Engine()
    {
        const int N = count();
        ranges.reserve ((size_t) N);
        for (int i = 0; i < N; ++i)
        {
            const auto& d = def (i);
            juce::NormalisableRange<float> r (d.mn, d.mx);
            if (d.kind == F && d.centre > d.mn && d.centre < d.mx) r.setSkewForCentre (d.centre);
            ranges.push_back (r);
        }
        eff.assign ((size_t) N, 0.0f); prevEff.assign ((size_t) N, 0.0f); modAcc.assign ((size_t) N, 0.0f);
        isTouched.assign ((size_t) N, 0);
        touched.reserve (NSLOT); prevTouched.reserve (NSLOT);
        display.modNorm = std::vector<std::atomic<float>> ((size_t) N);
        for (auto& a : display.modNorm) a = 0.0f;
        for (int k = 0; k < NLFO; ++k) lfo[k].seed (0x1000u + (uint32_t) k * 7919u);
        for (int k = 0; k < NRND; ++k) rnd[k].seed (0x2000u + (uint32_t) k * 104729u);
        for (int l = 0; l < NLANE; ++l) lanes[l].seed (0x3000u + (uint32_t) l * 15485863u);
    }

    void Engine::prepare (double sampleRate, int)
    {
        sr = (float) sampleRate;
        texture.prepare (sr); shepard.prepare (sr); shepFx.prepare (sr); glitch.prepare (sr);
        tape.prepare (sr); vinyl.prepare (sr); wobble.prepare (sr); delay.prepare (sr); reverb.prepare (sr); filter.prepare (sr);
        reso.prepare (sr); ring.prepare (sr); chorus.prepare (sr); phaser.prepare (sr); comp.prepare (sr); stereo.prepare (sr);
        limiter.prepare (sr, -0.3f);
        stopCoef = 1.0f - std::exp (-1.0f / (0.03f * sr * 0.33f));
        trFast.set (0.3f, 25, sr); trSlow.set (40, 120, sr);
        for (auto& l : lanes) l.reset();
        for (auto& p : perc) p.active = false;
        resetPending = false;
        normGain = 1; normRms = 0;
    }

    // ------------------------------------------------------------------------------------------- modulation
    static float curveShape (float s, int c)
    {
        switch (c)
        {
            case 1: return s * std::abs (s);
            case 2: return (s < 0 ? -1.0f : 1.0f) * std::sqrt (std::abs (s));
            case 3: { const float a = std::abs (s); return (s < 0 ? -1.0f : 1.0f) * a * a * (3 - 2 * a); }
            default: return s;
        }
    }

    void Engine::evalModulation (const float* base)
    {
        const auto& I = ids();
        const int N = count();
        prevEff.swap (eff);
        for (int i = 0; i < N; ++i) eff[(size_t) i] = base[i];
        for (int p : touched) { isTouched[(size_t) p] = 0; modAcc[(size_t) p] = 0; }
        prevTouched.swap (touched);
        touched.clear();
        const auto& dl = dests();
        for (int k = 0; k < NSLOT; ++k)
        {
            const int s = (int) base[I.mxSrc[k]], d = (int) base[I.mxDst[k]];
            if (s <= 0 || d <= 0 || d > (int) dl.size() || s >= NUM_SOURCES) continue;
            const float amt = prevEff[(size_t) I.mxAmt[k]];
            if (std::abs (amt) < 1.0e-5f) continue;
            float v = curveShape (src[s], (int) base[I.mxCurve[k]]);
            const int via = (int) base[I.mxVia[k]];
            if (via > 0 && via < NUM_SOURCES) v *= src[via];
            const int p = dl[(size_t) (d - 1)];
            if (! isTouched[(size_t) p]) { isTouched[(size_t) p] = 1; touched.push_back (p); }
            modAcc[(size_t) p] += v * amt;
        }
        for (int p : touched)
        {
            const auto& r = ranges[(size_t) p];
            const float n0 = r.convertTo0to1 (juce::jlimit (r.start, r.end, base[p]));
            float v = r.convertFrom0to1 (clampv (n0 + modAcc[(size_t) p], 0.0f, 1.0f));
            if (def (p).kind == prm::I) v = std::round (v);
            eff[(size_t) p] = v;
        }
        // first block: amounts have no previous value
        for (int p : prevTouched) if (! isTouched[(size_t) p]) modAcc[(size_t) p] = 0;
    }

    void Engine::tickModulators (int n, double beat, bool run, double bpm)
    {
        const auto& I = ids();
        const float dt = (float) n / sr;
        const double beatsPerSec = bpm / 60.0;
        for (int k = 0; k < NLFO; ++k)
        {
            const int retrig = (int) eff[(size_t) I.lfRetrig[k]];
            bool fire = false;
            if (retrig >= 1 && retrig <= 4) fire = pendLane[retrig - 1];
            else if (retrig == 5) fire = pendMidiTrig;
            else if (retrig == 6) fire = pendTransient;
            else if (retrig == 7) fire = pendBar;
            if (fire) lfo[k].retrigger();
            const bool sync = eff[(size_t) I.lfSync[k]] > 0.5f;
            const double divB = syncDivBeats ((int) eff[(size_t) I.lfDiv[k]]);
            const double hz = sync ? beatsPerSec / divB : (double) eff[(size_t) I.lfRate[k]];
            double locked = -1;
            if (sync && run && retrig == 0) locked = beat / divB;
            for (int i = 0; i < 16; ++i) lfo[k].custom[(size_t) i] = shared.lfoShape[k][i].load (std::memory_order_relaxed);
            float v = lfo[k].tick (hz * dt, locked, (int) eff[(size_t) I.lfShape[k]], eff[(size_t) I.lfPhase[k]], eff[(size_t) I.lfSkew[k]],
                                   eff[(size_t) I.lfSmooth[k]], (int) eff[(size_t) I.lfMode[k]] == 1, sr, n);
            if (eff[(size_t) I.lfUni[k]] > 0.5f) v = 0.5f + 0.5f * v;
            src[srcLfo1 + k] = v * eff[(size_t) I.lfAmp[k]];
        }
        for (int k = 0; k < NENV; ++k)
        {
            const int t = (int) eff[(size_t) I.enTrig[k]];
            if (t == 0) { if (pendMidiTrig) env[k].trigger (-1); if (notesHeld == 0 && env[k].gateLeft < 0) env[k].release(); }
            else if (t >= 1 && t <= 4) { if (pendLane[t - 1]) env[k].trigger (laneGateS[t - 1]); }
            else if (t == 5) { if (pendTransient) env[k].trigger (0.05f); }
            else if (t == 6) { if (pendGlitch) env[k].trigger (0.1f); }
            src[srcEnv1 + k] = env[k].tick (dt, eff[(size_t) I.enA[k]] * 0.001f, eff[(size_t) I.enH[k]] * 0.001f, eff[(size_t) I.enD[k]] * 0.001f,
                                            eff[(size_t) I.enS[k]], eff[(size_t) I.enR[k]] * 0.001f, eff[(size_t) I.enCurve[k]]);
        }
        for (int k = 0; k < NRND; ++k)
        {
            const bool sync = eff[(size_t) I.rnSync[k]] > 0.5f;
            const double hz = sync ? beatsPerSec / syncDivBeats ((int) eff[(size_t) I.rnDiv[k]]) : (double) eff[(size_t) I.rnRate[k]];
            src[srcRnd1 + k] = rnd[k].tick (hz * dt, (int) eff[(size_t) I.rnMode[k]], eff[(size_t) I.rnSmooth[k]], sr, n);
        }
        for (int k = 0; k < NFOL; ++k) src[srcFol1 + k] = clampv (folVal[k] * dbToGain (eff[(size_t) I.foGain[k]]) * 2.0f, 0.0f, 1.0f);
        for (int l = 0; l < NLANE; ++l) src[srcLane1 + l] = lanes[l].currentValue();
        for (int k = 0; k < NMAC; ++k) src[srcMac1 + k] = eff[(size_t) I.macro[k]];
        src[srcVel] = midiVel; src[srcNote] = midiNote; src[srcMW] = midiMW; src[srcAT] = midiAT; src[srcPB] = midiPB;
        src[srcNone] = 0;
        pendMidiTrig = pendTransient = pendBar = pendGlitch = false;
        for (auto& p : pendLane) p = false;
    }

    // ------------------------------------------------------------------------------------------- helpers
    void Engine::chord (int c, float root, float* out, int& n) const
    {
        static const float tables[7][4] = { { 0 }, { 0, 7 }, { 0, 12 }, { 0, 3, 7 }, { 0, 4, 7 }, { 0, 5, 7 }, { 0, 3, 7, 10 } };
        static const int sizes[7] = { 1, 2, 2, 3, 3, 3, 4 };
        const float snapAmt = eff[(size_t) ids().tuSnap];
        if (c >= 0 && c < 7)
        {
            n = sizes[c];
            for (int k = 0; k < n; ++k) out[k] = (float) tuning.snap (root + tables[c][k], snapAmt);
            return;
        }
        // "Scale": consecutive scale degrees
        n = 0;
        double x = root - 0.01;
        while (n < 6)
        {
            const double cand = tuning.snap (x + 0.35, 1.0f);
            if (n == 0 || cand > out[n - 1] + 0.05) out[n++] = (float) cand;
            x += 0.5;
            if (x > root + 24) break;
        }
        if (n == 0) { out[0] = root; n = 1; }
    }

    float Engine::stepSeconds (int lane, double bpm) const
    {
        const auto& c = cfg[lane];
        if (c.src != 0) return (float) (15.0 / bpm);
        if (! c.sync) return 1.0f / std::max (0.1f, c.hz);
        return (float) (c.stepBeats * 60.0 / bpm);
    }

    void Engine::triggerPerc (int v, float vel)
    {
        const auto& I = ids();
        const float hum = eff[(size_t) I.pcHum[v]];
        const float vv = clampv (vel * (1.0f - hum * 0.45f * rng.uni()), 0.05f, 1.0f);
        const int model = (int) eff[(size_t) I.pcModel[v]];
        const float tune = eff[(size_t) I.pcTune[v]], root = eff[(size_t) I.tuRoot], snapAmt = eff[(size_t) I.tuSnap];
        float hz;
        if (model == FIRST_808 || model == FIRST_808 + 1)
        {
            // Kick 808: G1 (49 Hz) at Tune 0, Tom 808: G2 (98 Hz); both follow the scale
            const float base808 = model == FIRST_808 ? -29.0f : -17.0f;
            hz = (float) tuning::Tuning::toHz (tuning.snap (base808 + tune, snapAmt));
        }
        else if (model == FIRST_808 + 2) hz = 180.0f * semiToRatio (tune);       // Snare: 180 / 330 Hz
        else if (model == FIRST_808 + 3) hz = 1100.0f * semiToRatio (tune);      // Clap: band centre
        else if (model >= FIRST_808 + 4) hz = semiToRatio (tune);                // Hats: oscillator ratio
        else hz = (float) tuning::Tuning::toHz (tuning.snap (root + tune, snapAmt));
        // the closed hat chokes ringing open hats, like on the 808
        if (model == FIRST_808 + 4)
            for (int k = 0; k < NPERC; ++k) if (k != v && perc[k].modelType() == FIRST_808 + 5) perc[k].startChoke (6.0f, sr);
        // input history for the "Input" exciter (oldest first)
        float hist[2048];
        for (int i = 0; i < 2048; ++i) hist[i] = inHist[(inHistPos + i) & 2047];
        const int histLen = (int) std::min (2048.0f, sr * 0.03f);
        perc[v].trigger (hz, (int) eff[(size_t) I.pcModel[v]], (int) eff[(size_t) I.pcExc[v]], eff[(size_t) I.pcDecay[v]], eff[(size_t) I.pcMat[v]],
                         eff[(size_t) I.pcInh[v]], eff[(size_t) I.pcPos[v]], vv, eff[(size_t) I.pcLvl[v]],
                         clampv (eff[(size_t) I.pcPan[v]] + rng.bi() * hum * 0.25f, -1.0f, 1.0f), sr, hist + 2048 - histLen, histLen);
    }

    void Engine::laneFired (int l, const LaneTrigger& t, double bpm)
    {
        const auto& I = ids();
        pendLane[l] = true;
        laneGateS[l] = stepSeconds (l, bpm) * 0.9f;
        // perc voices on this lane
        if (eff[(size_t) I.pcOn] > 0.5f)
            for (int v = 0; v < NPERC; ++v)
            {
                if ((int) eff[(size_t) I.pcLane[v]] != l + 1) continue;
                const int every = std::max (1, (int) eff[(size_t) I.pcEvery[v]]), offs = (int) eff[(size_t) I.pcOffs[v]];
                const int cnt = percCount[v]++;
                if (((cnt - offs) % every + every) % every != 0) continue;
                if (! rng.chance (Lane::densityProb (eff[(size_t) I.pcProb[v]], eff[(size_t) I.density]))) continue;
                triggerPerc (v, t.vel);
            }
        // glitch
        if (eff[(size_t) I.glOn] > 0.5f && (int) eff[(size_t) I.glLane] == l + 1 && heldGlitch < 0 && ! t.ratchet)
        {
            if (rng.chance (Lane::densityProb (eff[(size_t) I.glProb], eff[(size_t) I.density])))
            {
                float tot = 0, w[NUM_GLITCH];
                for (int g = 0; g < NUM_GLITCH; ++g) { w[g] = std::max (0.0f, eff[(size_t) I.glW[g]]); tot += w[g]; }
                if (tot > 0.0001f)
                {
                    float x = rng.uni() * tot; int type = 0;
                    for (int g = 0; g < NUM_GLITCH; ++g) { x -= w[g]; if (x <= 0) { type = g; break; } }
                    const float len = (float) eff[(size_t) I.glLen] * stepSeconds (l, bpm) * sr;
                    glitch.start (type, len, glp);
                    pendGlitch = true;
                }
            }
        }
    }

    void Engine::onTransient()
    {
        pendTransient = true;
        for (int l = 0; l < NLANE; ++l)
            if (cfg[l].src == 1 && running) { LaneTrigger t; if (lanes[l].external (cfg[l], t)) laneFired (l, t, spb > 0 ? 60.0 * sr / spb : 124.0); }
    }

    void Engine::handleMidi (const juce::MidiMessage& m)
    {
        if (m.isNoteOn())
        {
            const int n = m.getNoteNumber();
            midiVel = m.getFloatVelocity(); midiNote = (float) n / 127.0f;
            ++notesHeld; pendMidiTrig = true;
            if (n >= 36 && n < 36 + NUM_GLITCH) { heldGlitch = n - 36; glitch.startHeld (heldGlitch, glp); pendGlitch = true; }
            else if (n >= 48 && n < 48 + NPERC) triggerPerc (n - 48, midiVel);
            for (int l = 0; l < NLANE; ++l)
                if (cfg[l].src == 2 && running) { LaneTrigger t; if (lanes[l].external (cfg[l], t)) laneFired (l, t, spb > 0 ? 60.0 * sr / spb : 124.0); }
        }
        else if (m.isNoteOff())
        {
            notesHeld = std::max (0, notesHeld - 1);
            const int n = m.getNoteNumber();
            if (n >= 36 && n < 36 + NUM_GLITCH && heldGlitch == n - 36) { glitch.stopHeld (heldGlitch); heldGlitch = -1; }
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff()) { notesHeld = 0; if (heldGlitch >= 0) glitch.stopHeld (heldGlitch); heldGlitch = -1; }
        else if (m.isController() && m.getControllerNumber() == 1) midiMW = (float) m.getControllerValue() / 127.0f;
        else if (m.isChannelPressure()) midiAT = (float) m.getChannelPressureValue() / 127.0f;
        else if (m.isAftertouch()) midiAT = (float) m.getAfterTouchValue() / 127.0f;
        else if (m.isPitchWheel()) midiPB = ((float) m.getPitchWheelValue() - 8192.0f) / 8192.0f;
    }

    // ------------------------------------------------------------------------------------------- block parameters
    void Engine::setupBlockParams (double bpm)
    {
        const auto& I = ids();
        auto e = [this] (int i) { return eff[(size_t) i]; };
        auto ei = [this] (int i) { return (int) std::lround (eff[(size_t) i]); };
        auto on = [this] (int i) { return eff[(size_t) i] > 0.5f; };
        const float density = e (I.density);
        const float root = e (I.tuRoot);

        for (int l = 0; l < NLANE; ++l)
        {
            auto& c = cfg[l];
            c.on = on (I.lnOn[l]); c.sync = on (I.lnSync[l]); c.grid = ei (I.lnMode[l]) == 1;
            c.steps = std::max (1, ei (I.lnSteps[l])); c.pulses = ei (I.lnPulses[l]); c.rot = ei (I.lnRot[l]);
            c.ratch = std::max (1, ei (I.lnRatch[l])); c.src = ei (I.lnSrc[l]); c.cond = ei (I.lnCond[l]);
            c.stepBeats = laneDivBeats (ei (I.lnDiv[l])) * ratioFactor (ei (I.lnRatio[l]));
            c.hz = e (I.lnHz[l]); c.swing = e (I.lnSwing[l]); c.prob = e (I.lnProb[l]); c.rprob = e (I.lnRProb[l]); c.hum = e (I.lnHum[l]);
            c.density = density; c.gates = gates[l]; c.values = values[l];
        }

        // per-voice percussion filters
        for (int v = 0; v < NPERC; ++v) perc[v].setFilter (ei (I.pcFType[v]), e (I.pcCut[v]), e (I.pcRes[v]), sr);

        // texture
        txp.bed = e (I.txBed); txp.resq = e (I.txResQ); txp.drone = e (I.txDrone); txp.drift = e (I.txDrift); txp.grain = e (I.txGrain);
        txp.dens = e (I.txDens); txp.sizeMs = e (I.txSize); txp.spray = e (I.txSpray); txp.pitch = e (I.txPitch); txp.prand = e (I.txPRand);
        txp.spread = e (I.txSpread); txp.tilt = e (I.txTilt); txp.rev = e (I.txRev); txp.snap = e (I.tuSnap);
        txp.noise = ei (I.txNoise); txp.gsrc = ei (I.txGSrc); txp.win = ei (I.txWin); txp.freeze = on (I.txFreeze);
        txp.tuning = &tuning;
        droneNotes (bpm);

        // shepard generator
        {
            static const float iv[] = { 12, 7, 5, 6, 4 };
            const int ivSel = ei (I.shInterval);
            shInterval = ivSel < 5 ? iv[ivSel] : e (I.shCents) / 100.0f;
            const float speed = on (I.shSync) ? (float) (bpm / 60.0 / barBeats (ei (I.shBars))) : e (I.shRate);
            const int dir = ei (I.shDir);
            int sign = dir == 1 ? -1 : 1;
            if (dir == 2) sign = ((int64_t) std::floor (curBeat / barBeats (ei (I.shBars))) % 2 == 0) ? 1 : -1;
            shSpeed = speed * (float) sign;
        }
        // shepard fx
        {
            const float speed = on (I.sfSync) ? (float) (bpm / 60.0 / barBeats (ei (I.sfBars))) : e (I.sfRate);
            sfSpeed = speed * (ei (I.sfDir) == 1 ? -1.0f : 1.0f);
            sfMode = ei (I.sfMode); sfRes = e (I.sfRes); sfBands = ei (I.sfBands); sfMix = e (I.sfMix);
        }

        // glitch
        glp.mix = e (I.glMix); glp.rptBeats = noteDivBeats (ei (I.glRptDiv)); glp.rptDecay = e (I.glRptDecay); glp.rptPitch = e (I.glRptPitch);
        glp.rptShrink = e (I.glRptShrink); glp.slices = ei (I.glSlices); glp.stopTime = e (I.glStopTime); glp.gateShape = ei (I.glGateShape);
        glp.gateBeats = noteDivBeats (ei (I.glGateDiv)); glp.bits = e (I.glBits); glp.srate = e (I.glSRate);
        glp.pitch = (float) (tuning.snap (root + e (I.glPitch), e (I.tuSnap)) - root);
        glp.xfadeMs = e (I.glXfade); glp.freezeMs = e (I.glFreezeSize); glp.samplesPerBeat = (float) spb;

        // fx
        tpp = { e (I.tpDrive), e (I.tpSat), e (I.tpBias), e (I.tpHiss), e (I.tpComp), e (I.tpDrop), e (I.tpFlut), e (I.tpAge), e (I.tpMix), ei (I.tpSpeed), ei (I.tpMode) };
        vnp = { e (I.vnDust), e (I.vnCrackle), e (I.vnScratch), e (I.vnRumble), e (I.vnHum), e (I.vnYear), e (I.vnWear), e (I.vnWarp),
                e (I.vnBrake), e (I.vnMono), e (I.vnNoise), e (I.vnMix), ei (I.vnHumHz), ei (I.vnRpm) };
        wbp = { e (I.wbAmt), e (I.wbBal), e (I.wbWow), e (I.wbFlut), e (I.wbFlux), e (I.wbSpread), e (I.wbMix) };
        {
            const float t = on (I.dlSync) ? noteDivBeats (ei (I.dlDiv)) * (float) (60.0 / bpm) : e (I.dlMs) * 0.001f;
            const float pitch = std::abs (e (I.dlPitch)) > 0.01f ? (float) (tuning.snap (root + e (I.dlPitch), e (I.tuSnap)) - root) : 0.0f;
            dlp = { t, e (I.dlFb), e (I.dlLp), e (I.dlHp), e (I.dlSat), e (I.dlWob), pitch, e (I.dlDuck), e (I.dlMix), ei (I.dlMode), ei (I.dlTaps), on (I.dlFreeze) };
        }
        {
            const float shSt = (float) (tuning.snap (root + e (I.rvShInt), e (I.tuSnap)) - root);
            rvp = { e (I.rvSize), e (I.rvDecay), e (I.rvPre), e (I.rvDamp), e (I.rvMod), e (I.rvDiff), e (I.rvShim), shSt, e (I.rvDuck), e (I.rvLow),
                    e (I.rvMix), (float) (60.0 / bpm), ei (I.rvMode), on (I.rvFreeze) };
        }
        ftp = { e (I.ftCut), e (I.ftRes), e (I.ftDrive), e (I.ftVowel), e (I.ftMix), ei (I.ftType), ei (I.ftSlope) };
        chord (ei (I.rsChord), root + 12.0f * ei (I.rsOct), rsp.noteSemis, rsp.nNotes);
        rsp.decay = e (I.rsDecay); rsp.bright = e (I.rsBright); rsp.spread = e (I.rsSpread); rsp.mix = e (I.rsMix);
        rfp = { e (I.rfFreq), e (I.rfShift), e (I.rfMix), ei (I.rfMode) };
        crp = { e (I.crBits), e (I.crRate), e (I.crJitter), e (I.crMix) };
        chp = { e (I.chRate), e (I.chDepth), e (I.chMix), ei (I.chVoices) };
        php = { e (I.phRate), e (I.phDepth), e (I.phFb), e (I.phCentre), e (I.phMix), ei (I.phStages) };
        cpp = { e (I.cpThr), e (I.cpRatio), e (I.cpAtt), e (I.cpRel), e (I.cpGain), e (I.cpTAtk), e (I.cpTSus), e (I.cpMix) };
        stp = { e (I.stWidth), e (I.stHaas), e (I.stPanRate), e (I.stPanDepth), e (I.stMonoHz) };

        // transport stopped: surface noise / hiss fade with the generators, delay and reverb tails die out naturally
        if (! running)
        {
            vnp.noise *= stopGain; tpp.hiss *= stopGain;
            dlp.freeze = false; dlp.fb = std::min (dlp.fb, 0.9f);
            rvp.freeze = false;
        }

        // chain order (each module once; switched-on modules only)
        const int onIds[NUM_FXTYPES] = { -1, I.tpOn, I.vnOn, I.wbOn, I.dlOn, I.rvOn, I.ftOn, I.rsOn, I.rfOn, I.crOn, I.chOn, I.phOn, I.cpOn, I.stOn, I.sfOn };
        bool used[NUM_FXTYPES] {};
        nOrder = 0;
        for (int s = 0; s < NFX; ++s)
        {
            const int t = ei (I.fxSlot[s]);
            if (t <= 0 || t >= NUM_FXTYPES || used[t]) continue;
            used[t] = true;
            const bool isOn = on (onIds[t]);
            if (isOn && ! fxWasOn[t])
            {
                switch (t)
                {
                    case fxTape: tape.reset(); break; case fxVinyl: vinyl.reset(); break; case fxWobble: wobble.reset(); break;
                    case fxDelay: delay.reset(); break; case fxReverb: reverb.reset(); break; case fxFilter: filter.reset(); break;
                    case fxReso: reso.reset(); break; case fxChorus: chorus.reset(); break; case fxStereo: stereo.reset(); break;
                    default: break;
                }
            }
            if (isOn) order[nOrder++] = t;
        }
        for (int t = 1; t < NUM_FXTYPES; ++t) fxWasOn[t] = used[t] && on (onIds[t]);
    }

    // ------------------------------------------------------------------------------------------- drone chords
    void Engine::droneNotes (double bpm)
    {
        const auto& I = ids();
        auto e = [this] (int i) { return eff[(size_t) i]; };
        auto ei = [this] (int i) { return (int) std::lround (eff[(size_t) i]); };
        const float root = e (I.tuRoot);
        const float octOff = 12.0f * (float) ei (I.txOct) - 12.0f;
        const int chordSel = ei (I.txChord);
        float tgt[8] {}; int n = 0;
        if (chordSel == 7)
        {
            // "Scale": notes chosen in TUNING; with the progression on, up to 4 snapshots follow each other in time
            int snapIdx = 0;
            if (e (I.prOn) > 0.5f)
            {
                const int cnt = juce::jlimit (1, NSNAP, ei (I.prCount));
                const int mode = ei (I.prMode);
                int seq[2 * NSNAP]; int ns = 0;
                if (mode == 1) for (int k = cnt - 1; k >= 0; --k) seq[ns++] = k;
                else { for (int k = 0; k < cnt; ++k) seq[ns++] = k; if (mode == 2) for (int k = cnt - 2; k >= 1; --k) seq[ns++] = k; }
                int total = 0;
                for (int j = 0; j < ns; ++j) total += juce::jmax (1, ei (I.prLen[seq[j]]));
                const double unit = e (I.prSync) > 0.5f ? (double) syncDivBeats (ei (I.prDiv)) : (double) e (I.prSecs) * bpm / 60.0;
                if (running)
                {
                    const int64_t u = (int64_t) std::floor (std::max (0.0, curBeat) / std::max (1.0e-3, unit));
                    const int64_t cycle = u / total;
                    int w = (int) (u - cycle * total), j = 0;
                    while (j < ns - 1 && w >= juce::jmax (1, ei (I.prLen[seq[j]]))) { w -= juce::jmax (1, ei (I.prLen[seq[j]])); ++j; }
                    snapIdx = seq[j];
                    if (mode == 3)
                    {
                        uint32_t x = (uint32_t) (cycle * 16 + j) * 2654435761u; x ^= x >> 15; x *= 2246822519u; x ^= x >> 13;
                        snapIdx = (int) (x % (uint32_t) cnt);
                    }
                    curSnap = snapIdx;
                }
                else snapIdx = juce::jlimit (0, cnt - 1, curSnap);
            }
            else curSnap = 0;
            const uint64_t m = shared.snapMask[snapIdx].load (std::memory_order_relaxed);
            const int size = std::min (64, std::max (1, tuning.currentSize()));
            for (int d = 0; d < size && n < 8; ++d)
                if ((m >> d) & 1u) tgt[n++] = (float) tuning.degreeSemis (d) + octOff;
            if (n == 0) chord (3, root + octOff, tgt, n);       // nothing selected: minor triad in the scale
            display.snapshot = snapIdx;
        }
        else chord (chordSel, root + octOff, tgt, n);
        // voicing
        const int voicing = ei (I.prVoicing);
        for (int k = 0; k < n; ++k)
        {
            if (voicing == 1 && (k & 1)) tgt[k] += 12.0f;
            if (voicing == 2) tgt[k] += 12.0f * (float) (k % 3);
        }
        // glide between chords
        if (n != droneN) { for (int k = 0; k < n; ++k) droneCur[k] = tgt[k]; droneN = n; }
        else
        {
            const float g = e (I.prGlide);
            const float a = g < 1.0f ? 1.0f : 1.0f - std::exp (-(float) kBlock / (g * 0.001f * sr));
            for (int k = 0; k < n; ++k) droneCur[k] += a * (tgt[k] - droneCur[k]);
        }
        for (int k = 0; k < n; ++k) txp.noteSemis[k] = droneCur[k];
        txp.nNotes = juce::jmax (1, n);
    }

    void Engine::transportStopped()
    {
        glitch.release();
        for (auto& p : perc) p.startChoke (30.0f, sr);
    }

    // ------------------------------------------------------------------------------------------- per sample
    void Engine::processSample (float& l, float& r, float scl, float scr)
    {
        const auto& I = ids();
        // input gate + level
        const float inLevel = eff[(size_t) I.inLevel];
        const float gateDb = eff[(size_t) I.inGate];
        float inL = l * inLevel, inR = r * inLevel;
        if (gateDb > -89.0f)
        {
            const float e = std::max (std::abs (inL), std::abs (inR));
            inGateEnv += (e > inGateEnv ? 0.2f : 0.0005f) * (e - inGateEnv);
            const float target = inGateEnv > dbToGain (gateDb) ? 1.0f : 0.0f;
            inGateGain += (target > inGateGain ? 0.05f : 0.0008f) * (target - inGateGain);
            inL *= inGateGain; inR *= inGateGain;
        }
        pkIn = std::max (pkIn, std::max (std::abs (inL), std::abs (inR)));
        inHist[inHistPos] = 0.5f * (inL + inR); inHistPos = (inHistPos + 1) & 2047;

        // transient detection
        {
            const float x = (int) eff[(size_t) I.trigSrc] == 1 ? 0.5f * (scl + scr) : 0.5f * (inL + inR);
            const float f = trFast.process (x), s = trSlow.process (x);
            if (trHold > 0) --trHold;
            else if (f > dbToGain (eff[(size_t) I.trigThr]) && f > s * 1.8f) { trHold = (int) (0.045f * sr); onTransient(); }
        }

        // generators
        float gL = 0, gR = 0;
        if (eff[(size_t) I.pcOn] > 0.5f)
        {
            float pl = 0, pr = 0;
            for (auto& v : perc) v.process (pl, pr);
            const float g = eff[(size_t) I.pcLevel];
            gL += pl * g; gR += pr * g;
        }
        stopGain += ((running ? 1.0f : 0.0f) - stopGain) * stopCoef;
        const bool gensAudible = running || stopGain > 1.0e-4f;
        if (eff[(size_t) I.txOn] > 0.5f && gensAudible)
        {
            float tl, tr;
            texture.process (inL, inR, tl, tr, txp);
            const float g = eff[(size_t) I.txLevel] * stopGain;
            gL += tl * g; gR += tr * g;
        }
        if (eff[(size_t) I.shOn] > 0.5f && gensAudible)
        {
            float sl = 0, sr2 = 0;
            shepard.process (sl, sr2, (int) eff[(size_t) I.shPartials], shInterval, shSpeed, eff[(size_t) I.shBase], eff[(size_t) I.shWidth],
                             (int) eff[(size_t) I.shWave], eff[(size_t) I.shLevel]);
            gL += sl * stopGain; gR += sr2 * stopGain;
        }

        // followers
        for (int k = 0; k < NFOL; ++k)
        {
            const int s = (int) eff[(size_t) I.foSrc[k]];
            const float x = s == 0 ? 0.5f * (inL + inR) : s == 1 ? 0.5f * (scl + scr) : 0.5f * (inL + inR + gL + gR);
            folVal[k] = fol[k].process (x);
        }

        float xl = inL + gL, xr = inR + gR;
        if (eff[(size_t) I.glOn] > 0.5f || heldGlitch >= 0) glitch.process (xl, xr, glp);

        for (int k = 0; k < nOrder; ++k)
        {
            switch (order[k])
            {
                case fxTape:    tape.process (xl, xr, tpp); break;
                case fxVinyl:   vinyl.process (xl, xr, vnp); break;
                case fxWobble:  wobble.process (xl, xr, wbp); break;
                case fxDelay:   delay.process (xl, xr, dlp); break;
                case fxReverb:  reverb.process (xl, xr, rvp); break;
                case fxFilter:  filter.process (xl, xr, ftp); break;
                case fxReso:    reso.process (xl, xr, rsp); break;
                case fxRing:    ring.process (xl, xr, rfp); break;
                case fxCrush:   crusher.process (xl, xr, crp); break;
                case fxChorus:  chorus.process (xl, xr, chp); break;
                case fxPhaser:  phaser.process (xl, xr, php); break;
                case fxComp:    comp.process (xl, xr, cpp); break;
                case fxStereo:  stereo.process (xl, xr, stp); break;
                case fxShepard: shepFx.process (xl, xr, sfMode, sfSpeed, sfRes, sfBands, sfMix); break;
                default: break;
            }
        }
        if (! std::isfinite (xl) || ! std::isfinite (xr)) { xl = xr = 0; resetPending = true; }

        // dry / wet, width, output, norm, limiter
        mixSm += (eff[(size_t) I.mix] - mixSm) * 0.002f;
        widthSm += (eff[(size_t) I.width] - widthSm) * 0.002f;
        outSm += (dbToGain (eff[(size_t) I.out] + eff[(size_t) I.trim]) - outSm) * 0.002f;
        float ol = l * (1.0f - mixSm) + xl * mixSm, orr = r * (1.0f - mixSm) + xr * mixSm;
        const float m = 0.5f * (ol + orr), s = 0.5f * (ol - orr) * widthSm;
        ol = (m + s) * outSm; orr = (m - s) * outSm;
        if (eff[(size_t) I.norm] > 0.5f)
        {
            normRms += (0.5f * (ol * ol + orr * orr) - normRms) * (1.0f / (3.0f * sr));
            const float rms = std::sqrt (std::max (1.0e-9f, normRms));
            const float target = clampv (dbToGain (-16.0f) / rms, dbToGain (-12.0f), dbToGain (12.0f));
            normGain += (target - normGain) * (1.0f / (0.5f * sr));
            ol *= normGain; orr *= normGain;
            limiter.thr = dbToGain (-1.0f);
        }
        else limiter.thr = dbToGain (-0.3f);
        limited |= limiter.process (ol, orr);
        pkL = std::max (pkL, std::abs (ol)); pkR = std::max (pkR, std::abs (orr));
        l = ol; r = orr;
    }

    // ------------------------------------------------------------------------------------------- block
    void Engine::process (float* L, float* R, const float* scL, const float* scR, int n, const juce::MidiBuffer& midi,
                          const Transport& tr, const float* base)
    {
        const auto& I = ids();
        if (resetPending)
        {
            resetPending = false;
            tape.reset(); vinyl.reset(); wobble.reset(); delay.reset(); reverb.reset(); filter.reset(); reso.reset(); chorus.reset(); stereo.reset();
            glitch.prepare (sr); for (auto& p : perc) p.active = false;
        }
        const double bpm = juce::jlimit (30.0, 300.0, tr.hasHost && tr.bpm > 0 ? tr.bpm : (double) base[I.bpm]);
        spb = 60.0 * sr / bpm;
        tuning.update ((int) base[I.tuScale], (int) base[I.tuRoot], base[I.tuMts] > 0.5f);
        display.mts = tuning.mtsActive();

        // pattern (and chain)
        int pat = (int) base[I.pattern];
        const bool hostRun = tr.hasHost && tr.hostRunning;
        running = hostRun || tr.forceRun || ! tr.hasHost;
        display.running = running;
        if (wasRunning && ! running) transportStopped();
        wasRunning = running;
        double beat0 = hostRun ? tr.ppq : freeBeat;
        if (base[I.chain] > 0.5f)
        {
            const int a = std::min (pat, (int) base[I.chainEnd]), b = std::max (pat, (int) base[I.chainEnd]);
            pat = a + (int) ((int64_t) std::floor (std::max (0.0, beat0) / 4.0) % (b - a + 1));
        }
        activePattern = juce::jlimit (0, NPAT - 1, pat);
        for (int l = 0; l < NLANE; ++l)
            for (int s = 0; s < kMaxSteps; ++s)
            {
                gates[l][s] = shared.gate[activePattern][l][s].load (std::memory_order_relaxed);
                values[l][s] = shared.value[activePattern][l][s].load (std::memory_order_relaxed);
            }

        auto mit = midi.cbegin();
        const auto mend = midi.cend();
        for (int start = 0; start < n; start += kBlock)
        {
            const int len = std::min (kBlock, n - start);
            // MIDI of this control block
            while (mit != mend && (*mit).samplePosition < start + len) { handleMidi ((*mit).getMessage()); ++mit; }
            evalModulation (base);
            setupBlockParams (bpm);
            const double beatStart = beat0 + (double) start / spb;
            for (int i = 0; i < len; ++i)
            {
                const double beat = beatStart + (double) i / spb;
                curBeat = beat;
                const int64_t bar = (int64_t) std::floor (beat / 4.0);
                if (bar != lastBar) { lastBar = bar; pendBar = true; }
                for (int l = 0; l < NLANE; ++l)
                {
                    LaneTrigger t;
                    if (lanes[l].tick (cfg[l], beat, running, sr, t)) laneFired (l, t, bpm);
                }
                processSample (L[start + i], R[start + i], scL ? scL[start + i] : 0.0f, scR ? scR[start + i] : 0.0f);
            }
            tickModulators (len, beatStart + (double) len / spb, running, bpm);
        }
        if (! hostRun) freeBeat += (double) n / spb;
        else freeBeat = tr.ppq + (double) n / spb;

        // display
        for (int p : prevTouched) if (! isTouched[(size_t) p]) display.modNorm[(size_t) p].store (0.0f, std::memory_order_relaxed);
        for (int p : touched) display.modNorm[(size_t) p].store (modAcc[(size_t) p], std::memory_order_relaxed);
        for (int s = 0; s < NUM_SOURCES; ++s) display.src[s].store (src[s], std::memory_order_relaxed);
        for (int l = 0; l < NLANE; ++l) display.laneStep[l].store (lanes[l].currentIndex(), std::memory_order_relaxed);
        display.pattern = activePattern;
        display.glitchType = glitch.activeType();
        display.peakL = std::max (pkL, display.peakL.load() * 0.85f); display.peakR = std::max (pkR, display.peakR.load() * 0.85f);
        display.inPeak = std::max (pkIn, display.inPeak.load() * 0.85f);
        display.limiting = limited;
        display.beat = curBeat;
        pkL = pkR = pkIn = 0; limited = false;
    }
}
