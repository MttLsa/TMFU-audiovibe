#include "Panels.h"

#include "LookAndFeel.h"

namespace gc::ui {

using juce::String;

static const String kDot (juce::CharPointer_UTF8 ("  \xc2\xb7  "));

static void styleSearch (juce::TextEditor& e, const String& hint)
{
    e.setTextToShowWhenEmpty (hint, col::faint);
    e.setFont (uiFont (13.0f));
    e.setIndents (8, 5);
}

static void alertInfo (const String& title, const String& text)
{
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, title, text);
}

// ================================================================================================= HeaderBar
HeaderBar::HeaderBar (AppController& a) : app (a)
{
    addAndMakeVisible (scan);
    addAndMakeVisible (setup);
    scan.setColour (juce::TextButton::buttonColourId, col::accent.darker (0.25f));
    scan.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    scan.onClick = [this] { app.scanProject(); };
    setup.onClick = [this] { if (onSetup) onSetup(); };
    scan.setTooltip ("Scan every track, return, master and rack of the open Live Set");
}

void HeaderBar::resized()
{
    auto r = getLocalBounds().reduced (14, 12);
    scan.setBounds (r.removeFromRight (140));
    r.removeFromRight (8);
    setup.setBounds (r.removeFromRight (72));
}

void HeaderBar::refresh()
{
    scan.setEnabled (app.connectionState() == BridgeConnection::State::Connected && ! app.isScanning());
    scan.setButtonText (app.isScanning() ? "SCANNING..." : "SCAN PROJECT");
    repaint();
}

void HeaderBar::paint (juce::Graphics& g)
{
    g.setColour (col::panel);
    g.fillRect (getLocalBounds());
    g.setColour (col::outline);
    g.fillRect (getLocalBounds().removeFromBottom (1));

    auto r = getLocalBounds().reduced (16, 0);
    // logo
    auto logo = r.removeFromLeft (210);
    {
        auto c = logo.removeFromLeft (30).toFloat().withSizeKeepingCentre (26, 26);
        const uint32_t pal[8] = { 0xffe8a33d, 0xff3fc1d9, 0xffd65db1, 0xff8fd14f, 0xfff07c3a, 0xff5b8def, 0xffe5534b, 0xff3ec9a7 };
        for (int i = 0; i < 8; ++i) {
            juce::Path p;
            const float a0 = (float) i * juce::MathConstants<float>::twoPi / 8.0f + 0.12f;
            p.addCentredArc (c.getCentreX(), c.getCentreY(), 11, 11, 0, a0, a0 + 0.55f, true);
            g.setColour (juce::Colour (pal[i]));
            g.strokePath (p, juce::PathStrokeType (3.0f));
        }
        g.setColour (col::raised);
        g.fillEllipse (c.reduced (7));
        logo.removeFromLeft (10);
        g.setColour (col::text);
        g.setFont (uiFont (17.0f, true));
        g.drawText ("GLOBAL CONTROL", logo.removeFromTop (logo.getHeight() / 2 + 4).withTrimmedTop (6), juce::Justification::bottomLeft);
        g.setColour (col::faint);
        g.setFont (uiFont (10.5f, true));
        g.drawText ("TMFU AUDIOVIBE  " JUCE_STRINGIFY (GC_VERSION), logo, juce::Justification::topLeft);
    }

    const auto st = app.connectionState();
    const bool connected = st == BridgeConnection::State::Connected;
    const auto& snap = app.getSnapshot();
    auto pills = r.withTrimmedRight (240).reduced (0, 15).toFloat();
    auto pill = [&] (float w) { auto p = pills.removeFromLeft (w); pills.removeFromLeft (8); return p; };

    const bool live = connected || app.isLiveRunning();
    drawPill (g, pill (150), live ? col::ok : col::faint, "Ableton", connected ? "Live " + app.getBridge().getLiveVersion() : (live ? "running" : "not detected"));
    drawPill (g, pill (150), connected ? col::ok : st == BridgeConnection::State::Lost ? col::bad : col::warn, "Bridge",
              connected ? "connected" : st == BridgeConnection::State::Lost ? "lost - retrying" : st == BridgeConnection::State::Offline ? "offline" : "searching...");
    auto pr = pill (juce::jmax (180.0f, pills.getWidth() - 2 * 128.0f - 16.0f));
    projectArea = pr.toNearestInt();
    const auto pname = app.getProjectName();
    const auto pid = app.getProjectId();
    drawPill (g, pr, connected ? col::accent : col::faint, "Project",
              ! connected ? String ("-") : pname.isNotEmpty() ? pname : pid.isEmpty() ? String ("(identifying...)") : "unnamed  (" + pid.substring (4, 12) + ") - click to name");
    drawPill (g, pill (128), snap.valid ? col::ok : col::faint, "Last scan", snap.valid ? snap.scannedAt.toString (false, true, true, true) : String ("never"));
    drawPill (g, pill (128), snap.valid ? col::accent : col::faint, "Plug-ins", snap.valid ? String (snap.countInstances()) : String ("-"));
}

void HeaderBar::mouseUp (const juce::MouseEvent& e)
{
    if (! projectArea.contains (e.getPosition()) || app.getProjectId().isEmpty()) return;
    auto* w = new juce::AlertWindow ("Project name", "Live does not expose the name of the Set to Max for Live.\nName this project (stored in GLOBAL CONTROL, id "
                                     + app.getProjectId() + "):", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", app.getProjectName());
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<HeaderBar> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int r) {
        if (r == 1 && safe != nullptr) safe->app.setProjectName (w->getTextEditorContents ("name"));
    }), true);
}

// ================================================================================================= DeviceBrowser
DeviceBrowser::DeviceBrowser (AppController& a) : app (a)
{
    addAndMakeVisible (search);
    styleSearch (search, "Search devices");
    search.onTextChange = [this] { refresh(); };
    addAndMakeVisible (pluginsOnly);
    pluginsOnly.setToggleState (true, juce::dontSendNotification);
    pluginsOnly.onClick = [this] { refresh(); };
    list.setModel (this);
    list.setRowHeight (40);
    addAndMakeVisible (list);
}

void DeviceBrowser::paint (juce::Graphics& g)
{
    drawSection (g, getLocalBounds(), "Device browser");
    if (rows.isEmpty()) {
        g.setColour (col::faint);
        g.setFont (uiFont (13.0f));
        g.drawFittedText (app.getSnapshot().valid ? "No matching devices" : "Connect the bridge and press SCAN PROJECT",
                          list.getBounds().reduced (12), juce::Justification::centredTop, 3);
    }
}

void DeviceBrowser::resized()
{
    auto r = getLocalBounds().reduced (10).withTrimmedTop (22);
    search.setBounds (r.removeFromTop (28));
    r.removeFromTop (6);
    pluginsOnly.setBounds (r.removeFromTop (22));
    r.removeFromTop (6);
    list.setBounds (r);
}

void DeviceBrowser::refresh()
{
    const juce::ScopedValueSetter<bool> svs (updating, true);
    rows = app.pluginKinds (search.getText(), pluginsOnly.getToggleState());
    list.updateContent();
    int sel = -1;
    for (int i = 0; i < rows.size(); ++i) if (rows[i].key == app.getSelectedPlugin()) sel = i;
    if (sel >= 0) list.selectRow (sel, true, true); else list.deselectAllRows();
    list.repaint();
    repaint();
}

int DeviceBrowser::getNumRows() { return rows.size(); }

void DeviceBrowser::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= rows.size()) return;
    const auto& k = rows.getReference (row);
    auto r = juce::Rectangle<int> (0, 0, w, h).reduced (2, 2).toFloat();
    if (selected) { g.setColour (col::accent.withAlpha (0.14f)); g.fillRoundedRectangle (r, 5.0f); g.setColour (col::accent.withAlpha (0.6f)); g.drawRoundedRectangle (r, 5.0f, 1.0f); }
    auto in = r.toNearestInt().reduced (10, 4);
    auto badge = in.removeFromRight (34).toFloat().withSizeKeepingCentre (34, 22);
    g.setColour (selected ? col::accent : col::raised);
    g.fillRoundedRectangle (badge, 11.0f);
    g.setColour (selected ? juce::Colours::black : col::text);
    g.setFont (monoFont (12.5f, true));
    g.drawText (String (k.instances), badge, juce::Justification::centred);
    g.setColour (col::text);
    g.setFont (uiFont (14.0f, true));
    g.drawFittedText (k.name, in.removeFromTop (17), juce::Justification::centredLeft, 1);
    g.setColour (col::dim);
    g.setFont (uiFont (11.0f));
    g.drawFittedText (k.isPlugin ? k.format + (k.className == "AuPluginDevice" ? " (Audio Unit)" : " plug-in (VST/VST3)") : "Live device  " + k.className, in, juce::Justification::centredLeft, 1);
}

void DeviceBrowser::selectedRowsChanged (int row)
{
    if (updating || row < 0 || row >= rows.size()) return;
    app.selectPlugin (rows[row].key);
}

// ================================================================================================= InstancePanel
enum Col { cSel = 1, cTrack, cPlugin, cId, cParams, cStatus, cGroups, cMacros };

InstancePanel::InstancePanel (AppController& a) : app (a)
{
    addAndMakeVisible (search);
    styleSearch (search, "Search track, instance, group");
    search.onTextChange = [this] { refresh(); };
    addAndMakeVisible (filter);
    filter.addItemList ({ "All instances", "Selected", "Mapped", "Not mapped", "Excluded" }, 1);
    filter.setSelectedId (1, juce::dontSendNotification);
    filter.onChange = [this] { refresh(); };
    for (auto* b : { &all, &none, &group, &exclude, &include }) addAndMakeVisible (b);
    all.onClick = [this] { app.selectAllInstances (true); };
    none.onClick = [this] { app.selectAllInstances (false); };
    group.onClick = [this] { groupMenu(); };
    exclude.setTooltip ("Excluded instances are never controlled, even when they belong to a macro");
    exclude.onClick = [this] { for (const auto& id : app.selectedInstances()) app.setInstanceExcluded (id, true); };
    include.onClick = [this] {
        for (int i = 0; i < table.getNumSelectedRows(); ++i) {
            const int r = table.getSelectedRow (i);
            if (r >= 0 && r < rows.size() && rows[r].device) app.setInstanceExcluded (rows[r].device->id, false);
        }
    };

    auto& h = table.getHeader();
    h.addColumn ("", cSel, 28, 28, 28, juce::TableHeaderComponent::notResizable);
    h.addColumn ("Track", cTrack, 150, 80);
    h.addColumn ("Plug-in", cPlugin, 130, 70);
    h.addColumn ("Instance", cId, 110, 70);
    h.addColumn ("Params", cParams, 56, 40);
    h.addColumn ("Status", cStatus, 80, 60);
    h.addColumn ("Groups", cGroups, 90, 50);
    h.addColumn ("Macros", cMacros, 80, 50);
    h.setStretchToFitActive (true);
    table.setModel (this);
    table.setMultipleSelectionEnabled (true);
    table.setRowHeight (26);
    table.setHeaderHeight (24);
    addAndMakeVisible (table);
}

void InstancePanel::paint (juce::Graphics& g)
{
    String sub;
    for (const auto& k : app.pluginKinds ({}, false)) if (k.key == app.getSelectedPlugin()) sub = k.name;
    const int sel = app.selectedInstances().size();
    if (sub.isNotEmpty()) sub << kDot << sel << " selected" << (sel == 0 ? " (macros use all)" : "");
    drawSection (g, getLocalBounds(), "Instance manager", sub);
}

void InstancePanel::resized()
{
    auto r = getLocalBounds().reduced (10).withTrimmedTop (22);
    auto bar = r.removeFromTop (28);
    filter.setBounds (bar.removeFromRight (120));
    bar.removeFromRight (6);
    search.setBounds (bar);
    r.removeFromTop (6);
    auto bar2 = r.removeFromTop (26);
    for (auto* b : { &all, &none }) { b->setBounds (bar2.removeFromLeft (52)); bar2.removeFromLeft (4); }
    bar2.removeFromLeft (6);
    group.setBounds (bar2.removeFromLeft (76));
    include.setBounds (bar2.removeFromRight (70));
    bar2.removeFromRight (4);
    exclude.setBounds (bar2.removeFromRight (70));
    r.removeFromTop (6);
    table.setBounds (r);
}

void InstancePanel::refresh()
{
    const juce::ScopedValueSetter<bool> svs (updating, true);
    rows = app.instanceRows (search.getText(), filter.getSelectedId() - 1);
    table.updateContent();
    juce::SparseSet<int> sel;
    for (int i = 0; i < rows.size(); ++i) if (rows[i].selected) sel.addRange ({ i, i + 1 });
    table.setSelectedRows (sel, juce::dontSendNotification);
    table.repaint();
    repaint();
}

int InstancePanel::getNumRows() { return rows.size(); }

void InstancePanel::paintRowBackground (juce::Graphics& g, int row, int w, int h, bool selected)
{
    if (row < 0 || row >= rows.size()) return;
    if (selected) { g.setColour (col::accent.withAlpha (0.13f)); g.fillRect (0, 0, w, h); }
    else if (row % 2) { g.setColour (col::panel2.withAlpha (0.6f)); g.fillRect (0, 0, w, h); }
    if (rows[row].missing) { g.setColour (col::bad.withAlpha (0.08f)); g.fillRect (0, 0, w, h); }
}

void InstancePanel::paintCell (juce::Graphics& g, int row, int column, int w, int h, bool)
{
    if (row < 0 || row >= rows.size()) return;
    const auto& r = rows.getReference (row);
    const auto textCol = r.excluded ? col::faint : r.missing ? col::bad : col::text;
    g.setFont (uiFont (12.5f));
    auto cell = juce::Rectangle<int> (6, 0, w - 8, h);
    switch (column) {
        case cSel: {
            auto b = juce::Rectangle<float> (14, 14).withCentre ({ w * 0.5f, h * 0.5f });
            g.setColour (col::lcd); g.fillRoundedRectangle (b, 3);
            g.setColour (r.selected ? col::accent : col::outlineHi); g.drawRoundedRectangle (b, 3, 1);
            if (r.selected) g.fillRoundedRectangle (b.reduced (3.5f), 1.5f);
            if (r.excluded) { g.setColour (col::bad); g.drawLine (b.getX() + 2, b.getY() + 2, b.getRight() - 2, b.getBottom() - 2, 1.5f); }
            break;
        }
        case cTrack: g.setColour (textCol); g.setFont (uiFont (12.5f, true)); g.drawFittedText (r.trackName, cell, juce::Justification::centredLeft, 1); break;
        case cPlugin: g.setColour (textCol); g.drawFittedText (r.label, cell, juce::Justification::centredLeft, 1); break;
        case cId: g.setColour (col::dim); g.setFont (monoFont (11.5f)); g.drawFittedText (r.instanceId, cell, juce::Justification::centredLeft, 1); break;
        case cParams: g.setColour (col::dim); g.setFont (monoFont (12.0f)); g.drawText (r.missing ? String ("-") : String (r.paramCount), cell, juce::Justification::centredLeft); break;
        case cStatus: {
            const auto c = r.missing ? col::bad : r.excluded ? col::faint : r.connection == "online" ? col::ok : col::warn;
            g.setColour (c); g.fillEllipse (6.0f, h * 0.5f - 3.0f, 6.0f, 6.0f);
            g.setColour (textCol); g.drawFittedText (r.excluded ? String ("excluded") : r.connection, cell.withTrimmedLeft (12), juce::Justification::centredLeft, 1);
            break;
        }
        case cGroups: g.setColour (col::dim); g.drawFittedText (r.groupNames, cell, juce::Justification::centredLeft, 1); break;
        case cMacros: g.setColour (r.missing ? col::bad : col::accent); g.setFont (uiFont (12.0f, true)); g.drawFittedText (r.macros, cell, juce::Justification::centredLeft, 1); break;
        default: break;
    }
}

void InstancePanel::selectedRowsChanged (int)
{
    if (updating) return;
    const juce::ScopedValueSetter<bool> svs (updating, true);
    for (int i = 0; i < rows.size(); ++i) {
        if (! rows[i].device) continue;
        const bool s = table.isRowSelected (i);
        if (s != rows[i].selected) app.setInstanceSelected (rows[i].device->id, s && ! rows[i].excluded);
    }
}

void InstancePanel::cellClicked (int row, int column, const juce::MouseEvent&)
{
    if (column != cSel || row < 0 || row >= rows.size() || ! rows[row].device || rows[row].excluded) return;
    app.setInstanceSelected (rows[row].device->id, ! rows[row].selected);
}

void InstancePanel::askName (const String& title, const String& initial, std::function<void (String)> done)
{
    auto* w = new juce::AlertWindow (title, {}, juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", initial);
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([w, done] (int r) { if (r == 1) done (w->getTextEditorContents ("name")); }), true);
}

void InstancePanel::groupMenu()
{
    juce::PopupMenu m, sel, ren, del;
    const auto& gs = app.getGroups();
    for (int i = 0; i < (int) gs.size(); ++i) {
        const auto label = gs[(size_t) i].name + "  (" + String (gs[(size_t) i].deviceIds.size()) + ")";
        sel.addItem (100 + i, label); ren.addItem (200 + i, label); del.addItem (300 + i, label);
    }
    m.addItem (1, "Group selected instances...", ! app.selectedInstances().isEmpty());
    m.addSubMenu ("Select group", sel, ! gs.empty());
    m.addSubMenu ("Rename group", ren, ! gs.empty());
    m.addSubMenu ("Delete group", del, ! gs.empty());
    juce::Component::SafePointer<InstancePanel> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&group), [safe] (int r) {
        if (safe == nullptr || r == 0) return;
        auto& a = safe->app;
        if (r == 1) safe->askName ("New group", "Group " + String ((int) a.getGroups().size() + 1), [safe] (String n) {
            if (safe == nullptr) return;
            if (auto res = safe->app.createGroup (n); res.failed()) alertInfo ("Group", res.getErrorMessage()); });
        else if (r >= 300) a.deleteGroup (r - 300);
        else if (r >= 200) {
            const int gi = r - 200;
            safe->askName ("Rename group", a.getGroups()[(size_t) gi].name, [safe, gi] (String n) {
                if (safe == nullptr) return;
                if (auto res = safe->app.renameGroup (gi, n); res.failed()) alertInfo ("Group", res.getErrorMessage()); });
        } else if (r >= 100) a.selectGroup (r - 100);
    });
}

// ================================================================================================= ParameterPanel
ParameterPanel::ParameterPanel (AppController& a) : app (a)
{
    addAndMakeVisible (search);
    styleSearch (search, "Search parameters");
    search.onTextChange = [this] { refresh(); };
    list.setModel (this);
    list.setMultipleSelectionEnabled (true);
    list.setRowHeight (30);
    addAndMakeVisible (list);
    for (int i = 0; i < kNumMacros; ++i) target.addItem ("to Macro " + String (i + 1), i + 1);
    target.setSelectedId (1, juce::dontSendNotification);
    addAndMakeVisible (target);
    addAndMakeVisible (assignBtn);
    assignBtn.setColour (juce::TextButton::buttonColourId, col::accent.darker (0.25f));
    assignBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    assignBtn.onClick = [this] { assign(); };
    assignBtn.setTooltip ("Several parameters go to consecutive macros, starting from the chosen one");
    hint.setFont (uiFont (11.0f));
    hint.setColour (juce::Label::textColourId, col::faint);
    hint.setText ("Only the parameters Live exposes are listed. To expose others, use Configure on the device title bar in Live.", juce::dontSendNotification);
    hint.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (hint);
}

void ParameterPanel::setTargetMacro (int i) { target.setSelectedId (i + 1, juce::dontSendNotification); }

void ParameterPanel::paint (juce::Graphics& g)
{
    String sub;
    for (const auto& k : app.pluginKinds ({}, false)) if (k.key == app.getSelectedPlugin()) sub = k.name;
    if (! rows.isEmpty()) sub << kDot << rows.size() << " exposed";
    drawSection (g, getLocalBounds(), "Parameters", sub);
    if (rows.isEmpty()) {
        g.setColour (col::faint);
        g.setFont (uiFont (13.0f));
        g.drawFittedText (app.parametersLoading() ? "Reading parameters from Live..." : app.getSelectedPlugin().isEmpty() ? "Select a device" : "No parameter exposed by Live",
                          list.getBounds().reduced (12), juce::Justification::centredTop, 2);
    }
}

void ParameterPanel::resized()
{
    auto r = getLocalBounds().reduced (10).withTrimmedTop (22);
    search.setBounds (r.removeFromTop (28));
    r.removeFromTop (6);
    hint.setBounds (r.removeFromBottom (30));
    auto bar = r.removeFromBottom (30);
    assignBtn.setBounds (bar.removeFromRight (90));
    bar.removeFromRight (6);
    target.setBounds (bar);
    r.removeFromBottom (6);
    list.setBounds (r);
}

void ParameterPanel::refresh()
{
    juce::StringArray sel;
    for (int i = 0; i < list.getNumSelectedRows(); ++i) { const int r = list.getSelectedRow (i); if (r >= 0 && r < rows.size()) sel.add (rows[r].originalName); }
    rows.clearQuick();
    if (const auto* ps = app.selectedPluginParameters())
        for (const auto& p : *ps)
            if (search.isEmpty() || p.name.containsIgnoreCase (search.getText()) || p.originalName.containsIgnoreCase (search.getText())) rows.add (p);
    list.updateContent();
    juce::SparseSet<int> s;
    for (int i = 0; i < rows.size(); ++i) if (sel.contains (rows[i].originalName)) s.addRange ({ i, i + 1 });
    list.setSelectedRows (s, juce::dontSendNotification);
    list.repaint();
    repaint();
}

int ParameterPanel::getNumRows() { return rows.size(); }

void ParameterPanel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= rows.size()) return;
    const auto& p = rows.getReference (row);
    if (selected) { g.setColour (col::accent.withAlpha (0.14f)); g.fillRoundedRectangle (juce::Rectangle<int> (0, 0, w, h).reduced (2, 1).toFloat(), 4.0f); }
    auto r = juce::Rectangle<int> (0, 0, w, h).reduced (10, 0);
    const bool ok = p.controllable();
    String macros;
    for (int i = 0; i < kNumMacros; ++i) {
        const auto& m = app.getMacro (i);
        if (m.assigned && m.pluginKey() == app.getSelectedPlugin() && m.paramName == p.originalName) macros << "M" << (i + 1) << " ";
    }
    if (macros.isNotEmpty()) {
        auto b = r.removeFromRight (juce::jmin (70, 12 + 22 * macros.length() / 3)).toFloat().withSizeKeepingCentre (juce::jmin (70.0f, 8.0f + 7.5f * macros.trim().length()), 18);
        g.setColour (col::accent.withAlpha (0.2f)); g.fillRoundedRectangle (b, 9);
        g.setColour (col::accent); g.setFont (uiFont (10.5f, true)); g.drawText (macros.trim(), b, juce::Justification::centred);
        r.removeFromRight (6);
    }
    g.setFont (monoFont (12.0f));
    g.setColour (ok ? col::text.withAlpha (0.85f) : col::faint);
    g.drawFittedText (p.display, r.removeFromRight (96), juce::Justification::centredRight, 1);
    g.setColour (ok ? col::text : col::faint);
    g.setFont (uiFont (13.0f, true));
    auto top = r.removeFromTop (h * 0.58f);
    g.drawFittedText (p.name, top, juce::Justification::bottomLeft, 1);
    g.setColour (ok ? col::dim : col::bad.withAlpha (0.8f));
    g.setFont (uiFont (10.5f));
    g.drawFittedText ("#" + String (p.index) + "  " + p.statusText() + (p.quantized && ! p.items.isEmpty() ? " (" + String (p.items.size()) + " values)" : String()),
                      r, juce::Justification::topLeft, 1);
}

void ParameterPanel::listBoxItemDoubleClicked (int, const juce::MouseEvent&) { assign(); }

void ParameterPanel::assign()
{
    juce::StringArray names;
    for (int i = 0; i < list.getNumSelectedRows(); ++i) {
        const int r = list.getSelectedRow (i);
        if (r < 0 || r >= rows.size()) continue;
        if (! rows[r].controllable()) { alertInfo ("Not controllable", "\"" + rows[r].name + "\" is " + rows[r].statusText() + " in Live and cannot be controlled."); return; }
        names.add (rows[r].originalName);
    }
    if (auto res = app.assignParameters (names, getTargetMacro()); res.failed()) alertInfo ("Assign", res.getErrorMessage());
}

// ================================================================================================= PresetBar
PresetBar::PresetBar (AppController& a) : app (a)
{
    for (auto* b : { &projectBtn, &globalBtn }) {
        addAndMakeVisible (b);
        b->setRadioGroupId (77);
        b->setClickingTogglesState (true);
        b->onClick = [this] { refresh(); };
    }
    projectBtn.setToggleState (true, juce::dontSendNotification);
    projectBtn.setTooltip ("Presets of this Live Set: instances are re-identified when the Set is reopened");
    globalBtn.setTooltip ("Templates for any Live Set: plug-in, parameter and macro settings, applied to every instance of the plug-in");
    addAndMakeVisible (presets);
    presets.setTextWhenNothingSelected ("No preset");
    for (auto* b : { &saveBtn, &loadBtn, &renameBtn, &dupBtn, &delBtn }) addAndMakeVisible (b);
    saveBtn.onClick = [this] { save(); };
    loadBtn.onClick = [this] { load(); };
    renameBtn.onClick = [this] { rename(); };
    dupBtn.onClick = [this] { duplicate(); };
    delBtn.onClick = [this] { remove(); };
}

void PresetBar::paint (juce::Graphics& g)
{
    g.setColour (col::dim);
    g.setFont (uiFont (11.5f, true));
    g.drawText ("PRESETS", getLocalBounds().removeFromLeft (64), juce::Justification::centredLeft);
}

void PresetBar::resized()
{
    auto r = getLocalBounds().reduced (0, 4);
    r.removeFromLeft (66);
    projectBtn.setBounds (r.removeFromLeft (76)); globalBtn.setBounds (r.removeFromLeft (70));
    r.removeFromLeft (8);
    for (auto* b : { &delBtn, &dupBtn, &renameBtn, &loadBtn, &saveBtn }) { b->setBounds (r.removeFromRight (b == &dupBtn ? 80 : 64)); r.removeFromRight (4); }
    r.removeFromRight (4);
    presets.setBounds (r);
}

void PresetBar::refresh()
{
    const auto keep = presets.getText();
    items = app.getPresetStore().list (scope(), app.getProjectId());
    presets.clear (juce::dontSendNotification);
    for (int i = 0; i < items.size(); ++i) presets.addItem (items[i].name, i + 1);
    for (int i = 0; i < items.size(); ++i) if (items[i].name == keep) presets.setSelectedId (i + 1, juce::dontSendNotification);
    const bool needsProject = scope() == PresetScope::Project && app.getProjectId().isEmpty();
    saveBtn.setEnabled (! needsProject);
    for (auto* b : { &loadBtn, &renameBtn, &dupBtn, &delBtn }) b->setEnabled (presets.getSelectedId() > 0);
}

const PresetInfo* PresetBar::current() const
{
    const int i = presets.getSelectedId() - 1;
    return i >= 0 && i < items.size() ? &items.getReference (i) : nullptr;
}

void PresetBar::showReport (const String& title, const juce::StringArray& lines)
{
    alertInfo (title, lines.isEmpty() ? String ("Done.") : lines.joinIntoString ("\n"));
}

void PresetBar::save()
{
    auto* w = new juce::AlertWindow ("Save preset", scope() == PresetScope::Project ? "Project preset (this Live Set)" : "Global preset (template for any Live Set)",
                                     juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", current() ? current()->name : String ("Preset 1"));
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<PresetBar> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int r) {
        if (r != 1 || safe == nullptr) return;
        const auto name = w->getTextEditorContents ("name");
        auto& store = safe->app.getPresetStore();
        auto doSave = [safe, name] (bool overwrite) {
            if (safe == nullptr) return;
            auto res = safe->app.getPresetStore().save (safe->scope(), safe->app.getProjectId(), name, safe->app.toPresetVar(), overwrite);
            if (res.failed()) alertInfo ("Save preset", res.getErrorMessage());
            safe->refresh();
            for (int i = 0; i < safe->items.size(); ++i) if (safe->items[i].name == PresetStore::sanitize (name)) safe->presets.setSelectedId (i + 1);
        };
        if (store.exists (safe->scope(), safe->app.getProjectId(), name))
            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Save preset", "\"" + name + "\" already exists. Replace it?", "Replace", "Cancel", nullptr,
                                                juce::ModalCallbackFunction::create ([doSave] (int ok) { if (ok) doSave (true); }));
        else doSave (false);
    }), true);
}

void PresetBar::load()
{
    const auto* p = current();
    if (p == nullptr) return;
    juce::var v;
    if (auto res = app.getPresetStore().load (*p, v); res.failed()) { alertInfo ("Load preset", res.getErrorMessage()); return; }
    if (! app.getSnapshot().valid) { alertInfo ("Load preset", "Connect the bridge and scan the project first."); return; }
    showReport ("Preset \"" + p->name + "\"", app.applyPresetVar (v, p->scope));
}

void PresetBar::rename()
{
    const auto* p = current();
    if (p == nullptr) return;
    const auto info = *p;
    auto* w = new juce::AlertWindow ("Rename preset", {}, juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", info.name);
    w->addButton ("Rename", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<PresetBar> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w, info] (int r) {
        if (r != 1 || safe == nullptr) return;
        if (auto res = safe->app.getPresetStore().rename (info, w->getTextEditorContents ("name")); res.failed()) alertInfo ("Rename preset", res.getErrorMessage());
        safe->refresh();
    }), true);
}

void PresetBar::duplicate()
{
    const auto* p = current();
    if (p == nullptr) return;
    String n;
    if (auto res = app.getPresetStore().duplicate (*p, n); res.failed()) alertInfo ("Duplicate preset", res.getErrorMessage());
    refresh();
}

void PresetBar::remove()
{
    const auto* p = current();
    if (p == nullptr) return;
    const auto info = *p;
    juce::Component::SafePointer<PresetBar> safe (this);
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon, "Delete preset", "Delete \"" + info.name + "\"?", "Delete", "Cancel", nullptr,
                                        juce::ModalCallbackFunction::create ([safe, info] (int ok) {
        if (! ok || safe == nullptr) return;
        if (auto res = safe->app.getPresetStore().remove (info); res.failed()) alertInfo ("Delete preset", res.getErrorMessage());
        safe->refresh();
    }));
}

// ================================================================================================= SetupPanel
SetupPanel::SetupPanel (AppController& a) : app (a)
{
    for (auto* b : { &install, &reveal, &close }) addAndMakeVisible (b);
    install.setColour (juce::TextButton::buttonColourId, col::accent.darker (0.25f));
    install.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    install.onClick = [this] { installBridge (false); };
    reveal.onClick = [this] { (env.installed ? env.targetDir : env.userLibrary).revealToUser(); };
    close.onClick = [this] { if (onClose) onClose(); };
    refresh();
}

void SetupPanel::installBridge (bool overwrite)
{
    String msg;
    const auto outcome = BridgeInstaller::install (env, overwrite, msg);
    if (outcome == BridgeInstaller::Outcome::NeedsConfirmation) {
        juce::Component::SafePointer<SetupPanel> safe (this);
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Install bridge", msg, "Replace (keep a backup)", "Cancel", nullptr,
                                            juce::ModalCallbackFunction::create ([safe] (int ok) { if (ok && safe != nullptr) safe->installBridge (true); }));
        return;
    }
    installMessage = msg;
    lastInspect = 0;
    refresh();
}

void SetupPanel::refresh()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (now - lastInspect > 3000) { env = BridgeInstaller::inspect(); lastInspect = now; }
    install.setEnabled (env.sourceComplete && ! env.upToDate);
    install.setButtonText (env.installed && ! env.upToDate ? "Update bridge in User Library" : "Install bridge in User Library");
    repaint();
}

void SetupPanel::paint (juce::Graphics& g)
{
    g.fillAll (col::bg.withAlpha (0.93f));
    auto card = getLocalBounds().withSizeKeepingCentre (juce::jmin (720, getWidth() - 40), juce::jmin (560, getHeight() - 40));
    g.setColour (col::panel);
    g.fillRoundedRectangle (card.toFloat(), 10.0f);
    g.setColour (col::outlineHi);
    g.drawRoundedRectangle (card.toFloat().reduced (0.5f), 10.0f, 1.0f);
    auto r = card.reduced (28, 22);
    g.setColour (col::text);
    g.setFont (uiFont (20.0f, true));
    g.drawText ("Connect GLOBAL CONTROL to Ableton Live", r.removeFromTop (30), juce::Justification::centredLeft);
    g.setColour (col::dim);
    g.setFont (uiFont (13.0f));
    g.drawFittedText ("One bridge device per Live Set is enough: it reaches every track, rack and plug-in. Your plug-ins are never moved, replaced or wrapped.",
                      r.removeFromTop (36), juce::Justification::topLeft, 2);
    r.removeFromTop (8);

    const auto st = app.connectionState();
    const bool connected = st == BridgeConnection::State::Connected;
    struct Step { bool done; String title, detail; };
    const Step steps[] = {
        { env.maxForLiveFound || connected, "Ableton Live with Max for Live",
          connected ? "Live " + app.getBridge().getLiveVersion() + " connected" : env.maxForLiveDetail },
        { env.installed || connected, "GLOBAL_CONTROL_BRIDGE.amxd in the User Library",
          env.installed ? env.targetDir.getFullPathName() : (env.sourceComplete ? String ("Press \"Install bridge\" below") : String ("Bridge files not bundled with this build: see Documentation/TEST_PLAN.md")) },
        { connected, "Create a MIDI track named GLOBAL CONTROL and drop the bridge on it",
          "Browser > User Library > Presets > MIDI Effects > Max MIDI Effect > GLOBAL_CONTROL_BRIDGE. No instrument, no MIDI routing needed." },
        { connected, "Automatic connection (UDP 127.0.0.1:47120/47121)",
          connected ? String ("Bridge connected") : st == BridgeConnection::State::Lost ? String ("Connection lost - retrying every second") : String ("Waiting for the bridge... (start order of Live and GLOBAL CONTROL does not matter)") },
        { app.getSnapshot().valid, "SCAN PROJECT", app.getSnapshot().valid ? String (app.getSnapshot().countInstances()) + " plug-in instances found" : String ("Runs automatically when the bridge connects") },
    };
    int n = 1;
    for (const auto& s : steps) {
        auto row = r.removeFromTop (62);
        auto badge = row.removeFromLeft (34).toFloat().withSizeKeepingCentre (26, 26).withY ((float) row.getY() + 4);
        g.setColour (s.done ? col::ok : col::raised);
        g.fillEllipse (badge);
        g.setColour (s.done ? juce::Colours::black : col::text);
        g.setFont (uiFont (13.0f, true));
        g.drawText (s.done ? String (juce::CharPointer_UTF8 ("\xe2\x9c\x93")) : String (n), badge, juce::Justification::centred);
        row.removeFromLeft (8);
        g.setColour (col::text);
        g.setFont (uiFont (14.5f, true));
        g.drawText (s.title, row.removeFromTop (24), juce::Justification::centredLeft);
        g.setColour (col::dim);
        g.setFont (uiFont (12.5f));
        g.drawFittedText (s.detail, row.removeFromTop (34), juce::Justification::topLeft, 2);
        ++n;
    }
    if (installMessage.isNotEmpty()) {
        g.setColour (col::accent);
        g.setFont (uiFont (12.5f));
        g.drawFittedText (installMessage, r.removeFromTop (34), juce::Justification::topLeft, 2);
    }
}

void SetupPanel::resized()
{
    auto card = getLocalBounds().withSizeKeepingCentre (juce::jmin (720, getWidth() - 40), juce::jmin (560, getHeight() - 40));
    auto bar = card.reduced (28, 22).removeFromBottom (32);
    close.setBounds (bar.removeFromRight (90));
    bar.removeFromRight (8);
    reveal.setBounds (bar.removeFromRight (110));
    install.setBounds (bar.removeFromLeft (250));
}

// ================================================================================================= StatusBar
StatusBar::StatusBar (AppController& a) : app (a)
{
    addAndMakeVisible (logBtn);
    logBtn.onClick = [this] {
        auto* te = new juce::TextEditor();
        te->setMultiLine (true); te->setReadOnly (true); te->setScrollbarsShown (true);
        te->setFont (monoFont (12.0f));
        te->setText (app.getLog().joinIntoString ("\n"));
        te->setSize (760, 420);
        te->moveCaretToEnd();
        juce::CallOutBox::launchAsynchronously (std::unique_ptr<juce::Component> (te), logBtn.getScreenBounds(), nullptr);
    };
}

void StatusBar::resized() { logBtn.setBounds (getLocalBounds().removeFromRight (56).reduced (0, 3)); }

void StatusBar::paint (juce::Graphics& g)
{
    g.setColour (col::dim);
    g.setFont (uiFont (12.0f));
    const auto& s = app.getBridge().getStats();
    auto r = getLocalBounds().withTrimmedRight (64);
    g.drawText ("OSC in " + String (s.packetsIn) + "  out " + String (s.packetsOut) + "  writes " + String (s.setsSent), r.removeFromRight (260), juce::Justification::centredRight);
    g.drawFittedText (app.lastMessage(), r, juce::Justification::centredLeft, 1);
}

} // namespace gc::ui
