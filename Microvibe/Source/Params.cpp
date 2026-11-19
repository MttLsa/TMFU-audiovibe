#include "Params.h"
#include "dsp/Tuning.h"
#include <map>

namespace prm
{
    // ------------------------------------------------------------------------------------------- lists
    const juce::StringArray& fxNames()
    {
        static const juce::StringArray s { "Off", "Tape", "Vinyl", "Wobble", "Delay", "Reverb", "Filter", "Resonator", "Ring/Shift",
                                           "Crusher", "Chorus", "Phaser", "Comp", "Stereo", "Shepard FX" };
        return s;
    }
    const juce::StringArray& glitchNames()
    {
        static const juce::StringArray s { "Repeat", "Reverse", "Slice", "Tape Stop", "Gate", "Freeze", "Crush", "Dropout", "Pitch" };
        return s;
    }
    const juce::StringArray& syncDivNames()
    {
        static const juce::StringArray s { "8 bar", "4 bar", "2 bar", "1 bar", "1/2", "1/4", "1/8", "1/16", "1/32",
                                           "1/2T", "1/4T", "1/8T", "1/16T", "1/2D", "1/4D", "1/8D", "1/16D", "1/4 5let", "1/4 7let" };
        return s;
    }
    float syncDivBeats (int i)
    {
        static const float b[] = { 32, 16, 8, 4, 2, 1, 0.5f, 0.25f, 0.125f, 4.0f / 3, 2.0f / 3, 1.0f / 3, 1.0f / 6, 3, 1.5f, 0.75f, 0.375f, 0.8f, 4.0f / 7 };
        return b[juce::jlimit (0, (int) std::size (b) - 1, i)];
    }
    const juce::StringArray& noteDivNames()
    {
        static const juce::StringArray s { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64", "1/128",
                                           "1/4T", "1/8T", "1/16T", "1/32T", "1/4D", "1/8D", "1/16D", "1/8 5let", "1/8 7let" };
        return s;
    }
    float noteDivBeats (int i)
    {
        static const float b[] = { 4, 2, 1, 0.5f, 0.25f, 0.125f, 0.0625f, 0.03125f, 2.0f / 3, 1.0f / 3, 1.0f / 6, 1.0f / 12,
                                   1.5f, 0.75f, 0.375f, 0.4f, 2.0f / 7 };
        return b[juce::jlimit (0, (int) std::size (b) - 1, i)];
    }
    const juce::StringArray& laneDivNames()
    {
        static const juce::StringArray s { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64" };
        return s;
    }
    float laneDivBeats (int i)
    {
        static const float b[] = { 4, 2, 1, 0.5f, 0.25f, 0.125f, 0.0625f };
        return b[juce::jlimit (0, (int) std::size (b) - 1, i)];
    }
    const juce::StringArray& ratioNames()
    {
        static const juce::StringArray s { "1:1", "3:2", "2:3", "4:3", "3:4", "5:4", "4:5", "5:3", "7:4", "7:8", "5:8", "9:8", "11:8", "13:8", "6:5", "7:6" };
        return s;
    }
    float ratioFactor (int i)
    {
        // "N:M" = N steps in the time of M  ->  step length x M/N
        static const float n[] = { 1, 3, 2, 4, 3, 5, 4, 5, 7, 7, 5, 9, 11, 13, 6, 7 };
        static const float m[] = { 1, 2, 3, 3, 4, 4, 5, 3, 4, 8, 8, 8, 8, 8, 5, 6 };
        i = juce::jlimit (0, (int) std::size (n) - 1, i);
        return m[i] / n[i];
    }
    const juce::StringArray& barNames()
    {
        static const juce::StringArray s { "1/4 bar", "1/2 bar", "1 bar", "2 bar", "4 bar", "8 bar", "16 bar" };
        return s;
    }
    float barBeats (int i)
    {
        static const float b[] = { 1, 2, 4, 8, 16, 32, 64 };
        return b[juce::jlimit (0, (int) std::size (b) - 1, i)];
    }

    const juce::StringArray& sourceNames()
    {
        static const juce::StringArray s = [] {
            juce::StringArray r { "None" };
            for (int k = 0; k < NLFO; ++k) r.add ("LFO " + juce::String (k + 1));
            for (int k = 0; k < NENV; ++k) r.add ("Env " + juce::String (k + 1));
            for (int k = 0; k < NRND; ++k) r.add ("Random " + juce::String (k + 1));
            for (int k = 0; k < NFOL; ++k) r.add ("Follower " + juce::String (k + 1));
            for (int k = 0; k < NLANE; ++k) r.add ("Lane " + juce::String (k + 1) + " Step");
            for (int k = 0; k < NMAC; ++k) r.add ("Macro " + juce::String (k + 1));
            r.addArray ({ "Velocity", "Note", "Mod Wheel", "Aftertouch", "Pitch Bend" });
            jassert (r.size() == NUM_SOURCES);
            return r;
        }();
        return s;
    }

    bool sourceIsBipolar (int src)
    {
        return (src >= srcLfo1 && src < srcLfo1 + NLFO) || (src >= srcRnd1 && src < srcRnd1 + NRND) || src == srcPB;
    }

    // ------------------------------------------------------------------------------------------- registry
    struct Registry
    {
        std::vector<Def> d;
        Ids ids {};
        std::vector<int> dst;
        std::vector<int> dstOf;
        std::map<juce::String, int> byId;
        juce::String group;

        int add (const juce::String& id, const juce::String& name, Kind k, float mn, float mx, float df, float centre = 0,
                 const juce::String& unit = {}, const juce::StringArray& ch = {})
        {
            Def x { id, name, k, mn, mx, df, centre, unit, ch, k == F || k == I, group };
            if (k == C) { x.mn = 0; x.mx = (float) juce::jmax (0, ch.size() - 1); }
            if (k == B) { x.mn = 0; x.mx = 1; }
            d.push_back (x);
            return (int) d.size() - 1;
        }
        int f (const juce::String& id, const juce::String& name, float mn, float mx, float df, float centre = 0, const juce::String& unit = {})
        { return add (id, name, F, mn, mx, df, centre, unit); }
        int i (const juce::String& id, const juce::String& name, int mn, int mx, int df) { return add (id, name, I, (float) mn, (float) mx, (float) df); }
        int b (const juce::String& id, const juce::String& name, bool df) { return add (id, name, B, 0, 1, df ? 1.0f : 0.0f); }
        int c (const juce::String& id, const juce::String& name, const juce::StringArray& ch, int df) { return add (id, name, C, 0, 0, (float) df, 0, {}, ch); }

        Registry()
        {
            const juce::StringArray onOff;
            const juce::StringArray laneSel { "Off", "Lane 1", "Lane 2", "Lane 3", "Lane 4" };
            const juce::StringArray trigSel { "MIDI", "Lane 1", "Lane 2", "Lane 3", "Lane 4", "Transient", "Glitch" };
            const juce::StringArray retrigSel { "Free", "Lane 1", "Lane 2", "Lane 3", "Lane 4", "MIDI", "Transient", "Bar" };
            const juce::StringArray chordSel { "Unison", "Fifth", "Octaves", "Minor", "Major", "Sus4", "Minor 7", "Scale" };
            const juce::StringArray patSel { "A", "B", "C", "D", "E", "F", "G", "H" };
            auto& I_ = ids;

            // ---------------------------------------------------------------- GLOBAL
            group = "Global";
            I_.mix      = f ("g_mix", "Dry/Wet", 0, 1, 1);
            I_.out      = f ("g_out", "Output", -36, 12, 0, 0, "dB");
            I_.norm     = b ("g_norm", "Norm", false);
            I_.density  = f ("g_density", "Density", 0, 1, 0.5f);
            I_.bpm      = f ("g_bpm", "Tempo (no host)", 60, 200, 124, 0, "bpm");
            I_.width    = f ("g_width", "Width", 0, 2, 1);
            I_.pattern  = c ("g_pattern", "Pattern", patSel, 0);
            I_.chain    = b ("g_chain", "Pattern Chain", false);
            I_.chainEnd = c ("g_chainend", "Chain End", patSel, 3);
            group = "Macro";
            for (int k = 0; k < NMAC; ++k) I_.macro[k] = f ("m_" + juce::String (k + 1), "Macro " + juce::String (k + 1), 0, 1, 0);

            // ---------------------------------------------------------------- INPUT
            group = "Input";
            I_.inLevel = f ("in_level", "Input Level", 0, 2, 1);
            I_.inGate  = f ("in_gate", "Input Gate", -90, 0, -90, 0, "dB");
            I_.trigThr = f ("in_trigthr", "Transient Threshold", -60, 0, -24, 0, "dB");
            I_.trigSrc = c ("in_trigsrc", "Transient Source", { "Main In", "Sidechain" }, 0);

            // ---------------------------------------------------------------- TEXTURE
            group = "Texture";
            I_.txOn     = b ("tx_on", "Texture On", false);
            I_.txLevel  = f ("tx_level", "Texture Level", 0, 1, 0.6f);
            I_.txBed    = f ("tx_bed", "Noise Bed", 0, 1, 0.5f);
            I_.txNoise  = c ("tx_noise", "Noise Colour", { "White", "Pink", "Brown", "Crackle" }, 1);
            I_.txResQ   = f ("tx_resq", "Bed Resonance", 0, 1, 0.6f);
            I_.txDrone  = f ("tx_drone", "Drone", 0, 1, 0.2f);
            I_.txDrift  = f ("tx_drift", "Drift", 0, 1, 0.3f);
            I_.txOct    = i ("tx_oct", "Texture Octave", -3, 3, 0);
            I_.txChord  = c ("tx_chord", "Texture Chord", chordSel, 3);
            I_.txGrain  = f ("tx_grain", "Grains", 0, 1, 0.4f);
            I_.txGSrc   = c ("tx_gsrc", "Grain Source", { "Input", "Bed", "Input+Bed" }, 2);
            I_.txDens   = f ("tx_dens", "Grain Density", 1, 200, 20, 20, "/s");
            I_.txSize   = f ("tx_size", "Grain Size", 5, 500, 80, 80, "ms");
            I_.txSpray  = f ("tx_spray", "Grain Spray", 0, 1, 0.3f);
            I_.txPitch  = f ("tx_pitch", "Grain Pitch", -24, 24, 0, 0, "st");
            I_.txPRand  = f ("tx_prand", "Pitch Random", 0, 12, 0, 0, "st");
            I_.txSpread = f ("tx_spread", "Grain Spread", 0, 1, 0.7f);
            I_.txFreeze = b ("tx_freeze", "Grain Freeze", false);
            I_.txTilt   = f ("tx_tilt", "Texture Tilt", -1, 1, 0);
            I_.txWin    = c ("tx_win", "Grain Window", { "Hann", "Tukey", "Rect" }, 0);
            I_.txRev    = f ("tx_rev", "Grain Reverse", 0, 1, 0.2f);

            // ---------------------------------------------------------------- PERC
            group = "Perc";
            I_.pcOn    = b ("pc_on", "Perc On", false);
            I_.pcLevel = f ("pc_level", "Perc Level", 0, 1, 0.7f);
            const juce::StringArray models { "Wood", "Glass", "Metal", "Skin", "Tube", "Stone", "Plastic",
                                            "Kick 808", "Tom 808", "Snare 808", "Clap 808", "Closed Hat 808", "Open Hat 808" };
            const juce::StringArray excs { "Click", "Noise", "Mallet", "Input" };
            const int defModel[NPERC] = { 0, 1, 2, 3, 4, 5 };
            const float defTune[NPERC] = { 0, 12, 19, -12, 7, 24 };
            for (int v = 0; v < NPERC; ++v)
            {
                const juce::String p = "pc" + juce::String (v + 1) + "_", n = "Perc " + juce::String (v + 1) + " ";
                I_.pcLane[v]  = c (p + "lane", n + "Lane", laneSel, 0);
                I_.pcModel[v] = c (p + "model", n + "Model", models, defModel[v]);
                I_.pcExc[v]   = c (p + "exc", n + "Exciter", excs, v == 3 ? 2 : 0);
                I_.pcTune[v]  = f (p + "tune", n + "Tune", -36, 36, defTune[v], 0, "st");
                I_.pcDecay[v] = f (p + "decay", n + "Decay", 10, 4000, 250, 300, "ms");
                I_.pcMat[v]   = f (p + "mat", n + "Material", 0, 1, 0.5f);
                I_.pcInh[v]   = f (p + "inh", n + "Inharmonic", 0, 1, 0.2f);
                I_.pcPos[v]   = f (p + "pos", n + "Position", 0, 1, 0.3f);
                I_.pcLvl[v]   = f (p + "level", n + "Level", 0, 1, 0.7f);
                I_.pcPan[v]   = f (p + "pan", n + "Pan", -1, 1, 0);
                I_.pcHum[v]   = f (p + "hum", n + "Humanize", 0, 1, 0.2f);
                I_.pcProb[v]  = f (p + "prob", n + "Probability", 0, 1, 1);
                I_.pcEvery[v] = i (p + "every", n + "Every", 1, 8, 1);
                I_.pcOffs[v]  = i (p + "offs", n + "Offset", 0, 7, 0);
            }

            // ---------------------------------------------------------------- SHEPARD generator
            group = "Shepard";
            I_.shOn       = b ("sh_on", "Shepard On", false);
            I_.shLevel    = f ("sh_level", "Shepard Level", 0, 1, 0.4f);
            I_.shPartials = i ("sh_partials", "Partials", 3, 10, 7);
            I_.shInterval = c ("sh_interval", "Interval", { "Octave", "Fifth", "Fourth", "Tritone", "Major 3rd", "Custom" }, 0);
            I_.shCents    = f ("sh_cents", "Custom Interval", 50, 1200, 700, 0, "ct");
            I_.shSync     = b ("sh_sync", "Shepard Sync", true);
            I_.shRate     = f ("sh_rate", "Shepard Rate", 0.01f, 4, 0.1f, 0.3f, "Hz");
            I_.shBars     = c ("sh_bars", "Shepard Cycle", barNames(), 4);
            I_.shDir      = c ("sh_dir", "Direction", { "Up", "Down", "Pendulum" }, 0);
            I_.shBase     = f ("sh_base", "Centre", 40, 4000, 300, 400, "Hz");
            I_.shWidth    = f ("sh_width", "Spectral Width", 0.1f, 1, 0.45f);
            I_.shWave     = c ("sh_wave", "Shepard Wave", { "Sine", "Triangle", "Noise Band" }, 0);

            // ---------------------------------------------------------------- GLITCH
            group = "Glitch";
            I_.glOn   = b ("gl_on", "Glitch On", true);
            I_.glMix  = f ("gl_mix", "Glitch Mix", 0, 1, 1);
            I_.glLane = c ("gl_lane", "Glitch Lane", { "MIDI only", "Lane 1", "Lane 2", "Lane 3", "Lane 4" }, 1);
            I_.glProb = f ("gl_prob", "Glitch Probability", 0, 1, 0.35f);
            I_.glLen  = i ("gl_len", "Event Length", 1, 16, 2);
            const float defW[NUM_GLITCH] = { 1, 0.3f, 0.5f, 0.15f, 0.3f, 0.2f, 0.1f, 0.2f, 0.2f };
            for (int g = 0; g < NUM_GLITCH; ++g)
                I_.glW[g] = f ("gl_w" + juce::String (g), "Weight " + glitchNames()[g], 0, 1, defW[g]);
            I_.glRptDiv    = c ("gl_rptdiv", "Repeat Size", noteDivNames(), 4);
            I_.glRptDecay  = f ("gl_rptdecay", "Repeat Decay", 0, 1, 0.2f);
            I_.glRptPitch  = f ("gl_rptpitch", "Repeat Pitch Ramp", -24, 24, 0, 0, "st");
            I_.glRptShrink = f ("gl_rptshrink", "Repeat Shrink", 0, 1, 0);
            I_.glSlices    = i ("gl_slices", "Slices", 2, 16, 8);
            I_.glStopTime  = f ("gl_stoptime", "Stop Time", 0.05f, 1, 0.6f);
            I_.glGateShape = c ("gl_gateshape", "Gate Shape", { "Square", "Ramp Down", "Ramp Up", "Triangle", "Pulse" }, 0);
            I_.glGateDiv   = c ("gl_gatediv", "Gate Rate", noteDivNames(), 5);
            I_.glBits      = f ("gl_bits", "Glitch Bits", 1, 16, 6);
            I_.glSRate     = f ("gl_srate", "Glitch Downsample", 0, 1, 0.5f);
            I_.glPitch     = f ("gl_pitch", "Glitch Pitch", -24, 24, 7, 0, "st");
            I_.glXfade     = f ("gl_xfade", "Crossfade", 0.5f, 10, 2, 0, "ms");
            I_.glFreezeSize= f ("gl_freezesize", "Freeze Size", 10, 500, 60, 80, "ms");

            // ---------------------------------------------------------------- LANES
            group = "Lanes";
            const juce::StringArray conds { "Always", "1:2", "2:2", "1:3", "1:4", "Fill", "Not Fill", "First" };
            const int defSteps[NLANE] = { 16, 12, 7, 5 }, defPulses[NLANE] = { 5, 5, 3, 2 }, defDiv[NLANE] = { 4, 4, 4, 3 };
            for (int l = 0; l < NLANE; ++l)
            {
                const juce::String p = "ln" + juce::String (l + 1) + "_", n = "Lane " + juce::String (l + 1) + " ";
                I_.lnOn[l]     = b (p + "on", n + "On", true);
                I_.lnMode[l]   = c (p + "mode", n + "Mode", { "Euclid", "Grid" }, 0);
                I_.lnSteps[l]  = i (p + "steps", n + "Steps", 1, 64, defSteps[l]);
                I_.lnPulses[l] = i (p + "pulses", n + "Pulses", 0, 64, defPulses[l]);
                I_.lnRot[l]    = i (p + "rot", n + "Rotate", 0, 63, 0);
                I_.lnDiv[l]    = c (p + "div", n + "Division", laneDivNames(), defDiv[l]);
                I_.lnRatio[l]  = c (p + "ratio", n + "Tuplet", ratioNames(), 0);
                I_.lnSync[l]   = b (p + "sync", n + "Sync", true);
                I_.lnHz[l]     = f (p + "hz", n + "Free Rate", 0.1f, 40, 4, 4, "Hz");
                I_.lnSwing[l]  = f (p + "swing", n + "Swing", 0, 0.75f, 0);
                I_.lnProb[l]   = f (p + "prob", n + "Probability", 0, 1, 1);
                I_.lnRatch[l]  = i (p + "ratch", n + "Ratchet", 1, 8, 2);
                I_.lnRProb[l]  = f (p + "rprob", n + "Ratchet Prob", 0, 1, 0);
                I_.lnSrc[l]    = c (p + "src", n + "Clock Source", { "Clock", "Transient", "MIDI" }, 0);
                I_.lnCond[l]   = c (p + "cond", n + "Condition", conds, 0);
                I_.lnHum[l]    = f (p + "hum", n + "Humanize", 0, 1, 0);
            }

            // ---------------------------------------------------------------- FX CHAIN
            group = "FX";
            const int defSlots[NFX] = { fxWobble, fxTape, fxVinyl, fxDelay, fxReverb, fxOff, fxOff, fxOff };
            for (int s = 0; s < NFX; ++s) I_.fxSlot[s] = c ("fx_slot" + juce::String (s + 1), "FX Slot " + juce::String (s + 1), fxNames(), defSlots[s]);

            group = "Tape";
            I_.tpOn    = b ("tp_on", "Tape On", false);
            I_.tpMix   = f ("tp_mix", "Tape Mix", 0, 1, 1);
            I_.tpDrive = f ("tp_drive", "Tape Drive", 0, 1, 0.4f);
            I_.tpSat   = f ("tp_sat", "Tape Saturation", 0, 1, 0.5f);
            I_.tpBias  = f ("tp_bias", "Tape Bias", 0, 1, 0.6f);
            I_.tpSpeed = c ("tp_speed", "Tape Speed", { "7.5 ips", "15 ips", "30 ips" }, 1);
            I_.tpHiss  = f ("tp_hiss", "Tape Hiss", 0, 1, 0.15f);
            I_.tpComp  = f ("tp_comp", "Tape Compression", 0, 1, 0.3f);
            I_.tpDrop  = f ("tp_drop", "Tape Dropouts", 0, 1, 0);
            I_.tpFlut  = f ("tp_flut", "Tape Wow/Flutter", 0, 1, 0.15f);
            I_.tpAge   = f ("tp_age", "Tape Age", 0, 1, 0.2f);
            I_.tpMode  = c ("tp_mode", "Tape Model", { "Hysteresis HQ", "Eco" }, 0);

            group = "Vinyl";
            I_.vnOn      = b ("vn_on", "Vinyl On", false);
            I_.vnMix     = f ("vn_mix", "Vinyl Mix", 0, 1, 1);
            I_.vnDust    = f ("vn_dust", "Dust", 0, 1, 0.3f);
            I_.vnCrackle = f ("vn_crackle", "Crackle", 0, 1, 0.3f);
            I_.vnScratch = f ("vn_scratch", "Scratch", 0, 1, 0.05f);
            I_.vnRumble  = f ("vn_rumble", "Rumble", 0, 1, 0.2f);
            I_.vnHum     = f ("vn_hum", "Hum", 0, 1, 0);
            I_.vnHumHz   = c ("vn_humhz", "Hum Frequency", { "50 Hz", "60 Hz" }, 0);
            I_.vnYear    = f ("vn_year", "Year", 1930, 2025, 1975, 0, "");
            I_.vnWear    = f ("vn_wear", "Wear", 0, 1, 0.25f);
            I_.vnWarp    = f ("vn_warp", "Warp", 0, 1, 0.1f);
            I_.vnRpm     = c ("vn_rpm", "RPM", { "33", "45", "78" }, 0);
            I_.vnBrake   = f ("vn_brake", "Brake (spin down)", 0, 1, 0);
            I_.vnMono    = f ("vn_mono", "Bass Mono", 0, 1, 0.5f);
            I_.vnNoise   = f ("vn_noise", "Noise Level", 0, 2, 1);

            group = "Wobble";
            I_.wbOn     = b ("wb_on", "Wobble On", false);
            I_.wbMix    = f ("wb_mix", "Wobble Mix", 0, 1, 1);
            I_.wbAmt    = f ("wb_amt", "Wobble Amount", 0, 1, 0.3f);
            I_.wbBal    = f ("wb_bal", "Wow <> Flutter", 0, 1, 0.3f);
            I_.wbWow    = f ("wb_wow", "Wow Rate", 0.1f, 4, 0.7f, 0.8f, "Hz");
            I_.wbFlut   = f ("wb_flut", "Flutter Rate", 6, 20, 11, 0, "Hz");
            I_.wbFlux   = f ("wb_flux", "Flux", 0, 1, 0.2f);
            I_.wbSpread = f ("wb_spread", "Stereo Spread", 0, 1, 0.2f);

            group = "Delay";
            I_.dlOn     = b ("dl_on", "Delay On", false);
            I_.dlMix    = f ("dl_mix", "Delay Mix", 0, 1, 0.3f);
            I_.dlMode   = c ("dl_mode", "Delay Mode", { "Digital", "Tape", "Ping-Pong", "Multitap" }, 2);
            I_.dlSync   = b ("dl_sync", "Delay Sync", true);
            I_.dlDiv    = c ("dl_div", "Delay Time", noteDivNames(), 13);
            I_.dlMs     = f ("dl_ms", "Delay ms", 1, 2000, 350, 300, "ms");
            I_.dlFb     = f ("dl_fb", "Feedback", 0, 1.1f, 0.45f);
            I_.dlLp     = f ("dl_lp", "Delay LP", 200, 20000, 6000, 3000, "Hz");
            I_.dlHp     = f ("dl_hp", "Delay HP", 20, 2000, 150, 200, "Hz");
            I_.dlSat    = f ("dl_sat", "Delay Saturation", 0, 1, 0.2f);
            I_.dlWob    = f ("dl_wob", "Delay Wobble", 0, 1, 0.1f);
            I_.dlPitch  = f ("dl_pitch", "Feedback Pitch", -24, 24, 0, 0, "st");
            I_.dlTaps   = c ("dl_taps", "Multitap Ratio", { "3", "4", "5", "7" }, 0);
            I_.dlFreeze = b ("dl_freeze", "Delay Freeze", false);
            I_.dlDuck   = f ("dl_duck", "Delay Ducking", 0, 1, 0);

            group = "Reverb";
            I_.rvOn     = b ("rv_on", "Reverb On", false);
            I_.rvMix    = f ("rv_mix", "Reverb Mix", 0, 1, 0.25f);
            I_.rvMode   = c ("rv_mode", "Reverb Mode", { "Hall", "Gated", "Reverse" }, 0);
            I_.rvSize   = f ("rv_size", "Size", 0, 1, 0.6f);
            I_.rvDecay  = f ("rv_decay", "Decay", 0.1f, 30, 2.5f, 3, "s");
            I_.rvPre    = f ("rv_pre", "Pre-delay", 0, 500, 20, 80, "ms");
            I_.rvDamp   = f ("rv_damp", "Damping", 0, 1, 0.5f);
            I_.rvMod    = f ("rv_mod", "Reverb Mod", 0, 1, 0.3f);
            I_.rvDiff   = f ("rv_diff", "Diffusion", 0, 1, 0.7f);
            I_.rvShim   = f ("rv_shim", "Shimmer", 0, 1, 0);
            I_.rvShInt  = f ("rv_shint", "Shimmer Interval", -24, 24, 12, 0, "st");
            I_.rvFreeze = b ("rv_freeze", "Reverb Freeze", false);
            I_.rvDuck   = f ("rv_duck", "Reverb Ducking", 0, 1, 0);
            I_.rvLow    = f ("rv_low", "Reverb Low Cut", 20, 1000, 120, 150, "Hz");

            group = "Filter";
            I_.ftOn    = b ("ft_on", "Filter On", false);
            I_.ftMix   = f ("ft_mix", "Filter Mix", 0, 1, 1);
            I_.ftType  = c ("ft_type", "Filter Type", { "LP", "HP", "BP", "Notch", "Comb", "Formant" }, 0);
            I_.ftSlope = c ("ft_slope", "Filter Slope", { "12 dB", "24 dB" }, 0);
            I_.ftCut   = f ("ft_cut", "Cutoff", 20, 20000, 2000, 1000, "Hz");
            I_.ftRes   = f ("ft_res", "Resonance", 0, 1, 0.2f);
            I_.ftDrive = f ("ft_drive", "Filter Drive", 0, 1, 0);
            I_.ftVowel = f ("ft_vowel", "Vowel", 0, 1, 0);

            group = "Resonator";
            I_.rsOn     = b ("rs_on", "Resonator On", false);
            I_.rsMix    = f ("rs_mix", "Resonator Mix", 0, 1, 0.5f);
            I_.rsChord  = c ("rs_chord", "Resonator Chord", chordSel, 3);
            I_.rsOct    = i ("rs_oct", "Resonator Octave", -3, 3, 0);
            I_.rsDecay  = f ("rs_decay", "Resonator Decay", 0, 1, 0.5f);
            I_.rsBright = f ("rs_bright", "Resonator Bright", 0, 1, 0.5f);
            I_.rsSpread = f ("rs_spread", "Resonator Spread", 0, 1, 0.5f);

            group = "Ring/Shift";
            I_.rfOn    = b ("rf_on", "Ring/Shift On", false);
            I_.rfMix   = f ("rf_mix", "Ring/Shift Mix", 0, 1, 0.5f);
            I_.rfMode  = c ("rf_mode", "Ring/Shift Mode", { "Ring", "Freq Shift" }, 0);
            I_.rfFreq  = f ("rf_freq", "Ring Freq", 0.1f, 5000, 220, 200, "Hz");
            I_.rfShift = f ("rf_shift", "Shift", -1000, 1000, 5, 0, "Hz");

            group = "Crusher";
            I_.crOn     = b ("cr_on", "Crusher On", false);
            I_.crMix    = f ("cr_mix", "Crusher Mix", 0, 1, 1);
            I_.crBits   = f ("cr_bits", "Bits", 1, 16, 10);
            I_.crRate   = f ("cr_rate", "Downsample", 0, 1, 0.2f);
            I_.crJitter = f ("cr_jitter", "Jitter", 0, 1, 0);

            group = "Chorus";
            I_.chOn     = b ("ch_on", "Chorus On", false);
            I_.chMix    = f ("ch_mix", "Chorus Mix", 0, 1, 0.5f);
            I_.chRate   = f ("ch_rate", "Chorus Rate", 0.05f, 8, 0.6f, 1, "Hz");
            I_.chDepth  = f ("ch_depth", "Chorus Depth", 0, 1, 0.4f);
            I_.chVoices = i ("ch_voices", "Chorus Voices", 1, 4, 2);

            group = "Phaser";
            I_.phOn     = b ("ph_on", "Phaser On", false);
            I_.phMix    = f ("ph_mix", "Phaser Mix", 0, 1, 0.5f);
            I_.phRate   = f ("ph_rate", "Phaser Rate", 0.02f, 10, 0.3f, 1, "Hz");
            I_.phDepth  = f ("ph_depth", "Phaser Depth", 0, 1, 0.6f);
            I_.phFb     = f ("ph_fb", "Phaser Feedback", 0, 0.95f, 0.4f);
            I_.phStages = c ("ph_stages", "Phaser Stages", { "4", "6", "8", "12" }, 1);
            I_.phCentre = f ("ph_centre", "Phaser Centre", 100, 8000, 800, 800, "Hz");

            group = "Comp";
            I_.cpOn    = b ("cp_on", "Comp On", false);
            I_.cpMix   = f ("cp_mix", "Comp Mix", 0, 1, 1);
            I_.cpThr   = f ("cp_thr", "Threshold", -60, 0, -18, 0, "dB");
            I_.cpRatio = f ("cp_ratio", "Ratio", 1, 20, 4, 4);
            I_.cpAtt   = f ("cp_att", "Comp Attack", 0.1f, 100, 5, 10, "ms");
            I_.cpRel   = f ("cp_rel", "Comp Release", 10, 1000, 120, 150, "ms");
            I_.cpGain  = f ("cp_gain", "Makeup", 0, 24, 0, 0, "dB");
            I_.cpTAtk  = f ("cp_tatk", "Transient Attack", -1, 1, 0);
            I_.cpTSus  = f ("cp_tsus", "Transient Sustain", -1, 1, 0);

            group = "Stereo";
            I_.stOn       = b ("st_on", "Stereo On", false);
            I_.stWidth    = f ("st_width", "Stereo Width", 0, 2, 1.2f);
            I_.stHaas     = f ("st_haas", "Haas", 0, 30, 0, 0, "ms");
            I_.stPanRate  = f ("st_panrate", "Auto-pan Rate", 0.05f, 10, 0.5f, 1, "Hz");
            I_.stPanDepth = f ("st_pandepth", "Auto-pan Depth", 0, 1, 0);
            I_.stMonoHz   = f ("st_monohz", "Bass Mono Below", 20, 500, 20, 120, "Hz");

            group = "Shepard FX";
            I_.sfOn    = b ("sf_on", "Shepard FX On", false);
            I_.sfMix   = f ("sf_mix", "Shepard FX Mix", 0, 1, 0.6f);
            I_.sfMode  = c ("sf_mode", "Shepard FX Mode", { "Filter", "Phaser", "Flanger", "Notch" }, 0);
            I_.sfSync  = b ("sf_sync", "Shepard FX Sync", true);
            I_.sfRate  = f ("sf_rate", "Shepard FX Rate", 0.01f, 4, 0.1f, 0.3f, "Hz");
            I_.sfBars  = c ("sf_bars", "Shepard FX Cycle", barNames(), 3);
            I_.sfDir   = c ("sf_dir", "Shepard FX Direction", { "Up", "Down" }, 0);
            I_.sfRes   = f ("sf_res", "Shepard FX Resonance", 0, 1, 0.5f);
            I_.sfBands = i ("sf_bands", "Shepard FX Bands", 4, 10, 7);

            // ---------------------------------------------------------------- TUNING
            group = "Tuning";
            I_.tuScale = c ("tu_scale", "Scale", tuning::builtinNames(), 0);
            I_.tuRoot  = c ("tu_root", "Root", { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0);
            I_.tuSnap  = f ("tu_snap", "Snap", 0, 1, 1);
            I_.tuMts   = b ("tu_mts", "Follow MTS-ESP", true);

            // ---------------------------------------------------------------- MODULATORS
            group = "LFO";
            const juce::StringArray shapes { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H", "Smooth Random", "Custom" };
            for (int k = 0; k < NLFO; ++k)
            {
                const juce::String p = "lf" + juce::String (k + 1) + "_", n = "LFO " + juce::String (k + 1) + " ";
                I_.lfShape[k]  = c (p + "shape", n + "Shape", shapes, k % 4 == 3 ? 5 : 0);
                I_.lfSync[k]   = b (p + "sync", n + "Sync", true);
                I_.lfRate[k]   = f (p + "rate", n + "Rate", 0.01f, 40, 1, 2, "Hz");
                I_.lfDiv[k]    = c (p + "div", n + "Division", syncDivNames(), 3 + (k % 4));
                I_.lfPhase[k]  = f (p + "phase", n + "Phase", 0, 1, 0);
                I_.lfSkew[k]   = f (p + "skew", n + "Skew", 0.01f, 0.99f, 0.5f);
                I_.lfSmooth[k] = f (p + "smooth", n + "Smooth", 0, 1, 0);
                I_.lfMode[k]   = c (p + "mode", n + "Mode", { "Loop", "One-shot" }, 0);
                I_.lfRetrig[k] = c (p + "retrig", n + "Retrigger", retrigSel, 0);
                I_.lfAmp[k]    = f (p + "amp", n + "Depth", 0, 1, 1);
                I_.lfUni[k]    = b (p + "uni", n + "Unipolar", false);
            }
            group = "Envelope";
            for (int k = 0; k < NENV; ++k)
            {
                const juce::String p = "en" + juce::String (k + 1) + "_", n = "Env " + juce::String (k + 1) + " ";
                I_.enTrig[k]  = c (p + "trig", n + "Trigger", trigSel, k == 0 ? 0 : k);
                I_.enA[k]     = f (p + "a", n + "Attack", 0.1f, 5000, 5, 100, "ms");
                I_.enH[k]     = f (p + "h", n + "Hold", 0, 2000, 0, 100, "ms");
                I_.enD[k]     = f (p + "d", n + "Decay", 1, 10000, 300, 400, "ms");
                I_.enS[k]     = f (p + "s", n + "Sustain", 0, 1, 0);
                I_.enR[k]     = f (p + "r", n + "Release", 1, 10000, 200, 400, "ms");
                I_.enCurve[k] = f (p + "curve", n + "Curve", -1, 1, 0.3f);
            }
            group = "Random";
            for (int k = 0; k < NRND; ++k)
            {
                const juce::String p = "rn" + juce::String (k + 1) + "_", n = "Random " + juce::String (k + 1) + " ";
                I_.rnMode[k]   = c (p + "mode", n + "Mode", { "S&H", "Smooth", "Drift", "Brownian" }, k);
                I_.rnSync[k]   = b (p + "sync", n + "Sync", true);
                I_.rnRate[k]   = f (p + "rate", n + "Rate", 0.01f, 40, 2, 2, "Hz");
                I_.rnDiv[k]    = c (p + "div", n + "Division", syncDivNames(), 7);
                I_.rnSmooth[k] = f (p + "smooth", n + "Smooth", 0, 1, 0);
            }
            group = "Follower";
            for (int k = 0; k < NFOL; ++k)
            {
                const juce::String p = "fo" + juce::String (k + 1) + "_", n = "Follower " + juce::String (k + 1) + " ";
                I_.foSrc[k]  = c (p + "src", n + "Source", { "Main In", "Sidechain", "Sources" }, k == 0 ? 0 : 1);
                I_.foAtt[k]  = f (p + "att", n + "Attack", 0.1f, 200, 5, 20, "ms");
                I_.foRel[k]  = f (p + "rel", n + "Release", 5, 2000, 150, 200, "ms");
                I_.foGain[k] = f (p + "gain", n + "Gain", -12, 24, 6, 0, "dB");
            }
            group = "Matrix";
            const juce::StringArray curves { "Linear", "Exp", "Log", "S-Curve" };
            for (int k = 0; k < NSLOT; ++k)
            {
                const juce::String p = "mx" + juce::String (k + 1) + "_", n = "Mod " + juce::String (k + 1) + " ";
                I_.mxSrc[k]   = c (p + "src", n + "Source", sourceNames(), 0);
                I_.mxVia[k]   = c (p + "via", n + "Via", sourceNames(), 0);
                I_.mxDst[k]   = c (p + "dst", n + "Destination", { "None" }, 0);   // filled below
                I_.mxAmt[k]   = f (p + "amt", n + "Amount", -1, 1, 0);
                I_.mxCurve[k] = c (p + "curve", n + "Curve", curves, 0);
            }

            group = "Global";
            I_.trim = f ("g_trim", "Preset Trim", -24, 24, 0, 0, "dB");

            group = "Progression";
            I_.prOn      = b ("pr_on", "Progression On", false);
            I_.prCount   = i ("pr_count", "Chords", 1, NSNAP, 4);
            I_.prSync    = b ("pr_sync", "Progression Sync", true);
            I_.prDiv     = c ("pr_div", "Chord Unit", syncDivNames(), 3);
            I_.prSecs    = f ("pr_secs", "Chord Unit (free)", 0.1f, 30, 2, 2, "s");
            for (int k = 0; k < NSNAP; ++k) I_.prLen[k] = i ("pr_len" + juce::String (k + 1), "Chord " + juce::String (k + 1) + " Length", 1, 16, 1);
            I_.prMode    = c ("pr_mode", "Progression Order", { "Forward", "Reverse", "Ping-Pong", "Random" }, 0);
            I_.prGlide   = f ("pr_glide", "Chord Glide", 0, 4000, 150, 400, "ms");
            I_.prVoicing = c ("pr_voicing", "Drone Voicing", { "Close", "Open", "Wide" }, 0);

            group = "Perc";
            for (int v = 0; v < NPERC; ++v)
            {
                const juce::String p = "pc" + juce::String (v + 1) + "_", n = "Perc " + juce::String (v + 1) + " ";
                I_.pcFType[v] = c (p + "ftype", n + "Filter", { "Off", "HP", "BP", "LP" }, 0);
                I_.pcCut[v]   = f (p + "cut", n + "Cutoff", 20, 20000, 1000, 1000, "Hz");
                I_.pcRes[v]   = f (p + "res", n + "Resonance", 0, 1, 0.1f);
            }

            // ---------------------------------------------------------------- destinations
            dstOf.assign (d.size(), 0);
            juce::StringArray dnames { "None" };
            for (int k = 0; k < (int) d.size(); ++k)
                if (d[(size_t) k].dest) { dst.push_back (k); dstOf[(size_t) k] = (int) dst.size(); dnames.add (d[(size_t) k].name); }
            for (int k = 0; k < NSLOT; ++k)
            {
                auto& x = d[(size_t) I_.mxDst[k]];
                x.choices = dnames; x.mx = (float) (dnames.size() - 1);
            }
            for (int k = 0; k < (int) d.size(); ++k) byId[d[(size_t) k].id] = k;
        }
    };

    static const Registry& reg() { static Registry r; return r; }

    const Ids& ids() { return reg().ids; }
    const std::vector<Def>& defs() { return reg().d; }
    const std::vector<int>& dests() { return reg().dst; }
    int destIndexOf (int p) { return juce::isPositiveAndBelow (p, (int) reg().dstOf.size()) ? reg().dstOf[(size_t) p] : 0; }

    int indexOf (const juce::String& id)
    {
        auto it = reg().byId.find (id);
        return it == reg().byId.end() ? -1 : it->second;
    }

    static juce::String formatValue (const juce::String& unit, float v)
    {
        if (unit == "Hz") return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (v, v < 10 ? 2 : (v < 100 ? 1 : 0)) + " Hz";
        if (unit == "ms") return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s" : juce::String (v, v < 10 ? 1 : 0) + " ms";
        if (unit == "s")  return juce::String (v, 2) + " s";
        if (unit == "st") return (v > 0 ? "+" : "") + juce::String (v, 2) + " st";
        if (unit == "ct") return juce::String (v, 0) + " ct";
        if (unit == "dB") return v <= -89.5f ? juce::String ("off") : juce::String (v, 1) + " dB";
        if (unit == "bpm") return juce::String (v, 1) + " bpm";
        if (unit == "/s") return juce::String (v, 1) + " /s";
        if (unit.isEmpty()) return juce::String (v, 2);
        return juce::String (v, 1) + " " + unit;
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        for (int k = 0; k < count(); ++k)
        {
            const auto& x = def (k);
            const juce::ParameterID pid { x.id, 1 };
            switch (x.kind)
            {
                case F:
                {
                    juce::NormalisableRange<float> r (x.mn, x.mx);
                    if (x.centre > x.mn && x.centre < x.mx) r.setSkewForCentre (x.centre);
                    const juce::String unit = x.unit;
                    const bool year = x.id == "vn_year";
                    auto attr = juce::AudioParameterFloatAttributes().withLabel (unit)
                        .withStringFromValueFunction ([unit, year] (float v, int) { return year ? juce::String ((int) std::round (v)) : formatValue (unit, v); });
                    layout.add (std::make_unique<juce::AudioParameterFloat> (pid, x.name, r, x.def, attr));
                    break;
                }
                case I: layout.add (std::make_unique<juce::AudioParameterInt> (pid, x.name, (int) x.mn, (int) x.mx, (int) x.def)); break;
                case B: layout.add (std::make_unique<juce::AudioParameterBool> (pid, x.name, x.def > 0.5f)); break;
                case C: layout.add (std::make_unique<juce::AudioParameterChoice> (pid, x.name, x.choices, (int) x.def)); break;
            }
        }
        return layout;
    }
}
