// MICROVIBE offline harness
//   MicrovibeTest <outDir> check            render every preset over a synthetic minimal groove: NaN / silence / peak / CPU
//   MicrovibeTest <outDir> wav [substring]  render matching presets to WAV (input groove included)
//   MicrovibeTest <outDir> gui              GUI snapshots (PNG)
//   MicrovibeTest <outDir> units            unit checks (tuning, euclid, modulation, state round trip)
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <chrono>

using namespace juce;

struct FakeHead : public AudioPlayHead
{
    double ppq = 0, bpm = 124; bool playing = true;
    Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setBpm (bpm); p.setPpqPosition (ppq); p.setIsPlaying (playing);
        p.setTimeSignature (TimeSignature { 4, 4 });
        return p;
    }
};

// synthetic minimal groove at 124 bpm: kick, off-beat hat, rim, small chord stab
static void groove (AudioBuffer<float>& b, int pos, double sr, double bpm)
{
    const double spb = 60.0 * sr / bpm;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double t = (pos + i) / spb;           // beats
        const double ph = t - std::floor (t);
        const double ts = ph * spb / sr;            // seconds since beat
        float x = (float) (std::sin (2 * MathConstants<double>::pi * (45 + 120 * std::exp (-ts * 40)) * ts) * std::exp (-ts * 9) * 0.7);
        const double h = t + 0.5 - std::floor (t + 0.5);
        const double hs = h * spb / sr;
        static Random r (1);
        x += (float) ((r.nextFloat() * 2 - 1) * std::exp (-hs * 60) * 0.12);
        const double s16 = std::fmod (t, 4.0);
        if (std::abs (s16 - 1.75) < 0.2) { const double st = (s16 - 1.75) * spb / sr; if (st > 0) x += (float) (std::sin (2 * MathConstants<double>::pi * 1700 * st) * std::exp (-st * 90) * 0.3); }
        if (s16 >= 2.5 && s16 < 3.0)
        {
            const double st = (s16 - 2.5) * spb / sr;
            for (double f : { 220.0, 261.6, 329.6 }) x += (float) (std::sin (2 * MathConstants<double>::pi * f * st) * std::exp (-st * 6) * 0.08);
        }
        b.setSample (0, i, x); b.setSample (1, i, x);
    }
}

struct Stats { float peak = 0; int nans = 0; double loud = -200; double cpu = 0; };

static Stats render (MicrovibeProcessor& proc, double seconds, AudioBuffer<float>* keep = nullptr, bool input = true)
{
    const double sr = 48000.0; const int bs = 256;
    FakeHead head;
    proc.setPlayHead (&head);
    proc.setPlayConfigDetails (2, 2, sr, bs);
    proc.prepareToPlay (sr, bs);
    const int total = (int) (seconds * sr);
    AudioBuffer<float> buf (2, bs);
    if (keep) keep->setSize (2, total);
    MidiBuffer midi;
    Stats st;
    std::vector<double> win; double wacc = 0; int wn = 0;
    double cpuSec = 0;
    for (int pos = 0; pos + bs <= total; pos += bs)
    {
        if (input) groove (buf, pos, sr, head.bpm); else buf.clear();
        head.ppq = pos / (60.0 * sr / head.bpm);
        midi.clear();
        const auto t0 = std::chrono::high_resolution_clock::now();
        proc.processBlock (buf, midi);
        cpuSec += std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
        for (int i = 0; i < bs; ++i)
        {
            const float l = buf.getSample (0, i), r = buf.getSample (1, i);
            if (! std::isfinite (l) || ! std::isfinite (r)) ++st.nans;
            st.peak = jmax (st.peak, std::abs (l), std::abs (r));
            const double e = 0.5 * ((double) l * l + (double) r * r);
            wacc += e; ++wn;
            if (wn == 9600) { win.push_back (wacc / wn); wacc = 0; wn = 0; }
            if (keep) { keep->setSample (0, pos + i, l); keep->setSample (1, pos + i, r); }
        }
    }
    std::sort (win.begin(), win.end(), std::greater<double>());
    double s = 0; const int k = jmax (1, (int) (win.size() * 0.4));
    for (int i = 0; i < k && i < (int) win.size(); ++i) s += win[(size_t) i];
    st.loud = 10.0 * std::log10 (jmax (1.0e-12, s / k));
    st.cpu = 100.0 * cpuSec / seconds;
    proc.setPlayHead (nullptr);
    return st;
}

static bool units()
{
    bool ok = true;
    auto check = [&] (bool c, const char* what) { std::printf ("%s %s\n", c ? "ok  " : "FAIL", what); ok &= c; };
    // euclid
    int hits = 0; for (int i = 0; i < 16; ++i) hits += mv::Lane::euclid (i, 16, 5, 0) ? 1 : 0;
    check (hits == 5 && mv::Lane::euclid (0, 16, 5, 0), "euclid 5/16");
    // tuning
    tuning::Tuning t; t.update (1, 0, false);   // C major
    check (std::abs (t.snap (1.2, 1.0f) - 2.0) < 1e-6, "major snap 1.2 -> 2");
    check (std::abs (t.snap (1.2, 0.0f) - 1.2) < 1e-6, "snap amount 0");
    tuning::Tuning q; q.update (8, 0, false);   // 24-EDO
    check (std::abs (q.snap (0.6, 1.0f) - 0.5) < 1e-6, "24-EDO quarter tone");
    juce::String err;
    const juce::String scl = "! test\nTest pelog\n 5\n!\n 150.0\n 300.0\n 3/2\n 800.0\n 2/1\n";
    tuning::Tuning u; check (u.loadUser (scl, {}, err), "scl parse");
    u.update (tuning::builtinNames().size() - 1, 0, false);
    check (std::abs (u.snap (1.4, 1.0f) - 1.5) < 1e-6, "scl snap 150 cents");
    // modulation: LFO 1 -> Glitch Probability
    {
        MicrovibeProcessor p;
        const int slot = p.assignModulation (prm::srcLfo1, prm::ids().glProb);
        check (slot >= 0, "assign modulation");
        render (p, 1.0);
        float mn = 1, mx = -1;
        for (int i = 0; i < 50; ++i)
        {
            AudioBuffer<float> b (2, 256); MidiBuffer m; b.clear();
            p.processBlock (b, m);
            const float v = p.engine.display.modNorm[(size_t) prm::ids().glProb].load();
            mn = jmin (mn, v); mx = jmax (mx, v);
        }
        check (mx - mn > 0.01f, "modulation moves the destination");
        // state round trip
        p.engine.shared.gate[2][1][5] = 1; p.engine.shared.value[2][1][5] = 0.123f;
        MemoryBlock mb; p.getStateInformation (mb);
        MicrovibeProcessor p2; p2.setStateInformation (mb.getData(), (int) mb.getSize());
        check (p2.engine.shared.gate[2][1][5].load() == 1 && std::abs (p2.engine.shared.value[2][1][5].load() - 0.123f) < 1e-3f, "state: lane grid");
        check ((int) p2.getParam (prm::ids().mxSrc[slot]) == prm::srcLfo1, "state: matrix");
    }
    return ok;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const File outDir (argc > 1 ? File::getCurrentWorkingDirectory().getChildFile (argv[1]) : File::getCurrentWorkingDirectory());
    outDir.createDirectory();
    const String mode = argc > 2 ? argv[2] : "check";
    const auto& all = PresetLibrary::get().all();
    std::printf ("%d presets, %d parameters, %d destinations\n", (int) all.size(), prm::count(), (int) prm::dests().size());
    bool ok = true;

    if (mode == "units") ok = units();
    else if (mode == "check")
    {
        double minL = 0, maxL = -200, maxCpu = 0;
        for (const auto& pr : all)
            for (const auto& nv : pr.values)
            {
                const int idx = prm::indexOf (nv.name.toString());
                bool good = idx >= 0;
                if (good && nv.value.isString())
                {
                    const auto s = nv.value.toString();
                    good = s.startsWithChar ('@') ? prm::destIndexOf (prm::indexOf (s.substring (1))) > 0
                                                  : prm::def (idx).choices.contains (s);
                }
                if (! good) { ok = false; std::printf ("FAIL %s: bad value %s = %s\n", pr.name.toRawUTF8(), nv.name.toString().toRawUTF8(), nv.value.toString().toRawUTF8()); }
            }
        for (int i = 0; i < (int) all.size(); ++i)
        {
            MicrovibeProcessor proc;
            proc.loadPreset (i);
            const auto st = render (proc, 8.0);
            const bool silent = st.loud < -50.0;
            const bool hot = st.peak > 1.0f;
            if (st.nans > 0 || silent || hot) ok = false;
            minL = jmin (minL, st.loud); maxL = jmax (maxL, st.loud); maxCpu = jmax (maxCpu, st.cpu);
            std::printf ("%s %-30s %-12s loud %6.1f dB  peak %.2f  cpu %4.1f%%%s\n", (st.nans || silent || hot) ? "FAIL" : "    ",
                         all[(size_t) i].name.toRawUTF8(), all[(size_t) i].category.toRawUTF8(), st.loud, st.peak, st.cpu,
                         st.nans ? " NaN" : "");
        }
        std::printf ("loudness range: %.1f .. %.1f dB, max cpu %.1f%% of one core\n", minL, maxL, maxCpu);
    }
    else if (mode == "wav")
    {
        const String q = argc > 3 ? String (argv[3]).toLowerCase() : String();
        for (int i = 0; i < (int) all.size(); ++i)
        {
            if (q.isNotEmpty() && ! all[(size_t) i].name.toLowerCase().contains (q)) continue;
            MicrovibeProcessor proc; proc.loadPreset (i);
            AudioBuffer<float> b;
            const auto st = render (proc, 8.0, &b);
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
        const StringArray tabs { "", "src:1", "src:2", "src:3", "mod:1", "mod:2", "mod:3", "mod:4", "mod:5", "fx:1", "fx:4", "fx:5" };
        for (auto& t : tabs)
        {
            MicrovibeProcessor proc; proc.loadPreset (jmin (3, (int) all.size() - 1));
            render (proc, 2.0);
            std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
            if (auto* me = dynamic_cast<MicrovibeEditor*> (ed.get())) me->showTab (t);
            MessageManager::getInstance()->runDispatchLoopUntil (80);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            PNGImageFormat png;
            FileOutputStream os (outDir.getChildFile ("gui_" + (t.isEmpty() ? String ("main") : t.replaceCharacter (':', '_')) + ".png"));
            os.setPosition (0); os.truncate();
            png.writeImageToStream (img, os);
        }
    }
    std::printf (ok ? "ALL OK\n" : "SOME CHECKS FAILED\n");
    return ok ? 0 : 1;
}
