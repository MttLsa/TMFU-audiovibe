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
    bool keyPressed (const juce::KeyPress&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float scaleFactor) override;
    void resized() override;
    void setView (double start, double len);   // visible part of the loop region (fractions), interactive display only
    std::function<void()> onClick;   // non-interactive displays: select the module
private:
    void timerCallback() override;
    float param (const char* name) const;
    void setParam (const char* name, float v);
    double regionBeats() const;
    double regionMs() const;                 // length of the loop region in milliseconds at the current tempo
    void setSliceMs (bool start, double ms);
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
    // zoom / high resolution waveform
    double viewStart = 0.0, viewLen = 1.0;
    void zoomAround (double factor, float x);
    double minViewLen() const;
    void buildPeaks (const LoopPtr& l);
    void renderWave (const LoopData& l, double R, int w, int h);
    LoopPtr peaksLoop;
    std::vector<float> pkMin, pkMax, mono;     // 64-sample min/max blocks + mono mix of the loop
    float pkNorm = 1.0f;
    static constexpr int pkBlock = 64;
    juce::Image waveImg;
    struct Key { const void* loop = nullptr; double vs = -1, vl = -1, R = -1; int w = 0, h = 0; } waveKey;
    juce::TextButton zIn { "+" }, zOut { "-" }, zFit { "FIT" };
    bool draggingScroll = false;
    juce::Rectangle<float> scrollBar() const;
    int lastMarker = 0;                      // 0 = S, 1 = E (arrow keys nudge it)
    float dragStartPos = 0.0f, dragStartX = 0.0f;
    float lastPos = -1.0f;
};

// -------------------------------------------------------------------------------------------------
//  Compact module strip (top row): name, LCD, loop name, LOOP / SIMILAR buttons, GAIN, PITCH, ON, SOLO
// -------------------------------------------------------------------------------------------------
class ModuleStrip : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }
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
    juce::TextButton prev { "<" }, next { ">" }, rnd { "RND" }, sim { "SIM" }, clear { "CLEAR" };
    FloppyButton floppy;
    std::unique_ptr<juce::FileChooser> chooser;
    bool dragOver = false;
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
    Segmented grid, keySrc, ftype, warp, slsnap;
    juce::TextButton lenMinus { "-" }, lenPlus { "+" }, lenOrig { "ORIG" }, volReset { "RESET VOL" };
    Toggle slice { "SLICE" }, fade { "FADE 0.5ms" }, volA { "VOL.A" }, dpp { "PING-PONG" };
    juce::TextButton prev { "<" }, next { ">" }, rnd { "RND" }, sim { "SIMILAR" }, clear { "CLEAR" };
    FloppyButton floppy;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::ComboBox browser;
    std::vector<int> browserIndices;
    // panels
    Panel pAmp { "Gain / Pan", theme::red }, pAdsr { "ADSR", theme::red }, pPitch { "Pitch / Tempo / Warp", theme::red },
          pFilter { "Cutoff", theme::red }, pDirt { "Dirt", theme::red }, pDelay { "Delay", theme::red },
          pReverb { "Reverb", theme::red }, pLfo1 { "LFO 1", theme::red }, pLfo2 { "LFO 2", theme::red }, pLfo3 { "LFO 3", theme::red };
    Knob gain { "Gain" }, pan { "Pan" }, atk { "A" }, dec { "D" }, sus { "S" }, rel { "R" },
         pitch { "Pitch" }, tempo { "Tempo" }, cut { "Cutoff" }, res { "Res" }, dirt { "Dirt" },
         dtime { "Time" }, dfb { "Feedback" }, dmix { "Mix" }, rsize { "Size" }, rdecay { "Decay" }, rdamp { "Damp" },
         rpre { "Predelay" }, rmix { "Mix" }, l1rate { "Rate" }, l1depth { "Depth" }, l2rate { "Rate" }, l2depth { "Depth" },
         l3rate { "Rate" }, l3depth { "Depth" };
    juce::ComboBox l1shape, l1target, l2shape, l2target, l3shape, l3target;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtt;
    LoopPtr shownLoop;
    juce::String infoLine;
    int lastDbGen = -1;
};
