// SYNTHZILLA offline harness
//   SynthZillaTest <outDir> check            render every preset: no NaN, not silent, report peak / loudness
//   SynthZillaTest <outDir> levels           write <outDir>/levels.json: g_trim so that every preset peaks at -1 dBFS
//   SynthZillaTest <outDir> wav <substring>  render matching presets to WAV
//   SynthZillaTest <outDir> gui              GUI snapshots (PNG)
//   SynthZillaTest <outDir> params           <outDir>/params.json: every parameter (for tools/make_manual.py)
//   SynthZillaTest <outDir> units            unit checks (5 modules, LFO modulation, EQ, output filters, model change, FM 6-op)
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>

using namespace juce;
using namespace params;

struct Stats { float peak = 0; int nans = 0; double loud = -200; };

static bool isPoly (const Preset& p)
{
    for (auto c : { "Pad", "Chord", "Keys", "Strings", "Piano", "Organ", "Stab", "Brass", "Vox" }) if (p.category == c) return true;
    return false;
}

static void setP (SynthZillaProcessor& p, int i, float v) { auto* pr = p.apvts.getParameter (def (i).id); pr->setValueNotifyingHost (pr->convertTo0to1 (v)); }

using NoteList = std::vector<std::tuple<double, int, int, double>>;   // start s, note, velocity, duration s

static Stats render (SynthZillaProcessor& proc, const Preset* pr, double seconds, AudioBuffer<float>* keep = nullptr, int fixedNote = -1, const NoteList* notes = nullptr)
{
    const double sr = 48000.0; const int bs = 256;
    proc.setPlayConfigDetails (0, 2, sr, bs);
    proc.prepareToPlay (sr, bs);
    const int total = (int) (seconds * sr);
    AudioBuffer<float> buf (2, bs);
    if (keep) keep->setSize (2, total);
    MidiBuffer midi;
    Stats st;
    std::vector<std::tuple<double, int, int, double>> ev;
    if (notes != nullptr) ev = *notes;
    else if (fixedNote >= 0) ev.push_back ({ 0.05, fixedNote, 100, seconds * 0.6 });
    else
    {
        const bool poly = pr && isPoly (*pr);
        const bool low = pr && pr->category == "Bass";
        const int root = low ? 36 : (pr && (pr->category == "Lead" || pr->category == "Sequence" || pr->category == "Pluck") ? 60 : 57);
        if (poly) { for (int n : { 0, 3, 7, 10 }) ev.push_back ({ 0.1, root + n, 100, 1.2 }); for (int n : { 0, 5, 8 }) ev.push_back ({ 1.6, root + 2 + n, 100, 0.9 }); }
        else { const int seq[] = { 0, 0, 12, 7, 10, 0, 3, 5 }; for (int i = 0; i < 8; ++i) ev.push_back ({ 0.1 + i * 0.25, root + seq[i], i % 3 == 0 ? 120 : 95, 0.2 }); ev.push_back ({ 2.2, root, 100, 1.0 }); }
    }
    std::vector<double> win; double wacc = 0; int wn = 0;
    proc.resetPrePeak();
    for (int pos = 0; pos + bs <= total; pos += bs)
    {
        midi.clear();
        for (auto& [t, n, v, d] : ev)
        {
            const int on = (int) (t * sr), off = (int) ((t + d) * sr);
            if (on >= pos && on < pos + bs) midi.addEvent (MidiMessage::noteOn (1, n, (uint8) v), on - pos);
            if (off >= pos && off < pos + bs) midi.addEvent (MidiMessage::noteOff (1, n), off - pos);
        }
        proc.processBlock (buf, midi);
        for (int i = 0; i < bs; ++i)
        {
            const float l = buf.getSample (0, i), r = buf.getSample (1, i);
            if (! std::isfinite (l) || ! std::isfinite (r)) ++st.nans;
            st.peak = jmax (st.peak, std::abs (l), std::abs (r));
            const double e = 0.5 * ((double) l * l + (double) r * r);
            wacc += e; if (++wn == 9600) { win.push_back (wacc / wn); wacc = 0; wn = 0; }
            if (keep) { keep->setSample (0, pos + i, l); keep->setSample (1, pos + i, r); }
        }
    }
    std::sort (win.begin(), win.end(), std::greater<double>());
    double s = 0; const int k = jmax (1, (int) (win.size() * 0.4));
    for (int i = 0; i < k && i < (int) win.size(); ++i) s += win[(size_t) i];
    st.loud = 10.0 * std::log10 (jmax (1.0e-12, s / k));
    return st;
}

static int findPreset (const String& n)
{
    const auto& all = PresetLibrary::get().all();
    for (int i = 0; i < (int) all.size(); ++i) if (all[(size_t) i].name == n) return i;
    return -1;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const File outDir (argc > 1 ? File::getCurrentWorkingDirectory().getChildFile (argv[1]) : File::getCurrentWorkingDirectory());
    outDir.createDirectory();
    const String mode = argc > 2 ? argv[2] : "check";
    const auto& all = PresetLibrary::get().all();
    std::printf ("%d presets, %d parameters\n", (int) all.size(), numParams());
    bool ok = true;
    auto expect = [&] (bool c, const juce::String& what) { std::printf ("%s %s\n", c ? "ok  " : "FAIL", what.toRawUTF8()); ok &= c; };

    if (mode == "check" || mode == "levels")
    {
        DynamicObject::Ptr levels = new DynamicObject();
        float minP = 10, maxP = -200;
        for (int i = 0; i < (int) all.size(); ++i)
        {
            SynthZillaProcessor proc;
            proc.loadPreset (i);
            const auto st = render (proc, &all[(size_t) i], 4.0);
            const float pkDb = Decibels::gainToDecibels (proc.prePeak(), -200.0f);
            const float trimNow = proc.apvts.getRawParameterValue ("g_trim")->load();
            if (st.nans > 0 || pkDb < -60.0f) { ok = false; std::printf ("FAIL %-34s nans=%d peak=%.1f\n", all[(size_t) i].name.toRawUTF8(), st.nans, pkDb); }
            minP = jmin (minP, pkDb); maxP = jmax (maxP, pkDb);
            if (mode == "levels") levels->setProperty (all[(size_t) i].name, std::round ((trimNow + (-1.0f - pkDb)) * 100.0f) / 100.0f);
            else std::printf ("%4d %-34s %-6s peak %6.2f dBFS  loud %6.1f dB\n", all[(size_t) i].year, all[(size_t) i].name.toRawUTF8(), all[(size_t) i].sections().toRawUTF8(), pkDb, st.loud);
        }
        std::printf ("peak range (pre-limiter): %.2f .. %.2f dBFS\n", minP, maxP);
        if (mode == "levels") outDir.getChildFile ("levels.json").replaceWithText (JSON::toString (var (levels.get())));
    }
    else if (mode == "units")
    {
        const auto& L = layout();
        {   // 5 modules: turning on C, D, E adds sound
            SynthZillaProcessor p; p.loadPreset (findPreset ("Acid Tracks"));
            const auto a = render (p, nullptr, 1.0, nullptr, 48);
            setP (p, L.gSlots, 5);
            for (int m = 2; m < 5; ++m) { setP (p, mod (m, mOn), 1); setP (p, mod (m, mModel), 0); }
            const auto b = render (p, nullptr, 1.0, nullptr, 48);
            expect (b.loud > a.loud + 1.0, "modules C-E add sound");
            expect (b.nans == 0, "5 modules finite");
        }
        {   // model change loads the signature patch of that instrument
            SynthZillaProcessor p; p.loadPreset (findPreset ("Acid Tracks"));
            const int dx = PresetLibrary::get().modelIndex ("Yamaha DX7");
            p.loadModelPatch (0, dx);
            expect ((int) p.apvts.getRawParameterValue ("A_engine")->load() == 1, "change synth -> DX7 loads FM engine");
            const auto st = render (p, nullptr, 1.0, nullptr, 60);
            expect (st.loud > -50 && st.nans == 0, "DX7 patch plays");
        }
        {   // LFO modulates a parameter
            SynthZillaProcessor p; p.loadPreset (findPreset ("Can You Feel It Pad"));
            const auto a = render (p, nullptr, 2.0, nullptr, 60);
            setP (p, L.lfoBase[0] + lTarget, (float) (mod (0, mLevel) + 1));
            setP (p, L.lfoBase[0] + lDepth, 1.0f); setP (p, L.lfoBase[0] + lRate, 4.0f); setP (p, L.lfoBase[0] + lShape, 4);
            const auto b = render (p, nullptr, 2.0, nullptr, 60);
            expect (std::abs (b.loud - a.loud) > 0.5, "LFO 1 -> A level changes the output");
        }
        {   // output filters + EQ
            SynthZillaProcessor p; p.loadPreset (findPreset ("Jump Brass"));
            const auto a = render (p, nullptr, 1.5, nullptr, 60);
            setP (p, L.ofLpOn, 1); setP (p, L.ofLpCut, 200);
            const auto b = render (p, nullptr, 1.5, nullptr, 60);
            expect (b.loud < a.loud - 3, "output LP at 200 Hz attenuates");
            setP (p, L.ofLpOn, 0);
            setP (p, L.eqBase[2] + eqGain, 15); setP (p, L.eqBase[2] + eqFreq, 1500); setP (p, L.eqBase[2] + eqQ, 0.5f);
            const auto c = render (p, nullptr, 1.5, nullptr, 60);
            expect (c.loud > a.loud + 2, "EQ band 3 +15 dB boosts");
        }
        {   // FM 6-op: every algorithm renders
            SynthZillaProcessor p; p.loadPreset (findPreset ("E.PIANO 1"));
            bool fine = true;
            for (int a = 0; a < 32; ++a) { setP (p, mod (0, mAlg), (float) a); const auto st = render (p, nullptr, 0.4, nullptr, 60); fine &= st.nans == 0 && st.loud > -70; }
            expect (fine, "all 32 FM algorithms produce sound");
        }
        {   // 303 filter self-oscillation stays finite, RAT drive raises level
            SynthZillaProcessor p; p.loadPreset (findPreset ("Acid Tracks"));
            setP (p, mod (0, mRes), 1.0f);
            const auto a = render (p, nullptr, 1.0, nullptr, 36);
            expect (a.nans == 0, "303 filter at max resonance is stable");
            setP (p, mod (0, mRes), 0.5f);
            const auto b = render (p, nullptr, 1.0, nullptr, 36);
            setP (p, mod (0, mDist), 2); setP (p, mod (0, mDGain), 0.8f);
            const auto c = render (p, nullptr, 1.0, nullptr, 36);
            expect (c.nans == 0 && c.loud > b.loud, "RAT pedal drives the 303");
        }
        {   // mono / legato: overlapping notes must not click (oscillator phases keep running on a retrigger)
            for (int engine : { 0, 5 })
                for (int vm : { 1, 2 })
                {
                    SynthZillaProcessor p; p.loadPreset (findPreset ("Acid Tracks"));
                    setP (p, mod (0, mEngine), (float) engine); setP (p, mod (0, mO1Wave), 3); setP (p, mod (0, mO1Lvl), 1);
                    setP (p, mod (0, mO2Lvl), 0); setP (p, mod (0, mSub), 0); setP (p, mod (0, mNoise), 0); setP (p, mod (0, mUni), 1);
                    setP (p, mod (0, mDrift), 0); setP (p, mod (0, mFlt), 7); setP (p, mod (0, mDist), 0); setP (p, mod (0, mDlyMix), 0);
                    setP (p, mod (0, mAccent), 0); setP (p, mod (0, mPenv), 0); setP (p, mod (0, mAddM), 0); setP (p, mod (0, mBright), 0);
                    setP (p, mod (0, mAA), 5); setP (p, mod (0, mAS), 1); setP (p, mod (0, mAR), 30);
                    setP (p, L.gMode, (float) vm); setP (p, L.gGlide, 0); setP (p, L.gChoMode, 0); setP (p, L.gRevMix, 0); setP (p, L.gDrive, 0);
                    const NoteList ev { { 0.10, 48, 100, 0.50 }, { 0.45, 52, 100, 0.40 }, { 0.80, 55, 100, 0.50 }, { 1.25, 48, 100, 0.40 } };
                    AudioBuffer<float> out;
                    const auto st = render (p, nullptr, 1.8, &out, -1, &ev);
                    float worst = 0;   // largest jump of the 2nd difference, relative to the peak
                    for (int i = 2; i < out.getNumSamples(); ++i)
                        worst = jmax (worst, std::abs (out.getSample (0, i) - 2.0f * out.getSample (0, i - 1) + out.getSample (0, i - 2)));
                    worst /= jmax (1.0e-6f, st.peak);
                    std::printf ("  mono click test engine %d mode %d: max 2nd diff / peak = %.4f\n", engine, vm, worst);
                    expect (st.nans == 0 && st.peak > 0.01f && worst < 0.05f, "no click on overlapping notes (engine " + String (engine) + ", voice mode " + String (vm) + ")");
                }
        }
        {   // module delay
            SynthZillaProcessor p; p.loadPreset (findPreset ("Popcorn Pluck"));
            const auto a = render (p, nullptr, 2.0, nullptr, 60);
            setP (p, mod (0, mDlyMix), 0.8f); setP (p, mod (0, mDlyFb), 0.6f);
            const auto b = render (p, nullptr, 2.0, nullptr, 60);
            expect (b.loud > a.loud, "module delay adds echoes");
        }
    }
    else if (mode == "params")
    {
        Array<var> arr;
        for (int i = 0; i < numParams(); ++i)
        {
            const auto& d = def (i);
            auto o = std::make_unique<DynamicObject>();
            o->setProperty ("id", d.id); o->setProperty ("name", d.name); o->setProperty ("kind", (int) d.kind);
            o->setProperty ("min", d.mn); o->setProperty ("max", d.mx); o->setProperty ("def", d.def);
            o->setProperty ("unit", d.unit); o->setProperty ("choices", d.choices.joinIntoString ("|"));
            o->setProperty ("module", d.module);
            arr.add (var (o.release()));
        }
        outDir.getChildFile ("params.json").replaceWithText (JSON::toString (var (arr)));
        std::printf ("%d parameters\n", numParams());
    }
    else if (mode == "wav")
    {
        const String q = argc > 3 ? String (argv[3]).toLowerCase() : String();
        for (int i = 0; i < (int) all.size(); ++i)
        {
            if (q.isNotEmpty() && ! all[(size_t) i].name.toLowerCase().contains (q)) continue;
            SynthZillaProcessor proc; proc.loadPreset (i);
            AudioBuffer<float> b;
            const auto st = render (proc, &all[(size_t) i], 4.0, &b);
            WavAudioFormat wav;
            auto f = outDir.getChildFile (File::createLegalFileName (all[(size_t) i].name) + ".wav");
            f.deleteFile();
            std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), 48000, 2, 24, {}, 0));
            if (w) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
            std::printf ("%s  peak %.2f\n", f.getFileName().toRawUTF8(), st.peak);
        }
    }
    else if (mode == "gui")
    {
        const StringArray names { "Organ 2", "Acid Tracks", "E.PIANO 1", "What The", "Final Countdown Brass", "PPG Wave Pad", "Jump Brass",
                                  "Big Fun Lately Bass", "Doomsday Wobble", "Blade Runner Brass", "Just Want Another Chance", "Planet Rock ORCH5", "Vox Humana",
                                  "Powers of Ten Lead", "Sandstorm Lead", "On the Run", "Time After Time Pad", "Hungry Like the Wolf Arp" };
        auto snap = [&] (AudioProcessorEditor& ed, const String& file)
        {
            MessageManager::getInstance()->runDispatchLoopUntil (80);
            auto img = ed.createComponentSnapshot (ed.getLocalBounds(), true, 1.0f);
            PNGImageFormat png; FileOutputStream os (outDir.getChildFile (file)); os.setPosition (0); os.truncate(); png.writeImageToStream (img, os);
        };
        for (auto& n : names)
        {
            const int idx = findPreset (n);
            if (idx < 0) { std::printf ("missing %s\n", n.toRawUTF8()); continue; }
            SynthZillaProcessor proc; proc.loadPreset (idx);
            std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
            ed->setSize (1400, 900);
            snap (*ed, "gui_" + File::createLegalFileName (n).replaceCharacter (' ', '_') + ".png");
        }
        for (auto sz : { Point<int> (1920, 1080), Point<int> (2560, 1600) })
        {   // free aspect ratio: a full-screen window is filled edge to edge
            SynthZillaProcessor proc; proc.loadPreset (findPreset ("Final Countdown Brass"));
            std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
            ed->setSize (sz.x, sz.y);
            snap (*ed, "gui_fullscreen_" + String (sz.x) + "x" + String (sz.y) + ".png");
        }
        {   // modern FM view, 5 modules, browser, EQ overlay
            SynthZillaProcessor proc; proc.loadPreset (findPreset ("E.PIANO 1"));
            setP (proc, mod (0, mFmModern), 1);
            setP (proc, layout().gSlots, 5);
            std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
            ed->setSize (1400, 900);
            snap (*ed, "gui_FM_modern.png");
            std::function<Component* (Component*, const String&)> find = [&] (Component* c, const String& id) -> Component* {
                if (c->getComponentID() == id) return c;
                for (auto* ch : c->getChildren()) if (auto* r = find (ch, id)) return r;
                return nullptr; };
            auto click = [&] (const String& id) { if (auto* b = dynamic_cast<Button*> (find (ed.get(), id))) b->triggerClick(); else std::printf ("no %s\n", id.toRawUTF8()); };
            // every point of the < > preset buttons must reach the button (no label on top of them)
            ed->setVisible (true);
            for (auto* id : { "prev", "next", "browse" })
                if (auto* b = find (ed.get(), id))
                {
                    const auto r = ed->getLocalArea (b, b->getLocalBounds()).reduced (2);
                    for (int y = r.getY(); y < r.getBottom(); y += 2)
                        for (int x = r.getX(); x < r.getRight(); x += 2)
                        {
                            auto* hit = ed->getComponentAt (x, y);
                            if (hit != b && ! b->isParentOf (hit))
                            {
                                ok = false;
                                std::printf ("FAIL click on '%s' at %d,%d goes to %s\n", id, x, y, hit != nullptr ? typeid (*hit).name() : "nothing");
                                y = r.getBottom(); break;
                            }
                        }
                }
                else { ok = false; std::printf ("no %s\n", id); }
            click ("browse");
            snap (*ed, "gui_browser.png");
            click ("browse");
            click ("eqExpand");
            snap (*ed, "gui_eq.png");
        }
    }
    std::printf (ok ? "OK\n" : "FAILURES\n");
    return ok ? 0 : 1;
}
