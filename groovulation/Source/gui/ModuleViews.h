#pragma once
#include "LookAndFeel.h"
#include "../PluginProcessor.h"

// -------------------------------------------------------------------------------------------------
//  LCD loop display: waveform over the loop region (LENGTH = grid steps x GRID, the loop repeats or is
//  cut to fit -> polyrhythms), transparent grid, slice markers, clip-gain (VOL.A) breakpoints, playhead.
// -------------------------------------------------------------------------------------------------
class WaveDisplay : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    WaveDisplay (GroovulationProcessor& p, int module, bool interactive);
    void setModule (int m) { module = m; repaint(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    std::function<void()> onClick;   // non-interactive displays: select the module
private:
    void timerCallback() override;
    float param (const char* name) const;
    void setParam (const char* name, float v);
    double regionBeats() const;
    juce::Rectangle<float> plot() const;
    float xToPos (float x) const;
    float posToX (float p) const;
    float yToVol (float y) const;
    float volToY (float v) const;
    int hitVolPoint (juce::Point<float> p) const;
    enum Drag { none, sliceStart, sliceEnd, volPoint };

    GroovulationProcessor& proc;
    int module;
    bool interactive;
    Drag drag = none;
    int dragIndex = -1;
    VolEnvelope env;
    float lastPos = -1.0f;
};

// -------------------------------------------------------------------------------------------------
//  Compact module strip (top row): name, LCD, loop name, LOOP / SIMILAR buttons, GAIN, PITCH, ON, SOLO
// -------------------------------------------------------------------------------------------------
class ModuleStrip : public juce::Component, private juce::Timer
{
public:
    ModuleStrip (GroovulationProcessor& p, int module, std::function<void (int)> onSelect);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void setSelected (bool s) { selected = s; repaint(); }
private:
    void timerCallback() override;
    GroovulationProcessor& proc;
    int module;
    std::function<void (int)> select;
    bool selected = false;
    WaveDisplay display;
    juce::TextButton prev { "<" }, next { ">" }, rnd { "RND" }, sim { "SIMILAR" };
    Knob gain { "Gain" }, pitch { "Pitch" };
    Toggle on { "ON" }, solo { "S" };
    LoopPtr shownLoop;
    float level = 0.0f;
};

// -------------------------------------------------------------------------------------------------
//  Full editor of the selected module (A .. K)
// -------------------------------------------------------------------------------------------------
class ModuleEditor : public juce::Component, private juce::Timer
{
public:
    explicit ModuleEditor (GroovulationProcessor& p);
    void setModule (int m);
    int getModule() const { return module; }
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void refreshBrowser();
    void attachAll();

    GroovulationProcessor& proc;
    int module = 0;
    WaveDisplay display;
    // toolbar
    Segmented grid, keySrc, ftype;
    juce::TextButton lenMinus { "-" }, lenPlus { "+" }, lenOrig { "ORIG" }, volReset { "RESET VOL" };
    Toggle slice { "SLICE" }, fade { "FADE 0.5ms" }, volA { "VOL.A" }, dpp { "PING-PONG" };
    juce::TextButton prev { "<" }, next { ">" }, rnd { "RND" }, sim { "SIMILAR" };
    juce::ComboBox browser;
    std::vector<int> browserIndices;
    // panels
    Panel pAmp { "Gain / Shaper", theme::red }, pAdsr { "ADSR", theme::red }, pPitch { "Pitch / Tempo", theme::red },
          pFilter { "Cutoff", theme::red }, pDirt { "Dirt", theme::red }, pDelay { "Delay", theme::red },
          pReverb { "Reverb", theme::red }, pLfo1 { "LFO 1", theme::red }, pLfo2 { "LFO 2", theme::red };
    Knob gain { "Gain" }, pan { "Pan" }, shaper { "Shaper" }, atk { "A" }, dec { "D" }, sus { "S" }, rel { "R" },
         pitch { "Pitch" }, tempo { "Tempo" }, cut { "Cutoff" }, res { "Res" }, dirt { "Dirt" },
         dtime { "Time" }, dfb { "Feedback" }, dmix { "Mix" }, rsize { "Size" }, rdecay { "Decay" }, rdamp { "Damp" },
         rpre { "Predelay" }, rmix { "Mix" }, l1rate { "Rate" }, l1depth { "Depth" }, l2rate { "Rate" }, l2depth { "Depth" };
    juce::ComboBox l1shape, l1target, l2shape, l2target;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtt;
    LoopPtr shownLoop;
    juce::String infoLine;
    int lastDbGen = -1;
};
