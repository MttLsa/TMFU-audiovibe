#include "PluginEditor.h"

using namespace juce;

MainView::MainView (TwinboxProcessor& p) : global (p), sectionA (p), sectionB (p)
{
    addAndMakeVisible (sectionA);
    addAndMakeVisible (sectionB);
    addAndMakeVisible (global);
}

int MainView::designWidth() const
{
    return margin + sectionWidth + margin + (global.isExpanded() ? GlobalPanel::expandedWidth : GlobalPanel::collapsedWidth) + margin;
}

void MainView::paint (Graphics& g) { g.fillAll (theme::bg); }

void MainView::resized()
{
    sectionA.setBounds (margin, margin, sectionWidth, heightA);
    sectionB.setBounds (margin, margin + heightA + margin, sectionWidth, heightB);
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
