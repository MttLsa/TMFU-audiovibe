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
        const int folderNo = File (e.path).getParentDirectory().getFileName().substring (0, 2).getIntValue();
        const int folderType = folderNo <= 5 ? folderNo - 1 : folderNo;   // (module 6 = CUSTOM has no factory folder)
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
        check (knnOk >= (int) db->entries.size() * 3 / 4, "audio type classifier (k-NN, leave-one-out, no names): " + String (knnOk) + "/" + String ((int) db->entries.size()));
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
    // resampling (44.1 -> 48 kHz) keeps the loop cyclic: same length/timing, no silence at the end
    {
        for (auto& e : db->entries)
        {
            if (e.info.type != lb::kick) continue;
            auto l = LoopLibrary::load (e, 48000.0);
            const int N = l->numSamples();
            const double expectN = e.info.numSamples * 48000.0 / e.info.sampleRate;
            // energy of the last 4 ms vs the same span of the original file
            AudioFormatManager fm2; fm2.registerBasicFormats();
            std::unique_ptr<AudioFormatReader> rd (fm2.createReaderFor (File (e.path)));
            const int Ns = (int) rd->lengthInSamples, span = (int) (0.004 * rd->sampleRate);
            AudioBuffer<float> sb (1, span); rd->read (&sb, 0, span, Ns - span, true, false);
            double tail = 0, head = 0;
            for (int i = N - (int) (0.004 * 48000); i < N; ++i) tail += l->audio.getSample (0, i) * l->audio.getSample (0, i);
            for (int i = 0; i < span; ++i) head += sb.getSample (0, i) * sb.getSample (0, i);
            tail /= 0.004 * 48000; head /= span;
            const bool ok = std::abs (N - expectN) < 1.0 && std::abs (tail - head) < 0.1 * head + 1e-9;
            if (! ok) std::cout << "    resample: " << e.name << " N " << N << " vs " << expectN << " tail " << tail << " before " << head << "\n";
            check (ok, "resampled loop has no silent end: " + e.name);
        }
    }
    // kick detection decides KICK vs BOTTOM; vocals / fx only in their modules
    {
        int kickOk = 0, kickN = 0, noKickOk = 0, noKickN = 0, voxfx = 0, voxfxN = 0;
        for (auto& e : db->entries)
        {
            const String n = e.name.toLowerCase();
            const bool noKick = analysis::nameSaysNoKick (e.name);
            if (n.contains ("kick") && ! noKick) { ++kickN; kickOk += e.info.type == lb::kick; }
            if (noKick) { ++noKickN; noKickOk += e.info.type == lb::bottom; if (e.info.type != lb::bottom) std::cout << "    no-kick loop in " << lb::moduleName (e.info.type) << ": " << e.name << "\n"; }
            if (n.contains ("vocal") || n.contains ("fx")) { ++voxfxN; voxfx += e.info.type == (n.contains ("vocal") ? lb::vocals : lb::fx); }
            if (e.info.type != lb::vocals && e.info.type != lb::fx) check (! n.contains ("vocal") && ! n.contains (" fx"), "no vocals / fx outside VOCALS / FX: " + e.name);
            std::cout << "    kick score " << String (e.info.kickScore, 2) << "  " << e.name << "\n";
        }
        check (kickOk == kickN && kickN > 0, "kick loops -> KICK: " + String (kickOk) + "/" + String (kickN));
        check (noKickOk == noKickN && noKickN > 0, "'no kick' loops -> BOTTOM: " + String (noKickOk) + "/" + String (noKickN));
        check (voxfx == voxfxN && voxfxN > 0, "vocals / fx -> their modules: " + String (voxfx) + "/" + String (voxfxN));
        // a groove named "bottom" but with a loud kick goes to KICK (the audio wins); a "kick" file without kick goes to BOTTOM
        AudioFormatManager fm; fm.registerBasicFormats();
        File kickF, tomF;
        for (auto& e : db->entries) { if (e.name.startsWith ("House Kick")) kickF = File (e.path); if (e.name.startsWith ("House Bottom")) tomF = File (e.path); }
        auto readAll = [&fm] (const File& f) { std::unique_ptr<AudioFormatReader> r (fm.createReaderFor (f)); AudioBuffer<float> b (1, (int) r->lengthInSamples); r->read (&b, 0, b.getNumSamples(), 0, true, false); return b; };
        auto kb = readAll (kickF), tb = readAll (tomF);
        AudioBuffer<float> mix (1, jmin (kb.getNumSamples(), tb.getNumSamples()));
        mix.copyFrom (0, 0, kb, 0, 0, mix.getNumSamples()); mix.addFrom (0, 0, tb, 0, 0, mix.getNumSamples());
        mix.applyGain (0.6f);
        auto writeWav = [] (const File& f, const AudioBuffer<float>& b)
        {
            f.deleteFile(); WavAudioFormat wav;
            std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), 44100.0, 1, 16, {}, 0));
            w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        };
        auto f1 = outDir.getChildFile ("Groove Bottom Full 124bpm.wav"); writeWav (f1, mix);
        auto f2 = outDir.getChildFile ("Kick Loop Toms 124bpm.wav"); writeWav (f2, tb);
        const auto r1 = analysis::analyse (fm, f1, f1.getFullPathName()), r2 = analysis::analyse (fm, f2, f2.getFullPathName());
        check (r1.type == lb::kick, "'bottom' groove with a loud kick -> KICK (score " + String (r1.kickScore, 2) + ")");
        check (r2.type == lb::bottom, "'kick' file without a kick -> BOTTOM (score " + String (r2.kickScore, 2) + ")");
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
        if (n == lb::numModules - 1) break;
    }
    int loaded = 0; for (int m = 0; m < lb::numModules; ++m) loaded += proc->currentLoop (m) != nullptr;
    check (loaded == lb::numModules - 1 && proc->currentLoop (lb::custom) == nullptr, "random loop in every module, CUSTOM empty: " + String (loaded));
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
        setP (*proc, lb::pid (m, "dirt"), 0.5f); setP (*proc, lb::pid (m, "dmix"), 0.3f);
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

    // ---- audio quality checks (solo one module)
    {
        auto resetAll = [&]
        {
            for (int m = 0; m < lb::numModules; ++m)
            {
                setP (*proc, lb::pid (m, "solo"), 0); setP (*proc, lb::pid (m, "slice"), 0); setP (*proc, lb::pid (m, "dirt"), 0);
                setP (*proc, lb::pid (m, "dmix"), 0); setP (*proc, lb::pid (m, "rmix"), 0); setP (*proc, lb::pid (m, "cut"), 20000.0f);
                setP (*proc, lb::pid (m, "pitch"), 0); setP (*proc, lb::pid (m, "steps"), 0); setP (*proc, lb::pid (m, "atk"), 0); setP (*proc, lb::pid (m, "rel"), 0);
                setP (*proc, lb::pid (m, "lfo1depth"), 0); setP (*proc, lb::pid (m, "lfo2depth"), 0); setP (*proc, lb::pid (m, "lfo3depth"), 0);
            }
            setP (*proc, "swingOn", 0); setP (*proc, "brOn", 0); setP (*proc, "mHp", 20.0f); setP (*proc, "mBp", 0.0f); setP (*proc, "mLp", 20000.0f);
            setP (*proc, "deck", 0); setP (*proc, "scaleOn", 0); setP (*proc, "raw", 0);
        };
        auto renderBuf = [&] (double secs)
        {
            const File f = outDir.getChildFile ("q_tmp.wav");
            render (*proc, ph, secs, f);
            AudioFormatManager fm; fm.registerBasicFormats();
            std::unique_ptr<AudioFormatReader> r (fm.createReaderFor (f));
            AudioBuffer<float> b (1, (int) r->lengthInSamples); r->read (&b, 0, b.getNumSamples(), 0, true, false);
            return b;
        };
        // largest sample-to-sample jump relative to the signal's RMS (clicks show up as big jumps)
        auto clickiness = [] (const AudioBuffer<float>& b, int from)
        {
            const float* d = b.getReadPointer (0);
            double rms = 0; float mx = 0;
            for (int i = from + 1; i < b.getNumSamples(); ++i) { rms += d[i] * d[i]; mx = jmax (mx, std::abs (d[i] - d[i - 1])); }
            rms = std::sqrt (rms / (b.getNumSamples() - from));
            return (float) (mx / (rms + 1e-9));
        };
        resetAll();
        // 1) swing on drums: off-beat 1/16 slices move later by swing * 1/12 beat (60 % -> 0.05 beat), the rest stays put
        {
            setP (*proc, lb::pid (lb::top, "solo"), 1);
            {
                auto dbx = proc->library->db();
                for (int i = 0; i < (int) dbx->entries.size(); ++i) if (dbx->entries[(size_t) i].name.startsWith ("Techno Top")) proc->selectLoop (lb::top, i);
                for (int i = 0; i < 40 && ! proc->currentLoop (lb::top)->name.startsWith ("Techno Top"); ++i) pump (50);
                render (*proc, ph, 0.5);
            }
            auto onsetsOf = [] (const AudioBuffer<float>& b)
            {
                return analysis::detectOnsets (b.getReadPointer (0), b.getNumSamples(), 48000.0);
            };
            auto dry = renderBuf (4.0);
            setP (*proc, "swingOn", 1); setP (*proc, "swing", 0.6f); setP (*proc, "swingGrid", 1);
            auto wet = renderBuf (4.0);
            setP (*proc, "swingOn", 0);
            const auto od = onsetsOf (dry), ow = onsetsOf (wet);
            const double beatSec = 60.0 / ph.bpm, total = 4.0;
            int moved = 0, steady = 0, checked = 0;
            for (auto o : od)
            {
                const double t = o * total, beat = t / beatSec;
                const double frac = std::fmod (beat, 0.5);
                if (beat < 0.5) continue;
                double best = 1e9;
                for (auto w2 : ow) best = std::min (best, std::abs (w2 * total - (t + (std::abs (frac - 0.25) < 0.03 ? 0.05 * beatSec : 0.0))));
                ++checked;
                if (best < 0.006) { if (std::abs (frac - 0.25) < 0.03) ++moved; else ++steady; }
            }
            std::cout << "    top loop: " << proc->currentLoop (lb::top)->name << "\n    dry:";
            for (auto o : od) std::cout << " " << String (o * total / beatSec, 3);
            std::cout << "\n    wet:";
            for (auto o : ow) std::cout << " " << String (o * total / beatSec, 3);
            std::cout << "\n";
            // moving the SWING knob while playing must not break the drum slices
            {
                const double srr = 48000.0; const int bs = 512;
                AudioBuffer<float> buf (2 + 2 * lb::numModules, bs), outB (1, (int) (srr * 4.0));
                MidiBuffer midi;
                // COMPLEX (default): moving the knob must not make the stretcher re-seek
                {
                    const int s0c = proc->reseeks (lb::top);
                    setP (*proc, "swingOn", 1);
                    for (int pos = 0; pos + bs <= outB.getNumSamples(); pos += bs)
                    {
                        ph.ppq = 8.0 + pos / srr * ph.bpm / 60.0;
                        setP (*proc, "swing", 0.5f + 0.5f * (float) std::sin (pos / srr * 3.0));
                        proc->processBlock (buf, midi);
                    }
                    setP (*proc, "swingOn", 0);
                    const int rs = proc->reseeks (lb::top) - s0c;
                    std::cout << "  info COMPLEX re-seeks while the knob moved: " << rs << "\n";
                    check (rs == 0, "COMPLEX warp: no re-seeks (jumps) while the SWING knob moves");
                }
                setP (*proc, lb::pid (lb::top, "warp"), 1);   // BEATS engine
                setP (*proc, "swingOn", 1);
                const int restarts0 = proc->sliceRestarts (lb::top);
                for (int pos = 0; pos + bs <= outB.getNumSamples(); pos += bs)
                {
                    ph.ppq = pos / srr * ph.bpm / 60.0;
                    setP (*proc, "swing", 0.5f + 0.5f * (float) std::sin (pos / srr * 3.0));   // knob moving all the time
                    proc->processBlock (buf, midi);
                    outB.copyFrom (0, pos, buf, 0, 0, bs);
                }
                setP (*proc, "swingOn", 0);
                const int restarts = proc->sliceRestarts (lb::top) - restarts0;
                std::cout << "  info drum slices restarted over themselves while the knob moved: " << restarts << "\n";
                check (restarts == 0, "BEATS warp: no slice restarts while the SWING knob moves");
                setP (*proc, lb::pid (lb::top, "warp"), 0);
                const float moving = clickiness (outB, 4800);
                setP (*proc, "swingOn", 1); setP (*proc, "swing", 0.6f);
                const float fixed = clickiness (renderBuf (4.0), 4800);
                setP (*proc, "swingOn", 0);
                std::cout << "  info drum click index: swing fixed " << fixed << ", knob moving " << moving << "\n";
                check (moving < fixed * 1.5f, "moving the SWING knob while playing stays clean");
            }
            std::cout << "  info swing: " << moved << " off-beat slices moved, " << steady << " on-beat slices in place, of " << checked << "\n";
            check (checked > 8 && moved + steady >= checked * 8 / 10, "drum swing moves the off-beat slices by the groove amount");
            setP (*proc, lb::pid (lb::top, "solo"), 0);
        }
        // 1b) KICK module: every factory kick loop at the project tempo (124 BPM): no holes cut into the sound
        {
            setP (*proc, lb::pid (lb::kick, "solo"), 1);
            auto dbx = proc->library->db();
            int worst = 0; String worstName;
            for (int i = 0; i < (int) dbx->entries.size(); ++i)
            {
                if (dbx->entries[(size_t) i].info.type != lb::kick) continue;
                proc->selectLoop (lb::kick, i);
                for (int w = 0; w < 40 && proc->currentLoop (lb::kick)->path != dbx->entries[(size_t) i].path; ++w) pump (50);
                render (*proc, ph, 0.3);
                const int rs0 = proc->reseeks (lb::kick);
                auto b = renderBuf (4.0);
                std::cout << "      reseeks during render: " << proc->reseeks (lb::kick) - rs0 << "\n";
                outDir.getChildFile ("q_tmp.wav").copyFileTo (outDir.getChildFile ("kick_" + dbx->entries[(size_t) i].name + ".wav"));
                const float* d = b.getReadPointer (0);
                float pk = 0; for (int k = 0; k < b.getNumSamples(); ++k) pk = jmax (pk, std::abs (d[k]));
                // a "hole": >= 1 ms of near-silence right after audible sound (the sound was chopped, not decayed)
                int holes = 0, run = 0;
                for (int k = 4800; k < b.getNumSamples(); ++k)
                {
                    if (std::abs (d[k]) < pk * 0.002f) ++run;
                    else
                    {
                        if (run >= 48)
                        {
                            double e = 0; const int a0 = k - run - 240;
                            for (int j = jmax (0, a0); j < k - run; ++j) e += d[j] * d[j];
                            if (std::sqrt (e / 240.0) > pk * 0.05f) { ++holes; std::cout << "      hole at " << (k - run) << " len " << run << "\n"; }
                        }
                        run = 0;
                    }
                }
                std::cout << "    kick " << dbx->entries[(size_t) i].name << ": holes " << holes << "\n";
                if (holes > worst) { worst = holes; worstName = dbx->entries[(size_t) i].name; }
            }
            check (worst == 0, "KICK module: no chopped holes in any kick loop (worst " + String (worst) + " " + worstName + ")");
            setP (*proc, lb::pid (lb::kick, "solo"), 0);
        }
        // 2) beat repeat: no click when the repeats end and the loop comes back (Insert mode)
        {
            setP (*proc, lb::pid (lb::pad, "solo"), 1);
            const float base = clickiness (renderBuf (4.0), 48000);
            setP (*proc, "brOn", 1); setP (*proc, "brInterval", 3); setP (*proc, "brOffset", 2); setP (*proc, "brGrid", 2);
            setP (*proc, "brChance", 1.0f); setP (*proc, "brGate", 2); setP (*proc, "brMode", 1);
            const float br = clickiness (renderBuf (4.0), 48000);
            setP (*proc, "brOn", 0);
            std::cout << "  info click index pad: plain " << base << ", beat repeat " << br << "\n";
            check (br < base * 2.5f, "beat repeat: no clicks at the repeat boundaries");
            // 3) cutoff with a fast LFO: no zipper / glitches
            // reference: the filter standing still at the most open point the LFO reaches (800 Hz x 2^2 = 3.2 kHz)
            setP (*proc, lb::pid (lb::pad, "cut"), 3200.0f); setP (*proc, lb::pid (lb::pad, "res"), 0.4f);
            const float still = clickiness (renderBuf (3.0), 48000);
            setP (*proc, lb::pid (lb::pad, "cut"), 800.0f);
            setP (*proc, lb::pid (lb::pad, "lfo1target"), (float) lb::tCutoff); setP (*proc, lb::pid (lb::pad, "lfo1depth"), 0.5f); setP (*proc, lb::pid (lb::pad, "lfo1rate"), 4.0f);
            const float lfo = clickiness (renderBuf (3.0), 48000);
            std::cout << "  info click index filter: static " << still << ", fast LFO " << lfo << "\n";
            check (lfo < still * 1.5f, "cutoff LFO (fast 1/16 sweep): smooth, no zipper");
            setP (*proc, lb::pid (lb::pad, "lfo1depth"), 0.0f); setP (*proc, lb::pid (lb::pad, "cut"), 20000.0f); setP (*proc, lb::pid (lb::pad, "res"), 0.1f);
            setP (*proc, lb::pid (lb::pad, "solo"), 0);
        }
        resetAll();
    }

    // ---- CUSTOM: load a file from anywhere, LFO -> Tempo and LFO -> Loop
    {
        File src;
        for (auto& e : db->entries) if (e.info.type == lb::chords) { src = File (e.path); break; }
        auto tmp = outDir.getChildFile ("my custom sample.wav");
        src.copyFileTo (tmp);
        proc->loadCustomFile (tmp);
        for (int i = 0; i < 60 && proc->currentLoop (lb::custom) == nullptr; ++i) pump (50);
        check (proc->currentLoop (lb::custom) != nullptr, "CUSTOM loads a file from any folder");
        setP (*proc, lb::pid (8, "lfo3target"), (float) lb::tLoop); setP (*proc, lb::pid (8, "lfo3depth"), 1.0f); setP (*proc, lb::pid (8, "lfo3rate"), 7.0f);
        setP (*proc, lb::pid (1, "lfo2target"), (float) lb::tTempo); setP (*proc, lb::pid (1, "lfo2depth"), 0.3f);
        setP (*proc, lb::pid (1, "lfo1target"), (float) lb::tLoop); setP (*proc, lb::pid (1, "lfo1depth"), 1.0f); setP (*proc, lb::pid (1, "lfo1rate"), 7.0f);
        for (int i = 0; i < 20; ++i) pump (100);   // pools get built and loaded
        st = render (*proc, ph, 6.0, outDir.getChildFile ("lfo_loop_tempo.wav"));
        check (st.nans == 0 && st.peak > 0.005f, "LFO -> Loop and LFO -> Tempo render");
        setP (*proc, lb::pid (8, "lfo3depth"), 0.0f); setP (*proc, lb::pid (1, "lfo2depth"), 0.0f); setP (*proc, lb::pid (1, "lfo1depth"), 0.0f);
    }

    // ---- print
    proc->setSnapshotName ("TestSnapshot");
    auto pr = proc->printModules();
    std::cout << "  " << pr.message << "\n";
    check (pr.files.size() == lb::numModules, "PRINT wrote one file per loaded module (incl. CUSTOM)");
    {
        auto ex = proc->exportOriginals();
        check (ex.files.size() == lb::numModules && ex.files[0].getFileName().contains ("(original)"), "EXPORT copied the original loops: " + (ex.files.isEmpty() ? String() : ex.files[0].getFileName()));
    }
    if (pr.files.size() > 0)
        check (pr.files[0].getFileName() == "Kick - TestSnapshot.wav", "PRINT file naming: " + pr.files[0].getFileName());

    // ---- GUI snapshots
    {
        setP (*proc, "deck", 0); setP (*proc, "scaleOn", 1); setP (*proc, "swingOn", 0);
        for (int m = 0; m < lb::numModules; ++m) { setP (*proc, lb::pid (m, "steps"), 0); setP (*proc, lb::pid (m, "pitch"), 0); }
        setP (*proc, lb::pid (6, "vola"), 1);
        VolEnvelope env { { 0.0f, volUnityY }, { 0.3f, 0.6f }, { 0.55f, 1.0f }, { 1.0f, volUnityY } };
        proc->setVolEnvelope (6, env);
        render (*proc, ph, 1.0);
        std::unique_ptr<AudioProcessorEditor> ed (proc->createEditor());
        ed->setSize (MainView::designW, MainView::designH);
        pump (600);
        if (auto* mv = dynamic_cast<MainView*> (ed->getChildComponent (0))) mv->selectModule (6);
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
