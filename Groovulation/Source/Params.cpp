#include "Params.h"
#include "Scales.h"

using namespace juce;
using APF = AudioParameterFloat;
using APC = AudioParameterChoice;
using APB = AudioParameterBool;
using API = AudioParameterInt;

namespace
{
    NormalisableRange<float> freqRange()
    {
        NormalisableRange<float> r (20.0f, 20000.0f);
        r.setSkewForCentre (1000.0f);
        return r;
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
        {
            // fine resolution (like PITCH): 0.5x .. 2x, centre = 1x, 1/100000 steps
            NormalisableRange<float> tr (0.5f, 2.0f, 0.00001f);
            tr.setSkewForCentre (1.0f);
            add (std::make_unique<APF> (id ("tempo"), n + "Tempo", tr, 1.0f,
                AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return "x" + String (v, 4); })));
        }
        add (std::make_unique<APC> (id ("ftype"), n + "Filter Type", StringArray { "LP", "BP", "HP" }, 0));
        add (std::make_unique<APF> (id ("cut"), n + "Cutoff", freqRange(), 20000.0f, hz()));
        add (std::make_unique<APF> (id ("res"), n + "Resonance", NormalisableRange<float> (0.0f, 1.0f), 0.1f, percent()));
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
        // SLICE markers: move in milliseconds (1 ms, shift 0.1 ms) or snap to the grid
        add (std::make_unique<APC> (id ("slsnap"), n + "Slice Snap", StringArray { "ms", "Grid" }, 0));
        add (std::make_unique<APB> (id ("vola"), n + "Volume Automation", false));
        add (std::make_unique<APC> (id ("keysrc"), n + "Type (key source)", StringArray { "Audio", "File Name" }, 0));
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
    // master cutoff: HP (left = open) | BP (min = off) | LP (right = open)
    add (std::make_unique<APF> (ParameterID { "mHp", 1 }, "Master HP", freqRange(), 20.0f, hz()));
    add (std::make_unique<APF> (ParameterID { "mBp", 1 }, "Master BP", NormalisableRange<float> (0.0f, 1.0f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        { return v < 0.005f ? String ("OFF") : String (roundToInt (60.0f * std::pow (8000.0f / 60.0f, v))) + " Hz"; })));
    add (std::make_unique<APF> (ParameterID { "mLp", 1 }, "Master LP", freqRange(), 20000.0f, hz()));
    add (std::make_unique<APF> (ParameterID { "mRes", 1 }, "Master Resonance", NormalisableRange<float> (0.0f, 1.0f), 0.15f, percent()));
    // beat repeat
    add (std::make_unique<APB> (ParameterID { "brOn", 1 }, "Beat Repeat On", false));
    add (std::make_unique<APC> (ParameterID { "brInterval", 1 }, "BR Interval", brIntervalNames(), 5));
    add (std::make_unique<API> (ParameterID { "brOffset", 1 }, "BR Offset", 0, 15, 8));
    add (std::make_unique<APC> (ParameterID { "brGrid", 1 }, "BR Grid", brGridNames(), 2));
    add (std::make_unique<APB> (ParameterID { "brNoTrip", 1 }, "BR No Triplets", false));
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
