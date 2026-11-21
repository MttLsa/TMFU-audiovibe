#include "Params.h"
#include <map>

namespace params
{
    static juce::StringArray modelNames { "Generic Analog", "Generic Digital" };
    static std::vector<int> modelDefaults { 0, 1, 0, 1, 0 };
    void setModelNames (const juce::StringArray& n, const std::vector<int>& d) { if (n.size() > 0) { modelNames = n; modelDefaults = d; } }

    static juce::StringArray split (const char* s) { return juce::StringArray::fromTokens (s, "|", ""); }

    static const char* lfoShapes = "Triangle|Sine|Saw Up|Saw Down|Square|S&H|Random";
    static const char* divs = "8 bar|4 bar|2 bar|1 bar|1/2|1/2T|1/4|1/4D|1/4T|1/8|1/8D|1/8T|1/16|1/16T|1/32";

    float lfoDivBeats (int d)
    {
        static const float b[] = { 32, 16, 8, 4, 2, 4.0f / 3, 1, 1.5f, 2.0f / 3, 0.5f, 0.75f, 1.0f / 3, 0.25f, 1.0f / 6, 0.125f };
        return b[juce::jlimit (0, 14, d)];
    }

    struct Builder
    {
        std::vector<Def> d;
        int add (juce::String id, juce::String name, Kind k, float mn, float mx, float df, float ctr = 0, juce::String unit = {}, juce::StringArray ch = {})
        {
            Def x; x.id = id; x.name = name; x.kind = k; x.mn = mn; x.mx = mx; x.def = df; x.centre = ctr; x.unit = unit; x.choices = ch;
            if (k == C) { x.mn = 0; x.mx = (float) juce::jmax (0, ch.size() - 1); }
            if (k == B) { x.mn = 0; x.mx = 1; }
            d.push_back (x);
            return (int) d.size() - 1;
        }
    };

    static void moduleTable (Builder& b, int m)
    {
        const juce::String P = juce::String (moduleLetter (m)) + "_";
        const juce::String N = juce::String (moduleLetter (m)) + " ";
        auto add = [&] (const char* sfx, const char* name, Kind k, float mn, float mx, float df, float ctr = 0, const char* unit = "", juce::StringArray ch = {}, bool fader = false)
        {
            const int i = b.add (P + sfx, N + name, k, mn, mx, df, ctr, unit, ch);
            b.d[(size_t) i].module = m;
            b.d[(size_t) i].fader = fader;
            return i;
        };
        const int base = (int) b.d.size();
        add ("on", "On", B, 0, 1, m == 0 ? 1.0f : 0.0f);
        add ("model", "Synth Model", C, 0, 0, (float) juce::jlimit (0, modelNames.size() - 1, modelDefaults[(size_t) m]), 0, "", modelNames);
        add ("level", "Level", F, 0, 1, 0.8f, 0, "", {}, true);
        add ("pan", "Pan", F, -1, 1, 0);
        add ("engine", "Engine", C, 0, 0, m == 1 ? 1.0f : 0.0f, 0, "", split ("Analog|FM 6-op|Wavetable|Supersaw|Phase Dist|Additive"));
        add ("oct", "Octave", I, -3, 3, 0);
        add ("semi", "Semi", I, -12, 12, 0, 0, "st");
        add ("fine", "Fine", F, -100, 100, 0, 0, "ct");
        // waveforms of the analog oscillators (the panel of each instrument offers only the ones its original has)
        static const char* oscWaves = "Saw|Pulse|Triangle|Sine|Shark Tooth|Square|Wide Rectangle|Narrow Rectangle|Reverse Saw";
        add ("o1wave", "Osc1 Wave", C, 0, 0, 0, 0, "", split (oscWaves));
        add ("o1pw", "Osc1 PW", F, 0.05f, 0.95f, 0.5f, 0, "", {}, true);
        add ("o1lvl", "Osc1 Level", F, 0, 1, 1, 0, "", {}, true);
        add ("o2wave", "Osc2 Wave", C, 0, 0, 0, 0, "", split (oscWaves));
        add ("o2oct", "Osc2 Octave", I, -3, 3, 0);
        add ("o2semi", "Osc2 Semi", I, -12, 12, 0, 0, "st");
        add ("o2fine", "Osc2 Fine", F, -100, 100, 0, 0, "ct");
        add ("o2pw", "Osc2 PW", F, 0.05f, 0.95f, 0.5f, 0, "", {}, true);
        add ("o2lvl", "Osc2 Level", F, 0, 1, 0, 0, "", {}, true);
        add ("sync", "Sync", B, 0, 1, 0);
        add ("xmod", "Cross Mod", F, 0, 1, 0, 0, "", {}, true);
        add ("ring", "Ring Mod", F, 0, 1, 0, 0, "", {}, true);
        add ("sub", "Sub Osc", F, 0, 1, 0, 0, "", {}, true);
        add ("noise", "Noise", F, 0, 1, 0, 0, "", {}, true);
        add ("pwm", "PWM", F, 0, 1, 0, 0, "", {}, true);
        add ("uni", "Unison", I, 1, 7, 1);
        add ("detune", "Detune", F, 0, 1, 0.2f);
        add ("drift", "Drift", F, 0, 1, 0.15f);
        add ("fmmodern", "FM Modern", B, 0, 1, 0);   // no longer shown (MODERN view removed); kept so saved projects keep their parameter list
        juce::StringArray algs; for (int i = 1; i <= 32; ++i) algs.add (juce::String (i));
        add ("alg", "FM Algorithm", C, 0, 0, 0, 0, "", algs);
        add ("fb", "FM Feedback", F, 0, 1, 0);
        add ("fmvel", "FM Vel>Index", F, 0, 1, 0.5f);
        for (int o = 0; o < numOps; ++o)
        {
            const juce::String on = juce::String (o + 1);
            add (("op" + on + "crs").toRawUTF8(), ("Op" + on + " Coarse").toRawUTF8(), I, 0, 31, 1);
            add (("op" + on + "fin").toRawUTF8(), ("Op" + on + " Fine").toRawUTF8(), F, 0, 0.99f, 0);
            add (("op" + on + "lvl").toRawUTF8(), ("Op" + on + " Level").toRawUTF8(), F, 0, 1, o == 0 ? 1.0f : 0.0f);
            add (("op" + on + "atk").toRawUTF8(), ("Op" + on + " Attack").toRawUTF8(), F, 0.5f, 10000, 1, 300, "ms");
            add (("op" + on + "dec").toRawUTF8(), ("Op" + on + " Decay").toRawUTF8(), F, 5, 20000, 1000, 800, "ms");
            add (("op" + on + "sus").toRawUTF8(), ("Op" + on + " Sustain").toRawUTF8(), F, 0, 1, 1);
            add (("op" + on + "rel").toRawUTF8(), ("Op" + on + " Release").toRawUTF8(), F, 5, 20000, 300, 800, "ms");
        }
        add ("wt", "Wavetable", C, 0, 0, 0, 0, "", split ("Analog Morph|Pulse Width|PPG Digital|Vocal|Bell|Organ|Hollow|Sync Sweep|Growl|Bit Steps|Strings|Reso Sweep"));
        add ("wpos", "WT Position", F, 0, 1, 0);
        add ("wenv", "WT Env", F, -1, 1, 0);
        add ("wlfo", "WT LFO", F, 0, 1, 0);
        add ("ssdet", "Supersaw Detune", F, 0, 1, 0.5f);
        add ("ssmix", "Supersaw Mix", F, 0, 1, 0.6f);
        add ("pdw", "PD Wave", C, 0, 0, 0, 0, "", split ("Saw|Square|Pulse|Double Sine|Reso Saw|Reso Tri|Reso Trap"));
        add ("pdamt", "PD Amount", F, 0, 1, 0.5f);
        add ("pdenv", "PD Env", F, -1, 1, 0);
        add ("addm", "Additive Model", C, 0, 0, 0, 0, "", split ("House Organ|Drawbar Organ|E.Piano|Piano|Strings|Choir|Bell|Mallet|Glass|Brass|Soft Pad"));
        add ("bright", "Brightness", F, 0, 1, 0.5f);
        add ("tilt", "Partial Decay", F, 0, 1, 0.5f);
        add ("inh", "Inharmonicity", F, 0, 1, 0);
        add ("even", "Even Partials", F, 0, 1, 1);
        add ("flt", "Filter Type", C, 0, 0, 0, 0, "", split ("LP24 Ladder|LP 303 Diode|LP12|BP12|HP12|LP24 Clean|Notch|Off"));
        add ("cut", "Cutoff", F, 20, 20000, 8000, 1000, "Hz", {}, true);
        add ("res", "Resonance", F, 0, 1, 0.1f, 0, "", {}, true);
        add ("fdrive", "Filter Drive", F, 0, 1, 0.1f);
        add ("key", "Key Track", F, 0, 1, 0.5f, 0, "", {}, true);
        add ("env", "Env Amount", F, -1, 1, 0.3f, 0, "", {}, true);
        add ("velf", "Vel>Filter", F, 0, 1, 0.2f);
        add ("dist", "Distortion", C, 0, 0, 0, 0, "", split ("Off|Tube|RAT|DS-1"));
        add ("dgain", "Dist Gain", F, 0, 1, 0.5f);
        add ("dtone", "Dist Tone", F, 0, 1, 0.5f);
        add ("dmix", "Dist Mix", F, 0, 1, 1);
        add ("fa", "F.Attack", F, 0.5f, 10000, 2, 300, "ms", {}, true);
        add ("fd", "F.Decay", F, 0.5f, 10000, 400, 300, "ms", {}, true);
        add ("fs", "F.Sustain", F, 0, 1, 0.4f, 0, "", {}, true);
        add ("fr", "F.Release", F, 0.5f, 10000, 300, 300, "ms", {}, true);
        add ("aa", "Attack", F, 0.5f, 10000, 2, 300, "ms", {}, true);
        add ("ad", "Decay", F, 0.5f, 10000, 500, 300, "ms", {}, true);
        add ("as", "Sustain", F, 0, 1, 1, 0, "", {}, true);
        add ("ar", "Release", F, 0.5f, 10000, 250, 300, "ms", {}, true);
        add ("lfoshape", "LFO Shape", C, 0, 0, 0, 0, "", split ("Triangle|Sine|Saw Up|Saw Down|Square|S&H"));
        add ("lforate", "LFO Rate", F, 0.05f, 30, 4, 3, "Hz", {}, true);
        add ("lfodelay", "LFO Delay", F, 0, 5000, 0, 500, "ms");
        add ("lfopitch", "LFO>Pitch", F, 0, 12, 0, 1, "st", {}, true);
        add ("lfocut", "LFO>Cutoff", F, 0, 1, 0, 0, "", {}, true);
        add ("lfotim", "LFO>Timbre", F, 0, 1, 0);
        add ("penv", "Pitch Env", F, -48, 48, 0, 0, "st");
        add ("pdec", "Pitch Decay", F, 1, 3000, 100, 150, "ms");
        add ("accent", "Accent", F, 0, 1, 0.5f);
        add ("bits", "Bit Crush", F, 0, 1, 0);
        add ("srr", "Rate Reduce", F, 0, 1, 0);
        add ("dlydiv", "Delay Time", C, 0, 0, 3, 0, "", split ("1/16|1/8T|1/8|1/8D|1/4T|1/4|1/4D|1/2"));
        add ("dlyfb", "Delay Feedback", F, 0, 0.95f, 0.35f);
        add ("dlymix", "Delay Mix", F, 0, 1, 0);
        add ("dlyping", "Delay Ping-Pong", B, 0, 1, 1);
        add ("relsw", "Decay Switch", B, 0, 1, 1);
        jassert ((int) b.d.size() - base == numModSuffix);
        for (int i = base; i < (int) b.d.size(); ++i) b.d[(size_t) i].suffix = i - base;
    }

    static Layout L;
    static std::vector<Def> D;

    static void build()
    {
        Builder b;
        for (int m = 0; m < numModules; ++m) { L.moduleBase[m] = (int) b.d.size(); moduleTable (b, m); }
        L.gMaster  = b.add ("g_master", "Master", F, -36, 6, 0, 0, "dB");
        L.gTrim    = b.add ("g_trim", "Preset Trim", F, -36, 36, 0, 0, "dB");
        L.gMode    = b.add ("g_mode", "Voice Mode", C, 0, 0, 0, 0, "", split ("Poly|Mono|Legato"));
        L.gGlide   = b.add ("g_glide", "Glide", F, 0, 3000, 0, 200, "ms");
        L.gBend    = b.add ("g_bend", "Bend Range", I, 0, 24, 2, 0, "st");
        L.gVel     = b.add ("g_vel", "Velocity", F, 0, 1, 0.5f);
        L.gWidth   = b.add ("g_width", "Stereo Width", F, 0, 1, 0.6f);
        L.gMwVib   = b.add ("g_mwvib", "MW Vibrato", F, 0, 1, 0.3f);
        L.gMwCut   = b.add ("g_mwcut", "MW Cutoff", F, 0, 1, 0);
        L.gDrive   = b.add ("g_drive", "Drive", F, 0, 1, 0);
        L.gChoMode = b.add ("g_chomode", "Chorus", C, 0, 0, 0, 0, "", split ("Off|I|II|I+II|Ensemble"));
        L.gChoMix  = b.add ("g_chomix", "Chorus Mix", F, 0, 1, 0.5f);
        L.gRevSize = b.add ("g_revsize", "Reverb Size", F, 0, 1, 0.6f);
        L.gRevDamp = b.add ("g_revdamp", "Reverb Damp", F, 0, 1, 0.5f);
        L.gRevMix  = b.add ("g_revmix", "Reverb Mix", F, 0, 1, 0);
        L.gBpm     = b.add ("g_bpm", "Tempo (no host)", F, 60, 200, 124, 0, "bpm");
        L.gSlots   = b.add ("g_slots", "Modules", I, 1, 5, 1);   // a patch shows only the modules it uses; + CUSTOM adds more
        L.ofLpOn  = b.add ("of_lpon", "Out LP On", B, 0, 1, 0);
        L.ofLpCut = b.add ("of_lpcut", "Out LP Cutoff", F, 20, 20000, 20000, 1000, "Hz");
        L.ofLpRes = b.add ("of_lpres", "Out LP Reso", F, 0, 1, 0.1f);
        L.ofBpOn  = b.add ("of_bpon", "Out BP On", B, 0, 1, 0);
        L.ofBpCut = b.add ("of_bpcut", "Out BP Cutoff", F, 20, 20000, 1000, 1000, "Hz");
        L.ofBpRes = b.add ("of_bpres", "Out BP Reso", F, 0, 1, 0.1f);
        L.ofHpOn  = b.add ("of_hpon", "Out HP On", B, 0, 1, 0);
        L.ofHpCut = b.add ("of_hpcut", "Out HP Cutoff", F, 20, 20000, 20, 1000, "Hz");
        L.ofHpRes = b.add ("of_hpres", "Out HP Reso", F, 0, 1, 0.1f);
        static const float eqF[] = { 80, 250, 1000, 4000, 10000 };
        static const int eqT[] = { 1, 2, 2, 2, 3 };
        for (int e = 0; e < numEqBands; ++e)
        {
            const juce::String P = "eq" + juce::String (e + 1) + "_", N = "EQ" + juce::String (e + 1) + " ";
            L.eqBase[e] = b.add (P + "on", N + "On", B, 0, 1, 1);
            b.add (P + "type", N + "Type", C, 0, 0, (float) eqT[e], 0, "", split ("Low Cut|Low Shelf|Bell|High Shelf|High Cut|Notch"));
            b.add (P + "freq", N + "Freq", F, 20, 20000, eqF[e], 1000, "Hz");
            b.add (P + "gain", N + "Gain", F, -18, 18, 0, 0, "dB");
            b.add (P + "q", N + "Q", F, 0.1f, 18, 0.9f, 1.5f);
        }
        L.numTargets = (int) b.d.size();
        for (int l = 0; l < numLfos; ++l)
        {
            const juce::String P = "l" + juce::String (l + 1) + "_", N = "LFO" + juce::String (l + 1) + " ";
            L.lfoBase[l] = b.add (P + "target", N + "Target", I, 0, (float) L.numTargets, 0);
            b.add (P + "shape", N + "Shape", C, 0, 0, 1, 0, "", split (lfoShapes));
            b.add (P + "rate", N + "Rate", F, 0.01f, 40, 1, 2, "Hz");
            b.add (P + "sync", N + "Sync", B, 0, 1, 0);
            b.add (P + "div", N + "Division", C, 0, 0, 6, 0, "", split (divs));
            b.add (P + "depth", N + "Depth", F, -1, 1, 0.5f);
            b.add (P + "phase", N + "Phase", F, 0, 360, 0, 0, "deg");
        }
        L.num = (int) b.d.size();
        D = std::move (b.d);
    }

    const Layout& layout() { if (D.empty()) build(); return L; }
    const std::vector<Def>& defs() { if (D.empty()) build(); return D; }

    int indexOf (const juce::String& id)
    {
        static std::map<juce::String, int> m;
        if (m.empty()) for (int i = 0; i < numParams(); ++i) m[def (i).id] = i;
        auto it = m.find (id);
        return it == m.end() ? -1 : it->second;
    }

    static juce::String fmtValue (const Def& d, float v)
    {
        const auto& u = d.unit;
        if (u == "Hz") return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (v, v < 100 ? 2 : 1) + " Hz";
        if (u == "ms") return v >= 1000.0f ? juce::String (v / 1000.0f, 3) + " s" : juce::String (v, v < 100 ? 2 : 1) + " ms";
        if (u.isEmpty()) return juce::String (v, 3);
        return juce::String (v, 2) + " " + u;
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout lay;
        const auto& ds = defs();
        for (int i = 0; i < (int) ds.size(); ++i)
        {
            const auto& d = ds[(size_t) i];
            const juce::ParameterID pid { d.id, 1 };
            switch (d.kind)
            {
                case F:
                {
                    juce::NormalisableRange<float> r (d.mn, d.mx);
                    if (d.centre > d.mn && d.centre < d.mx) r.setSkewForCentre (d.centre);
                    const Def copy = d;
                    auto attr = juce::AudioParameterFloatAttributes().withLabel (d.unit)
                        .withStringFromValueFunction ([copy] (float v, int) { return fmtValue (copy, v); });
                    lay.add (std::make_unique<juce::AudioParameterFloat> (pid, d.name, r, d.def, attr));
                    break;
                }
                case I:
                {
                    if (d.id.endsWith ("_target"))
                    {
                        auto attr = juce::AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) {
                            return v <= 0 ? juce::String ("-- off") : def (v - 1).name; });
                        lay.add (std::make_unique<juce::AudioParameterInt> (pid, d.name, (int) d.mn, (int) d.mx, (int) d.def, attr));
                    }
                    else lay.add (std::make_unique<juce::AudioParameterInt> (pid, d.name, (int) d.mn, (int) d.mx, (int) d.def));
                    break;
                }
                case B: lay.add (std::make_unique<juce::AudioParameterBool> (pid, d.name, d.def > 0.5f)); break;
                case C: lay.add (std::make_unique<juce::AudioParameterChoice> (pid, d.name, d.choices, (int) d.def)); break;
            }
        }
        return lay;
    }
}
