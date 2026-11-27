#include "Params.h"
#include "Scales.h"

using namespace juce;
using APF = AudioParameterFloat;
using APC = AudioParameterChoice;
using APB = AudioParameterBool;
using API = AudioParameterInt;

namespace
{
    // logarithmic 20 Hz .. 20 kHz: equal knob travel = equal octaves, so HP (left = open) and LP (right = open)
    // close and open exactly as mirror images of each other
    NormalisableRange<float> freqRange()
    {
        return NormalisableRange<float> (20.0f, 20000.0f,
            [] (float s, float e, float x) { return s * std::pow (e / s, x); },
            [] (float s, float e, float v) { return std::log (jlimit (s, e, v) / s) / std::log (e / s); },
            [] (float s, float e, float v) { return jlimit (s, e, v); });
    }
    NormalisableRange<float> msRange (float maxMs, float centre)
    {
        NormalisableRange<float> r (0.0f, maxMs);
        r.setSkewForCentre (centre);
        return r;
    }
    AudioParameterFloatAttributes withLabel (const char* l, int decimals = 1)
    {
        return AudioParameterFloatAttributes().withLabel (l)
            .withStringFromValueFunction ([decimals, l] (float v, int) { return String (v, decimals) + (strlen (l) ? String (" ") + l : String()); });
    }
    AudioParameterFloatAttributes percent()
    {
        return AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
    }
    AudioParameterFloatAttributes hz()
    {
        return AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        { return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (roundToInt (v)) + " Hz"; });
    }
    AudioParameterFloatAttributes db()
    {
        return AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        { return v <= -59.9f ? String ("-inf") : String (v, 1) + " dB"; });
    }
}

AudioProcessorValueTreeState::ParameterLayout lb::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;
    auto add = [&layout] (auto p) { layout.add (std::move (p)); };

    for (int m = 0; m < numModules; ++m)
    {
        const String n = moduleName (m) + " ";
        auto id = [m] (const char* s) { return ParameterID { pid (m, s), 1 }; };
        add (std::make_unique<APB> (id ("on"), n + "On", true));
        add (std::make_unique<APB> (id ("solo"), n + "Solo", false));
        add (std::make_unique<APF> (id ("gain"), n + "Gain", NormalisableRange<float> (-60.0f, 12.0f, 0.01f, 2.0f), -6.0f, db()));
        add (std::make_unique<APF> (id ("pan"), n + "Pan", NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
            { return std::abs (v) < 0.005f ? String ("C") : (v < 0 ? "L" : "R") + String (roundToInt (std::abs (v) * 100.0f)); })));
        add (std::make_unique<APF> (id ("atk"), n + "Attack", msRange (2000.0f, 60.0f), 0.0f, withLabel ("ms", 0)));
        add (std::make_unique<APF> (id ("dec"), n + "Decay", msRange (4000.0f, 300.0f), 500.0f, withLabel ("ms", 0)));
        add (std::make_unique<APF> (id ("sus"), n + "Sustain", NormalisableRange<float> (0.0f, 1.0f), 1.0f, percent()));
        add (std::make_unique<APF> (id ("rel"), n + "Release", msRange (4000.0f, 120.0f), 0.0f, withLabel ("ms", 0)));
        add (std::make_unique<APF> (id ("pitch"), n + "Pitch", NormalisableRange<float> (-32.0f, 32.0f, 0.001f), 0.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
            { return (v >= 0 ? "+" : "") + String (v, std::abs (v - std::round (v)) < 0.0005f ? 0 : 3) + " st"; })));
        // cutoff: three independent filters HP -> BP -> LP, each with its own ON and resonance
        add (std::make_unique<APB> (id ("hpOn"), n + "HP On", false));
        add (std::make_unique<APF> (id ("hpCut"), n + "HP Cutoff", freqRange(), 20.0f, hz()));
        add (std::make_unique<APF> (id ("hpRes"), n + "HP Resonance", NormalisableRange<float> (0.0f, 1.0f), 0.1f, percent()));
        add (std::make_unique<APB> (id ("bpOn"), n + "BP On", false));
        add (std::make_unique<APF> (id ("bpCut"), n + "BP Cutoff", freqRange(), 1000.0f, hz()));
        add (std::make_unique<APF> (id ("bpRes"), n + "BP Resonance", NormalisableRange<float> (0.0f, 1.0f), 0.3f, percent()));
        add (std::make_unique<APB> (id ("lpOn"), n + "LP On", false));
        add (std::make_unique<APF> (id ("lpCut"), n + "LP Cutoff", freqRange(), 20000.0f, hz()));
        add (std::make_unique<APF> (id ("lpRes"), n + "LP Resonance", NormalisableRange<float> (0.0f, 1.0f), 0.1f, percent()));
        add (std::make_unique<APF> (id ("dirt"), n + "Dirt", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent()));
        add (std::make_unique<APC> (id ("dtime"), n + "Delay Time", syncNames(), 8));
        add (std::make_unique<APF> (id ("dfb"), n + "Delay Feedback", NormalisableRange<float> (0.0f, 0.95f), 0.35f, percent()));
        add (std::make_unique<APF> (id ("dmix"), n + "Delay Mix", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent()));
        add (std::make_unique<APB> (id ("dpp"), n + "Delay Ping-Pong", false));
        add (std::make_unique<APF> (id ("rsize"), n + "Reverb Size", NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent()));
        add (std::make_unique<APF> (id ("rdecay"), n + "Reverb Decay", NormalisableRange<float> (0.2f, 20.0f, 0.01f, 0.4f), 2.0f, withLabel ("s", 2)));
        add (std::make_unique<APF> (id ("rdamp"), n + "Reverb Damping", NormalisableRange<float> (0.0f, 1.0f), 0.4f, percent()));
        add (std::make_unique<APF> (id ("rpre"), n + "Reverb Predelay", NormalisableRange<float> (0.0f, 200.0f), 10.0f, withLabel ("ms", 0)));
        add (std::make_unique<APF> (id ("rmix"), n + "Reverb Mix", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent()));
        // chorus (same DSP as the CHROMA5YNTH chorus)
        add (std::make_unique<APB> (id ("chOn"), n + "Chorus On", false));
        add (std::make_unique<APC> (id ("chMode"), n + "Chorus Mode", chorusModes(), 0));
        add (std::make_unique<APF> (id ("chRate"), n + "Chorus Rate", NormalisableRange<float> (0.01f, 10.0f, 0.0f, 0.4f), 0.6f, withLabel ("Hz", 2)));
        add (std::make_unique<APF> (id ("chDepth"), n + "Chorus Depth", NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent()));
        add (std::make_unique<APF> (id ("chMix"), n + "Chorus Mix", NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent()));
        // utility: stereo width (0 % = mono, 100 % = unchanged, up to 400 %) and MONO
        {
            NormalisableRange<float> wr (0.0f, 4.0f);
            wr.setSkewForCentre (1.0f);
            add (std::make_unique<APF> (id ("width"), n + "Width", wr, 1.0f, percent()));
        }
        add (std::make_unique<APB> (id ("mono"), n + "Mono", false));
        for (int l = 1; l <= numLfos; ++l)
        {
            const String L = "lfo" + String (l);
            add (std::make_unique<APC> (ParameterID { pid (m, (L + "rate").toRawUTF8()), 1 }, n + "LFO" + String (l) + " Rate", syncNames(), 10));
            add (std::make_unique<APC> (ParameterID { pid (m, (L + "shape").toRawUTF8()), 1 }, n + "LFO" + String (l) + " Shape", lfoShapes(), 0));
            add (std::make_unique<APF> (ParameterID { pid (m, (L + "depth").toRawUTF8()), 1 }, n + "LFO" + String (l) + " Depth", NormalisableRange<float> (-1.0f, 1.0f), 0.0f, percent()));
            add (std::make_unique<APC> (ParameterID { pid (m, (L + "target").toRawUTF8()), 1 }, n + "LFO" + String (l) + " Target", lfoTargets(), 0));
        }
        add (std::make_unique<APC> (id ("grid"), n + "Grid", gridNames(), 5));
        add (std::make_unique<API> (id ("steps"), n + "Length (steps)", 0, maxSteps, 0,
            AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return v == 0 ? String ("ORIG") : String (v); })));
        add (std::make_unique<APB> (id ("slice"), n + "Slice", false));
        add (std::make_unique<APF> (id ("sstart"), n + "Slice Start", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent()));
        add (std::make_unique<APF> (id ("send"), n + "Slice End", NormalisableRange<float> (0.0f, 1.0f), 1.0f, percent()));
        add (std::make_unique<APB> (id ("fade"), n + "Slice Fades", true));
        add (std::make_unique<APB> (id ("slloop"), n + "Slice Loop", false));
        // LOOP markers: the playback repeats between LS and LE (normal and SLICE mode)
        add (std::make_unique<APB> (id ("lmOn"), n + "Loop Markers", false));
        add (std::make_unique<APF> (id ("lmStart"), n + "Loop Start", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent()));
        add (std::make_unique<APF> (id ("lmEnd"), n + "Loop End", NormalisableRange<float> (0.0f, 1.0f), 1.0f, percent()));
        // SLICE markers: move in milliseconds (1 ms, shift 0.1 ms) or snap to the grid
        add (std::make_unique<APC> (id ("slsnap"), n + "Slice Snap", StringArray { "ms", "Grid" }, 0));
        add (std::make_unique<APB> (id ("vola"), n + "Volume Automation", false));
        add (std::make_unique<APC> (id ("keysrc"), n + "Type (key source)", StringArray { "Audio", "File Name" }, 0));
        // multi-out: Main mix / own stereo output (Separate) / both
        add (std::make_unique<APC> (id ("out"), n + "Output", StringArray { "Main", "Separate", "Main + Separate" }, 0));
    }

    // ---------------- global
    add (std::make_unique<APF> (ParameterID { "master", 1 }, "Master", NormalisableRange<float> (-60.0f, 12.0f, 0.01f, 2.0f), 0.0f, db()));
    add (std::make_unique<APB> (ParameterID { "scaleOn", 1 }, "Scale", false));
    add (std::make_unique<APC> (ParameterID { "scaleRoot", 1 }, "Scale Root", scales::rootNames(), 0));
    add (std::make_unique<APC> (ParameterID { "scaleType", 1 }, "Scale Type", scales::names(), 0));
    add (std::make_unique<APB> (ParameterID { "raw", 1 }, "Raw", false));
    add (std::make_unique<APB> (ParameterID { "run", 1 }, "Run (internal clock)", false));
    add (std::make_unique<APF> (ParameterID { "bpm", 1 }, "Internal BPM", NormalisableRange<float> (40.0f, 240.0f, 0.01f), 120.0f, withLabel ("BPM", 1)));
    // global swing
    add (std::make_unique<APB> (ParameterID { "swingOn", 1 }, "Swing On", false));
    add (std::make_unique<APF> (ParameterID { "swing", 1 }, "Swing Amount", NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent()));
    add (std::make_unique<APC> (ParameterID { "swingGrid", 1 }, "Swing Grid", StringArray { "1/8", "1/16" }, 1));
    // master cutoff: HP (left = open) -> BP -> LP (right = open), each with its own ON and resonance
    add (std::make_unique<APB> (ParameterID { "mHpOn", 1 }, "Master HP On", false));
    add (std::make_unique<APF> (ParameterID { "mHp", 1 }, "Master HP", freqRange(), 20.0f, hz()));
    add (std::make_unique<APF> (ParameterID { "mHpRes", 1 }, "Master HP Resonance", NormalisableRange<float> (0.0f, 1.0f), 0.1f, percent()));
    add (std::make_unique<APB> (ParameterID { "mBpOn", 1 }, "Master BP On", false));
    add (std::make_unique<APF> (ParameterID { "mBpCut", 1 }, "Master BP", freqRange(), 1000.0f, hz()));
    add (std::make_unique<APF> (ParameterID { "mBpRes", 1 }, "Master BP Resonance", NormalisableRange<float> (0.0f, 1.0f), 0.3f, percent()));
    add (std::make_unique<APB> (ParameterID { "mLpOn", 1 }, "Master LP On", false));
    add (std::make_unique<APF> (ParameterID { "mLp", 1 }, "Master LP", freqRange(), 20000.0f, hz()));
    add (std::make_unique<APF> (ParameterID { "mLpRes", 1 }, "Master LP Resonance", NormalisableRange<float> (0.0f, 1.0f), 0.1f, percent()));
    // beat repeat
    add (std::make_unique<APB> (ParameterID { "brOn", 1 }, "Beat Repeat On", false));
    add (std::make_unique<APC> (ParameterID { "brInterval", 1 }, "BR Interval", brIntervalNames(), 5));
    add (std::make_unique<API> (ParameterID { "brOffset", 1 }, "BR Offset", 0, 15, 8));
    add (std::make_unique<APC> (ParameterID { "brGrid", 1 }, "BR Grid", brGridNames(), 2));
    add (std::make_unique<APF> (ParameterID { "brVar", 1 }, "BR Variation", NormalisableRange<float> (0.0f, 10.0f, 0.01f), 0.0f, withLabel ("", 1)));
    add (std::make_unique<APF> (ParameterID { "brChance", 1 }, "BR Chance", NormalisableRange<float> (0.0f, 1.0f), 1.0f, percent()));
    add (std::make_unique<API> (ParameterID { "brGate", 1 }, "BR Length (16ths)", 1, 16, 4));
    add (std::make_unique<APF> (ParameterID { "brDecay", 1 }, "BR Decay", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent()));
    add (std::make_unique<API> (ParameterID { "brPitch", 1 }, "BR Pitch", -48, 0, 0));
    add (std::make_unique<APF> (ParameterID { "brPDecay", 1 }, "BR Pitch Decay", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent()));
    add (std::make_unique<APC> (ParameterID { "brMode", 1 }, "BR Mix Mode", StringArray { "Mix", "Insert" }, 1));
    add (std::make_unique<APF> (ParameterID { "brVol", 1 }, "BR Volume", NormalisableRange<float> (-36.0f, 6.0f, 0.01f), 0.0f, db()));
    // tape / vinyl
    add (std::make_unique<APC> (ParameterID { "deck", 1 }, "Remix Deck", StringArray { "Off", "Tape", "Vinyl" }, 0));
    add (std::make_unique<APF> (ParameterID { "deckCtl", 1 }, "Deck Hand (controller)", NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)); })));
    // turntable: pitch fader (speed + pitch, like a real deck), 33 / 45 rpm, motor START / STOP
    add (std::make_unique<APF> (ParameterID { "vinylPitch", 1 }, "Vinyl Pitch Fader", NormalisableRange<float> (-8.0f, 8.0f, 0.001f), 0.0f, withLabel ("%", 2)));
    add (std::make_unique<APB> (ParameterID { "vinyl45", 1 }, "Vinyl 45 rpm", false));
    add (std::make_unique<APB> (ParameterID { "vinylMotor", 1 }, "Vinyl Motor (start / stop)", true));
    return layout;
}
