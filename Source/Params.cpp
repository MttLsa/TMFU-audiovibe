#include "Params.h"

using namespace juce;

namespace params
{
namespace
{
    using Layout = AudioProcessorValueTreeState::ParameterLayout;

    NormalisableRange<float> logRange (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    String hzText (float v, int)   { return v >= 1000.f ? String (v / 1000.f, 2) + " kHz" : String (v, v < 100.f ? 1 : 0) + " Hz"; }
    String msText (float v, int)   { return v >= 1000.f ? String (v / 1000.f, 2) + " s" : String (v, v < 10.f ? 2 : 1) + " ms"; }
    String pctText (float v, int)  { return String (roundToInt (v)) + " %"; }
    String dbText (float v, int)   { return v <= -59.9f ? String ("-inf dB") : String (v, 1) + " dB"; }
    String stText (float v, int)   { return (v > 0 ? "+" : "") + String (v, 1) + " st"; }
    String panText (float v, int)
    {
        const int i = roundToInt (v);
        return i == 0 ? String ("C") : (i < 0 ? String (-i) + " L" : String (i) + " R");
    }

    void addFloat (Layout& l, const String& id, const String& name, NormalisableRange<float> r, float def,
                   std::function<String (float, int)> txt, const String& label = {})
    {
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                                                      AudioParameterFloatAttributes().withLabel (label).withStringFromValueFunction (txt)));
    }
    void addChoice (Layout& l, const String& id, const String& name, const StringArray& c, int def)
    {
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, c, def));
    }
    void addBool (Layout& l, const String& id, const String& name, bool def)
    {
        l.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    }
    void addInt (Layout& l, const String& id, const String& name, int lo, int hi, int def)
    {
        l.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def));
    }

    // Default drum kit for Section A (one drum patch per channel)
    struct MicroDefault
    {
        const char* name;
        int wave; float freq, atk, dcy; int pmMode; float pmAmt, pmRate;
        int nMode; float nFreq, nQ; bool stereo; int nEnv; float nAtk, nDcy;
        float mix, dist, eqF, eqG, lvl, pan; bool choke;
    };
    const MicroDefault microKit[numMicroChannels] = {
        //name     wave freq   atk dcy   pm  amt    rate | nm nFreq  Q    st  env atk  dcy  | mix dist eqF   eqG lvl pan choke
        { "Kick",   0,  48,    0,  420,  0,  36.f, 38.f,   0, 1800, 0.9f, false, 0, 0, 12,    6,  18,  90,   3,  0,  0, false },
        { "Snare",  1,  185,   0,  140,  0,  12.f, 12.f,   1, 2600, 1.1f, true,  0, 0, 210,   62, 8,   6000, 4,  -2, 0, false },
        { "CH",     2,  3200,  0,  20,   2,  12.f, 70.f,   2, 8800, 1.2f, true,  0, 0, 55,    92, 0,   10000,3,  -5, 10, true },
        { "OH",     2,  3200,  0,  60,   2,  12.f, 70.f,   2, 8000, 1.2f, true,  0, 0, 380,   92, 0,   10000,3,  -6, 10, true },
        { "Clap",   1,  900,   0,  10,   0,  0.f,  10.f,   1, 1250, 2.4f, true,  2, 0, 260,   100,10,  3000, 2,  -3, -8, false },
        { "Tom",    0,  110,   0,  320,  0,  14.f, 40.f,   0, 1200, 0.8f, false, 0, 0, 30,    10, 6,   200,  2,  -3, -20, false },
        { "Rim",    1,  880,   0,  35,   0,  -5.f, 4.f,    1, 3000, 1.5f, false, 0, 0, 20,    35, 30,  2500, 5,  -6, 18, false },
        { "Bell",   2,  560,   0,  180,  1,  0.7f, 46.f,   1, 5000, 3.0f, false, 0, 0, 40,    8,  20,  1600, 6,  -8, 25, false },
    };
}

Layout createLayout (int numBankSamples, const StringArray& sampleNames, const int defaultSample[numLimeParts], int defaultHatOpen)
{
    ignoreUnused (sampleNames);
    Layout l;

    // ===================================================================== SECTION A
    const StringArray waves { "Sine", "Triangle", "Saw" };
    const StringArray pmModes { "Decay", "Sine", "Random" };
    const StringArray filterModes { "LP", "BP", "HP" };
    const StringArray envModes { "Exp", "Lin", "Mod" };

    for (int ch = 0; ch < numMicroChannels; ++ch)
    {
        const auto& d = microKit[ch];
        const String p = "A" + String (ch + 1) + " ";
        auto id = [ch] (const char* s) { return a (ch, s); };

        addChoice (l, id (mt::oscWave), p + "Osc Wave", waves, d.wave);
        addFloat  (l, id (mt::oscFreq), p + "Osc Freq", logRange (20.f, 20000.f, 440.f), d.freq, hzText);
        addFloat  (l, id (mt::oscAtk),  p + "Osc Attack", logRange (0.f, 10000.f, 100.f), d.atk, msText);
        addFloat  (l, id (mt::oscDcy),  p + "Osc Decay", logRange (10.f, 10000.f, 500.f), d.dcy, msText);
        addChoice (l, id (mt::pmMode),  p + "Pitch Mod Mode", pmModes, d.pmMode);
        addFloat  (l, id (mt::pmAmt),   p + "Pitch Mod Amount", NormalisableRange<float> (-96.f, 96.f), d.pmAmt, stText);
        addFloat  (l, id (mt::pmRate),  p + "Pitch Mod Rate", NormalisableRange<float> (0.f, 100.f), d.pmRate, pctText);
        addChoice (l, id (mt::nMode),   p + "Noise Filter Mode", filterModes, d.nMode);
        addFloat  (l, id (mt::nFreq),   p + "Noise Filter Freq", logRange (20.f, 20000.f, 1500.f), d.nFreq, hzText);
        addFloat  (l, id (mt::nQ),      p + "Noise Filter Q", logRange (0.1f, 100.f, 2.f), d.nQ,
                   [] (float v, int) { return String (v, 2); });
        addBool   (l, id (mt::nStereo), p + "Noise Stereo", d.stereo);
        addChoice (l, id (mt::nEnv),    p + "Noise Env Mode", envModes, d.nEnv);
        addFloat  (l, id (mt::nAtk),    p + "Noise Attack", logRange (0.f, 10000.f, 100.f), d.nAtk, msText);
        addFloat  (l, id (mt::nDcy),    p + "Noise Decay", logRange (10.f, 10000.f, 500.f), d.nDcy, msText);
        addFloat  (l, id (mt::mix),     p + "Osc/Noise Mix", NormalisableRange<float> (0.f, 100.f), d.mix,
                   [] (float v, int) { return String (roundToInt (100 - v)) + "/" + String (roundToInt (v)); });
        addFloat  (l, id (mt::dist),    p + "Distort", NormalisableRange<float> (0.f, 100.f), d.dist, pctText);
        addFloat  (l, id (mt::eqFreq),  p + "EQ Freq", logRange (20.f, 20000.f, 1000.f), d.eqF, hzText);
        addFloat  (l, id (mt::eqGain),  p + "EQ Gain", NormalisableRange<float> (-40.f, 40.f), d.eqG, dbText);
        addFloat  (l, id (mt::level),   p + "Level", NormalisableRange<float> (-60.f, 10.f), d.lvl, dbText);
        addFloat  (l, id (mt::pan),     p + "Pan", NormalisableRange<float> (-100.f, 100.f), d.pan, panText);
        addBool   (l, id (mt::choke),   p + "Choke", d.choke);
        addBool   (l, id (mt::mute),    p + "Mute", false);
        addFloat  (l, id (mt::velOsc),  p + "Vel > Osc", NormalisableRange<float> (0.f, 200.f), 60.f, pctText);
        addFloat  (l, id (mt::velNoise),p + "Vel > Noise", NormalisableRange<float> (0.f, 200.f), 60.f, pctText);
        addFloat  (l, id (mt::velMod),  p + "Vel > Mod", NormalisableRange<float> (0.f, 200.f), 0.f, pctText);
    }

    StringArray patternNames;
    for (int i = 0; i < numMicroPatterns; ++i) patternNames.add (String::charToString ((juce_wchar) ('A' + i)));
    addChoice (l, aPattern, "A Pattern", patternNames, 0);
    addFloat  (l, aSwing, "A Swing", NormalisableRange<float> (0.f, 100.f), 0.f, pctText);
    addChoice (l, aFillRate, "A Fill Rate", StringArray { "Manual", "2", "3", "4", "5", "6", "7", "8" }, 3);
    addBool   (l, aFill, "A Fill", false);
    addFloat  (l, aMorph, "A Morph", NormalisableRange<float> (0.f, 1.f), 0.f,
               [] (float v, int) { return String (roundToInt (v * 100)) + " %"; });
    addFloat  (l, aLevel, "A Master", NormalisableRange<float> (-60.f, 6.f), 0.f, dbText);

    // ===================================================================== SECTION B
    const int maxSmp = jmax (0, numBankSamples - 1);
    for (int pt = 0; pt < numLimeParts; ++pt)
    {
        const String P = String (limePartName (pt)) + " ";
        auto id = [pt] (const char* s) { return b (pt, s); };

        addInt   (l, id (lm::smp), P + (pt == hat ? "Closed Sample" : "Sample"), 0, maxSmp, defaultSample[pt]);
        if (pt == hat)
            addInt (l, id (lm::smpOpen), P + "Open Sample", 0, maxSmp, defaultHatOpen);
        addFloat (l, id (lm::tune), P + "Tune", NormalisableRange<float> (-24.f, 24.f), 0.f, stText);
        addFloat (l, id (lm::dcy), P + (pt == hat ? "Closed Decay" : "Decay"), NormalisableRange<float> (0.f, 100.f),
                  pt == kick ? 70.f : (pt == hat ? 40.f : 60.f), pctText);
        if (pt == hat)
        {
            addFloat (l, id (lm::dcyOpen), P + "Open Decay", NormalisableRange<float> (0.f, 100.f), 70.f, pctText);
            addBool  (l, id (lm::link), P + "Link", true);
        }
        if (pt == snare)
            addChoice (l, id (lm::fmode), P + "Filter Mode", StringArray { "LP", "BP", "HP" }, 0);

        const float cutDef = pt == hat ? 20.f : 20000.f;
        if (pt == hat)
            addFloat (l, id (lm::cut), P + "Color (HP)", logRange (20.f, 16000.f, 1000.f), cutDef, hzText);
        else
            addFloat (l, id (lm::cut), P + "Cutoff", logRange (20.f, 20000.f, 1500.f), cutDef, hzText);
        addFloat (l, id (lm::res), P + "Reso", NormalisableRange<float> (0.f, 100.f), 0.f, pctText);
        if (pt == kick)
            addBool (l, id (lm::lowcut), P + "Low Cut", false);
        if (pt == tie)
        {
            addFloat (l, id (lm::fmFreq), P + "FM Freq", logRange (0.5f, 5000.f, 200.f), 120.f, hzText);
            addFloat (l, id (lm::fmAmt), P + "FM Amount", NormalisableRange<float> (0.f, 100.f), 0.f, pctText);
        }
        if (pt == pod)
        {
            addFloat (l, id (lm::zap), P + "Zap", NormalisableRange<float> (-100.f, 100.f), 0.f, pctText);
            addFloat (l, id (lm::zapDcy), P + "Zap Decay", NormalisableRange<float> (0.f, 100.f), 30.f, pctText);
        }
        addFloat (l, id (lm::lvl), P + "Level", NormalisableRange<float> (-60.f, 6.f), pt == hat ? -4.f : (pt >= tie ? -6.f : 0.f), dbText);
        addFloat (l, id (lm::pan), P + "Pan", NormalisableRange<float> (-100.f, 100.f), pt == tie ? -25.f : (pt == pod ? 25.f : 0.f), panText);
        addFloat (l, id (lm::send), P + "Delay Send", NormalisableRange<float> (0.f, 100.f), pt == pod ? 30.f : 0.f, pctText);
        addBool  (l, id (lm::mute), P + "Mute", false);
    }

    StringArray limePatterns;
    for (int i = 0; i < numLimePatterns; ++i) limePatterns.add (String (i + 1));
    addChoice (l, bPattern, "B Pattern", limePatterns, 0);
    addInt    (l, bLen,    "B Length", 1, numSteps, 16);
    addInt    (l, bTieLen, "B Tie Length", 1, numSteps, 16);
    addInt    (l, bPodLen, "B Pod Length", 1, numSteps, 16);
    addInt    (l, bModLen, "B Mod Length", 1, numSteps, 16);
    addBool   (l, bTieStr, "B Tie Stretch", false);
    addBool   (l, bPodStr, "B Pod Stretch", false);
    addBool   (l, bModStr, "B Mod Stretch", false);
    addFloat  (l, bShuffle, "B Shuffle", NormalisableRange<float> (0.f, 100.f), 0.f, pctText);
    addFloat  (l, bAccent, "B Accent", NormalisableRange<float> (0.f, 100.f), 50.f, pctText);

    addFloat  (l, bGain,  "B Gain", NormalisableRange<float> (-60.f, 6.f), 0.f, dbText);
    addFloat  (l, bPitch, "B Pitch", NormalisableRange<float> (-12.f, 12.f), 0.f, stText);
    addFloat  (l, bDirt,  "B Dirt", NormalisableRange<float> (0.f, 100.f), 0.f, pctText);
    addBool   (l, bNorm,  "B Norm", false);

    addFloat  (l, bLfoRate, "B LFO Rate", logRange (0.02f, 20.f, 1.f), 0.5f, hzText);
    addFloat  (l, bLfoFM, "B LFO FM", NormalisableRange<float> (0.f, 100.f), 0.f, pctText);
    addFloat  (l, bLfoDM, "B LFO DM", NormalisableRange<float> (0.f, 100.f), 0.f, pctText);

    addChoice (l, bDlyTime, "B Delay Time", delayTimeNames(), 7);
    addFloat  (l, bDlyFb,   "B Delay Feedback", NormalisableRange<float> (0.f, 95.f), 40.f, pctText);
    addFloat  (l, bDlyDamp, "B Delay Damp", NormalisableRange<float> (0.f, 100.f), 40.f, pctText);
    addFloat  (l, bDlyLvl,  "B Delay Level", NormalisableRange<float> (-60.f, 6.f), -6.f, dbText);

    addFloat  (l, bFltCut, "B Filter Cutoff", logRange (40.f, 20000.f, 1500.f), 20000.f, hzText);
    addFloat  (l, bFltRes, "B Filter Reso", NormalisableRange<float> (0.f, 100.f), 0.f, pctText);

    addChoice (l, bModDest, "B Mod Destination", modDestNames(), 0);
    addFloat  (l, bModAmt,  "B Mod Amount", NormalisableRange<float> (-100.f, 100.f), 50.f, pctText);

    // ===================================================================== GLOBAL
    addFloat  (l, gXfade, "A/B Mix", NormalisableRange<float> (0.f, 100.f), 50.f,
               [] (float v, int) { return "A " + String (roundToInt (100 - v)) + " / B " + String (roundToInt (v)); });
    addFloat  (l, gCutoff, "Cutoff", logRange (20.f, 20000.f, 1000.f), 20000.f, hzText);
    addChoice (l, gMode, "Filter Mode", StringArray { "High Pass", "Band Pass", "Low Pass" }, 2);
    addFloat  (l, gRes, "Resonance", NormalisableRange<float> (0.f, 100.f), 0.f, pctText);
    addFloat  (l, gMaster, "Master", NormalisableRange<float> (-60.f, 6.f), 0.f, dbText);
    addFloat  (l, gTempo, "Tempo", NormalisableRange<float> (40.f, 240.f, 0.1f), 124.f,
               [] (float v, int) { return String (v, 1) + " BPM"; });
    addBool   (l, gRun, "Run (internal clock)", false);

    return l;
}
} // namespace params
