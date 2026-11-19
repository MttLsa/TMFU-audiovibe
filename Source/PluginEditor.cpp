#include "PluginEditor.h"
#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "2.14.1"
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
    return margin + preferredSectionWidth + margin + (global.isExpanded() ? GlobalPanel::expandedWidth : GlobalPanel::collapsedWidth) + margin;
}

int MainView::minimumWidth() const
{
    return margin + minSectionWidth + margin + (global.isExpanded() ? GlobalPanel::expandedWidth : GlobalPanel::collapsedWidth) + margin;
}

int MainView::sectionWidth() const
{
    return jmax (minSectionWidth, getWidth() - 3 * margin - (global.isExpanded() ? GlobalPanel::expandedWidth : GlobalPanel::collapsedWidth));
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
    auto bar = Rectangle<float> ((float) margin, 6.0f, (float) sectionWidth(), (float) titleH - 12.0f);
    GlyphArrangement ga;
    const auto f = theme::font (26.0f, true).withExtraKerningFactor (0.18f);
    const float logoBase = bar.getCentreY() + 4.0f;              // wordmark raised a little to make room for the maker line
    ga.addLineOfText (f, "TWIN", bar.getX() + 4.0f, logoBase);
    const float twinW = ga.getBoundingBox (0, -1, true).getWidth();
    g.setColour (theme::text);
    ga.draw (g);
    GlyphArrangement gb;
    gb.addLineOfText (f, "BOX", bar.getX() + 4.0f + twinW + 4.0f, logoBase);
    const float boxW = gb.getBoundingBox (0, -1, true).getWidth();
    g.setColour (theme::text.withAlpha (0.55f));
    gb.draw (g);
    // maker line under the wordmark
    GlyphArrangement gm;
    gm.addLineOfText (theme::font (9.5f).withExtraKerningFactor (0.12f), "TMFU audiovibe", bar.getX() + 5.0f, logoBase + 12.0f);
    g.setColour (theme::textDim);
    gm.draw (g);
    // two small accent bars = the two sections (A orange / B lime)
    const float x = bar.getX() + 4.0f + twinW + 4.0f + boxW + 14.0f;
    const float midY = logoBase - 9.0f;                          // optical middle of the wordmark
    g.setColour (theme::accentA); g.fillRoundedRectangle (x, midY - 8.0f, 6.0f, 16.0f, 1.5f);
    g.setColour (theme::accentB); g.fillRoundedRectangle (x + 9.0f, midY - 8.0f, 6.0f, 16.0f, 1.5f);
    g.setColour (theme::textDim);
    g.setFont (theme::font (12.0f));
    g.drawText ("dual groovebox  /  synth drums + sample groovebox", (int) (x + 28.0f), (int) (midY - 10.0f), 400, 20, Justification::centredLeft);
    g.setColour (theme::textDim.withAlpha (0.6f));
    g.drawText (String ("v") + JucePlugin_VersionString, bar.toNearestInt().withTrimmedRight (6), Justification::centredRight);
}

void MainView::resized()
{
    const int sw = sectionWidth();
    sectionA.setBounds (margin, titleH, sw, heightA);
    sectionB.setBounds (margin, titleH + heightA + margin, sw, heightB);
    const int gx = margin + sw + margin;
    global.setBounds (gx, margin, getWidth() - gx - margin, designHeight - 2 * margin);
}

//==============================================================================
TwinboxEditor::TwinboxEditor (TwinboxProcessor& p) : AudioProcessorEditor (&p), proc (p), view (p)
{
    setLookAndFeel (&lnf);
    LookAndFeel::setDefaultLookAndFeel (&lnf);
    addAndMakeVisible (view);
    scale = (float) proc.uiState.getProperty ("scale", 0.8f);
    view.global.onToggle = [this] { resized(); };
    setResizable (true, true);
    // free resizing: the height sets the scale, extra width makes the sections wider (fills any screen when maximised)
    if (auto* c = getConstrainer())
        c->setSizeLimits (roundToInt (view.minimumWidth() * 0.4f), roundToInt (MainView::designHeight * 0.4f), 10000, 6000);
    const int w = (int) proc.uiState.getProperty ("editorW", roundToInt (view.designWidth() * scale));
    const int h = (int) proc.uiState.getProperty ("editorH", roundToInt (MainView::designHeight * scale));
    setSize (w, h);
    resized();
}

TwinboxEditor::~TwinboxEditor()
{
    LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void TwinboxEditor::updateSizeForGlobal() { resized(); }

void TwinboxEditor::paint (Graphics& g) { g.fillAll (theme::bg); }

void TwinboxEditor::resized()
{
    if (getWidth() <= 0 || getHeight() <= 0) return;
    const int minW = view.minimumWidth(), H = MainView::designHeight;
    scale = (float) getHeight() / (float) H;                       // the height decides the scale ...
    float viewW = (float) getWidth() / scale;                      // ... and the extra width widens the sections
    if (viewW < (float) minW) { scale = (float) getWidth() / (float) minW; viewW = (float) minW; }
    proc.uiState.setProperty ("scale", scale, nullptr);
    proc.uiState.setProperty ("editorW", getWidth(), nullptr);
    proc.uiState.setProperty ("editorH", getHeight(), nullptr);
    view.setTransform (AffineTransform::scale (scale));
    view.setBounds (0, 0, roundToInt (viewW), H);
}
