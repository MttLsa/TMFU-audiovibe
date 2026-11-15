// Offline test harness: library scan + analysis accuracy, rendering through the processor, print.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/Scales.h"
#include <juce_audio_formats/juce_audio_formats.h>

using namespace juce;

static void setP (GroovulationProcessor& p, const String& id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    if (prm == nullptr) { std::cout << "missing param " << id << "\n"; return; }
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

struct FakePlayHead : AudioPlayHead
{
    double ppq = 0, bpm = 124; bool playing = true;
    Optional<PositionInfo> getPosition() const override
    {
        PositionInfo i; i.setBpm (bpm); i.setPpqPosition (ppq); i.setIsPlaying (playing); return i;
    }
};

static void pump (int ms)
{
    const auto end = Time::getMillisecondCounter() + (uint32) ms;
    while (Time::getMillisecondCounter() < end) MessageManager::getInstance()->runDispatchLoopUntil (20);
}

struct Stats { float peak = 0; int nans = 0; double rms = 0; };

static Stats render (GroovulationProcessor& p, FakePlayHead& ph, double seconds, const File& out = {})
{
    const double sr = 48000.0; const int bs = 512;
    AudioBuffer<float> all (2, (int) (seconds * sr));
    AudioBuffer<float> buf (2 + 2 * lb::numModules, bs);
    MidiBuffer midi;
    Stats st; double acc = 0;
    for (int pos = 0; pos + bs <= all.getNumSamples(); pos += bs)
    {
        ph.ppq = pos / sr * ph.bpm / 60.0;
        p.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < bs; ++i)
            {
                const float v = buf.getSample (c, i);
                if (! std::isfinite (v)) ++st.nans;
                st.peak = jmax (st.peak, std::abs (v)); acc += (double) v * v;
                all.setSample (c, pos + i, v);
            }
    }
    st.rms = std::sqrt (acc / (2.0 * all.getNumSamples()));
    if (out != File())
    {
        out.deleteFile();
        WavAudioFormat wav;
        std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (out), sr, 2, 24, {}, 0));
        if (w) w->writeFromAudioSampleBuffer (all, 0, all.getNumSamples());
    }
    return st;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    if (argc > 2 && String (argv[1]) == "--analyse")
    {
        AudioFormatManager fm; fm.registerBasicFormats();
        for (int a = 2; a < argc; ++a)
        {
            auto r = analysis::analyse (fm, File::getCurrentWorkingDirectory().getChildFile (argv[a]), argv[a]);
            std::cout << argv[a] << ": type " << r.type << " bpm " << r.bpm << " beats " << r.beats << " key " << analysis::keyName (r.keyRoot, r.minor)
                      << " conf " << r.keyConf << " tune " << r.tuneCents << " f0 " << r.f0 << " voiced " << r.voiced << "\n  chroma:";
            for (auto c : r.chroma) std::cout << " " << String (c, 3);
            std::cout << "\n  feat:"; for (auto f : r.features) std::cout << " " << String (f, 2);
            std::cout << "\n";
        }
        return 0;
    }
    const File outDir = File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "test_out");
    outDir.createDirectory();
    int failures = 0;
    auto check = [&failures] (bool ok, const String& what) { std::cout << (ok ? "  ok   " : "  FAIL ") << what << "\n"; if (! ok) ++failures; };

    auto proc = std::make_unique<GroovulationProcessor>();
    auto& lib = *proc->library;
    for (int i = 0; i < 600 && lib.isScanning(); ++i) pump (50);
    pump (300);
    auto db = lib.db();
    std::cout << "database: " << db->entries.size() << " loops\n";
    check (db->entries.size() >= 40, "factory loops scanned");

    // ---- analysis accuracy on the factory set (names hold the truth)
    int typeOk = 0, typeAudioOk = 0, keyOk = 0, keyN = 0, bpmOk = 0;
    for (auto& e : db->entries)
    {
        const int folderType = File (e.path).getParentDirectory().getFileName().substring (0, 2).getIntValue() - 1;
        typeOk += e.info.type == folderType;
        // audio-only classifier (no names)
        String noName = "x";
        ignoreUnused (noName);
        int nr; bool nm;
        const bool hasKey = analysis::keyFromName (e.name, nr, nm);
        const double nb = analysis::bpmFromName (e.name);
        bpmOk += std::abs (e.info.bpm - nb) < 0.6;
        if (hasKey && lb::isTonal (e.info.type))
        {
            ++keyN;
            // equal key or relative key (same pitch set) counts as correct for SCALE purposes
            const bool same = e.info.keyRoot == nr && e.info.minor == nm;
            const int relRoot = nm ? (nr + 3) % 12 : (nr + 9) % 12;
            const bool relative = e.info.keyRoot == relRoot && e.info.minor != nm;
            const bool bassRoot = e.info.type == 5 && e.info.keyRoot == nr;   // bass: the root is what SCALE needs
            keyOk += same || relative || bassRoot;
            std::cout << "    " << e.name.paddedRight (' ', 32) << " audio: " << analysis::keyName (e.info.keyRoot, e.info.minor)
                      << (same ? "  (exact)" : (relative ? "  (relative)" : "  (WRONG)")) << "  f0 " << analysis::noteName (e.info.f0)
                      << "  tune " << String (e.info.tuneCents, 1) << "c  family " << e.family << "\n";
        }
    }
    check (typeOk == (int) db->entries.size(), "type from folder/file names: " + String (typeOk) + "/" + String ((int) db->entries.size()));
    check (keyOk >= keyN * 8 / 10, "key detection (audio): " + String (keyOk) + "/" + String (keyN));
    check (bpmOk == (int) db->entries.size(), "bpm: " + String (bpmOk) + "/" + String ((int) db->entries.size()));

    // audio-only classification
    {
        AudioFormatManager fm; fm.registerBasicFormats();
        for (auto& e : db->entries)
        {
            auto r = analysis::analyse (fm, File (e.path), "untitled");
            typeAudioOk += r.type == e.info.type;
            if (false) std::cout << "    audio classifier: " << e.name << " -> " << lb::moduleName (r.type) << "\n";
        }
        std::cout << "  info rule-based audio classifier: " << typeAudioOk << "/" << (int) db->entries.size() << "\n";
        int knnOk = 0;
        for (auto& e : db->entries)
        {
            auto r = e.info; r.typeFromName = false;
            const int t = LoopLibrary::classifyByNeighbours (db->entries, r);
            knnOk += t == e.info.type;
            if (t != e.info.type) std::cout << "    k-NN (leave-one-out): " << e.name << " -> " << lb::moduleName (t) << "\n";
        }
        check (knnOk >= (int) db->entries.size() * 8 / 10, "audio type classifier (k-NN, leave-one-out, no names): " + String (knnOk) + "/" + String ((int) db->entries.size()));
        // tempo without the name: duration-based estimate
        int tempoOk = 0;
        for (auto& e : db->entries)
        {
            auto tmp = File::getSpecialLocation (File::tempDirectory).getChildFile ("noname_loop" + File (e.path).getFileExtension());
            File (e.path).copyFileTo (tmp);
            auto r = analysis::analyse (fm, tmp, "untitled");
            tempoOk += std::abs (r.bpm - e.info.bpm) < 0.6;
            if (std::abs (r.bpm - e.info.bpm) >= 0.6) std::cout << "    tempo w/o name: " << e.name << " -> " << r.bpm << "\n";
            tmp.deleteFile();
        }
        check (tempoOk >= (int) db->entries.size() * 8 / 10, "tempo without the name: " + String (tempoOk) + "/" + String ((int) db->entries.size()));
    }
    // key-from-name parser
    {
        int r; bool mi;
        check (analysis::keyFromName ("bass rocket league Gmin", r, mi) && r == 7 && mi, "name: 'bass rocket league Gmin' = G minor");
        check (analysis::keyFromName ("pad rocket league D", r, mi) && r == 2 && ! mi, "name: 'pad rocket league D' = D major");
        check (analysis::keyFromName ("Chords_F#m_120", r, mi) && r == 6 && mi, "name: 'Chords_F#m_120' = F# minor");
        check (analysis::keyFromName ("Stabs Deep 122bpm Ebmaj", r, mi) && r == 3 && ! mi, "name: Ebmaj");
        check (! analysis::keyFromName ("Kick Deep 4x4 124bpm", r, mi), "name without key");
    }
    // scale logic (examples from the spec)
    {
        std::array<float, 12> emaj {}; for (int s : { 4, 6, 8, 9, 11, 1, 3 }) emaj[(size_t) s] = 1.0f / 7; emaj[4] *= 2;
        std::array<float, 12> dmaj {}; for (int s : { 2, 4, 6, 7, 9, 11, 1 }) dmaj[(size_t) s] = 1.0f / 7; dmaj[2] *= 2;
        std::array<float, 12> amin {}; for (int s : { 9, 11, 0, 2, 4, 5, 7 }) amin[(size_t) s] = 1.0f / 7; amin[9] *= 2;
        check (scales::bestShift (emaj, 4, 0, 0) == -4, "C major scale: E major loop -> -4 st");
        check (scales::bestShift (dmaj, 2, 0, 0) == -2, "C major scale: D major loop -> -2 st");
        check (scales::bestShift (amin, 9, 0, 0) == 0, "C major scale: A minor loop -> 0 st");
    }

    // ---- render
    FakePlayHead ph;
    proc->setPlayHead (&ph);
    proc->setPlayConfigDetails (0, 2, 48000.0, 512);
    proc->prepareToPlay (48000.0, 512);
    for (int i = 0; i < 100; ++i)
    {
        pump (100);
        int n = 0; for (int m = 0; m < lb::numModules; ++m) n += proc->currentLoop (m) != nullptr && proc->currentLoop (m)->sampleRate == 48000.0;
        if (n == lb::numModules) break;
    }
    int loaded = 0; for (int m = 0; m < lb::numModules; ++m) loaded += proc->currentLoop (m) != nullptr;
    check (loaded == lb::numModules, "random loop in every module: " + String (loaded));
    for (int m = 0; m < lb::numModules; ++m) if (auto l = proc->currentLoop (m)) std::cout << "    " << lb::moduleName (m) << ": " << l->name << "\n";
    auto t0 = Time::getMillisecondCounterHiRes();
    auto st = render (*proc, ph, 8.0, outDir.getChildFile ("manual.wav"));
    const double ms = Time::getMillisecondCounterHiRes() - t0;
    std::cout << "  render 8 s: " << ms << " ms (" << String (8000.0 / ms, 1) << "x realtime), peak " << st.peak << " rms " << st.rms << "\n";
    check (st.nans == 0 && st.peak > 0.01f && st.peak < 8.0f, "MANUAL mode renders audio");

    setP (*proc, "scaleOn", 1); setP (*proc, "scaleRoot", 0); setP (*proc, "scaleType", 0);
    st = render (*proc, ph, 4.0, outDir.getChildFile ("scale_cmaj.wav"));
    for (int m = 5; m < 9; ++m) if (auto l = proc->currentLoop (m))
        std::cout << "    SCALE C maj: " << l->name << " (" << analysis::keyName (l->info.keyRoot, l->info.minor) << ") -> " << String (proc->appliedSemitones (m), 2) << " st\n";
    check (st.nans == 0 && st.peak > 0.01f, "SCALE mode renders audio");
    setP (*proc, "scaleOn", 0);
    setP (*proc, "raw", 1);
    st = render (*proc, ph, 4.0, outDir.getChildFile ("raw.wav"));
    check (st.nans == 0 && st.peak > 0.01f, "RAW mode renders audio");
    setP (*proc, "raw", 0);
    // every module feature at once
    for (int m = 0; m < lb::numModules; ++m)
    {
        setP (*proc, lb::pid (m, "slice"), 1); setP (*proc, lb::pid (m, "sstart"), 0.25f); setP (*proc, lb::pid (m, "send"), 0.75f);
        setP (*proc, lb::pid (m, "shaper"), 0.4f); setP (*proc, lb::pid (m, "dirt"), 0.5f); setP (*proc, lb::pid (m, "dmix"), 0.3f);
        setP (*proc, lb::pid (m, "rmix"), 0.3f); setP (*proc, lb::pid (m, "cut"), 3000.0f); setP (*proc, lb::pid (m, "pitch"), 3.5f);
        setP (*proc, lb::pid (m, "lfo1target"), (float) lb::tCutoff); setP (*proc, lb::pid (m, "lfo1depth"), 0.5f);
        setP (*proc, lb::pid (m, "steps"), 7); setP (*proc, lb::pid (m, "grid"), 5); setP (*proc, lb::pid (m, "atk"), 20.0f); setP (*proc, lb::pid (m, "rel"), 50.0f);
    }
    setP (*proc, "swingOn", 1); setP (*proc, "swing", 0.6f);
    setP (*proc, "brOn", 1); setP (*proc, "brInterval", 3); setP (*proc, "brChance", 1.0f); setP (*proc, "brGate", 4);
    setP (*proc, "mHp", 120.0f); setP (*proc, "mBp", 0.5f); setP (*proc, "mLp", 9000.0f);
    st = render (*proc, ph, 6.0, outDir.getChildFile ("all_features.wav"));
    check (st.nans == 0 && st.peak > 0.005f && st.peak < 8.0f, "all module features + swing + beat repeat + master cutoff");
    setP (*proc, "deck", 1);
    proc->deck.pendingAction = fx::Deck::stop;
    st = render (*proc, ph, 3.0, outDir.getChildFile ("tape_stop.wav"));
    check (st.nans == 0, "tape stop");
    setP (*proc, "deck", 2);
    proc->deck.pendingAction = fx::Deck::scratch;
    st = render (*proc, ph, 3.0, outDir.getChildFile ("vinyl_scratch.wav"));
    check (st.nans == 0, "vinyl scratch");

    // ---- print
    proc->setSnapshotName ("TestSnapshot");
    auto pr = proc->printModules();
    std::cout << "  " << pr.message << "\n";
    check (pr.files.size() == lb::numModules, "PRINT wrote one file per module");
    if (pr.files.size() > 0)
        check (pr.files[0].getFileName() == "Kick - TestSnapshot.wav", "PRINT file naming: " + pr.files[0].getFileName());

    // ---- GUI snapshots
    {
        setP (*proc, "deck", 0); setP (*proc, "scaleOn", 1); setP (*proc, "swingOn", 0);
        for (int m = 0; m < lb::numModules; ++m) { setP (*proc, lb::pid (m, "steps"), 0); setP (*proc, lb::pid (m, "pitch"), 0); }
        setP (*proc, lb::pid (5, "vola"), 1);
        VolEnvelope env { { 0.0f, volUnityY }, { 0.3f, 0.6f }, { 0.55f, 1.0f }, { 1.0f, volUnityY } };
        proc->setVolEnvelope (5, env);
        render (*proc, ph, 1.0);
        std::unique_ptr<AudioProcessorEditor> ed (proc->createEditor());
        ed->setSize (MainView::designW, MainView::designH);
        pump (600);
        if (auto* mv = dynamic_cast<MainView*> (ed->getChildComponent (0))) mv->selectModule (5);
        pump (400);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        PNGImageFormat png;
        { auto f = outDir.getChildFile ("gui_main.png"); f.deleteFile(); FileOutputStream os (f); png.writeImageToStream (img, os); }
        check (img.getWidth() == MainView::designW, "GUI snapshot");
        {
            auto logo = ed->createComponentSnapshot ({ 0, 0, 420, 64 }, true, 3.0f);
            auto f = outDir.getChildFile ("gui_logo.png"); f.deleteFile(); FileOutputStream os (f); png.writeImageToStream (logo, os);
        }
        for (int t = 1; t <= 2; ++t)
        {
            DeckView dv (*proc, t);
            dv.setSize (980, 650);
            proc->deck.pendingAction = fx::Deck::wobble;
            render (*proc, ph, 0.3);
            for (int i = 0; i < 20; ++i) pump (20);
            auto im = dv.createComponentSnapshot (dv.getLocalBounds(), true, 1.0f);
            auto f = outDir.getChildFile (t == 1 ? "gui_tape.png" : "gui_vinyl.png"); f.deleteFile(); FileOutputStream os (f); png.writeImageToStream (im, os);
        }
        ed.reset();
    }

    proc->setPlayHead (nullptr);
    proc.reset();
    std::cout << (failures == 0 ? "ALL TESTS PASSED\n" : String (failures) + " FAILURE(S)\n");
    return failures == 0 ? 0 : 1;
}
