#include "PluginEditor.h"
#include "Scales.h"
#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "1.0.0"
#endif

using namespace juce;

namespace
{
    constexpr int titleH = 60, stripsY = 84, stripsH = 280, lowerY = stripsY + stripsH + 8, margin = 8, masterW = 400;
}

//==============================================================================
ExportPanel::ExportPanel()
{
    addAndMakeVisible (reveal);
    addAndMakeVisible (close);
    reveal.onClick = [this] { if (folder.isDirectory()) folder.revealToUser(); };
    close.onClick = [this] { if (onClose) onClose(); };
}

void ExportPanel::setFiles (const Array<File>& f, const File& d) { files = f; folder = d; repaint(); }

void ExportPanel::paint (Graphics& g)
{
    g.fillAll (Colours::black.withAlpha (0.45f));
    auto box = getLocalBounds().toFloat().withSizeKeepingCentre (760.0f, 520.0f);
    g.setColour (theme::panel); g.fillRoundedRectangle (box, 8.0f);
    g.setColour (theme::outline); g.drawRoundedRectangle (box, 8.0f, 2.0f);
    auto head = box.removeFromTop (34.0f);
    g.setColour (theme::header); g.fillRoundedRectangle (head, 8.0f); g.fillRect (head.withTrimmedTop (17.0f));
    g.setColour (theme::headerText); g.setFont (theme::font (16.0f, true));
    g.drawText ("EXPORT  -  " + String (files.size()) + " loops printed", head.reduced (14.0f, 0), Justification::centredLeft);
    // drag area
    g.setColour (theme::lcdBg); g.fillRoundedRectangle (dragArea.toFloat(), 6.0f);
    const float dash[] { 6.0f, 4.0f };
    Path border; border.addRoundedRectangle (dragArea.toFloat().reduced (3.0f), 6.0f);
    PathStrokeType (2.0f).createDashedStroke (border, border, dash, 2);
    g.setColour (theme::lcdInk); g.fillPath (border);
    g.setFont (theme::font (20.0f, true));
    g.drawText ("DRAG ME INTO YOUR DAW", dragArea.withTrimmedBottom (dragArea.getHeight() / 2), Justification::centredBottom);
    g.setFont (theme::font (13.0f));
    g.drawText ("Ableton Live / Logic Pro create one new audio track per loop", dragArea.withTrimmedTop (dragArea.getHeight() / 2 + 4), Justification::centredTop);
    // list
    auto list = box.reduced (20.0f).withTop ((float) dragArea.getBottom() + 12.0f).withTrimmedBottom (44.0f);
    g.setColour (theme::text); g.setFont (theme::font (13.0f));
    for (auto& f : files)
    {
        if (list.getHeight() < 16.0f) break;
        g.drawText (f.getFileName(), list.removeFromTop (17.0f), Justification::centredLeft);
    }
    g.setColour (theme::textDim); g.setFont (theme::font (12.0f));
    g.drawFittedText ("Folder: " + folder.getFullPathName() + "\nLive routing instead: enable the plug-in's extra outputs (Kick ... Melodic 03) in the DAW -"
                      " each enabled module leaves the main mix and plays on its own track.",
                      box.reduced (20.0f).removeFromBottom (40.0f).toNearestInt().withTrimmedRight (300), Justification::centredLeft, 3);
}

void ExportPanel::resized()
{
    auto box = getLocalBounds().withSizeKeepingCentre (760, 520);
    dragArea = box.reduced (20).withTrimmedTop (44).withHeight (130);
    auto b = box.reduced (20).removeFromBottom (36);
    close.setBounds (b.removeFromRight (120));
    b.removeFromRight (10);
    reveal.setBounds (b.removeFromRight (160));
}

void ExportPanel::mouseDrag (const MouseEvent& e)
{
    if (! dragArea.contains (e.getMouseDownPosition()) || files.isEmpty()) return;
    StringArray paths;
    for (auto& f : files) paths.add (f.getFullPathName());
    DragAndDropContainer::performExternalDragDropOfFiles (paths, false, this);
}

//==============================================================================
MainView::MainView (GroovulationProcessor& p) : proc (p), editor (p), master (p)
{
    for (int m = 0; m < lb::numModules; ++m)
    {
        strips.push_back (std::make_unique<ModuleStrip> (p, m, [this] (int i) { selectModule (i); }));
        addAndMakeVisible (*strips.back());
    }
    addAndMakeVisible (editor);
    addAndMakeVisible (master);
    master.onOpenDeck = [this] (int t) { openDeck (t); };
    for (auto* c : std::initializer_list<Component*> { &menu, &randomAll, &print, &exportBtn, &scaleOn, &raw, &run, &scaleRoot, &scaleType, &bpm })
        addAndMakeVisible (c);
    scaleOn.attach (p.apvts, "scaleOn");
    raw.attach (p.apvts, "raw");
    run.attach (p.apvts, "run");
    scaleRoot.addItemList (scales::rootNames(), 1);
    scaleType.addItemList (scales::names(), 1);
    rootAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "scaleRoot", scaleRoot);
    typeAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "scaleType", scaleType);
    bpm.setSliderStyle (Slider::LinearBar);
    bpm.setColour (Slider::trackColourId, theme::lcdBg2);
    bpm.setColour (Slider::textBoxTextColourId, theme::text);
    bpmAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "bpm", bpm);
    bpm.setTooltip ("Internal tempo (used by RUN when the host is stopped / in the standalone app)");
    scaleOn.setTooltip ("SCALE: tonal loops (Bass, Melodic) are transposed into the chosen scale. Off = MANUAL: original pitch at the project tempo");
    raw.setTooltip ("RAW: loops play at their original pitch AND original tempo (PITCH / TEMPO still work)");
    run.setTooltip ("Internal clock (when the host is stopped)");
    randomAll.setTooltip ("Loads a random loop of the right type into every module");
    print.setTooltip ("PRINT: renders each module (as it leaves the module) into PRINT/<snapshot>/<Module> - <snapshot>.wav");
    exportBtn.setTooltip ("EXPORT: prints, then lets you drag all loops into Ableton Live / Logic (one audio track each)");
    menu.onClick = [this] { showMenu(); };
    randomAll.onClick = [this] { proc.randomAll(); };
    print.onClick = [this] { startPrint (false); };
    exportBtn.onClick = [this] { startPrint (true); };
    for (auto* b : { &print, &exportBtn }) { b->setColour (TextButton::buttonColourId, theme::red); b->setColour (TextButton::textColourOffId, theme::white); }
    addChildComponent (exportPanel);
    exportPanel.onClose = [this] { exportPanel.setVisible (false); };
    selectModule ((int) proc.uiState.getProperty ("selected", 0));
    startTimerHz (30);
}

MainView::~MainView() { deckWindow.reset(); }

void MainView::selectModule (int m)
{
    m = jlimit (0, lb::numModules - 1, m);
    for (int i = 0; i < lb::numModules; ++i) strips[(size_t) i]->setSelected (i == m);
    editor.setModule (m);
    proc.uiState.setProperty ("selected", m, nullptr);
}

void MainView::openDeck (int type)
{
    deckWindow.reset();
    deckWindow = std::make_unique<DeckWindow> (proc, type, [this] { MessageManager::callAsync ([sp = SafePointer<MainView> (this)] { if (sp) sp->deckWindow.reset(); }); });
}

void MainView::timerCallback()
{
    phase += 1.0 / 30.0;
    level = proc.masterLevel();
    String s;
    auto d = proc.library->db();
    if (proc.library->isScanning()) s = "SCAN " + String (roundToInt (proc.library->scanProgress() * 100.0f)) + "%  " + proc.library->scanStatus();
    else if (printProgress.load() >= 0.0f) s = "PRINTING... " + String (roundToInt (printProgress.load() * 100.0f)) + "%";
    else s = String ((int) d->entries.size()) + " LOOPS  |  SNAPSHOT: " + proc.snapshotName().toUpperCase();
    status = s;
    repaint (logoArea.toNearestInt().expanded (4));
    repaint (lcdArea.toNearestInt().expanded (2));
}

void MainView::askText (const String& title, const String& initial, std::function<void (String)> done)
{
    auto* aw = new AlertWindow (title, {}, MessageBoxIconType::NoIcon, this);
    aw->addTextEditor ("name", initial);
    aw->addButton ("OK", 1, KeyPress (KeyPress::returnKey));
    aw->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
    aw->enterModalState (true, ModalCallbackFunction::create ([aw, done] (int r)
    {
        if (r == 1) done (aw->getTextEditorContents ("name"));
    }), true);
}

void MainView::showMenu()
{
    PopupMenu m, remove, load;
    const auto folders = proc.library->userFolders();
    for (int i = 0; i < folders.size(); ++i) remove.addItem (100 + i, folders[i]);
    if (folders.isEmpty()) remove.addItem (99, "(no custom folders)", false);
    auto snaps = LoopLibrary::snapshotFolder().findChildFiles (File::findFiles, false, "*.groovulation");
    snaps.sort();
    for (int i = 0; i < snaps.size(); ++i) load.addItem (300 + i, snaps[i].getFileNameWithoutExtension());
    if (snaps.isEmpty()) load.addItem (299, "(no snapshots)", false);
    m.addSectionHeader ("LOOP FOLDERS");
    m.addItem (1, "Add loop folder to scan...");
    m.addSubMenu ("Remove folder", remove);
    m.addItem (2, "Rescan folders");
    m.addItem (3, "Rebuild database (analyse everything again)");
    m.addItem (4, "Show loop folder");
    m.addItem (5, "Show 'similar loops' registry");
    m.addSectionHeader ("SNAPSHOT  (" + proc.snapshotName() + ")");
    m.addItem (10, "Save snapshot as...");
    m.addSubMenu ("Load snapshot", load);
    m.addItem (11, "Rename snapshot...");
    m.addSectionHeader ("PRINT");
    m.addItem (20, "Show PRINT folder");
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (menu), [this, folders, snaps] (int r)
    {
        if (r == 1)
        {
            chooser = std::make_unique<FileChooser> ("Choose a folder with loops", File::getSpecialLocation (File::userMusicDirectory));
            chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories, [this] (const FileChooser& fc)
            {
                if (fc.getResult().isDirectory()) proc.library->addUserFolder (fc.getResult());
            });
        }
        else if (r == 2) proc.library->startScan (false);
        else if (r == 3) proc.library->startScan (true);
        else if (r == 4) LoopLibrary::factoryFolder().revealToUser();
        else if (r == 5) LoopLibrary::databaseFolder().getChildFile ("similar_loops.json").revealToUser();
        else if (r == 10) askText ("Save snapshot", proc.snapshotName(), [this] (String n) { proc.saveSnapshot (n); });
        else if (r == 11) askText ("Snapshot name", proc.snapshotName(), [this] (String n) { proc.setSnapshotName (n); });
        else if (r == 20) { LoopLibrary::printFolder().createDirectory(); LoopLibrary::printFolder().revealToUser(); }
        else if (r >= 100 && r < 100 + folders.size()) proc.library->removeUserFolder (folders[r - 100]);
        else if (r >= 300 && r < 300 + snaps.size()) proc.loadSnapshot (snaps[r - 300]);
    });
}

void MainView::startPrint (bool exportAfter)
{
    if (proc.printing.load()) return;
    printProgress = 0.0f;
    Thread::launch ([this, exportAfter, sp = SafePointer<MainView> (this)]
    {
        auto res = proc.printModules ([this] (float pr) { printProgress = pr; });
        MessageManager::callAsync ([sp, res, exportAfter]
        {
            if (sp == nullptr) return;
            sp->printProgress = -1.0f;
            if (exportAfter && res.ok)
            {
                sp->exportPanel.setFiles (res.files, res.folder);
                sp->exportPanel.setVisible (true);
                sp->exportPanel.toFront (false);
            }
            else if (res.ok) res.folder.revealToUser();
        });
    });
}

void MainView::drawWoofer (Graphics& g, Rectangle<float> r) { drawGroovLogo (g, r, level, phase); }

void MainView::paint (Graphics& g)
{
    g.fillAll (theme::bg);
    // ---- title bar
    auto bar = Rectangle<float> ((float) margin, 4.0f, (float) designW - 2 * margin, (float) titleH - 8.0f);
    g.setColour (theme::header);
    g.fillRoundedRectangle (bar, 6.0f);
    logoArea = Rectangle<float> (bar.getX() + 8.0f, bar.getY() + 1.0f, bar.getHeight() - 2.0f, bar.getHeight() - 2.0f);
    drawWoofer (g, logoArea.reduced (2.0f));
    GlyphArrangement ga;
    const auto f = theme::font (25.0f, true).withExtraKerningFactor (0.12f);
    ga.addLineOfText (f, "GROOVULATION", logoArea.getRight() + 12.0f, bar.getCentreY() + 10.0f);
    g.setColour (theme::headerText);
    ga.draw (g);
    const float tx = logoArea.getRight() + 12.0f + ga.getBoundingBox (0, -1, true).getWidth();
    g.setColour (theme::red); g.fillRoundedRectangle (tx + 10.0f, bar.getCentreY() - 9.0f, 6.0f, 18.0f, 1.5f);
    g.setColour (theme::amber); g.fillRoundedRectangle (tx + 19.0f, bar.getCentreY() - 9.0f, 6.0f, 18.0f, 1.5f);
    g.setColour (theme::teal); g.fillRoundedRectangle (tx + 28.0f, bar.getCentreY() - 9.0f, 6.0f, 18.0f, 1.5f);
    g.setColour (theme::headerText.withAlpha (0.55f));
    g.setFont (theme::font (11.0f));
    g.drawText ("loop mixer  v" + String (JucePlugin_VersionString), Rectangle<float> (tx + 40.0f, bar.getY(), 120.0f, bar.getHeight()), Justification::centredLeft);
    // LCD
    lcdArea = Rectangle<float> (tx + 150.0f, bar.getY() + 4.0f, jmax (200.0f, (float) menu.getX() - 12.0f - (tx + 150.0f)), bar.getHeight() - 8.0f);
    drawLcd (g, lcdArea);
    g.setColour (theme::lcdInk);
    g.setFont (theme::mono (14.0f, true));
    const bool rawOn = proc.apvts.getRawParameterValue ("raw")->load() > 0.5f;
    const bool sc = proc.apvts.getRawParameterValue ("scaleOn")->load() > 0.5f;
    const String mode = rawOn ? "RAW" : (sc ? "SCALE " + scaleRoot.getText() + " " + scaleType.getText() : String ("MANUAL"));
    auto lr = lcdArea.reduced (10.0f, 4.0f);
    g.drawText (String (proc.currentBpm(), 1) + " BPM  " + (proc.isTransportRunning() ? String::fromUTF8 ("\xe2\x96\xb6") : String::fromUTF8 ("\xe2\x96\xa0")) + "  " + mode.toUpperCase(),
                lr.removeFromTop (lr.getHeight() * 0.5f), Justification::centredLeft);
    g.setFont (theme::mono (12.0f));
    g.drawText (status, lr, Justification::centredLeft, true);

    // ---- section tabs above the strips
    for (int s = 0; s < 3; ++s)
    {
        const int a = s == 0 ? 0 : (s == 1 ? 5 : 6), b = s == 0 ? 4 : (s == 1 ? 5 : 8);
        auto r = strips[(size_t) a]->getBounds().getUnion (strips[(size_t) b]->getBounds()).withY (stripsY - 20).withHeight (16).toFloat();
        g.setColour (theme::header); g.fillRoundedRectangle (r, 3.0f);
        g.setColour (s == 0 ? theme::red : (s == 1 ? theme::amber : theme::teal)); g.fillRoundedRectangle (r.withWidth (5.0f), 2.0f);
        g.setColour (theme::headerText); g.setFont (theme::font (11.5f, true));
        g.drawText (s == 0 ? "DRUMS" : (s == 1 ? "BASS" : "MELODIC"), r.withTrimmedLeft (10.0f), Justification::centredLeft);
    }
}

void MainView::resized()
{
    // title bar controls (right side)
    auto bar = Rectangle<int> (margin, 4, designW - 2 * margin, titleH - 8).reduced (8, 10);
    exportBtn.setBounds (bar.removeFromRight (84)); bar.removeFromRight (6);
    print.setBounds (bar.removeFromRight (76)); bar.removeFromRight (14);
    bpm.setBounds (bar.removeFromRight (96)); bar.removeFromRight (4);
    run.setBounds (bar.removeFromRight (52)); bar.removeFromRight (12);
    raw.setBounds (bar.removeFromRight (60)); bar.removeFromRight (8);
    scaleType.setBounds (bar.removeFromRight (150)); bar.removeFromRight (4);
    scaleRoot.setBounds (bar.removeFromRight (56)); bar.removeFromRight (4);
    scaleOn.setBounds (bar.removeFromRight (70)); bar.removeFromRight (12);
    randomAll.setBounds (bar.removeFromRight (96)); bar.removeFromRight (6);
    menu.setBounds (bar.removeFromRight (64));

    // module strips: 5 drums | bass | 3 melodic
    const int gap = 6, sectionGap = 18;
    const int stripW = (designW - 2 * margin - 8 * gap - 2 * sectionGap) / 9;
    int x = margin;
    for (int m = 0; m < lb::numModules; ++m)
    {
        if (m == 5 || m == 6) x += sectionGap;
        strips[(size_t) m]->setBounds (x, stripsY, stripW, stripsH);
        x += stripW + gap;
    }
    editor.setBounds (margin, lowerY, designW - 3 * margin - masterW, designH - lowerY - margin);
    master.setBounds (designW - margin - masterW, lowerY, masterW, designH - lowerY - margin);
    exportPanel.setBounds (getLocalBounds());
}

//==============================================================================
GroovulationEditor::GroovulationEditor (GroovulationProcessor& p) : AudioProcessorEditor (&p), proc (p), view (p)
{
    setLookAndFeel (&lnf);
    LookAndFeel::setDefaultLookAndFeel (&lnf);
    addAndMakeVisible (view);
    scale = (float) (double) proc.uiState.getProperty ("scale", 0.75);
    view.setSize (MainView::designW, MainView::designH);
    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) MainView::designW / MainView::designH);
        c->setSizeLimits (MainView::designW / 2, MainView::designH / 2, MainView::designW * 3 / 2, MainView::designH * 3 / 2);
    }
    setSize (roundToInt (MainView::designW * scale), roundToInt (MainView::designH * scale));
}

GroovulationEditor::~GroovulationEditor()
{
    LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void GroovulationEditor::paint (Graphics& g) { g.fillAll (theme::bg); }

void GroovulationEditor::resized()
{
    scale = jlimit (0.4f, 1.6f, (float) getWidth() / (float) MainView::designW);
    proc.uiState.setProperty ("scale", scale, nullptr);
    view.setTransform (AffineTransform::scale (scale));
    view.setBounds (0, 0, MainView::designW, MainView::designH);
}

AudioProcessorEditor* GroovulationProcessor::createEditor() { return new GroovulationEditor (*this); }
