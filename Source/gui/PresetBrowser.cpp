#include "PresetBrowser.h"

using namespace juce;

PresetBrowser::PresetBrowser (TwinboxProcessor& p, int s) : proc (p), section (s)
{
    const auto genres = PresetLibrary::get().genres();
    const auto accent = section == 0 ? theme::accentA : theme::accentB;
    for (int i = 0; i < genres.size(); ++i)
    {
        auto* b = genreBtns.add (new TextButton (genres[i]));
        b->setColour (TextButton::buttonOnColourId, accent);
        b->onClick = [this, i] { selectGenre (i); };
        addAndMakeVisible (b);
    }
    list.setModel (this);
    list.setRowHeight (30);
    list.setColour (ListBox::backgroundColourId, theme::panel);
    addAndMakeVisible (list);
    for (auto* t : { &kitTgl, &patTgl, &tempoTgl })
    {
        t->setColour (ToggleButton::textColourId, theme::text);
        t->setColour (ToggleButton::tickColourId, accent);
        t->setToggleState (t != &tempoTgl, dontSendNotification);
        addAndMakeVisible (t);
    }
    loadBtn.setColour (TextButton::buttonColourId, accent.withAlpha (0.8f));
    loadBtn.setColour (TextButton::textColourOffId, Colours::black);
    loadBtn.onClick = [this] { load (list.getSelectedRow()); };
    addAndMakeVisible (loadBtn);
    selectGenre (0);
}

void PresetBrowser::selectGenre (int g)
{
    genre = g;
    for (int i = 0; i < genreBtns.size(); ++i) genreBtns[i]->setToggleState (i == g, dontSendNotification);
    filtered.clear();
    const auto& items = PresetLibrary::get().items (section);
    const auto name = genreBtns[g]->getButtonText();
    for (int i = 0; i < (int) items.size(); ++i) if (items[(size_t) i].genre == name) filtered.push_back (i);
    list.updateContent();
    list.selectRow (0);
    repaint();
}

void PresetBrowser::load (int row)
{
    if (row < 0 || row >= (int) filtered.size()) return;
    const auto& it = PresetLibrary::get().items (section)[(size_t) filtered[(size_t) row]];
    if (section == 0)
    {
        const int slot = (int) proc.apvts.getRawParameterValue (params::aPattern)->load();
        PresetLibrary::applyA (proc, it, slot, kitTgl.getToggleState(), patTgl.getToggleState(), tempoTgl.getToggleState());
        lastLoaded = it.name + "  ->  pattern " + String::charToString ((juce_wchar) ('A' + slot));
    }
    else
    {
        PresetLibrary::applyB (proc, it, kitTgl.getToggleState(), patTgl.getToggleState(), tempoTgl.getToggleState());
        lastLoaded = it.name + "  ->  patterns A-D";
    }
    repaint();
}

void PresetBrowser::paintListBoxItem (int row, Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int) filtered.size()) return;
    const auto& it = PresetLibrary::get().items (section)[(size_t) filtered[(size_t) row]];
    g.fillAll (selected ? (section == 0 ? theme::accentA : theme::accentB).withAlpha (0.35f) : (row % 2 ? theme::panel : theme::panel2));
    g.setColour (theme::textDim);
    g.setFont (theme::font (12.0f));
    g.drawText (String (row + 1).paddedLeft ('0', 2), 8, 0, 26, h, Justification::centredLeft);
    g.setColour (theme::text);
    g.setFont (theme::font (14.0f, selected));
    g.drawText (it.name, 38, 0, w - 140, h, Justification::centredLeft);
    g.setColour (theme::textDim);
    g.setFont (theme::font (12.0f));
    g.drawText (String (it.bpm) + " BPM", w - 100, 0, 90, h, Justification::centredRight);
}

void PresetBrowser::paint (Graphics& g)
{
    g.fillAll (theme::bg);
    g.setColour (theme::textDim);
    g.setFont (theme::font (12.0f));
    g.drawText (section == 0 ? "Loads into the current pattern slot of Section A (kit = 8 drum patches)"
                             : "Loads a full Section B snapshot: kit (samples, voices, FX) + patterns A-D",
                12, getHeight() - 52, getWidth() - 24, 18, Justification::centredLeft);
    if (lastLoaded.isNotEmpty())
    {
        g.setColour (theme::text);
        g.drawText ("Loaded: " + lastLoaded, 12, getHeight() - 30, getWidth() - 24, 18, Justification::centredLeft);
    }
}

void PresetBrowser::resized()
{
    auto r = getLocalBounds().reduced (12);
    r.removeFromBottom (56);
    auto left = r.removeFromLeft (170);
    for (auto* b : genreBtns) b->setBounds (left.removeFromTop (34).reduced (0, 2));
    r.removeFromLeft (10);
    auto right = r.removeFromRight (170);
    for (auto* t : { &kitTgl, &patTgl, &tempoTgl }) t->setBounds (right.removeFromTop (30));
    right.removeFromTop (10);
    loadBtn.setBounds (right.removeFromTop (40));
    r.removeFromRight (10);
    list.setBounds (r);
}
