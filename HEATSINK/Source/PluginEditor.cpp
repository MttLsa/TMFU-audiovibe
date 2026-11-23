#include "PluginEditor.h"
#include "BinaryData.h"

juce::Typeface::Ptr ui::typeface (bool bold)
{
    static const juce::Typeface::Ptr regular = juce::Typeface::createSystemTypefaceFor (BinaryData::UrbanistLight_ttf, BinaryData::UrbanistLight_ttfSize);
    static const juce::Typeface::Ptr heavy   = juce::Typeface::createSystemTypefaceFor (BinaryData::UrbanistSemiBold_ttf, BinaryData::UrbanistSemiBold_ttfSize);
    return bold ? heavy : regular;
}

// =============================================================================================== look
HsLook::HsLook()
{
    setDefaultSansSerifTypeface (ui::typeface (false));
    setColour (juce::ResizableWindow::backgroundColourId, ui::bg);
    setColour (juce::ComboBox::backgroundColourId, ui::panel2);
    setColour (juce::ComboBox::textColourId, ui::text);
    setColour (juce::ComboBox::outlineColourId, ui::line);
    setColour (juce::ComboBox::arrowColourId, ui::dim);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1d2026));
    setColour (juce::PopupMenu::textColourId, ui::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, ui::line);
    setColour (juce::PopupMenu::highlightedTextColourId, ui::text);
    setColour (juce::TextButton::buttonColourId, ui::panel2);
    setColour (juce::TextButton::textColourOffId, ui::dim);
    setColour (juce::TextButton::textColourOnId, ui::bg);
    setColour (juce::Label::textColourId, ui::text);
    setColour (juce::Slider::textBoxTextColourId, ui::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, ui::line);
    setColour (juce::TextEditor::backgroundColourId, ui::panel2);
    setColour (juce::TextEditor::textColourId, ui::text);
    setColour (juce::TextEditor::highlightColourId, ui::line);
    setColour (juce::CaretComponent::caretColourId, ui::text);
    setColour (juce::TooltipWindow::backgroundColourId, ui::panel2);
    setColour (juce::TooltipWindow::textColourId, ui::text);
    setColour (juce::TooltipWindow::outlineColourId, ui::line);
}

void HsLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
    const float d = (float) juce::jmin (w, h) - 6.0f;
    const auto c = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const float r = d * 0.5f;
    juce::Path bgArc;
    bgArc.addCentredArc (c.x, c.y, r, r, 0, a0, a1, true);
    g.setColour (ui::line);
    g.strokePath (bgArc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const bool bip = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float ref = bip ? a0 + (a1 - a0) * (float) s.valueToProportionOfLength (0.0) : a0;
    const float av = a0 + (a1 - a0) * pos;
    juce::Path v;
    v.addCentredArc (c.x, c.y, r, r, 0, juce::jmin (ref, av), juce::jmax (ref, av), true);
    g.setColour (s.isEnabled() ? accent : ui::dim);
    g.strokePath (v, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (ui::panel2);
    g.fillEllipse (c.x - r + 5, c.y - r + 5, (r - 5) * 2, (r - 5) * 2);
    juce::Path p;
    p.addRoundedRectangle (-1.2f, -(r - 6), 2.4f, (r - 6) * 0.55f, 1.0f);
    g.setColour (ui::text);
    g.fillPath (p, juce::AffineTransform::rotation (av).translated (c.x, c.y));
}

void HsLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.hasKeyboardFocus (true) ? ui::dim : box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    juce::Path a;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    a.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (a);
}

void HsLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 24, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void HsLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const auto accent = b.findColour (juce::TextButton::buttonOnColourId);
    g.setColour (b.getToggleState() ? accent : (over || down ? ui::line : ui::panel2));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (b.getToggleState() ? accent : ui::line);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

// =============================================================================================== controls
Knob::Knob (juce::AudioProcessorValueTreeState& s, const char* paramId, const juce::String& cap, juce::Colour a,
            const juce::String& tip, int target)
    : modTarget (target), caption (cap), accent (a)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 18);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setTooltip (tip);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, paramId, slider);
}

void Knob::paint (juce::Graphics& g)
{
    auto top = getLocalBounds().removeFromTop (15);
    g.setFont (ui::font (12.5f, true));
    g.setColour (isEnabled() ? ui::text : ui::dim);
    g.drawFittedText (caption, top, juce::Justification::centred, 1, 0.8f);
    int dx = getWidth() / 2 + (int) juce::GlyphArrangement::getStringWidth (ui::font (12.5f, true), caption) / 2 + 5;
    if (lfoMod)
    {
        g.setColour (ui::lfo);
        g.fillEllipse ((float) dx, 4.0f, 6.0f, 6.0f);
        dx += 8;
    }
    if (envMod)
    {
        g.setColour (ui::env);
        g.fillEllipse ((float) dx, 4.0f, 6.0f, 6.0f);
    }
}

void Knob::resized()
{
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, juce::jmin (76, getWidth() - 4), 18);
    slider.setBounds (getLocalBounds().withTrimmedTop (16));
}

void Knob::setModFlags (bool l, bool e)
{
    if (l == lfoMod && e == envMod) return;
    lfoMod = l;
    envMod = e;
    repaint();
}

Choice::Choice (juce::AudioProcessorValueTreeState& s, const char* paramId, const juce::String& cap, const juce::StringArray& items,
                const juce::String& tip)
    : caption (cap)
{
    box.addItemList (items, 1);
    box.setTooltip (tip);
    addAndMakeVisible (box);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, paramId, box);
}

void Choice::paint (juce::Graphics& g)
{
    g.setFont (ui::font (12.5f, true));
    g.setColour (isEnabled() ? ui::dim : ui::line);
    g.drawText (caption, getLocalBounds().removeFromTop (15), juce::Justification::centredLeft);
}

void Choice::resized() { box.setBounds (getLocalBounds().withTrimmedTop (16).withHeight (24)); }

Toggle::Toggle (juce::AudioProcessorValueTreeState& s, const char* paramId, const juce::String& text, juce::Colour accent,
                const juce::String& tip)
{
    button.setButtonText (text);
    button.setClickingTogglesState (true);
    button.setColour (juce::TextButton::buttonOnColourId, accent);
    button.setTooltip (tip);
    addAndMakeVisible (button);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, paramId, button);
}

// =============================================================================================== editor
HeatsinkEditor::HeatsinkEditor (HeatsinkProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      input (p.apvts, pid::input, "Input", ui::heat, "Guadagno d'ingresso prima del circuito di distorsione", pid::tInput),
      drive (p.apvts, pid::drive, "Drive", ui::heat, "Quantità di saturazione del circuito scelto", pid::tDrive),
      cutoff (p.apvts, pid::cutoff, "Cutoff", ui::filt, "Frequenza di taglio del filtro", pid::tCutoff),
      reso (p.apvts, pid::reso, "Reso", ui::filt, "Risonanza (saturata nel feedback del filtro)", pid::tReso),
      lowFreq (p.apvts, pid::lowFreq, "Low Freq", ui::eq, "Frequenza dello shelf sui bassi", pid::tLowFreq),
      lowGain (p.apvts, pid::lowGain, "Low Gain", ui::eq, "Guadagno dello shelf sui bassi", pid::tLowGain),
      highFreq (p.apvts, pid::highFreq, "High Freq", ui::eq, "Frequenza dello shelf sugli acuti", pid::tHighFreq),
      highGain (p.apvts, pid::highGain, "High Gain", ui::eq, "Guadagno dello shelf sugli acuti", pid::tHighGain),
      mix (p.apvts, pid::mix, "Mix", ui::out, "Miscela tra segnale processato e segnale originale", pid::tMix),
      output (p.apvts, pid::output, "Output", ui::out, "Volume d'uscita", pid::tOutput),
      lfoRate (p.apvts, pid::lfoRate, "Rate", ui::lfo, "Velocità dell'LFO (libera)", pid::tLfoRate),
      lfoDepth (p.apvts, pid::lfoDepth, "Depth", ui::lfo, "Profondità dell'LFO sulla destinazione (bipolare)", pid::tLfoDepth),
      envAttack (p.apvts, pid::envAttack, "Attack", ui::env, "Tempo di attacco dell'inviluppo", pid::tNone),
      envRelease (p.apvts, pid::envRelease, "Release", ui::env, "Tempo di rilascio dell'inviluppo", pid::tNone),
      envThresh (p.apvts, pid::envThresh, "Threshold", ui::env,
                 "Follower: livello da cui l'inviluppo inizia a salire. Generator: soglia che fa partire l'inviluppo", pid::tNone),
      envDepth (p.apvts, pid::envDepth, "Depth", ui::env, "Profondità dell'inviluppo sulla destinazione (bipolare)", pid::tEnvDepth),
      circuit (p.apvts, pid::circuit, "Circuit", pid::circuitNames(), "Circuito di distorsione"),
      filterType (p.apvts, pid::filterType, "Type", pid::filterNames(), "Tipo di filtro"),
      lfoShape (p.apvts, pid::lfoShape, "Shape", pid::lfoShapeNames(), "Forma d'onda dell'LFO"),
      lfoDiv (p.apvts, pid::lfoDiv, "Division", pid::lfoDivNames(), "Durata di un ciclo a tempo con il progetto"),
      lfoTarget (p.apvts, pid::lfoTarget, "Target", pid::targetNames(), "Parametro modulato dall'LFO"),
      envMode (p.apvts, pid::envMode, "Mode", pid::envModeNames(),
               "Follower: segue il livello d'ingresso. Generator: attacco/rilascio avviato dai transienti"),
      envTarget (p.apvts, pid::envTarget, "Target", pid::targetNames(), "Parametro modulato dall'inviluppo"),
      filterOn (p.apvts, pid::filterOn, "On", ui::filt, "Attiva/disattiva il filtro"),
      lfoSync (p.apvts, pid::lfoSync, "Sync", ui::lfo, "Aggancia l'LFO al tempo del progetto")
{
    setLookAndFeel (&look);

    for (auto* c : std::initializer_list<juce::Component*> { &input, &drive, &cutoff, &reso, &lowFreq, &lowGain, &highFreq,
                                                             &highGain, &mix, &output, &lfoRate, &lfoDepth, &envAttack,
                                                             &envRelease, &envThresh, &envDepth, &circuit, &filterType,
                                                             &lfoShape, &lfoDiv, &lfoTarget, &envMode, &envTarget,
                                                             &filterOn, &lfoSync, &presetBox })
        addAndMakeVisible (c);

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setTooltip ("Preset di fabbrica");
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedItemIndex();
        if (idx < 0) return;
        proc.setCurrentProgram (idx);
        shownProgram = idx;
    };

    setSize (960, 420);
    timerCallback();
    startTimerHz (30);
}

HeatsinkEditor::~HeatsinkEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void HeatsinkEditor::paintPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, juce::Colour accent) const
{
    g.setColour (ui::panel);
    g.fillRoundedRectangle (r.toFloat(), 6.0f);
    g.setColour (accent);
    g.fillRoundedRectangle (r.toFloat().removeFromTop (3.0f).reduced (8.0f, 0.0f), 1.5f);
    g.setFont (ui::font (14.0f, true));
    g.setColour (ui::text);
    g.drawText (title, r.reduced (10, 0).withTrimmedTop (7).withHeight (16), juce::Justification::centredLeft);
}

void HeatsinkEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::bg);

    g.setFont (ui::font (26.0f, true));
    g.setColour (ui::text);
    g.drawText ("HEATSINK", 16, 8, 200, 32, juce::Justification::centredLeft);
    g.setFont (ui::font (13.0f));
    g.setColour (ui::dim);
    g.drawText ("Drive  /  Filter  /  EQ", 160, 8, 260, 32, juce::Justification::centredLeft);
    g.drawText ("Preset", presetBox.getX() - 60, presetBox.getY(), 52, presetBox.getHeight(), juce::Justification::centredRight);

    paintPanel (g, rDrive, "DRIVE", ui::heat);
    paintPanel (g, rFilter, "FILTER", ui::filt);
    paintPanel (g, rEq, "EQ", ui::eq);
    paintPanel (g, rOut, "OUTPUT", ui::out);
    paintPanel (g, rLfo, "LFO", ui::lfo);
    paintPanel (g, rEnv, "ENVELOPE", ui::env);

    // EQ band captions
    g.setFont (ui::font (12.0f));
    g.setColour (ui::dim);
    g.drawText ("Low Shelf", lowFreq.getX(), rEq.getY() + 40, lowGain.getRight() - lowFreq.getX(), 16, juce::Justification::centred);
    g.drawText ("High Shelf", highFreq.getX(), rEq.getY() + 40, highGain.getRight() - highFreq.getX(), 16, juce::Justification::centred);
    g.setColour (ui::line);
    g.drawHorizontalLine (rEq.getY() + 58, (float) lowFreq.getX() + 8, (float) lowGain.getRight() - 8);
    g.drawHorizontalLine (rEq.getY() + 58, (float) highFreq.getX() + 8, (float) highGain.getRight() - 8);

    // modulator activity
    auto meter = [&g] (juce::Rectangle<int> panelR, float v, bool bipolar, juce::Colour c)
    {
        auto m = juce::Rectangle<float> ((float) panelR.getRight() - 90.0f, (float) panelR.getY() + 11.0f, 78.0f, 6.0f);
        g.setColour (ui::panel2);
        g.fillRoundedRectangle (m, 3.0f);
        g.setColour (c);
        if (bipolar)
        {
            const float cx = m.getCentreX(), x = cx + v * m.getWidth() * 0.5f;
            g.fillRoundedRectangle (juce::jmin (cx, x), m.getY(), std::abs (x - cx) + 1.0f, m.getHeight(), 3.0f);
        }
        else
        {
            g.fillRoundedRectangle (m.withWidth (juce::jmax (1.0f, v * m.getWidth())), 3.0f);
        }
    };
    meter (rLfo, shownLfo, true, ui::lfo);
    meter (rEnv, shownEnv, false, ui::env);
}

void HeatsinkEditor::resized()
{
    presetBox.setBounds (getWidth() - 16 - 240, 12, 240, 26);

    const int top1 = 56, h1 = 190, top2 = top1 + h1 + 10, h2 = 152, knobH = 106;
    rDrive  = { 12, top1, 200, h1 };
    rFilter = { 222, top1, 200, h1 };
    rEq     = { 432, top1, 350, h1 };
    rOut    = { 792, top1, 156, h1 };
    rLfo    = { 12, top2, 420, h2 };
    rEnv    = { 442, top2, 506, h2 };

    auto inner = [] (juce::Rectangle<int> r) { return r.reduced (10).withTrimmedTop (22); };
    auto knobRow = [knobH] (juce::Rectangle<int> r, std::initializer_list<Knob*> knobs)
    {
        auto row = r.removeFromBottom (knobH);
        const int w = row.getWidth() / (int) knobs.size();
        for (auto* k : knobs) k->setBounds (row.removeFromLeft (w));
    };

    {
        auto r = inner (rDrive);
        circuit.setBounds (r.removeFromTop (40));
        knobRow (r, { &input, &drive });
    }
    {
        auto r = inner (rFilter);
        auto t = r.removeFromTop (40);
        filterOn.setBounds (t.removeFromRight (48).withTrimmedTop (16).withHeight (24));
        t.removeFromRight (8);
        filterType.setBounds (t);
        knobRow (r, { &cutoff, &reso });
    }
    knobRow (inner (rEq), { &lowFreq, &lowGain, &highFreq, &highGain });
    knobRow (inner (rOut), { &mix, &output });
    {
        auto r = inner (rLfo);
        auto colA = r.removeFromLeft (120);
        lfoShape.setBounds (colA.removeFromTop (40));
        colA.removeFromTop (8);
        lfoTarget.setBounds (colA.removeFromTop (40));
        r.removeFromLeft (10);
        auto colB = r.removeFromLeft (100);
        lfoSync.setBounds (colB.removeFromTop (40).withTrimmedTop (16));
        colB.removeFromTop (8);
        lfoDiv.setBounds (colB.removeFromTop (40));
        r.removeFromLeft (10);
        knobRow (r, { &lfoRate, &lfoDepth });
    }
    {
        auto r = inner (rEnv);
        auto colA = r.removeFromLeft (120);
        envMode.setBounds (colA.removeFromTop (40));
        colA.removeFromTop (8);
        envTarget.setBounds (colA.removeFromTop (40));
        r.removeFromLeft (10);
        knobRow (r, { &envAttack, &envRelease, &envThresh, &envDepth });
    }
}

void HeatsinkEditor::timerCallback()
{
    const bool sync = proc.apvts.getRawParameterValue (pid::lfoSync)->load() > 0.5f;
    lfoRate.setEnabled (! sync);
    lfoDiv.setEnabled (sync);
    lfoRate.setAlpha (sync ? 0.45f : 1.0f);
    lfoDiv.setAlpha (sync ? 1.0f : 0.45f);

    const int lt = (int) proc.apvts.getRawParameterValue (pid::lfoTarget)->load();
    const int et = (int) proc.apvts.getRawParameterValue (pid::envTarget)->load();
    if (lt != shownLfoT || et != shownEnvT)
    {
        shownLfoT = lt;
        shownEnvT = et;
        for (auto* k : { &input, &drive, &cutoff, &reso, &lowFreq, &lowGain, &highFreq, &highGain, &mix, &output, &lfoRate,
                         &lfoDepth, &envAttack, &envRelease, &envThresh, &envDepth })
            k->setModFlags (k->modTarget != pid::tNone && k->modTarget == lt, k->modTarget != pid::tNone && k->modTarget == et);
    }

    const int prog = proc.getCurrentProgram();
    if (prog != shownProgram)
    {
        shownProgram = prog;
        presetBox.setSelectedItemIndex (prog, juce::dontSendNotification);
    }

    const float l = proc.lfoMeter.load(), e = proc.envMeter.load();
    if (std::abs (l - shownLfo) > 0.005f || std::abs (e - shownEnv) > 0.005f)
    {
        shownLfo = l;
        shownEnv = e;
        repaint (rLfo.withHeight (24));
        repaint (rEnv.withHeight (24));
    }
}
