#include "MicroEngine.h"
#include <algorithm>

using namespace params;

//==============================================================================
//  MicroVoice
//==============================================================================
void MicroVoice::prepare (double sampleRate)
{
    sr = sampleRate;
    active = false;
    noiseFilt[0].reset(); noiseFilt[1].reset(); eq.reset();
}

void MicroVoice::trigger (const MicroPatch& p, float velocity)
{
    auto velGain = [velocity] (float sensPct)
    {
        const float s = sensPct * 0.01f;
        return juce::jmax (0.0f, 1.0f + s * (velocity - 1.0f));
    };
    oscVelGain   = velGain (p.velOsc);
    noiseVelGain = velGain (p.velNoise);
    modVelScale  = velGain (p.velMod);
    t = 0.0;
    phase = 0.0;
    randLp = 0; randCount = 0; randTarget = 0;
    active = true;
    choking = false;
    chokeGain = 1.0f;
    noiseFilt[0].reset(); noiseFilt[1].reset();
    updateFilters (p);
}

void MicroVoice::updateFilters (const MicroPatch& p)
{
    noiseFilt[0].set (p.nFreq, sr, p.nQ);
    noiseFilt[1] .g = noiseFilt[0].g; noiseFilt[1].k = noiseFilt[0].k;
    noiseFilt[1].a1 = noiseFilt[0].a1; noiseFilt[1].a2 = noiseFilt[0].a2; noiseFilt[1].a3 = noiseFilt[0].a3;
    eq.setPeak (p.eqFreq, sr, p.eqGain, 1.0);
}

void MicroVoice::render (const MicroPatch& p, float* L, float* R, int numSamples)
{
    if (! active) return;

    const double dt = 1.0 / sr;
    const double r = p.pmRate * 0.01;
    // Pitch-mod rate mapping (depends on mode):
    //   Decay  : time constant 1 ms … 10 s
    //   Sine   : 0.1 Hz … 20 kHz
    //   Random : low-pass cutoff of the noise source 1 Hz … 20 kHz
    const double pmTau  = 0.001 * std::pow (10000.0, r);
    const double pmHz   = 0.1 * std::pow (200000.0, r);
    const double rndHz  = std::pow (20000.0, r);
    const float  rndCoef = (float) (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * rndHz / sr));
    const double pmAmt  = p.pmAmt * modVelScale;

    const double oscAtk = p.atkMs * 0.001, oscTau = p.dcyMs * 0.001 / 6.9;
    const double nAtk = p.nAtkMs * 0.001, nDcy = p.nDcyMs * 0.001, nTau = nDcy / 6.9;
    const double modPeriod = juce::jlimit (0.003, 0.02, nDcy * 0.04);

    const float mix = p.mix * 0.01f;
    const float oscG = (1.0f - mix) * oscVelGain;
    const float noiseG = mix * noiseVelGain;
    const float drive = 1.0f + p.dist * 0.01f * 24.0f;
    const float driveNorm = 1.0f / std::tanh (drive);
    const float lvl = juce::Decibels::decibelsToGain (p.levelDb, -60.0f);
    const float panN = (p.pan * 0.01f + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
    const float gl = lvl * std::cos (panN) * 1.41421356f, gr = lvl * std::sin (panN) * 1.41421356f;
    const bool useEq = std::abs (p.eqGain) > 0.05f;

    for (int i = 0; i < numSamples; ++i)
    {
        // ---- pitch modulation (semitones)
        double pm = 0.0;
        if (p.pmMode == 0)      pm = pmAmt * std::exp (-t / pmTau);
        else if (p.pmMode == 1) pm = pmAmt * std::sin (2.0 * juce::MathConstants<double>::pi * pmHz * t);
        else
        {
            randLp += rndCoef * (white (rngL) - randLp);
            pm = pmAmt * juce::jlimit (-1.0f, 1.0f, randLp * 3.0f);
            if (r <= 0.0001) { if (randCount++ == 0) randTarget = white (rngR); pm = pmAmt * randTarget; }
        }

        const double f = juce::jlimit (1.0, sr * 0.45, (double) p.freq * std::pow (2.0, pm / 12.0));
        const double inc = f / sr;
        phase += inc;
        if (phase >= 1.0) phase -= std::floor (phase);

        float osc;
        if (p.wave == 0)      osc = (float) std::sin (2.0 * juce::MathConstants<double>::pi * phase);
        else if (p.wave == 1) osc = (float) (phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase);
        else
        {
            // PolyBLEP saw
            double s = 2.0 * phase - 1.0;
            if (phase < inc)             { const double x = phase / inc; s -= x + x - x * x - 1.0; }
            else if (phase > 1.0 - inc)  { const double x = (phase - 1.0) / inc; s -= x * x + x + x + 1.0; }
            osc = (float) s;
        }

        // ---- oscillator envelope: linear attack, exponential decay
        double oEnv;
        if (t < oscAtk) oEnv = t / oscAtk;
        else            oEnv = std::exp (-(t - oscAtk) / juce::jmax (1.0e-5, oscTau));

        // ---- noise envelope: Exp / Lin / Mod (clap)
        double nEnv;
        if (t < nAtk) nEnv = t / nAtk;
        else
        {
            const double td = t - nAtk;
            if (p.nEnv == 0)      nEnv = std::exp (-td / juce::jmax (1.0e-5, nTau));
            else if (p.nEnv == 1) nEnv = juce::jmax (0.0, 1.0 - td / juce::jmax (1.0e-4, nDcy));
            else
            {
                if (td < modPeriod * 3.0)
                {
                    const double ph = td / modPeriod - std::floor (td / modPeriod);
                    nEnv = std::exp (-ph * 4.0);
                }
                else nEnv = std::exp (-(td - modPeriod * 3.0) / juce::jmax (1.0e-5, nTau));
            }
        }

        const float nl = white (rngL);
        const float nr = p.stereo ? white (rngR) : nl;
        float noiseL = noiseFilt[0].process (nl, p.nMode);
        float noiseR = p.stereo ? noiseFilt[1].process (nr, p.nMode) : noiseL;

        const float o = osc * (float) oEnv * oscG;
        float sL = o + noiseL * (float) nEnv * noiseG;
        float sR = o + noiseR * (float) nEnv * noiseG;

        if (p.dist > 0.01f)
        {
            sL = std::tanh (sL * drive) * driveNorm;
            sR = std::tanh (sR * drive) * driveNorm;
        }
        if (useEq)
        {
            sL = eq.process (sL, 0);
            sR = eq.process (sR, 1);
        }
        if (choking)
        {
            chokeGain *= 0.995f;
            sL *= chokeGain; sR *= chokeGain;
        }

        L[i] += sL * gl;
        R[i] += sR * gr;
        t += dt;

        if ((oEnv * oscG < 1.0e-5 && nEnv * noiseG < 1.0e-5 && t > oscAtk + nAtk + 0.005) || chokeGain < 1.0e-4)
        {
            active = false;
            break;
        }
    }
}

//==============================================================================
//  MicroEngine
//==============================================================================
MicroEngine::MicroEngine()
{
    loadDefaultPatterns();
}

void MicroEngine::bind (juce::AudioProcessorValueTreeState& apvts)
{
    const auto& sfx = mt::patchSuffixes();
    for (int ch = 0; ch < NC; ++ch)
    {
        for (size_t i = 0; i < sfx.size(); ++i)
            cp[(size_t) ch].v[i] = apvts.getRawParameterValue (a (ch, sfx[i]));
        cp[(size_t) ch].v[sfx.size()] = apvts.getRawParameterValue (a (ch, mt::mute));
    }
    pPattern  = apvts.getRawParameterValue (aPattern);
    pSwing    = apvts.getRawParameterValue (aSwing);
    pFillRate = apvts.getRawParameterValue (aFillRate);
    pFill     = apvts.getRawParameterValue (aFill);
    pLevel    = apvts.getRawParameterValue (aLevel);
}

void MicroEngine::prepare (double sr)
{
    sampleRate = sr;
    for (auto& v : voices) v.prepare (sr);
}

void MicroEngine::readPatch (int ch, MicroPatch& p) const
{
    auto& v = cp[(size_t) ch].v;
    auto f = [&] (int i) { return v[i]->load(); };
    p.wave = (int) f (0);  p.freq = f (1); p.atkMs = f (2); p.dcyMs = f (3);
    p.pmMode = (int) f (4); p.pmAmt = f (5); p.pmRate = f (6);
    p.nMode = (int) f (7); p.nFreq = f (8); p.nQ = f (9); p.stereo = f (10) > 0.5f; p.nEnv = (int) f (11);
    p.nAtkMs = f (12); p.nDcyMs = f (13);
    p.mix = f (14); p.dist = f (15); p.eqFreq = f (16); p.eqGain = f (17); p.levelDb = f (18); p.pan = f (19);
    p.choke = f (20) > 0.5f; p.velOsc = f (21); p.velNoise = f (22); p.velMod = f (23);
    p.mute = f (24) > 0.5f;
}

void MicroEngine::auditionChannel (int ch, float vel)
{
    if (ch >= 0 && ch < NC) audition[(size_t) ch].store (juce::jlimit (1, 1000, (int) (vel * 1000.0f)));
}

void MicroEngine::startVoice (int ch, float vel)
{
    if (patch[(size_t) ch].choke)
        for (int o = 0; o < NC; ++o)
            if (o != ch && patch[(size_t) o].choke) voices[(size_t) o].choke();
    voices[(size_t) ch].trigger (patch[(size_t) ch], vel);
    hits[(size_t) ch].fetch_add (1);
}

void MicroEngine::process (float* L, float* R, int numSamples, const BlockTime& bt, const std::vector<TriggerEvent>& midiEvents)
{
    for (int ch = 0; ch < NC; ++ch)
    {
        readPatch (ch, patch[(size_t) ch]);
        if (voices[(size_t) ch].isActive()) voices[(size_t) ch].updateFilters (patch[(size_t) ch]);
    }

    // ---------------- collect events
    std::array<TriggerEvent, 512> ev;
    int nev = 0;
    auto push = [&] (TriggerEvent e) { if (nev < (int) ev.size()) ev[(size_t) nev++] = e; };

    for (auto& e : midiEvents) push (e);
    for (int ch = 0; ch < NC; ++ch)
        if (const int a = audition[(size_t) ch].exchange (0); a > 0)
            push ({ 0, ch, (float) a / 1000.0f });

    const int wantedPattern = juce::jlimit (0, NP - 1, (int) pPattern->load());
    const bool manualFill = pFill->load() > 0.5f;
    const int fillRateIdx = (int) pFillRate->load();       // 0 = manual, 1..7 -> every 2..8 loops

    if (! bt.playing)
    {
        currentPattern.store (wantedPattern);
        playStep.store (-1);
        fillNow.store (manualFill);
    }
    else
    {
        const double swing = pSwing->load() * 0.01;
        forEachOnset (bt, 0.25, swing, [&] (long long k, double ppq)
        {
            int cur = currentPattern.load();
            int len = juce::jlimit (1, NS, patterns[(size_t) cur].length.load());
            if (k % len == 0 || bt.justStarted)
            {
                cur = wantedPattern;               // queued pattern change takes effect at loop start
                currentPattern.store (cur);
                len = juce::jlimit (1, NS, patterns[(size_t) cur].length.load());
            }
            const int step = (int) (k % len);
            const long long loop = k / len;
            const bool autoFill = fillRateIdx > 0 && (loop % (fillRateIdx + 1)) == fillRateIdx;
            const bool fillOn = manualFill || autoFill;
            fillNow.store (fillOn);
            playStep.store (step);

            const int off = bt.toSample (ppq);
            auto& pat = patterns[(size_t) cur];
            for (int ch = 0; ch < NC; ++ch)
            {
                if (patch[(size_t) ch].mute) continue;
                const uint8_t f = pat.steps[(size_t) ch][(size_t) step].load();
                if ((f & trig) || (fillOn && (f & fill)))
                    push ({ off, ch, (f & accent) ? 1.0f : 0.5f });
            }
        });
    }

    std::stable_sort (ev.begin(), ev.begin() + nev, [] (const TriggerEvent& x, const TriggerEvent& y) { return x.offset < y.offset; });

    // ---------------- render with sample-accurate triggers
    // fixed -6 dB of section headroom (8 drums summing), then the "Level A" control
    levelGain = 0.5f * juce::Decibels::decibelsToGain (pLevel->load(), -60.0f);
    int pos = 0, ei = 0;
    while (pos < numSamples)
    {
        while (ei < nev && ev[(size_t) ei].offset <= pos)
        {
            const auto& e = ev[(size_t) ei++];
            if (e.target >= 0 && e.target < NC) startVoice (e.target, e.velocity);
        }
        const int next = ei < nev ? juce::jmin (numSamples, ev[(size_t) ei].offset) : numSamples;
        const int n = juce::jmax (1, next - pos);
        for (int ch = 0; ch < NC; ++ch)
            voices[(size_t) ch].render (patch[(size_t) ch], L + pos, R + pos, n);
        pos += n;
    }
    if (std::abs (levelGain - 1.0f) > 1.0e-4f)
        for (int i = 0; i < numSamples; ++i) { L[i] *= levelGain; R[i] *= levelGain; }
}

//==============================================================================
void MicroEngine::loadDefaultPatterns()
{
    for (auto& p : patterns) { p.clear(); p.length.store (NS); }
    auto set = [this] (int pat, int ch, const char* s)
    {
        // '-' off, 'x' trigger, 'X' accented trigger, 'f' fill-only, 'F' accented fill
        for (int i = 0; i < NS && s[i]; ++i)
        {
            uint8_t v = 0;
            switch (s[i]) { case 'x': v = trig; break; case 'X': v = trig | accent; break;
                            case 'f': v = fill; break; case 'F': v = fill | accent; break; default: break; }
            patterns[(size_t) pat].steps[(size_t) ch][(size_t) i].store (v);
        }
    };
    // A : straight four-on-the-floor
    set (0, 0, "X---x---X---x---");
    set (0, 1, "----X-------X--f");
    set (0, 2, "x-x-x-x-x-x-x-xf");
    set (0, 3, "--x---x---x---x-");
    set (0, 4, "------------f-F-");
    // B : broken
    set (1, 0, "X-----x---X--x--");
    set (1, 1, "----X--x----X---");
    set (1, 2, "xxx-xxx-xxx-xx-x");
    set (1, 5, "----------x--x-X");
    set (1, 6, "--x-----x-----x-");
    // C : electro
    set (2, 0, "X--x--x---X-----");
    set (2, 4, "----X-------X---");
    set (2, 2, "x-xxx-x-x-xxx-x-");
    set (2, 7, "x--x--x--x--x---");
    // D : minimal
    set (3, 0, "X---X---X---X---");
    set (3, 6, "---x--x----x--x-");
    set (3, 2, "--X---X---X---X-");
    set (3, 7, "-x------x----x--");
}

juce::ValueTree MicroEngine::saveState() const
{
    juce::ValueTree vt ("MICRO_PATTERNS");
    for (int p = 0; p < NP; ++p)
    {
        juce::ValueTree pt ("P");
        pt.setProperty ("len", patterns[(size_t) p].length.load(), nullptr);
        juce::String s;
        for (int ch = 0; ch < NC; ++ch)
            for (int st = 0; st < NS; ++st)
                s << juce::String::charToString ((juce::juce_wchar) ('0' + patterns[(size_t) p].steps[(size_t) ch][(size_t) st].load()));
        pt.setProperty ("steps", s, nullptr);
        vt.appendChild (pt, nullptr);
    }
    return vt;
}

void MicroEngine::loadState (const juce::ValueTree& vt)
{
    if (! vt.isValid()) return;
    for (int p = 0; p < NP && p < vt.getNumChildren(); ++p)
    {
        auto pt = vt.getChild (p);
        patterns[(size_t) p].length.store (juce::jlimit (1, NS, (int) pt.getProperty ("len", NS)));
        const juce::String s = pt.getProperty ("steps").toString();
        for (int ch = 0; ch < NC; ++ch)
            for (int st = 0; st < NS; ++st)
            {
                const int idx = ch * NS + st;
                const int v = idx < s.length() ? juce::jlimit (0, 7, (int) (s[idx] - '0')) : 0;
                patterns[(size_t) p].steps[(size_t) ch][(size_t) st].store ((uint8_t) v);
            }
    }
}
