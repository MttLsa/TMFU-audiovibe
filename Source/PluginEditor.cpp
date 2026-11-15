#include "PluginEditor.h"
#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "2.6.0"
#endif

using namespace juce;

MainView::MainView (TwinboxProcessor& p) : global (p), proc (p), sectionA (p), sectionB (p)
{
    sectionA.onOpenBrowser = [this] { openBrowser (0); };
    sectionB.onOpenBrowser = [this] { openBrowser (1); };
    addAndMakeVisible (sectionA);
    addAndMakeVisible (sectionB);
    addAndMakeVisible (global);
}

int MainView::designWidth() const
{
    return margin + sectionWidth + margin + (global.isExpanded() ? GlobalPanel::expandedWidth : GlobalPanel::collapsedWidth) + margin;
}

void MainView::openBrowser (int section)
{
    auto& w = browsers[(size_t) section];
    if (w != nullptr) { w->toFront (true); return; }
    w = std::make_unique<ExpandWindow> (section == 0 ? "Section A - Preset Browser" : "Section B - Preset Browser",
                                        new PresetBrowser (proc, section), 820, 700, [this, section] { browsers[(size_t) section].reset(); });
}

void MainView::paint (Graphics& g)
{
    g.fillAll (theme::bg);
    // ---- title bar with the TWINBOX wordmark
    auto bar = Rectangle<float> ((float) margin, 6.0f, (float) sectionWidth, (float) titleH - 12.0f);
    GlyphArrangement ga;
    const auto f = theme::font (26.0f, true).withExtraKerningFactor (0.18f);
    ga.addLineOfText (f, "TWIN", bar.getX() + 4.0f, bar.getCentreY() + 9.0f);
    const float twinW = ga.getBoundingBox (0, -1, true).getWidth();
    g.setColour (theme::text);
    ga.draw (g);
    GlyphArrangement gb;
    gb.addLineOfText (f, "BOX", bar.getX() + 4.0f + twinW + 4.0f, bar.getCentreY() + 9.0f);
    const float boxW = gb.getBoundingBox (0, -1, true).getWidth();
    g.setColour (theme::text.withAlpha (0.55f));
    gb.draw (g);
    // two small accent bars = the two sections (A orange / B lime)
    const float x = bar.getX() + 4.0f + twinW + 4.0f + boxW + 14.0f;
    g.setColour (theme::accentA); g.fillRoundedRectangle (x, bar.getCentreY() - 8.0f, 6.0f, 16.0f, 1.5f);
    g.setColour (theme::accentB); g.fillRoundedRectangle (x + 9.0f, bar.getCentreY() - 8.0f, 6.0f, 16.0f, 1.5f);
    g.setColour (theme::textDim);
    g.setFont (theme::font (12.0f));
    g.drawText ("dual groovebox  /  synth drums + sample groovebox", (int) (x + 28.0f), (int) bar.getY(), 400, (int) bar.getHeight(), Justification::centredLeft);
    g.setColour (theme::textDim.withAlpha (0.6f));
    g.drawText (String ("v") + JucePlugin_VersionString, bar.toNearestInt().withTrimmedRight (6), Justification::centredRight);
}

void MainView::resized()
{
    sectionA.setBounds (margin, titleH, sectionWidth, heightA);
    sectionB.setBounds (margin, titleH + heightA + margin, sectionWidth, heightB);
    const int gx = margin + sectionWidth + margin;
    global.setBounds (gx, margin, getWidth() - gx - margin, designHeight - 2 * margin);
}

//==============================================================================
TwinboxEditor::TwinboxEditor (TwinboxProcessor& p) : AudioProcessorEditor (&p), proc (p), view (p)
{
    setLookAndFeel (&lnf);
    LookAndFeel::setDefaultLookAndFeel (&lnf);
    addAndMakeVisible (view);
    scale = (float) proc.uiState.getProperty ("scale", 0.8f);
    view.global.onToggle = [this] { updateSizeForGlobal(); };
    setResizable (true, true);
    updateSizeForGlobal();
}

TwinboxEditor::~TwinboxEditor()
{
    LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void TwinboxEditor::updateSizeForGlobal()
{
    const int w = view.designWidth(), h = MainView::designHeight;
    view.setSize (w, h);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) w / (double) h);
        c->setSizeLimits (roundToInt (w * 0.45f), roundToInt (h * 0.45f), roundToInt (w * 1.6f), roundToInt (h * 1.6f));
    }
    adjusting = true;
    setSize (roundToInt ((float) w * scale), roundToInt ((float) h * scale));
    adjusting = false;
    resized();
}

void TwinboxEditor::paint (Graphics& g) { g.fillAll (theme::bg); }

void TwinboxEditor::resized()
{
    const int w = view.designWidth();
    if (! adjusting && w > 0) scale = jlimit (0.4f, 1.8f, (float) getWidth() / (float) w);
    proc.uiState.setProperty ("scale", scale, nullptr);
    view.setTransform (AffineTransform::scale (scale));
    view.setBounds (0, 0, w, MainView::designHeight);
}
