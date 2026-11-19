// TWINSYNTH offline harness
//   TwinsynthTest <outDir> check            render every preset, check NaN / silence / clipping, print stats
//   TwinsynthTest <outDir> levels           write tools/levels.json (g_trim calibration) into <outDir>/levels.json
//   TwinsynthTest <outDir> wav <substring>  render matching presets to WAV
//   TwinsynthTest <outDir> gui              GUI snapshots (PNG) of a few presets
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>

using namespace juce;

struct Stats { float peak = 0; int nans = 0; double loud = -200; double rmsAll = 0; };

static bool isPoly (const Preset& p)
{
    for (auto c : { "Pad", "Chord", "Keys", "Strings", "Piano", "Organ", "Stab", "Brass" }) if (p.category == c) return true;
    return false;
}

static Stats render (TwinsynthProcessor& proc, const Preset& pr, double seconds, AudioBuffer<float>* keep = nullptr)
{
    const double sr = 48000.0; const int bs = 256;
    proc.setPlayConfigDetails (0, 2, sr, bs);
    proc.prepareToPlay (sr, bs);
    const int total = (int) (seconds * sr);
    AudioBuffer<float> buf (2, bs);
    if (keep) keep->setSize (2, total);
    MidiBuffer midi;
    Stats st;
    const bool poly = isPoly (pr);
    const bool low = pr.category == "Bass";
    const int root = low ? 36 : (pr.category == "Lead" || pr.category == "Sequence" || pr.category == "Pluck" ? 60 : 57);
    // phrase: (time s, note, vel, dur s)
    std::vector<std::tuple<double, int, int, double>> ev;
    if (poly) { for (int n : { 0, 3, 7, 10 }) ev.push_back ({ 0.1, root + n, 100, 1.2 }); for (int n : { 0, 5, 8 }) ev.push_back ({ 1.6, root + 2 + n, 90, 0.9 }); }
    else { const int seq[] = { 0, 0, 12, 7, 10, 0, 3, 5 }; for (int i = 0; i < 8; ++i) ev.push_back ({ 0.1 + i * 0.25, root + seq[i], i % 3 == 0 ? 120 : 90, 0.2 }); ev.push_back ({ 2.2, root, 100, 1.0 }); }
    std::vector<double> win;
    double acc = 0, wacc = 0; int wn = 0;
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
            acc += e; wacc += e; ++wn;
            if (wn == 9600) { win.push_back (wacc / wn); wacc = 0; wn = 0; }
            if (keep) { keep->setSample (0, pos + i, l); keep->setSample (1, pos + i, r); }
        }
    }
    st.rmsAll = std::sqrt (acc / total);
    // loudness = mean of the loudest 40 % of the 200 ms windows
    std::sort (win.begin(), win.end(), std::greater<double>());
    double s = 0; const int k = jmax (1, (int) (win.size() * 0.4));
    for (int i = 0; i < k; ++i) s += win[(size_t) i];
    st.loud = 10.0 * std::log10 (jmax (1.0e-12, s / k));
    return st;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const File outDir (argc > 1 ? File::getCurrentWorkingDirectory().getChildFile (argv[1]) : File::getCurrentWorkingDirectory());
    outDir.createDirectory();
    const String mode = argc > 2 ? argv[2] : "check";
    const auto& all = PresetLibrary::get().all();
    std::printf ("%d presets\n", (int) all.size());
    bool ok = true;

    if (mode == "check" || mode == "levels")
    {
        DynamicObject::Ptr levels = new DynamicObject();
        double minL = 0, maxL = -200;
        for (int i = 0; i < (int) all.size(); ++i)
        {
            TwinsynthProcessor proc;
            proc.loadPreset (i);
            const auto st = render (proc, all[(size_t) i], 4.0);
            const float trimNow = proc.apvts.getRawParameterValue ("g_trim")->load();
            const bool silent = st.loud < -60.0;
            if (st.nans > 0 || silent) { ok = false; std::printf ("FAIL %-32s nans=%d loud=%.1f\n", all[(size_t) i].name.toRawUTF8(), st.nans, st.loud); }
            minL = jmin (minL, st.loud); maxL = jmax (maxL, st.loud);
            if (mode == "levels") levels->setProperty (all[(size_t) i].name, std::round ((trimNow + (-17.0 - st.loud)) * 10.0) / 10.0);
            else std::printf ("%-34s %-3s loud %6.1f dB  peak %.2f\n", all[(size_t) i].name.toRawUTF8(), all[(size_t) i].type.toRawUTF8(), st.loud, st.peak);
        }
        std::printf ("loudness range: %.1f .. %.1f dB\n", minL, maxL);
        if (mode == "levels") outDir.getChildFile ("levels.json").replaceWithText (JSON::toString (var (levels.get())));
    }
    else if (mode == "wav")
    {
        const String q = argc > 3 ? String (argv[3]).toLowerCase() : String();
        for (int i = 0; i < (int) all.size(); ++i)
        {
            if (q.isNotEmpty() && ! all[(size_t) i].name.toLowerCase().contains (q)) continue;
            TwinsynthProcessor proc; proc.loadPreset (i);
            AudioBuffer<float> b;
            const auto st = render (proc, all[(size_t) i], 4.0, &b);
            WavAudioFormat wav;
            auto f = outDir.getChildFile (File::createLegalFileName (all[(size_t) i].name) + ".wav");
            f.deleteFile();
            std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), 48000, 2, 24, {}, 0));
            if (w) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
            std::printf ("%s  loud %.1f peak %.2f\n", f.getFileName().toRawUTF8(), st.loud, st.peak);
        }
    }
    else if (mode == "gui")
    {
        const StringArray names { "Organ 2", "Acid Tracks", "E.PIANO 1", "What The", "Final Countdown Brass", "PPG Wave Pad", "Jump Brass",
                                  "Lately Bass", "Wobble Bass", "Virus Hypersaw", "Blade Runner Brass", "Just Want Another Chance", "ORCH5", "Vox Humana",
                                  "Powers of Ten Lead", "Hungry Bass", "Sandstorm Lead", "On the Run Sequence" };
        for (auto& n : names)
        {
            int idx = -1;
            for (int i = 0; i < (int) all.size(); ++i) if (all[(size_t) i].name == n) idx = i;
            if (idx < 0) { std::printf ("missing %s\n", n.toRawUTF8()); continue; }
            TwinsynthProcessor proc; proc.loadPreset (idx);
            std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
            ed->setSize (1360, 756);
            MessageManager::getInstance()->runDispatchLoopUntil (50);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            PNGImageFormat png;
            FileOutputStream os (outDir.getChildFile ("gui_" + File::createLegalFileName (n).replaceCharacter (' ', '_') + ".png"));
            os.setPosition (0); os.truncate();
            png.writeImageToStream (img, os);
        }
        // browser snapshot
        TwinsynthProcessor proc; proc.loadPreset (0);
        std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
        ed->setSize (1360, 756);
        if (auto* b = dynamic_cast<TextButton*> ([&]() -> Component* {
                std::function<Component* (Component*)> find = [&] (Component* c) -> Component* {
                    if (auto* tb = dynamic_cast<TextButton*> (c)) if (tb->getButtonText() == "BROWSER") return tb;
                    for (auto* ch : c->getChildren()) if (auto* r = find (ch)) return r;
                    return nullptr; };
                return find (ed.get()); }()))
            b->triggerClick();
        MessageManager::getInstance()->runDispatchLoopUntil (100);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        PNGImageFormat png;
        FileOutputStream os (outDir.getChildFile ("gui_browser.png"));
        os.setPosition (0); os.truncate();
        png.writeImageToStream (img, os);
    }
    std::printf (ok ? "OK\n" : "FAILURES\n");
    return ok ? 0 : 1;
}
