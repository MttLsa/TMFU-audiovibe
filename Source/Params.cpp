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

Layout createLayout()
{
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

    // ===================================================================== SECTION B (Limelite 1.3)
    auto pctR = NormalisableRange<float> (0.f, 100.f);
    auto bip  = NormalisableRange<float> (-100.f, 100.f);
    for (int v = 0; v < numLimeVoices; ++v)
    {
        const String P = String (limeVoiceName (v)) + " ";
        auto id = [v] (const char* k) { return b (v, k); };
        const bool sampler = v != pulp;
        addBool  (l, id (lv::on),   P + "On", true);
        addFloat (l, id (lv::gain), P + "Gain", NormalisableRange<float> (-60.f, 6.f), v == hat ? -4.f : (v == pulp ? -60.f : (v >= tie ? -5.f : 0.f)), dbText);
        addFloat (l, id (lv::pan),  P + "Pan", bip, v == tie ? -20.f : (v == pod ? 25.f : (v == pulp ? 10.f : 0.f)), panText);
        if (v <= hat) addFloat (l, id (lv::acc), P + "Accent", pctR, 50.f, pctText);
        if (v == tie || v == pod) addFloat (l, id (lv::vamt), P + "V>A", pctR, 70.f, pctText);
        if (sampler)  addInt   (l, id (lv::wav), P + "Wave", 0, 255, 0);
        addFloat (l, id (lv::tune), P + "Tune", NormalisableRange<float> (-24.f, 24.f), 0.f, stText);
        if (v == tie || v == pulp) addFloat (l, id (lv::port), P + "Portamento", pctR, 0.f, pctText);
        if (sampler && v != hat) addFloat (l, id (lv::st), P + "Start", pctR, 0.f, pctText);
        if (v == kick || v == snare)
        {
            addFloat (l, id (lv::hold), P + "Hold", pctR, 0.f, pctText);
            addFloat (l, id (lv::dcy),  P + "Decay", pctR, v == kick ? 65.f : 55.f, pctText);
            addFloat (l, id (lv::sat),  P + "Saturation", pctR, 0.f, pctText);
            addFloat (l, id (lv::shp),  P + "Shape", bip, 0.f, [] (float x, int) { return x < -33 ? String ("thin ") + String (roundToInt (x)) : (x > 33 ? String ("fat +") + String (roundToInt (x)) : String ("normal ") + String (roundToInt (x))); });
            addFloat (l, id (lv::sm),   P + "Shape Smooth", pctR, 0.f, pctText);
        }
        if (v == tie || v == pod)
        {
            addFloat (l, id (lv::att), P + "Attack", pctR, 0.f, pctText);
            addFloat (l, id (lv::dcy), P + "Decay", pctR, 60.f, pctText);
        }
        if (v == hat)
        {
            addFloat (l, id (lv::cd), P + "Closed Decay", pctR, 35.f, pctText);
            addFloat (l, id (lv::od), P + "Open Decay", pctR, 70.f, pctText);
            addBool  (l, id (lv::link), P + "Link", true);
        }
        if (v == tie)
        {
            addFloat (l, id (lv::fm), P + "FM", pctR, 0.f, pctText);
            addFloat (l, id (lv::mp), P + "Mod Pitch", NormalisableRange<float> (-24.f, 48.f), 12.f, stText);
            addFloat (l, id (lv::noise), P + "Noise", pctR, 0.f, pctText);
            addFloat (l, id (lv::sat), P + "Saturation", pctR, 0.f, pctText);
        }
        if (v == pod) addFloat (l, id (lv::zap), P + "Zap", bip, 0.f, pctText);
        if (v == tie || v == pod) addBool (l, id (lv::scale), P + "Scale", false);
        if (v == tie || v == pod) addBool (l, id (lv::nameKey), P + "Key From File Name", false);
        if (v == kick || v == hat) addChoice (l, id (lv::mod), P + "Mod Dest", StringArray { "--", "wav", "tune", "cut" }, 0);
        if (v == snare) addChoice (l, id (lv::mod), P + "Mod Dest", StringArray { "--", "wav", "tune", "cut", "flam" }, 0);
        if (v == tie) addChoice (l, id (lv::vel), P + "Vel Dest", StringArray { "--", "wav", "tune", "att", "dcy", "st", "fm", "cut" }, 0);
        if (v == pod) addChoice (l, id (lv::vel), P + "Vel Dest", StringArray { "--", "wav", "tune", "att", "dcy", "zap", "cut" }, 0);
        if (v == kick) addBool (l, id (lv::lo), P + "Low Cut", false);
        if (v == snare || v == tie || v == pod) addChoice (l, id (lv::ft), P + "Filter Type", StringArray { "LP", "BP", "HP" }, 0);
        if (v == snare) addFloat (l, id (lv::flam), P + "Flam", pctR, 0.f, pctText);
        if (sampler)
        {
            if (v == hat) addFloat (l, id (lv::cut), P + "Cutoff (HP)", logRange (20.f, 16000.f, 1000.f), 20.f, hzText);
            else          addFloat (l, id (lv::cut), P + "Cutoff", logRange (20.f, 20000.f, 1500.f), 20000.f, hzText);
            addFloat (l, id (lv::res), P + "Resonance", pctR, 0.f, pctText);
        }
        if (v == pulp)
        {
            addFloat (l, id (lv::trkLo),  P + "Track Low", logRange (20.f, 2000.f, 150.f), 40.f, hzText);
            addFloat (l, id (lv::trkHi),  P + "Track High", logRange (50.f, 5000.f, 600.f), 800.f, hzText);
            addChoice (l, id (lv::src), P + "Input Source", StringArray { "Kick", "Snare", "Hat", "Tie", "Pod", "All" }, 3);
            addFloat  (l, id (lv::inGain), P + "Input Gain", NormalisableRange<float> (-24.f, 24.f), 0.f, dbText);
            addFloat (l, id (lv::dcy),    P + "Decay", pctR, 40.f, pctText);
            addFloat (l, id (lv::osc),    P + "Osc (Sine-Input-Noise)", pctR, 0.f,
                      [] (float x, int) { return x < 25 ? String ("Sine") : (x < 75 ? String ("Input") : String ("Noise")); });
            addFloat (l, id (lv::comb), P + "Comb Mix", pctR, 0.f, pctText);
            addFloat (l, id (lv::fb),   P + "Comb Feedback", pctR, 50.f, pctText);
            addFloat (l, id (lv::freq), P + "Comb Freq", logRange (30.f, 4000.f, 300.f), 220.f, hzText);
            addFloat (l, id (lv::fm),   P + "Comb FM", pctR, 0.f, pctText);
            addFloat (l, id (lv::cut),  P + "Comb Cutoff", logRange (100.f, 20000.f, 3000.f), 8000.f, hzText);
        }
    }

    const StringArray abcd { "A", "B", "C", "D" };
    addChoice (l, bPattern, "B Pattern", abcd, 0);
    addInt    (l, bLen,    "B Length", 1, numLimeSteps, 16);
    addInt    (l, bStart,  "B Start Step", 1, numLimeSteps, 1);
    addInt    (l, bTieLen, "B Tie Length", 1, numLimeSteps, 16);
    addInt    (l, bPodLen, "B Pod Length", 1, numLimeSteps, 16);
    addInt    (l, bModLen, "B Mod Length", 1, numLimeSteps, 16);
    addBool   (l, bTieStr, "B Tie Stretch", false);
    addBool   (l, bPodStr, "B Pod Stretch", false);
    addBool   (l, bModStr, "B Mod Stretch", false);
    addFloat  (l, bSwing,  "B Swing", pctR, 0.f, pctText);
    addBool   (l, bHalf,   "B Half Tempo", false);
    addBool   (l, bChainOn, "B Pattern Seq On", false);
    addInt    (l, bChainLen, "B Pattern Seq Length", 1, 8, 4);
    for (int i = 0; i < 8; ++i) addChoice (l, bChainSlot (i), "B Pattern Seq " + String (i + 1), abcd, i % 4);
    addBool   (l, bLock,   "B Lock Display", false);
    addInt    (l, bRemix,  "B Remix Pattern", 0, 127, 0);
    addChoice (l, bRemixRate, "B Remix Rate", StringArray { "OFF", "2", "4", "8", "12", "16" }, 0);
    addInt    (l, bShift,  "B Remix Shift", -16, 16, 0);
    addChoice (l, bScaleRoot, "B Scale Root", StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0);
    {
        StringArray names; for (auto& sc : scales()) names.add (sc.name);
        addChoice (l, bScaleType, "B Scale", names, 1);
    }

    addFloat  (l, bDirt, "B Dirt", pctR, 0.f, pctText);
    addFloat  (l, bGain, "B Gain (Master)", NormalisableRange<float> (-60.f, 6.f), 0.f, dbText);
    addFloat  (l, bTune, "B Tune", NormalisableRange<float> (-12.f, 12.f), 0.f, stText);
    addBool   (l, bNorm, "B Norm", false);
    addFloat  (l, bNormAmp, "B Norm Amp", pctR, 50.f, pctText);
    addFloat  (l, bNormRel, "B Norm Release", pctR, 40.f, pctText);
    addFloat  (l, bNormSm,  "B Norm Smooth", pctR, 30.f, pctText);

    addFloat  (l, bFltCut, "B Filter Cutoff", logRange (40.f, 20000.f, 1500.f), 20000.f, hzText);
    addFloat  (l, bFltTrk, "B Filter Track", bip, 0.f, pctText);
    addFloat  (l, bFltSm,  "B Filter Smooth", pctR, 40.f, pctText);
    addFloat  (l, bFltMod, "B Filter Mod", bip, 0.f, pctText);

    addFloat  (l, bEqHi,  "B EQ High", NormalisableRange<float> (-18.f, 18.f), 0.f, dbText);
    addFloat  (l, bEqMid, "B EQ Mid", NormalisableRange<float> (-18.f, 18.f), 0.f, dbText);
    addFloat  (l, bEqLo,  "B EQ Low", NormalisableRange<float> (-18.f, 18.f), 0.f, dbText);
    addFloat  (l, bEqAmp, "B EQ Amp (saturator)", NormalisableRange<float> (0.f, 24.f), 0.f, dbText);
    addFloat  (l, bEqSplit, "B EQ Split", logRange (60.f, 1000.f, 250.f), 250.f, hzText);

    addFloat  (l, bMagPitch, "B Magic Pitch", NormalisableRange<float> (-12.f, 12.f), 0.f, stText);
    addFloat  (l, bMagGrain, "B Magic Grain", pctR, 40.f, pctText);
    addFloat  (l, bMagMod,   "B Magic Mod", bip, 0.f, pctText);
    addFloat  (l, bMagRmx,   "B Magic Remix", bip, 0.f, pctText);
    addFloat  (l, bMagMix,   "B Magic Mix", pctR, 0.f, pctText);
    addFloat  (l, bMagNotch, "B Magic Notch", logRange (100.f, 10000.f, 1000.f), 1000.f, hzText);

    addInt    (l, bDlyOne, "B Delay One (16ths)", 1, 16, 3);
    addInt    (l, bDlyTwo, "B Delay Two (16ths)", 1, 16, 4);
    addFloat  (l, bDlyFb,  "B Delay Feedback", NormalisableRange<float> (0.f, 98.f), 40.f, pctText);
    addFloat  (l, bDlyGain,"B Delay Gain", NormalisableRange<float> (-60.f, 0.f), -60.f, dbText);
    addFloat  (l, bDlyLfo, "B Delay LFO", logRange (0.02f, 10.f, 0.5f), 0.3f, hzText);
    addFloat  (l, bDlyFM,  "B Delay FM", pctR, 0.f, pctText);
    addFloat  (l, bDlyDM,  "B Delay DM", pctR, 0.f, pctText);
    addFloat  (l, bDlyWide,"B Delay Wide", pctR, 50.f, pctText);
    addFloat  (l, bDlyCut, "B Delay Cutoff", logRange (200.f, 20000.f, 3000.f), 6000.f, hzText);
    addFloat  (l, bDlyRes, "B Delay Res", pctR, 10.f, pctText);
    addBool   (l, bDlyLo,  "B Delay Low Cut", true);

    addChoice (l, bXMode, "B xFx Mode", xfxModeNames(), 0);
    addFloat  (l, bXParam, "B xFx Parameter X", pctR, 50.f, pctText);
    addFloat  (l, bXAdd,   "B xFx Additional", pctR, 50.f, pctText);
    addFloat  (l, bXDly,   "B xFx Delay Input", pctR, 0.f, [] (float x, int) { return x < 1 ? String ("dry") : (x > 99 ? String ("post-delay") : String (roundToInt (x)) + " %"); });
    addFloat  (l, bXGain,  "B xFx Gain", NormalisableRange<float> (-60.f, 0.f), -60.f, dbText);
    addFloat  (l, bXLo,    "B xFx Low Cut", logRange (20.f, 2000.f, 150.f), 20.f, hzText);
    addFloat  (l, bXHi,    "B xFx High Cut", logRange (500.f, 20000.f, 5000.f), 20000.f, hzText);
    addFloat  (l, bXSpd,   "B xFx Speed (XY playback/record)", pctR, 50.f,
               [] (float x, int) { return String (std::pow (2.0f, (x - 50.0f) / 25.0f), 2) + "x"; });
    addFloat  (l, bXLen,   "B xFx Length (XY loop)", NormalisableRange<float> (1.f, 64.f, 1.f), 16.f,
               [] (float x, int) { const int v = roundToInt (x); return v % 16 == 0 ? String (v / 16) + (v == 16 ? " bar" : " bars") : String (v) + "/16"; });
    addBool   (l, bXPlay,  "B xFx Play (XY motion)", false);
    addBool   (l, bXRec,   "B xFx Rec (XY motion)", false);
    addFloat  (l, bXSm,    "B xFx Smooth (XY interpolation)", pctR, 20.f, pctText);

    addFloat  (l, bFxMix,  "B FX Mix", pctR, 50.f, [] (float x, int) { return x < 1 ? String ("dry") : (x > 99 ? String ("wet") : String (roundToInt (x)) + " %"); });
    addBool   (l, bModInput, "B Mod > FX Input", false);
    addBool   (l, bModMix,   "B Mod > FX Mix", false);

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
