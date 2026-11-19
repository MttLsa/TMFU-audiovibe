#include "MacroInspector.h"

#include "LookAndFeel.h"

namespace gc::ui {

static const uint32_t kSwatches[] = { 0xffe8a33d, 0xff3fc1d9, 0xffd65db1, 0xff8fd14f, 0xfff07c3a, 0xff5b8def, 0xffe5534b, 0xff3ec9a7, 0xffb9bcc4, 0xffa77bf3 };

static void styleSlider (juce::Slider& s)
{
    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 84, 22);
}

static void styleLabel (juce::Label& l, const juce::String& t)
{
    l.setText (t, juce::dontSendNotification);
    l.setFont (uiFont (12.0f, true));
    l.setColour (juce::Label::textColourId, col::dim);
}

MacroInspector::MacroInspector (AppController& a, int i) : app (a), index (i)
{
    addAndMakeVisible (name);
    name.setFont (uiFont (15.0f, true));
    name.onTextChange = [this] { push(); };

    for (auto c : kSwatches) {
        auto* b = swatches.add (new juce::TextButton());
        b->setColour (juce::TextButton::buttonColourId, juce::Colour (c));
        b->onClick = [this, c] { auto s = app.getMacro (index).settings; s.colour = c; app.setMacroSettings (index, s); };
        addAndMakeVisible (b);
    }
    for (auto* b : { &absBtn, &relBtn }) {
        addAndMakeVisible (b);
        b->setClickingTogglesState (false);
    }
    absBtn.onClick = [this] { app.setMacroMode (index, MacroMode::Absolute); };
    relBtn.onClick = [this] { app.setMacroMode (index, MacroMode::Relative); };

    styleLabel (minLabel, "MINIMUM"); styleLabel (maxLabel, "MAXIMUM"); styleLabel (spanLabel, "VARIATION +/-");
    styleLabel (precLabel, "PRECISION"); styleLabel (fineLabel, "FINE (SHIFT)");
    for (auto* l : { &minLabel, &maxLabel, &spanLabel, &precLabel, &fineLabel }) addAndMakeVisible (l);
    for (auto* s : { &minSlider, &maxSlider, &spanSlider, &precSlider, &fineSlider }) {
        styleSlider (*s);
        addAndMakeVisible (s);
        s->onValueChange = [this] { push(); };
    }
    precSlider.setRange (0, 4, 1);
    precSlider.setTextValueSuffix (" dec.");
    fineSlider.setRange (0.01, 0.5, 0.01);
    fineSlider.setNumDecimalPlacesToDisplay (2);
    addAndMakeVisible (invert);
    invert.onClick = [this] { push(); };

    addAndMakeVisible (resetBtn); addAndMakeVisible (captureBtn); addAndMakeVisible (clearBtn);
    resetBtn.onClick = [this] { app.resetMacro (index); };
    captureBtn.onClick = [this] { app.captureRelative (index); };
    clearBtn.onClick = [this] { app.clearMacro (index); };
    resetBtn.setTooltip ("Absolute: parameter default. Relative: back to the starting values (no variation).");
    captureBtn.setTooltip ("Relative: the current values become the new starting point, knob back to centre.");

    info.setFont (uiFont (12.0f));
    info.setColour (juce::Label::textColourId, col::dim);
    info.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (info);

    instances.setModel (this);
    instances.setRowHeight (34);
    addAndMakeVisible (instances);

    app.addChangeListener (this);
    load();
    setSize (440, 600);
}

MacroInspector::~MacroInspector() { app.removeChangeListener (this); }

void MacroInspector::changeListenerCallback (juce::ChangeBroadcaster*) { load(); }

void MacroInspector::configureRanges()
{
    const auto& m = app.getMacro (index);
    double lo = 0, hi = 1, spanMax = 1, spanMin = 0.01, step = 0;
    juce::String unit;
    bool logRange = false;
    if (m.scale) {
        unit = juce::String (m.scale->unit());
        if (auto r = m.scale->displayRange()) { lo = r->first; hi = r->second; }
        switch (m.scale->kind()) {
            case ScaleKind::Enumerated: lo = m.scale->minRaw(); hi = m.scale->maxRaw(); step = 1; spanMin = 1; spanMax = juce::jmax (1.0, hi - lo); unit = {}; break;
            case ScaleKind::Discrete: step = 0; spanMin = 1; spanMax = juce::jmax (1.0, m.scale->maxRaw() - m.scale->minRaw()); break;
            case ScaleKind::Unknown: lo = m.scale->minRaw(); hi = m.scale->maxRaw(); spanMax = 1; unit = "(raw)"; break;
            case ScaleKind::Linear:
            case ScaleKind::Logarithmic:
            case ScaleKind::Curved:
                if (MacroEngine::multiplicative (*m.scale)) { spanMin = 0.05; spanMax = 6; logRange = lo > 0; }
                else spanMax = juce::jmax (1e-6, hi - lo);
        }
    }
    if (! (hi > lo)) hi = lo + 1;
    const double interval = step > 0 ? step : (hi - lo) / 10000.0;
    for (auto* s : { &minSlider, &maxSlider }) {
        s->setRange (lo, hi, interval);
        if (logRange) s->setSkewFactorFromMidPoint (std::sqrt (lo * hi));
        else s->setSkewFactor (1.0);
        s->setTextValueSuffix (unit.isNotEmpty() ? " " + unit : juce::String());
        s->setNumDecimalPlacesToDisplay (juce::jlimit (0, 4, m.settings.precision < 0 ? 3 : m.settings.precision));
    }
    spanSlider.setRange (spanMin, spanMax, step > 0 || (m.scale && m.scale->kind() == ScaleKind::Discrete) ? 1.0 : (spanMax - spanMin) / 1000.0);
    const bool mult = m.scale && MacroEngine::multiplicative (*m.scale);
    spanSlider.setTextValueSuffix (mult ? " oct" : (m.scale && (m.scale->kind() == ScaleKind::Discrete || m.scale->kind() == ScaleKind::Enumerated)) ? " steps"
                                   : (m.scale && m.scale->kind() == ScaleKind::Unknown) ? " (x50%)" : (unit.isNotEmpty() ? " " + unit : juce::String()));
}

void MacroInspector::load()
{
    const juce::ScopedValueSetter<bool> svs (loading, true);
    const auto& m = app.getMacro (index);
    if (! name.hasKeyboardFocus (true)) name.setText (juce::String (m.settings.name), false);
    configureRanges();
    minSlider.setValue (m.settings.rangeMin, juce::dontSendNotification);
    maxSlider.setValue (m.settings.rangeMax, juce::dontSendNotification);
    spanSlider.setValue (m.settings.relativeSpan, juce::dontSendNotification);
    precSlider.setValue (juce::jmax (0, m.settings.precision), juce::dontSendNotification);
    fineSlider.setValue (m.settings.fineFactor, juce::dontSendNotification);
    invert.setToggleState (m.settings.invert, juce::dontSendNotification);
    const bool rel = m.settings.mode == MacroMode::Relative;
    absBtn.setToggleState (! rel, juce::dontSendNotification);
    relBtn.setToggleState (rel, juce::dontSendNotification);
    for (auto* c : { (juce::Component*) &minSlider, (juce::Component*) &maxSlider, (juce::Component*) &minLabel, (juce::Component*) &maxLabel }) c->setVisible (! rel);
    for (auto* c : { (juce::Component*) &spanSlider, (juce::Component*) &spanLabel, (juce::Component*) &captureBtn }) c->setVisible (rel);
    for (auto* c : { (juce::Component*) &absBtn, (juce::Component*) &relBtn, (juce::Component*) &resetBtn, (juce::Component*) &clearBtn,
                     (juce::Component*) &minSlider, (juce::Component*) &maxSlider, (juce::Component*) &spanSlider }) c->setEnabled (m.assigned);

    juce::String t;
    if (! m.assigned) t = "Not assigned. Select a plug-in, the instances and one or more parameters, then ASSIGN.";
    else {
        t << m.pluginName << " / " << m.paramName << "\n";
        if (m.scale) t << "Scale: " << toString (m.scale->kind()) << (m.scale->unit().empty() ? juce::String() : "  (" + juce::String (m.scale->unit()) + ")");
        else t << "Measuring the parameter scale...";
        if (rel) t << "\nRelative: every instance keeps its own value and receives the same variation"
                   << (m.scale && MacroEngine::multiplicative (*m.scale) ? " (multiplicative, in octaves)." : ".");
        if (m.outOfSync) t << "\nValues in Live differ from start + variation: press Capture to restart from them.";
        if (m.mixed) t << "\nInstances currently have different values (shown as 'mixed').";
    }
    info.setText (t, juce::dontSendNotification);
    instances.updateContent();
    instances.repaint();
    repaint();
}

void MacroInspector::push()
{
    if (loading) return;
    auto s = app.getMacro (index).settings;
    s.name = name.getText().toStdString();
    s.rangeMin = minSlider.getValue();
    s.rangeMax = maxSlider.getValue();
    s.relativeSpan = spanSlider.getValue();
    s.precision = (int) precSlider.getValue();
    s.fineFactor = fineSlider.getValue();
    s.invert = invert.getToggleState();
    app.setMacroSettings (index, s);
}

void MacroInspector::paint (juce::Graphics& g)
{
    g.fillAll (col::panel);
    g.setColour (col::dim);
    g.setFont (uiFont (11.0f, true));
    g.drawText ("MACRO " + juce::String (index + 1), 14, 8, 200, 16, juce::Justification::centredLeft);
    g.drawText ("INSTANCES", instances.getBounds().translated (0, -18).withHeight (16), juce::Justification::centredLeft);
}

void MacroInspector::resized()
{
    auto r = getLocalBounds().reduced (14);
    r.removeFromTop (18);
    name.setBounds (r.removeFromTop (30));
    r.removeFromTop (8);
    auto sw = r.removeFromTop (20);
    for (auto* b : swatches) { b->setBounds (sw.removeFromLeft (24).reduced (2)); }
    r.removeFromTop (10);
    auto modes = r.removeFromTop (28);
    absBtn.setBounds (modes.removeFromLeft (modes.getWidth() / 2).reduced (0, 0).withTrimmedRight (3));
    relBtn.setBounds (modes.withTrimmedLeft (3));
    r.removeFromTop (8);
    auto row = [&r] (juce::Label& l, juce::Component& c) { auto x = r.removeFromTop (26); l.setBounds (x.removeFromLeft (110)); c.setBounds (x); r.removeFromTop (2); };
    if (minSlider.isVisible()) { row (minLabel, minSlider); row (maxLabel, maxSlider); }
    else row (spanLabel, spanSlider);
    row (precLabel, precSlider);
    row (fineLabel, fineSlider);
    auto opts = r.removeFromTop (28);
    invert.setBounds (opts.removeFromLeft (100));
    clearBtn.setBounds (opts.removeFromRight (70).reduced (0, 2));
    opts.removeFromRight (6);
    captureBtn.setBounds (opts.removeFromRight (80).reduced (0, 2));
    opts.removeFromRight (6);
    resetBtn.setBounds (opts.removeFromRight (70).reduced (0, 2));
    r.removeFromTop (6);
    info.setBounds (r.removeFromTop (68));
    r.removeFromTop (20);
    instances.setBounds (r);
}

int MacroInspector::getNumRows() { return (int) app.getMacro (index).instances.size(); }

void MacroInspector::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool)
{
    const auto& m = app.getMacro (index);
    if (row < 0 || row >= (int) m.instances.size()) return;
    const auto& in = m.instances[(size_t) row];
    if (row % 2) { g.setColour (col::panel2); g.fillRect (0, 0, w, h); }
    const auto c = in.status == InstanceStatus::Ok ? col::ok : in.status == InstanceStatus::Pending ? col::dim : col::bad;
    g.setColour (c);
    g.fillEllipse (6.0f, h * 0.5f - 7.0f, 6.0f, 6.0f);
    g.setColour (app.isExcluded (in.deviceId) ? col::faint : col::text);
    g.setFont (uiFont (12.5f, true));
    g.drawText (juce::String (in.fp.trackName) + "  " + juce::String (in.fp.locationKey), 18, 2, w - 120, 16, juce::Justification::centredLeft);
    g.setFont (monoFont (12.5f));
    g.drawText (in.hasValue ? in.display : juce::String ("-"), w - 110, 2, 104, 16, juce::Justification::centredRight);
    g.setColour (col::dim);
    g.setFont (uiFont (11.0f));
    juce::String sub = juce::String (toString (in.status));
    if (app.isExcluded (in.deviceId)) sub = "excluded";
    if (in.lastSource.isNotEmpty()) sub << " - last change: " << in.lastSource;
    if (in.note.isNotEmpty() && in.status != InstanceStatus::Ok) sub = in.note;
    g.drawFittedText (sub, 18, 17, w - 24, 15, juce::Justification::centredLeft, 1);
}

} // namespace gc::ui
