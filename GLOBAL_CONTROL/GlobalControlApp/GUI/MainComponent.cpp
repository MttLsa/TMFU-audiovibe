#include "MainComponent.h"

#include "LookAndFeel.h"
#include "MacroInspector.h"

namespace gc::ui {

MainComponent::MainComponent (AppController& a)
    : app (a), header (a), presetBar (a), devices (a), instances (a), parameters (a), status (a), setup (a)
{
    for (auto* c : std::initializer_list<juce::Component*> { &header, &presetBar, &devices, &instances, &parameters, &status }) addAndMakeVisible (c);
    for (int i = 0; i < kNumMacros; ++i) {
        auto* k = knobs.add (new MacroKnob (app, i));
        k->onSelect = [this] (int idx) { selectMacro (idx); };
        k->onOpenSettings = [this] (int idx) { openInspector (idx); };
        addAndMakeVisible (k);
    }
    addChildComponent (setup);
    setup.onClose = [this] { setupDismissed = true; showSetup (false); };
    header.onSetup = [this] { showSetup (! setup.isVisible()); };
    app.addChangeListener (this);
    selectMacro (0);
    setWantsKeyboardFocus (true);
    setSize (1440, 900);
    startTimerHz (10);
    refreshAll();
}

MainComponent::~MainComponent() { app.removeChangeListener (this); }

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto r = macroArea;
    g.setColour (col::dim);
    g.setFont (uiFont (11.5f, true));
    g.drawText ("MACRO CONTROL", r.removeFromTop (20).withTrimmedLeft (4), juce::Justification::centredLeft);
    g.setColour (col::faint);
    g.setFont (uiFont (11.0f));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("drag / wheel to move  \xc2\xb7  Shift = fine  \xc2\xb7  double-click = reset  \xc2\xb7  right-click = options")), macroArea.removeFromTop (20).withTrimmedRight (4),
                juce::Justification::centredRight);
}

void MainComponent::resized()
{
    auto r = getLocalBounds();
    header.setBounds (r.removeFromTop (62));
    status.setBounds (r.removeFromBottom (28).reduced (14, 2));
    r.reduce (12, 0);
    presetBar.setBounds (r.removeFromTop (44).reduced (2, 4));
    const int macroH = juce::jlimit (260, 340, (int) (getHeight() * 0.36));
    macroArea = r.removeFromBottom (macroH);
    r.removeFromBottom (8);
    auto panels = r;
    const int w = panels.getWidth();
    devices.setBounds (panels.removeFromLeft ((int) (w * 0.21f)));
    panels.removeFromLeft (8);
    parameters.setBounds (panels.removeFromRight ((int) (w * 0.29f)));
    panels.removeFromRight (8);
    instances.setBounds (panels);

    auto ka = macroArea.withTrimmedTop (22);
    const int gap = 8;
    const int kw = (ka.getWidth() - gap * (kNumMacros - 1)) / kNumMacros;
    for (int i = 0; i < kNumMacros; ++i) { knobs[i]->setBounds (ka.removeFromLeft (kw)); ka.removeFromLeft (gap); }
    setup.setBounds (getLocalBounds().withTrimmedTop (62));
}

void MainComponent::selectMacro (int i)
{
    selectedMacro = i;
    for (int k = 0; k < knobs.size(); ++k) knobs[k]->setSelected (k == i);
    parameters.setTargetMacro (i);
}

void MainComponent::openInspector (int i)
{
    selectMacro (i);
    auto content = std::make_unique<MacroInspector> (app, i);
    juce::CallOutBox::launchAsynchronously (std::move (content), knobs[i]->getScreenBounds(), nullptr);
}

void MainComponent::showSetup (bool show)
{
    setup.setVisible (show);
    if (show) { setup.refresh(); setup.toFront (false); }
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*) { refreshAll(); }

void MainComponent::refreshAll()
{
    header.refresh();
    presetBar.refresh();
    devices.refresh();
    instances.refresh();
    parameters.refresh();
    for (auto* k : knobs) k->refresh();
    status.refresh();
    if (setup.isVisible()) setup.refresh();
}

void MainComponent::timerCallback()
{
    const bool connected = app.connectionState() == BridgeConnection::State::Connected;
    if (connected && ! wasConnected && setup.isVisible() && ! setupDismissed) showSetup (false);
    if (! connected && ! setupDismissed && ! setup.isVisible() && juce::Time::getMillisecondCounter() - startedAt > 2500) showSetup (true);
    wasConnected = connected;
    header.refresh();
    status.refresh();
    if (setup.isVisible()) setup.refresh();
}

bool MainComponent::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress ('r', juce::ModifierKeys::commandModifier, 0)) { app.scanProject(); return true; }
    if (k.getModifiers().isCommandDown() && k.getKeyCode() >= '1' && k.getKeyCode() <= '8') { selectMacro (k.getKeyCode() - '1'); return true; }
    return false;
}

} // namespace gc::ui
