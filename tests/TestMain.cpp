// Offline test harness: renders audio through the processor, checks presets / FX / Legacy import, snapshots the GUI.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/Presets.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <map>
#include <set>
#include <algorithm>

using namespace juce;

static void setP (TwinboxProcessor& p, const String& id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    jassert (prm != nullptr);
    if (prm) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

static void waitLibrary (TwinboxProcessor& p)
{
    for (int i = 0; i < 400 && (p.library.isLoading() || p.library.set (0)->size() == 0); ++i) Thread::sleep (10);
    Thread::sleep (50);
}

struct RenderStats { float peak = 0; int nans = 0; double rms = 0; };

static RenderStats render (TwinboxProcessor& p, double seconds, const File& out = {})
{
    const double sr = 48000.0; const int bs = 512;
    p.setPlayConfigDetails (0, 2, sr, bs);
    p.prepareToPlay (sr, bs);
    AudioBuffer<float> all (2, (int) (seconds * sr));
    AudioBuffer<float> buf (2, bs);
    MidiBuffer midi;
    RenderStats st; double acc = 0;
    for (int pos = 0; pos + bs <= all.getNumSamples(); pos += bs)
    {
        midi.clear();
        if (pos == 48000 * 2) { midi.addEvent (MidiMessage::noteOn (1, 36, (uint8) 100), 10); midi.addEvent (MidiMessage::noteOn (1, 54, (uint8) 100), 20); }
        p.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < bs; ++i)
            {
                const float v = buf.getSample (c, i);
                if (! std::isfinite (v)) ++st.nans;
                st.peak = jmax (st.peak, std::abs (v));
                acc += (double) v * v;
                all.setSample (c, pos + i, v);
            }
    }
    st.rms = std::sqrt (acc / (2.0 * all.getNumSamples()));
    if (out != File())
    {
        WavAudioFormat wav;
        out.deleteFile();
        std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (out), sr, 2, 24, {}, 0));
        if (w) w->writeFromAudioSampleBuffer (all, 0, all.getNumSamples());
    }
    return st;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const File outDir (argc > 1 ? argv[1] : ".");
    const File ens (argc > 2 ? argv[2] : "");
    bool ok = true;
    if (argc > 3 && String (argv[3]) == "levels")
    {
        // loudness of every preset, per section (RMS and peak of the section output, before the master)
        const auto& lib = PresetLibrary::get();
        for (int sec = 0; sec < 2; ++sec)
        {
            std::vector<double> rms, pk;
            for (size_t i = 0; i < lib.items (sec).size(); ++i)
            {
                TwinboxProcessor p; waitLibrary (p);
                setP (p, params::gRun, 1); setP (p, params::gXfade, sec == 0 ? 0.0f : 100.0f);
                if (sec == 0) PresetLibrary::applyA (p, lib.items (0)[i], 0, true, true, true);
                else          PresetLibrary::applyB (p, lib.items (1)[i], true, true, true);
                p.setPlayConfigDetails (0, 2, 48000, 512); p.prepareToPlay (48000, 512);
                AudioBuffer<float> buf (2, 512); MidiBuffer m; double acc = 0; float peak = 0; long cnt = 0;
                for (int b = 0; b < 48000 * 6 / 512; ++b)
                {
                    buf.clear(); p.processBlock (buf, m);
                    if (b < 48000 / 512) continue;                                     // skip the first second (cross-fade settle)
                    for (int c = 0; c < 2; ++c) for (int k = 0; k < 512; ++k) { const float v = buf.getSample (c, k); acc += v * v; peak = jmax (peak, std::abs (v)); ++cnt; }
                }
                rms.push_back (10.0 * std::log10 (acc / cnt + 1e-12)); pk.push_back (20.0 * std::log10 (peak + 1e-9));
            }
            auto sorted = [] (std::vector<double> v) { std::sort (v.begin(), v.end()); return v; };
            const auto r = sorted (rms), q = sorted (pk);
            int over = 0; for (auto x : pk) if (x > -1.0) ++over;
            std::printf ("%s  rms median %.1f dB (p10 %.1f, p90 %.1f)   peak median %.1f dB, max %.1f dB, presets above -1 dBFS: %d/%d\n",
                         sec ? "B" : "A", r[r.size() / 2], r[r.size() / 10], r[r.size() * 9 / 10], q[q.size() / 2], q.back(), over, (int) q.size());
        }
        return 0;
    }
    if (argc > 5 && String (argv[3]) == "calibrate")
    {
        // For every Section B preset: limit the level increase that each FX (Delay, xFx) adds over the dry
        // signal to +2.5 dB (RMS), by lowering that FX's return gain / mix. Writes the corrected preset file.
        auto root = JSON::parse (File (argv[4]));
        auto* arr = root.getProperty ("presets", var()).getArray();
        if (arr == nullptr) { std::printf ("bad preset file\n"); return 1; }
        const float limitDb = 2.5f;
        auto measure = [] (const var& pv, std::function<void (TwinboxProcessor&)> tweak)
        {
            TwinboxProcessor p;
            waitLibrary (p);
            PresetLibrary::Item it { pv.getProperty ("name", "").toString(), "", (int) pv.getProperty ("bpm", 124), pv };
            PresetLibrary::applyB (p, it, true, true, true);
            setP (p, params::gRun, 1); setP (p, params::gXfade, 100);
            tweak (p);
            return render (p, 4.0).rms;
        };
        int changed = 0;
        for (auto& pv : *arr)
        {
            auto* prm = pv.getProperty ("params", var()).getDynamicObject();
            if (prm == nullptr) continue;
            auto get = [prm] (const char* id, float def) { return prm->hasProperty (id) ? (float) prm->getProperty (id) : def; };
            const double dry = measure (pv, [] (TwinboxProcessor& p) { setP (p, params::bFxMix, 0); });
            String log;
            struct Fx { const char* id; bool isMix; std::function<void (TwinboxProcessor&)> isolate; };
            const Fx fxs[] = {
                { params::bDlyGain, false, [] (TwinboxProcessor& p) { setP (p, params::bXGain, -60); } },
                { params::bXGain,   false, [] (TwinboxProcessor& p) { setP (p, params::bDlyGain, -60); } },
            };
            for (auto& fx : fxs)
            {
                for (int iter = 0; iter < 10; ++iter)
                {
                    const double wet = measure (pv, fx.isolate);
                    const double inc = 20.0 * std::log10 (jmax (1e-9, wet) / jmax (1e-9, dry));
                    if (inc <= limitDb) { log << " " << fx.id << " +" << String (inc, 1) << "dB"; break; }
                    if (fx.isMix) prm->setProperty (fx.id, jmax (0.0f, get (fx.id, 0) * 0.7f - 1.0f));
                    else          prm->setProperty (fx.id, jmax (-60.0f, get (fx.id, String (fx.id) == String (params::bDlyGain) ? -6.0f : 0.0f) - (float) (inc - limitDb) - 1.0f));
                    ++changed;
                }
            }
            std::printf ("%-24s%s\n", pv.getProperty ("name", "").toString().toRawUTF8(), log.toRawUTF8());
        }
        File (argv[5]).replaceWithText (JSON::toString (root, true));
        std::printf ("calibrated, %d adjustments\n", changed);
        return 0;
    }
    if (argc > 3 && String (argv[3]) == "fuzz")
    {
        Random rnd (12345);
        for (int round = 0; round < 40; ++round)
        {
            TwinboxProcessor p;
            waitLibrary (p);
            const double srs[] { 22050, 44100, 48000, 96000, 192000 };
            const double sr = srs[rnd.nextInt (5)]; const int bs = 1 + rnd.nextInt (2048);
            p.setPlayConfigDetails (0, 2, sr, bs);
            p.prepareToPlay (sr, bs);
            setP (p, params::gRun, 1);
            AudioBuffer<float> buf (2, bs); MidiBuffer midi;
            for (int blk = 0; blk < 300; ++blk)
            {
                for (auto* prm : p.getParameters())
                    if (rnd.nextFloat() < 0.3f) prm->setValue (rnd.nextFloat());
                midi.clear();
                midi.addEvent (MidiMessage::noteOn (1, 36 + rnd.nextInt (20), (uint8) (1 + rnd.nextInt (126))), rnd.nextInt (bs));
                p.processBlock (buf, midi);
            }
            std::printf ("fuzz round %d sr %.0f bs %d ok\n", round, sr, bs);
        }
        return 0;
    }
    auto report = [&ok] (const String& name, bool pass, const String& info)
    {
        std::printf ("%-34s %s  %s\n", name.toRawUTF8(), pass ? "OK  " : "FAIL", info.toRawUTF8());
        ok = ok && pass;
    };

    // ---------------------------------------------------------------- mix / filter cases
    {
        struct Case { const char* name; float xfade; float cutoff; int mode; float res; };
        const Case cases[] = { { "A_only", 0, 20000, 2, 0 }, { "B_only", 100, 20000, 2, 0 }, { "mix50", 50, 20000, 2, 0 },
                               { "lp_800_res70", 50, 800, 2, 70 }, { "hp_2k", 50, 2000, 0, 30 }, { "bp_1k_res100", 50, 1000, 1, 100 } };
        for (auto& c : cases)
        {
            TwinboxProcessor p;
            waitLibrary (p);
            setP (p, params::gRun, 1); setP (p, params::gXfade, c.xfade); setP (p, params::gCutoff, c.cutoff);
            setP (p, params::gMode, (float) c.mode); setP (p, params::gRes, c.res);
            setP (p, params::aFill, 1);
            auto st = render (p, 6.0, outDir.getChildFile (String ("render_") + c.name + ".wav"));
            report (String ("render ") + c.name, st.nans == 0 && st.peak > 0.01f,
                    "peak " + String (Decibels::gainToDecibels (st.peak), 1) + " dBFS");
        }
    }

    // ---------------------------------------------------------------- every xFx mode + FX on
    {
        TwinboxProcessor p;
        waitLibrary (p);
        setP (p, params::gRun, 1); setP (p, params::gXfade, 100);
        setP (p, params::bFxMix, 70); setP (p, params::bDirt, 30);
        setP (p, params::bNorm, 1); setP (p, params::bEqAmp, 6); setP (p, params::bFltTrk, 40); setP (p, params::bFltCut, 3000);
        setP (p, params::bRemix, 37); setP (p, params::bRemixRate, 1); setP (p, params::bChainOn, 1);
        setP (p, params::b (params::kick, params::lv::shp), 60); setP (p, params::b (params::snare, params::lv::flam), 50);
        int bad = 0; String info;
        for (int m = 0; m < params::xfxModeNames().size(); ++m)
        {
            setP (p, params::bXMode, (float) m);
            auto st = render (p, 1.5);
            if (st.nans > 0 || st.peak > 8.0f || st.peak < 0.001f) { ++bad; info << params::xfxModeNames()[m] << "(" << st.peak << ") "; }
        }
        report ("xFx all 23 modes", bad == 0, bad ? info : String ("all modes finite and audible"));
    }

    // ---------------------------------------------------------------- DIRT must be gentle; XY motion record/playback
    {
        auto renderDirt = [] (float dirt, AudioBuffer<float>& out)
        {
            TwinboxProcessor p;
            waitLibrary (p);
            setP (p, params::gRun, 1); setP (p, params::gXfade, 100); setP (p, params::bDirt, dirt);
            const int bs = 512; p.setPlayConfigDetails (0, 2, 48000, bs); p.prepareToPlay (48000, bs);
            out.setSize (2, 48000 * 3); AudioBuffer<float> buf (2, bs); MidiBuffer m;
            for (int pos = 0; pos + bs <= out.getNumSamples(); pos += bs)
            {
                p.processBlock (buf, m);
                for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, buf, c, 0, bs);
            }
        };
        AudioBuffer<float> clean, clean2, d5, d100;
        renderDirt (0, clean); renderDirt (0, clean2); renderDirt (5, d5); renderDirt (100, d100);
        auto diff = [] (const AudioBuffer<float>& a, const AudioBuffer<float>& b2)
        {
            double e = 0, d = 0;
            for (int i = 0; i < a.getNumSamples(); ++i) { e += a.getSample (0, i) * a.getSample (0, i); const double x = b2.getSample (0, i) - a.getSample (0, i); d += x * x; }
            return 10.0 * std::log10 (jmax (1e-12, d / jmax (1e-12, e)));
        };
        const double base = diff (clean, clean2);
        const double diffDb = diff (clean, d5);
        std::printf ("   dirt: baseline clean-vs-clean %.1f dB, clean-vs-5%% %.1f dB\n", base, diffDb);
        const float pk100 = d100.getMagnitude (0, d100.getNumSamples());
        report ("dirt 5% is gentle", diffDb < -30.0, "change vs clean " + String (diffDb, 1) + " dB; dirt 100% peak " + String (Decibels::gainToDecibels (pk100), 1) + " dBFS");

        TwinboxProcessor p;
        waitLibrary (p);
        setP (p, params::gRun, 1); setP (p, params::bXMode, 10); setP (p, params::bXLen, 16); setP (p, params::bXRec, 1);
        const int bs = 256; p.setPlayConfigDetails (0, 2, 48000, bs); p.prepareToPlay (48000, bs);
        AudioBuffer<float> buf (2, bs); MidiBuffer m;
        const int barBlocks = (int) (48000.0 * 60.0 / 124.0 * 4.0 / bs);   // one bar at 124 BPM
        for (int i = 0; i < barBlocks; ++i)          // record a ramp 0 -> 1 over one bar
        {
            p.lime.xyTouch ((float) i / (float) barBlocks, 1.0f - (float) i / (float) barBlocks, true);
            p.processBlock (buf, m);
        }
        p.lime.xyTouch (1.0f, 0.0f, false);
        setP (p, params::bXRec, 0); setP (p, params::bXPlay, 1);
        float minX = 1, maxX = 0; bool playing = true;
        for (int i = 0; i < barBlocks; ++i)
        {
            p.processBlock (buf, m);
            playing = playing && p.lime.xyPlayingBack();
            minX = jmin (minX, p.lime.xyCurrentX()); maxX = jmax (maxX, p.lime.xyCurrentX());
        }
        report ("xFx XY motion rec/play", playing && minX < 0.15f && maxX > 0.85f,
                "playback X range " + String (minX, 2) + " .. " + String (maxX, 2));
    }

    // ---------------------------------------------------------------- presets
    {
        const auto& lib = PresetLibrary::get();
        int bad = 0; float maxPeak = 0; float secPeak[2] {}; double secRms[2] {}; int secN[2] {};
        for (int sec = 0; sec < 2; ++sec)
            for (size_t i = 0; i < lib.items (sec).size(); i += 3)   // every 3rd preset (keeps the test fast)
            {
                TwinboxProcessor p;
                waitLibrary (p);
                setP (p, params::gRun, 1); setP (p, params::gXfade, sec == 0 ? 0.0f : 100.0f);
                if (sec == 0) PresetLibrary::applyA (p, lib.items (0)[i], 0, true, true, true);
                else          PresetLibrary::applyB (p, lib.items (1)[i], true, true, true);
                auto st = render (p, 2.0);
                maxPeak = jmax (maxPeak, st.peak); secPeak[sec] = jmax (secPeak[sec], st.peak); secRms[sec] += st.rms; ++secN[sec];
                if (st.nans > 0 || st.peak < 0.01f) { ++bad; std::printf ("   preset %s/%s peak %f nans %d\n", sec ? "B" : "A", lib.items (sec)[i].name.toRawUTF8(), st.peak, st.nans); }
            }
        report ("presets A+B (" + String ((int) (lib.items (0).size() + lib.items (1).size())) + ")", bad == 0,
                "A max " + String (Decibels::gainToDecibels (secPeak[0]), 1) + " dBFS avg rms " + String (Decibels::gainToDecibels ((float) (secRms[0] / jmax (1, secN[0]))), 1)
                + " | B max " + String (Decibels::gainToDecibels (secPeak[1]), 1) + " dBFS avg rms " + String (Decibels::gainToDecibels ((float) (secRms[1] / jmax (1, secN[1]))), 1));
        // render one demo per genre for listening
        for (int sec = 0; sec < 2; ++sec)
            for (size_t i = 0; i < lib.items (sec).size(); i += 16)
            {
                TwinboxProcessor p;
                waitLibrary (p);
                setP (p, params::gRun, 1); setP (p, params::gXfade, sec == 0 ? 0.0f : 100.0f);
                const auto& it = lib.items (sec)[i];
                if (sec == 0) PresetLibrary::applyA (p, it, 0, true, true, true);
                else          PresetLibrary::applyB (p, it, true, true, true);
                render (p, 8.0, outDir.getChildFile (String ("demo_") + (sec ? "B_" : "A_") + it.genre.replace (" ", "") + ".wav"));
            }
    }

    // ---------------------------------------------------------------- B presets: every FX gain at -60 dB / off, knobs reach 0 dB
    {
        int bad = 0;
        for (auto& it : PresetLibrary::get().items (1))
            if (auto* prm = it.data.getProperty ("params", var()).getDynamicObject())
            {
                for (auto* id : { params::bDlyGain, params::bXGain })
                    if ((float) prm->getProperty (id) > -59.9f) ++bad;
                for (auto* id : { params::bDirt, params::bEqAmp })
                    if ((float) prm->getProperty (id) > 0.001f) ++bad;
            }
        TwinboxProcessor p;
        auto* d = p.apvts.getParameter (params::bDlyGain); auto* x = p.apvts.getParameter (params::bXGain);
        auto def = [] (juce::RangedAudioParameter* q) { return q->convertFrom0to1 (q->getDefaultValue()); };
        const bool range = std::abs (d->getNormalisableRange().end) < 1.0e-6f && std::abs (x->getNormalisableRange().end) < 1.0e-6f
                        && std::abs (def (d) + 60.0f) < 0.01f && std::abs (def (x) + 60.0f) < 0.01f;
        report ("B preset FX gains at -60 dB", bad == 0 && range, String (bad) + " FX values above minimum; delay/xFx default -60 dB: " + (range ? "yes" : "no"));
    }

    // ---------------------------------------------------------------- B simple presets: 8 different grooves per genre, Tie/Pod velocity excursion
    {
        std::map<String, std::set<String>> grooves; int flatVel = 0, presets = 0;
        for (auto& it : PresetLibrary::get().items (1))
        {
            auto* pats = it.data.getProperty ("patterns", var()).getArray();
            if (pats == nullptr || pats->isEmpty()) continue;
            const auto& a = (*pats)[0];
            if (it.level == "Simple")
            {
                String key;
                if (auto* tr = a.getProperty ("trig", var()).getArray()) for (auto& t : *tr) key << t.toString() << "|";
                for (auto* ln : { "tie", "pod" })
                    if (auto* v = a.getProperty (ln, var()).getArray()) for (auto& x : *v) key << ((float) x > 0.001f ? "x" : "-");
                grooves[it.genre].insert (key);
            }
            // velocity excursion on the notes of Tie / Pod
            float lo = 1.0f, hi = 0.0f;
            for (auto* ln : { "tieVel", "podVel" })
                if (auto* v = a.getProperty (ln, var()).getArray())
                    for (int i = 0; i < v->size(); ++i)
                        if ((float) (*v)[i] < 0.999f) { lo = jmin (lo, (float) (*v)[i]); hi = jmax (hi, (float) (*v)[i]); }
            ++presets;
            if (hi > 0.0f && hi - lo < 0.15f) ++flatVel;
        }
        int minDistinct = 99; String info;
        for (auto& g : grooves) { minDistinct = jmin (minDistinct, (int) g.second.size()); info << g.first << " " << (int) g.second.size() << "  "; }
        report ("B simple presets: distinct grooves", minDistinct == 8, info);
        report ("B Tie/Pod velocity excursion", flatVel <= presets / 10, String (flatVel) + " of " + String (presets) + " presets with flat velocity");
    }

    // ---------------------------------------------------------------- Tie / Pod step velocity reaches the voice (also with VEL = wav)
    {
        TwinboxProcessor p;
        waitLibrary (p);
        auto playPeak = [&] (float vel, int velMode)
        {
            auto& P = p.lime.pattern (0);
            P.clear();
            P.value[LimeEngine::laneTie][0].store (0.6f);
            P.vel[LimeEngine::laneTie][0].store (vel);
            setP (p, params::bPattern, 0);
            setP (p, params::b (params::tie, params::lv::vel), (float) velMode);
            p.setPlayConfigDetails (0, 2, 44100, 512); p.prepareToPlay (44100, 512);
            setP (p, params::gXfade, 100); setP (p, params::bFxMix, 0);
            for (int v : { params::kick, params::snare, params::hat, params::pod }) setP (p, params::b (v, params::lv::on), 0);
            setP (p, params::gRun, 1);
            AudioBuffer<float> buf (2, 512); MidiBuffer m; float pk = 0.0f;
            for (int b2 = 0; b2 < 20; ++b2) { p.processBlock (buf, m); pk = jmax (pk, buf.getMagnitude (0, 512)); }
            setP (p, params::gRun, 0);
            return pk;
        };
        const float loud = playPeak (1.0f, 1), soft = playPeak (0.4f, 1), loud0 = playPeak (1.0f, 0), soft0 = playPeak (0.4f, 0);
        p.lime.pattern (0).vel[LimeEngine::lanePod][5].store (0.37f);
        MemoryBlock mb; p.getStateInformation (mb);
        TwinboxProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
        const bool stateOk = std::abs (q.lime.pattern (0).vel[LimeEngine::lanePod][5].load() - 0.37f) < 0.01f;
        report ("Tie/Pod step velocity", loud > 0.01f && soft < loud * 0.8f && soft0 < loud0 * 0.8f && stateOk,
                "VEL=wav peak vel 1.0: " + String (loud, 3) + ", vel 0.4: " + String (soft, 3) + "; VEL=--: " + String (loud0, 3) + " / " + String (soft0, 3)
                + ", state: " + (stateOk ? "ok" : "lost"));
    }

    // ---------------------------------------------------------------- LFOs: rate (ms) and sync (beat), any parameter of the section
    {
        auto targetIndex = [] (int sec, const String& id) { const auto& l = params::lfoTargets (sec); for (int i = 0; i < (int) l.size(); ++i) if (l[(size_t) i].id == id) return i + 1; return 0; };
        auto run = [&] (int sec, const String& id, bool sync, float msOrDiv, int shape, int blocks, std::vector<float>& out)
        {
            TwinboxProcessor p; waitLibrary (p);
            setP (p, params::gTempo, 120.0f);
            setP (p, params::lfoId (sec, 0, "target"), (float) targetIndex (sec, id));
            setP (p, params::lfoId (sec, 0, "sync"), sync ? 1.0f : 0.0f);
            setP (p, params::lfoId (sec, 0, sync ? "div" : "ms"), msOrDiv);
            setP (p, params::lfoId (sec, 0, "shape"), (float) shape);
            setP (p, params::lfoId (sec, 0, "depth"), 100.0f);
            p.setPlayConfigDetails (0, 2, 48000, 480); p.prepareToPlay (48000, 480);    // 10 ms blocks
            if (sync) setP (p, params::gRun, 1);
            AudioBuffer<float> buf (2, 480); MidiBuffer m;
            for (int i = 0; i < blocks; ++i) { p.processBlock (buf, m); out.push_back (p.mod.get (id)->load()); }
            const float raw = p.apvts.getRawParameterValue (id)->load();
            out.push_back (raw);                                   // last element = unmodulated value
        };
        // A: channel 1 Osc Freq, sine, 200 ms period (rate in ms): 20 blocks of 10 ms = one cycle
        std::vector<float> a; run (0, params::a (0, params::mt::oscFreq), false, 200.0f, 0, 40, a);
        const float rawA = a.back(); a.pop_back();
        const auto mmA = std::minmax_element (a.begin(), a.end());
        int signChangesA = 0; for (size_t i = 1; i < a.size(); ++i) if ((a[i] > rawA) != (a[i - 1] > rawA)) ++signChangesA;
        const bool okA = *mmA.first < rawA * 0.8f && *mmA.second > rawA * 1.2f && signChangesA >= 3 && signChangesA <= 5;
        // B: Kick Gain, square synced to 1/4 at 120 BPM: flips every 250 ms = 25 blocks
        std::vector<float> b; const int quarter = 5; run (1, params::b (params::kick, params::lv::gain), true, (float) quarter, 4, 100, b);
        b.pop_back();
        int flips = 0; std::vector<int> at;
        for (size_t i = 1; i < b.size(); ++i) if (std::abs (b[i] - b[i - 1]) > 1.0f) { ++flips; at.push_back ((int) i); }
        bool spacing = flips >= 3;
        for (size_t i = 1; i < at.size(); ++i) spacing = spacing && std::abs ((at[i] - at[i - 1]) - 25) <= 1;
        // the knob (APVTS) value is not changed by the LFO
        TwinboxProcessor q;
        const bool uiUntouched = std::abs (q.apvts.getRawParameterValue (params::b (params::kick, params::lv::gain))->load()) < 1.0e-6f;
        report ("LFO rate (ms) + sync (beat)", okA && spacing && uiUntouched,
                "A Osc Freq " + String (*mmA.first, 1) + " .. " + String (*mmA.second, 1) + " Hz around " + String (rawA, 1) + " (" + String (signChangesA) + " crossings / 400 ms at 200 ms); "
                + "B Kick Gain square 1/4: " + String (flips) + " flips every " + (at.size() > 1 ? String (at[1] - at[0]) : String ("?")) + " blocks (25 expected)");
        // LFO 7 of 8 modulates only when the LFO count reaches 7 (default 3)
        {
            TwinboxProcessor p; waitLibrary (p);
            const String id = params::b (params::snare, params::lv::tune);
            setP (p, params::lfoId (1, 6, "target"), (float) targetIndex (1, id));
            setP (p, params::lfoId (1, 6, "sync"), 0.0f); setP (p, params::lfoId (1, 6, "ms"), 100.0f);
            setP (p, params::lfoId (1, 6, "shape"), 4.0f); setP (p, params::lfoId (1, 6, "depth"), 100.0f);
            p.setPlayConfigDetails (0, 2, 48000, 480); p.prepareToPlay (48000, 480);
            AudioBuffer<float> buf (2, 480); MidiBuffer m;
            auto spread = [&]
            {
                float lo = 1e9f, hi = -1e9f;
                for (int i = 0; i < 30; ++i) { p.processBlock (buf, m); const float v = p.mod.get (id)->load(); lo = jmin (lo, v); hi = jmax (hi, v); }
                return hi - lo;
            };
            const float off = spread();
            setP (p, params::lfoCountId (1), 8.0f);
            const float on = spread();
            const bool def3 = std::abs (p.apvts.getParameter (params::lfoCountId (0))->convertFrom0to1 (p.apvts.getParameter (params::lfoCountId (0))->getDefaultValue()) - 3.0f) < 0.01f;
            report ("8 LFOs (count)", off < 1.0e-4f && on > 10.0f && def3 && params::numLfos == 8,
                    "LFO 7 with count 3: range " + String (off, 3) + " st; count 8: range " + String (on, 1) + " st; default count 3: " + (def3 ? "yes" : "no"));
        }
        // every parameter of each section is a target
        int nA = (int) params::lfoTargets (0).size(), nB = (int) params::lfoTargets (1).size();
        report ("LFO targets", nA >= 200 && nB >= 150, "Section A: " + String (nA) + " parameters, Section B: " + String (nB) + " parameters");
    }

    // ---------------------------------------------------------------- Hat filter type LP / BP / HP, Sat on Hat and Pod
    {
        auto hit = [] (int voice, int target, std::function<void (TwinboxProcessor&)> cfg) -> std::pair<double, double>
        {
            TwinboxProcessor p; waitLibrary (p);
            for (int v = 0; v < params::numLimeVoices; ++v) setP (p, params::b (v, params::lv::on), v == voice ? 1.0f : 0.0f);
            setP (p, params::gXfade, 100); setP (p, params::bFxMix, 0);
            cfg (p);
            p.setPlayConfigDetails (0, 2, 48000, 512); p.prepareToPlay (48000, 512);
            p.lime.audition (target, 1.0f);
            AudioBuffer<float> buf (2, 512); MidiBuffer m; double e = 0, hf = 0; float prev = 0;
            for (int blk = 0; blk < 40; ++blk)
            {
                buf.clear(); p.processBlock (buf, m);
                for (int i = 0; i < 512; ++i) { const float x = buf.getSample (0, i); e += x * x; hf += (x - prev) * (x - prev); prev = x; }
            }
            return { std::sqrt (e), hf / jmax (1.0e-12, e) };      // energy, brightness (high-frequency ratio)
        };
        using namespace params;
        auto hatMode = [&] (int mode, float hz) { return hit (hat, LimeEngine::tCHat, [=] (TwinboxProcessor& p) { setP (p, b (hat, lv::ft), (float) mode); setP (p, b (hat, lv::cut), hz); }); };
        const auto lp = hatMode (0, 600.0f), bp = hatMode (1, 3000.0f), hp = hatMode (2, 6000.0f), def = hatMode (2, 20.0f);
        const bool filtOk = lp.second < bp.second && bp.second < hp.second && def.first > 0.0;
        auto satOf = [&] (int voice, int target, float sat) { return hit (voice, target, [=] (TwinboxProcessor& p) { setP (p, b (voice, lv::sat), sat); setP (p, b (voice, lv::gain), 0.0f); }); };
        const auto h0 = satOf (hat, LimeEngine::tCHat, 0), h1 = satOf (hat, LimeEngine::tCHat, 80), p0 = satOf (pod, LimeEngine::tPod, 0), p1 = satOf (pod, LimeEngine::tPod, 80);
        const bool satOk = h1.first > h0.first * 1.05 && p1.first > p0.first * 1.05;
        report ("Hat filter LP/BP/HP + Sat on Hat/Pod", filtOk && satOk,
                "brightness LP " + String (lp.second, 4) + " < BP " + String (bp.second, 4) + " < HP " + String (hp.second, 4)
                + "; energy with Sat 80 %: hat x" + String (h1.first / jmax (1e-9, h0.first), 2) + ", pod x" + String (p1.first / jmax (1e-9, p0.first), 2));
    }

    // ---------------------------------------------------------------- CLICK layer on the Section A kick
    {
        auto hitEnergy = [] (bool clickOn, float& hitsOut) -> double
        {
            TwinboxProcessor p; waitLibrary (p);
            for (int i = 0; i < 100 && p.library.set (SampleLibrary::click)->size() == 0; ++i) Thread::sleep (10);
            setP (p, params::gXfade, 0); setP (p, params::aClickOn, clickOn ? 1.0f : 0.0f); setP (p, params::aClickWav, 2); setP (p, params::aClickGain, 0);
            p.setPlayConfigDetails (0, 2, 48000, 480); p.prepareToPlay (48000, 480);
            AudioBuffer<float> buf (2, 480); MidiBuffer m; double e = 0;
            for (int i = 0; i < 40; ++i) { buf.clear(); p.processBlock (buf, m); }          // let the A/B cross-fade settle
            p.micro.auditionChannel (0, 1.0f);
            buf.clear(); p.processBlock (buf, m);
            for (int k = 0; k < 480; ++k) { const float hf = buf.getSample (0, k) - (k ? buf.getSample (0, k - 1) : 0.0f); e += hf * hf; }   // transient (high-passed) energy, 10 ms
            hitsOut = (float) p.micro.clickHits();
            return e;
        };
        float h0 = 0, h1 = 0;
        const double off = hitEnergy (false, h0), on = hitEnergy (true, h1);
        TwinboxProcessor q; waitLibrary (q);
        const int n = q.library.set (SampleLibrary::click)->size();
        report ("CLICK layer on the A kick", h0 < 0.5f && h1 > 0.5f && on > off * 1.5 && n >= 10,
                String (n) + " factory clicks; transient energy x" + String (on / jmax (1.0e-12, off), 1) + " with CLICK on");
    }

    // ---------------------------------------------------------------- knobs: arrow keys (Pan 1 unit, continuous fine, sample select 1 sample)
    {
        TwinboxProcessor p; waitLibrary (p);
        Knob pan, cut, gain;
        pan.attach (p.apvts, params::b (params::kick, params::lv::pan));
        cut.attach (p.apvts, params::b (params::kick, params::lv::cut));
        gain.attach (p.apvts, params::b (params::kick, params::lv::gain));
        auto press = [] (FineSlider& s, int key, bool shift = false) { s.keyPressed (KeyPress (key, shift ? ModifierKeys::shiftModifier : 0, 0)); };
        const double p0 = pan.slider.getValue(); press (pan.slider, KeyPress::rightKey); const double p1 = pan.slider.getValue();
        press (gain.slider, KeyPress::rightKey); const double g1 = gain.slider.getValue();      // default 0 dB
        press (cut.slider, KeyPress::leftKey); const double c1 = cut.slider.getValue();        // default 20 kHz
        const auto set = p.library.set (SampleLibrary::kick);
        WavKnob wk (p, SampleLibrary::kick, theme::accentB);
        auto* ws = dynamic_cast<FineSlider*> (wk.getChildComponent (0));
        const float w0 = p.apvts.getRawParameterValue (params::b (params::kick, params::lv::wav))->load();
        if (ws) { ws->keyPressed (KeyPress (KeyPress::rightKey)); ws->keyPressed (KeyPress (KeyPress::rightKey)); }
        const float w2 = p.apvts.getRawParameterValue (params::b (params::kick, params::lv::wav))->load();
        const bool ok = std::abs ((p1 - p0) - 1.0) < 1e-6 && g1 > 0.0 && g1 < 0.3 && c1 < 20000.0 && c1 > 19500.0 && ws != nullptr && std::abs (w2 - w0 - 2.0f) < 0.01f;
        report ("knobs: arrow keys", ok, "Pan +" + String (p1 - p0, 2) + ", Gain 0 -> " + String (g1, 3) + " dB, Cutoff 20 kHz -> " + String (c1, 1)
                + " Hz, sample select " + String ((int) w0) + " -> " + String ((int) w2));
    }

    // ---------------------------------------------------------------- MIDI A / MIDI B buttons and NORM (normaliser + limiter)
    {
        auto play = [] (int target) -> std::pair<int, int>
        {
            TwinboxProcessor p; waitLibrary (p);
            setP (p, params::gMidiTarget, (float) target);
            p.setPlayConfigDetails (0, 2, 48000, 512); p.prepareToPlay (48000, 512);
            AudioBuffer<float> buf (2, 512); MidiBuffer m;
            m.addEvent (MidiMessage::noteOn (1, 36, (uint8) 100), 0);
            m.addEvent (MidiMessage::noteOn (1, 43, (uint8) 100), 10);
            p.processBlock (buf, m);
            return { p.micro.hitCount (0) + p.micro.hitCount (7), p.lime.hitCount (LimeEngine::tKick) + p.lime.hitCount (LimeEngine::tSnare) };
        };
        const auto a = play (0), b = play (1);
        report ("MIDI keyboard plays one section", a.first == 2 && a.second == 0 && b.first == 0 && b.second == 2,
                "MIDI A: A hits " + String (a.first) + ", B hits " + String (a.second) + "; MIDI B: A hits " + String (b.first) + ", B hits " + String (b.second)
                + " (note 43 on B = Snare, the row repeats every 6 keys)");

        // SEQ off: the sequencer stops playing, the MIDI keyboard still plays
        {
            TwinboxProcessor p; waitLibrary (p);
            setP (p, params::aSeq, 0); setP (p, params::bSeq, 0); setP (p, params::gRun, 1); setP (p, params::gMidiTarget, 1);
            p.setPlayConfigDetails (0, 2, 48000, 512); p.prepareToPlay (48000, 512);
            AudioBuffer<float> buf (2, 512); MidiBuffer m;
            for (int i = 0; i < 200; ++i) { m.clear(); if (i == 100) m.addEvent (MidiMessage::noteOn (1, 36, (uint8) 100), 0); p.processBlock (buf, m); }
            int aHits = 0; for (int c = 0; c < 8; ++c) aHits += p.micro.hitCount (c);
            int bHits = 0; for (int t = 0; t < LimeEngine::numTargets; ++t) bHits += p.lime.hitCount (t);
            report ("SEQ off (MIDI still plays)", aHits == 0 && bHits == 1, "2 s of playback with SEQ off: A hits " + String (aHits) + ", B hits " + String (bHits) + " (1 = the MIDI note)");
        }
        OutputNorm nrm; nrm.prepare (48000);
        std::vector<float> L (48000), R (48000);
        double inR = 0, outR = 0; float outPk = 0;
        for (int sec = 0; sec < 4; ++sec)
        {
            for (int i = 0; i < 48000; ++i)
            {
                const float x = (i % 12000 < 200 ? 1.6f : 0.02f) * (float) std::sin (i * 0.05);   // loud bursts + quiet tail
                L[(size_t) i] = R[(size_t) i] = x;
                if (sec == 3) inR += x * x;
            }
            nrm.process (L.data(), R.data(), 48000, true);
            if (sec == 3) for (int i = 0; i < 48000; ++i) { outR += L[(size_t) i] * L[(size_t) i]; outPk = jmax (outPk, std::abs (L[(size_t) i])); }
        }
        const double gainDb = 10.0 * std::log10 ((outR + 1e-12) / (inR + 1e-12));
        report ("NORM normaliser + limiter", outPk <= 0.8913f + 1e-3f && std::abs (gainDb) > 0.5,
                "peak after NORM " + String (20.0 * std::log10 (outPk), 2) + " dBFS (ceiling -1), level change " + String (gainDb, 1) + " dB");
    }

    // ---------------------------------------------------------------- per-track nudge: measured onset of a hit on beat 2
    {
        auto onsetMs = [] (bool sectionB, float nudgeMs) -> double
        {
            TwinboxProcessor p; waitLibrary (p);
            for (int v = 0; v < params::numLimeVoices; ++v) setP (p, params::b (v, params::lv::on), 0);   // silence the other section
            for (int ch = 0; ch < params::numMicroChannels; ++ch) setP (p, params::a (ch, params::mt::mute), 1);
            setP (p, params::gTempo, 120.0f);
            setP (p, params::gXfade, sectionB ? 100.0f : 0.0f);
            if (sectionB)
            {
                auto& P = p.lime.pattern (0); P.clear();
                P.trig[LimeEngine::laneKick][4].store (2);                    // beat 2 (step 5) = 500 ms at 120 BPM
                setP (p, params::bPattern, 0); setP (p, params::bFxMix, 0);
                setP (p, params::b (params::kick, params::lv::on), 1);
                setP (p, params::b (params::kick, params::lv::nudge), nudgeMs);
            }
            else
            {
                for (int pi = 0; pi < 12; ++pi) p.micro.pattern (pi).clear();
                p.micro.pattern (0).steps[0][4].store (MicroEngine::trig | MicroEngine::accent);
                setP (p, params::aPattern, 0); setP (p, params::aSwing, 0);
                setP (p, params::a (0, params::mt::mute), 0);
                setP (p, params::a (0, params::mt::nudge), nudgeMs);
            }
            p.setPlayConfigDetails (0, 2, 48000, 256); p.prepareToPlay (48000, 256);
            setP (p, params::gRun, 1);
            AudioBuffer<float> buf (2, 256); MidiBuffer m; double at = -1.0;
            for (int blk = 0; blk < 300 && at < 0; ++blk)
            {
                buf.clear(); p.processBlock (buf, m);
                for (int i = 0; i < 256; ++i) if (std::abs (buf.getSample (0, i)) > 0.01f) { at = (blk * 256 + i) / 48.0; break; }
            }
            setP (p, params::gRun, 0);
            return at;
        };
        String info; bool okAll = true;
        for (int sec = 0; sec < 2; ++sec)
        {
            const double z = onsetMs (sec == 1, 0.0f), e = onsetMs (sec == 1, -20.0f), l = onsetMs (sec == 1, 20.0f), x = onsetMs (sec == 1, -50.0f);
            const bool ok = std::abs (z - 500.0) < 2.0 && std::abs ((z - e) - 20.0) < 0.6 && std::abs ((l - z) - 20.0) < 0.6 && std::abs ((z - x) - 50.0) < 0.6;
            okAll = okAll && ok;
            info << (sec ? "  B kick: " : "A ch1: ") << String (z, 1) << " ms, -20 -> " << String (e, 1) << ", +20 -> " << String (l, 1) << ", -50 -> " << String (x, 1);
        }
        report ("per-track nudge (measured)", okAll, info);
    }

    // ---------------------------------------------------------------- pitch detection, SCALE, manual pitch, on/off kept
    {
        // 1) detector on synthetic tones
        String info; int bad = 0;
        for (double f : { 55.0, 98.0, 130.81, 261.63, 440.0, 880.0, 1500.0 })
        {
            Sample s; s.sourceRate = 44100;
            for (int i = 0; i < 44100; ++i)
            {
                const double t = i / 44100.0;
                s.L.push_back ((float) ((0.6 * std::sin (2 * MathConstants<double>::pi * f * t) + 0.25 * std::sin (4 * MathConstants<double>::pi * f * t)) * std::exp (-t * 2.0)));
            }
            float conf; const float m = SampleLibrary::detectPitch (s, conf);
            const float expect = (float) (69.0 + 12.0 * std::log2 (f / 440.0));
            if (std::abs (m - expect) > 0.2f) { ++bad; info << String (f) << "Hz->" << String (m, 2) << " "; }
        }
        Sample noise; noise.sourceRate = 44100; Random r (3);
        for (int i = 0; i < 22050; ++i) noise.L.push_back ((r.nextFloat() * 2 - 1) * std::exp (-i / 4000.0f));
        float c2; const float nm = SampleLibrary::detectPitch (noise, c2);
        report ("pitch detection (tones / noise)", bad == 0 && nm < 0, bad ? info : String ("7 tones within 0.2 st, noise = no pitch"));

        // 2) factory Tie folder (all tonal samples are rooted on C)
        TwinboxProcessor p;
        waitLibrary (p);
        for (int i = 0; i < 300 && p.library.set (SampleLibrary::tie)->size() == 0; ++i) Thread::sleep (10);
        auto tieSet = p.library.set (SampleLibrary::tie);
        int pitched = 0, onC = 0, basses = 0, bassOnC = 0, other = 0;
        for (int i = 0; i < tieSet->size(); ++i)
        {
            const auto& smp = tieSet->samples[(size_t) i];
            const String nm = tieSet->names[i];
            const bool isBass = nm.contains ("Bass") || nm.contains ("Sub");
            const bool isChord = nm.contains (" Cm") || nm.contains (" Csus");
            if (! isBass && ! isChord) ++other;                          // Tie holds only basses and chords
            if (smp->pitch < 0) continue;
            ++pitched;
            const int pc = ((int) std::round (smp->pitch) % 12 + 12) % 12;
            if (pc == 0) ++onC;
            if (isBass) { ++basses; if (pc == 0) ++bassOnC; }
        }
        report ("Tie factory samples analysed", pitched > tieSet->size() / 2 && basses >= 20 && bassOnC == basses && other == 0,
                String (pitched) + "/" + String (tieSet->size()) + " pitched, " + String (onC) + " on C; basses on C: " + String (bassOnC) + "/" + String (basses)
                + "; non bass/chord samples: " + String (other));

        // 3) SCALE: play a pitched sample, measure the output pitch
        auto renderAudition = [&] (int idx, double seconds) -> float
        {
            p.setPlayConfigDetails (0, 2, 44100, 512); p.prepareToPlay (44100, 512);
            setP (p, params::gRun, 0); setP (p, params::gXfade, 100); setP (p, params::bFxMix, 0);
            for (int v : { params::kick, params::snare, params::hat, params::pod }) setP (p, params::b (v, params::lv::on), 0);
            setP (p, params::b (params::tie, params::lv::dcy), 100);
            p.lime.auditionSample (SampleLibrary::tie, idx);
            Sample out; out.sourceRate = 44100;
            AudioBuffer<float> buf (2, 512); MidiBuffer m;
            for (int b2 = 0; b2 < (int) (seconds * 44100 / 512); ++b2)
            {
                p.processBlock (buf, m);
                for (int i = 0; i < 512; ++i) out.L.push_back (buf.getSample (0, i));
            }
            float c; return SampleLibrary::detectPitch (out, c);
        };
        int idx = -1;
        for (int i = 0; i < tieSet->size(); ++i) if (tieSet->names[i].contains ("Deep Sub C")) idx = i;
        const float src = idx >= 0 ? tieSet->samples[(size_t) idx]->pitch : -1.0f;
        setP (p, params::b (params::tie, params::lv::scale), 1);
        setP (p, params::bScaleRoot, 2); setP (p, params::bScaleType, 0);           // D Major: C is not in the scale -> C#
        const float scaled = renderAudition (idx, 0.6);
        setP (p, params::b (params::tie, params::lv::scale), 0);
        p.library.setSampleOffset (SampleLibrary::tie, idx, 5.0f);                  // MANUAL +5 st
        const float manual = renderAudition (idx, 0.6);
        p.library.setSampleOffset (SampleLibrary::tie, idx, 0.0f);
        const bool okScale = std::abs (scaled - (float) params::quantizeToScale (src, 2, 0)) < 0.35f && std::abs (scaled - src) > 0.5f;
        const bool okManual = std::abs (manual - (src + 5.0f)) < 0.35f;
        report ("SCALE + MANUAL pitch (measured)", okScale && okManual,
                "sample " + SampleLibrary::noteName (src) + " -> scale D Major " + SampleLibrary::noteName (scaled) + ", manual +5 " + SampleLibrary::noteName (manual));

        // 3b) key from file name: parser + measured effect
        {
            struct C { const char* name; float expect; };
            const C cases[] { { "pad chord Db", 61 }, { "T09 Rhodes Cm9", 60 }, { "Bass_F#2", 54 }, { "Kick C1", 36 }, { "La minore", 57 },
                              { "Sol# lead", 56 }, { "Am7 stab", 57 }, { "120bpm Ebmaj7", 63 }, { "loop_Bb", 58 }, { "pad chord db", 61 },
                              { "Kick", -1 }, { "T05 Dub Chord m7", -1 }, { "Cello", -1 }, { "Glass Bell", -1 }, { "Sub Bass", -1 }, { "Remix", -1 } };
            String fails;
            for (auto& c : cases)
            {
                const float got = SampleLibrary::pitchFromName (c.name);
                if (std::abs (got - c.expect) > 0.01f) fails << c.name << "=" << String (got) << "  ";
            }
            report ("key from file name: parser", fails.isEmpty(), fails.isEmpty() ? String ("16 names parsed correctly") : fails);

            // user folder with a 440 Hz tone deliberately named "Db": NAME off -> A4 stays A (C Major); NAME on -> Db -> D (+1)
            auto dir = File::getSpecialLocation (File::tempDirectory).getChildFile ("twinbox_name_test");
            dir.deleteRecursively(); dir.createDirectory();
            {
                AudioBuffer<float> tone (1, 44100);
                for (int i = 0; i < 44100; ++i) tone.setSample (0, i, 0.5f * (float) std::sin (2 * MathConstants<double>::pi * 440.0 * i / 44100.0) * std::exp (-i / 30000.0f));
                WavAudioFormat wav;
                std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (dir.getChildFile ("test tone Db.wav")), 44100, 1, 24, {}, 0));
                w->writeFromAudioSampleBuffer (tone, 0, tone.getNumSamples());
            }
            p.library.setUserFolder (SampleLibrary::tie, dir);
            Thread::sleep (30); while (p.library.isLoading()) Thread::sleep (5);
            setP (p, params::b (params::tie, params::lv::scale), 1);
            setP (p, params::bScaleRoot, 0); setP (p, params::bScaleType, 0);       // C Major
            setP (p, params::b (params::tie, params::lv::nameKey), 0);
            const float offPitch = renderAudition (0, 0.6);
            setP (p, params::b (params::tie, params::lv::nameKey), 1);
            const float onPitch = renderAudition (0, 0.6);
            setP (p, params::b (params::tie, params::lv::nameKey), 0);
            setP (p, params::b (params::tie, params::lv::scale), 0);
            p.library.setSource (SampleLibrary::tie, SampleLibrary::twinbox);
            Thread::sleep (30); while (p.library.isLoading()) Thread::sleep (5);
            report ("key from file name: measured", std::abs (offPitch - 69.0f) < 0.35f && std::abs (onPitch - 70.0f) < 0.35f,
                    "A4 tone named 'Db', C Major: NAME off " + SampleLibrary::noteName (offPitch) + ", NAME on " + SampleLibrary::noteName (onPitch));
            dir.deleteRecursively();
        }

        // 3c) Tie lane: column height selects the sample (notch 1 = bottom of the list = last file)
        {
            auto dir = File::getSpecialLocation (File::tempDirectory).getChildFile ("twinbox_height_test");
            dir.deleteRecursively(); dir.createDirectory();
            const double freqs[] { 440.0, 523.25, 659.26 };   // "1 tone" = top of the list ... "3 tone" = bottom
            for (int i = 0; i < 3; ++i)
            {
                AudioBuffer<float> tone (1, 44100);
                for (int j = 0; j < 44100; ++j) tone.setSample (0, j, 0.5f * (float) std::sin (2 * MathConstants<double>::pi * freqs[i] * j / 44100.0));
                WavAudioFormat wav;
                std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (dir.getChildFile (String (i + 1) + " tone.wav")), 44100, 1, 24, {}, 0));
                w->writeFromAudioSampleBuffer (tone, 0, tone.getNumSamples());
            }
            p.library.setUserFolder (SampleLibrary::tie, dir);
            Thread::sleep (30); while (p.library.isLoading()) Thread::sleep (5);
            setP (p, params::b (params::tie, params::lv::wav), 1);                    // WAV knob on the middle sample: must be ignored
            setP (p, params::b (params::tie, params::lv::vel), 1);                    // VEL = wav
            auto playStep = [&] (float value) -> float
            {
                auto& P = p.lime.pattern (0);
                P.clear();
                P.value[LimeEngine::laneTie][0].store (value);
                setP (p, params::bPattern, 0);
                p.setPlayConfigDetails (0, 2, 44100, 512); p.prepareToPlay (44100, 512);
                setP (p, params::gXfade, 100); setP (p, params::bFxMix, 0); setP (p, params::b (params::tie, params::lv::dcy), 100);
                for (int v : { params::kick, params::snare, params::hat, params::pod }) setP (p, params::b (v, params::lv::on), 0);
                setP (p, params::gRun, 1);
                Sample out; out.sourceRate = 44100;
                AudioBuffer<float> buf (2, 512); MidiBuffer m;
                for (int b2 = 0; b2 < 40; ++b2) { p.processBlock (buf, m); for (int i = 0; i < 512; ++i) out.L.push_back (buf.getSample (0, i)); }
                setP (p, params::gRun, 0);
                float c; return SampleLibrary::detectPitch (out, c);
            };
            const float bottom = playStep (1.0f / 3.0f), top = playStep (1.0f);
            setP (p, params::b (params::tie, params::lv::vel), 0);                    // VEL = -- : height is velocity, WAV knob plays
            const float velMode = playStep (1.0f / 3.0f);
            p.library.setSource (SampleLibrary::tie, SampleLibrary::twinbox);
            Thread::sleep (30); while (p.library.isLoading()) Thread::sleep (5);
            dir.deleteRecursively();
            report ("Tie column height selects sample", std::abs (bottom - 76.0f) < 0.4f && std::abs (top - 69.0f) < 0.4f && std::abs (velMode - 72.0f) < 0.4f,
                    "VEL=wav: notch 1 -> " + SampleLibrary::noteName (bottom) + " (last file), top -> " + SampleLibrary::noteName (top)
                    + " (first file); VEL=--: WAV knob -> " + SampleLibrary::noteName (velMode));
        }

        // 4) loading a preset keeps every module's on/off state
        setP (p, params::b (params::kick, params::lv::on), 0); setP (p, params::b (params::pod, params::lv::on), 1);
        PresetLibrary::applyB (p, PresetLibrary::get().items (1)[5], true, true, false);
        const bool keep = p.apvts.getRawParameterValue (params::b (params::kick, params::lv::on))->load() < 0.5f
                       && p.apvts.getRawParameterValue (params::b (params::pod, params::lv::on))->load() > 0.5f;
        report ("preset load keeps module on/off", keep, keep ? String ("Kick stays off, Pod stays on") : String ("state changed"));
    }

    // ---------------------------------------------------------------- state round trip
    {
        TwinboxProcessor a, b;
        setP (a, params::a (2, params::mt::oscFreq), 777.0f);
        a.micro.pattern (5).steps[3][47].store (3);
        a.lime.pattern (2).value[2][40].store (0.9f);
        a.lime.pattern (3).trig[1][63].store (2);
        MemoryBlock mb; a.getStateInformation (mb); b.setStateInformation (mb.getData(), (int) mb.getSize());
        const bool st = std::abs (b.apvts.getRawParameterValue (params::a (2, params::mt::oscFreq))->load() - 777.0f) < 0.5f
                     && b.micro.pattern (5).steps[3][47].load() == 3 && std::abs (b.lime.pattern (2).value[2][40].load() - 0.9f) < 0.01f
                     && b.lime.pattern (3).trig[1][63].load() == 2;
        report ("state round-trip", st, String ((int) mb.getSize()) + " bytes, " + String (a.getParameters().size()) + " params");
    }

    // ---------------------------------------------------------------- Legacy import (only if an .ens was given)
    if (ens.existsAsFile())
    {
        auto r = LegacyImporter::run (ens);
        int counts[5] {};
        for (int m = 0; m < 5; ++m) counts[m] = SampleLibrary::namesIn (SampleLibrary::folderFor (m, SampleLibrary::legacy)).size();
        const int open = SampleLibrary::namesIn (SampleLibrary::folderFor (SampleLibrary::hat, SampleLibrary::legacy).getChildFile ("open")).size();
        report ("legacy import", r.ok && counts[0] > 100, r.message + "  K" + String (counts[0]) + " S" + String (counts[1]) + " H" + String (counts[2])
                + "/" + String (open) + " T" + String (counts[3]) + " P" + String (counts[4]));
        TwinboxProcessor p;
        {   // analysis time of the 128 Legacy Tie samples (cache removed -> full analysis)
            SampleLibrary::folderFor (SampleLibrary::tie, SampleLibrary::legacy).getChildFile (".twinbox_pitch.txt").deleteFile();
            const auto t0 = Time::getMillisecondCounterHiRes();
            p.library.setSource (SampleLibrary::tie, SampleLibrary::legacy);
            Thread::sleep (20);
            while (p.library.isLoading()) Thread::sleep (5);
            const auto t1 = Time::getMillisecondCounterHiRes();
            p.library.setSource (SampleLibrary::tie, SampleLibrary::twinbox); while (p.library.isLoading()) Thread::sleep (5);
            p.library.setSource (SampleLibrary::tie, SampleLibrary::legacy); Thread::sleep (20); while (p.library.isLoading()) Thread::sleep (5);
            const auto t2 = Time::getMillisecondCounterHiRes();
            auto set = p.library.set (SampleLibrary::tie); int pitched = 0; for (auto& sm : set->samples) if (sm->pitch >= 0) ++pitched;
            std::printf ("   Legacy Tie: %d samples, load+analysis %.0f ms, with cache %.0f ms, %d pitched\n", set->size(), t1 - t0, t2 - t1, pitched);
        }
        for (int m = 0; m < 5; ++m) p.library.setSource (m, SampleLibrary::legacy);
        for (int i = 0; i < 1000 && (p.library.isLoading() || p.library.set (0)->size() < 100); ++i) Thread::sleep (10);
        setP (p, params::gRun, 1); setP (p, params::gXfade, 100);
        auto st = render (p, 6.0, outDir.getChildFile ("render_B_legacy.wav"));
        report ("render with Legacy samples", st.nans == 0 && st.peak > 0.01f, "peak " + String (Decibels::gainToDecibels (st.peak), 1) + " dBFS");
    }

    // ---------------------------------------------------------------- GUI snapshots
    {
        TwinboxProcessor p;
        waitLibrary (p);
        setP (p, params::gRun, 1);
        p.prepareToPlay (48000, 512);
        std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
        ed->setVisible (true);
        MessageManager::getInstance()->runDispatchLoopUntil (300);
        auto shot = [&] (Component& c, const String& name)
        {
            auto img = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
            PNGImageFormat png; FileOutputStream os (outDir.getChildFile (name));
            os.setPosition (0); os.truncate(); png.writeImageToStream (img, os);
        };
        ed->setSize ((int) (ed->getWidth() / 0.8f), (int) (ed->getHeight() / 0.8f));
        MessageManager::getInstance()->runDispatchLoopUntil (300);
        shot (*ed, "gui_main.png");
        auto snapWin = [&] (Component* c, int w, int h, const String& name)
        {
            std::unique_ptr<Component> owner (c);
            c->setLookAndFeel (&ed->getLookAndFeel());
            c->setSize (w, h);
            MessageManager::getInstance()->runDispatchLoopUntil (150);
            shot (*c, name);
        };
        snapWin (new MicroPatternWindowContent (p, {}), 1280, 870, "win_A_pattern.png");
        snapWin (new LimeSeqPanel (p, true), 1400, 640, "win_B_sequencer.png");
        snapWin (new LimeModulePanel (p, params::tie, true), 900, 640, "win_B_tie.png");
        snapWin (new LimeModulePanel (p, params::hat, true), 900, 640, "win_B_hat.png");
        snapWin (new LimeFxWindowContent (p), 1300, 860, "win_B_fx.png");
        snapWin (new PresetBrowser (p, 1), 820, 700, "win_B_browser.png");
        snapWin (new PitchWindowContent (p, params::tie), 760, 640, "win_B_pitch.png");
        snapWin (new LfoWindowContent (p, 1), 1240, 400, "win_B_lfo.png");
        setP (p, params::lfoCountId (1), 8.0f);
        snapWin (new LfoWindowContent (p, 1), 1240, 700, "win_B_lfo8.png");
        { LfoPanel lp (p, 1); lp.setSize (240, 190); shot (lp, "B_lfo_panel8.png"); }
        setP (p, params::lfoCountId (1), 3.0f);
        {   // keyboard editing of a Tie column (VEL = wav: one notch per sample)
            setP (p, params::b (params::tie, params::lv::vel), 1);
            auto& P = p.lime.pattern ((int) p.apvts.getRawParameterValue (params::bPattern)->load());
            P.value[LimeEngine::laneTie][2].store (0.0f);
            LimeSeqGrid grid (p, true);
            grid.setSize (1200, 400); grid.setVisible (true);
            auto src = Desktop::getInstance().getMainMouseSource();
            const auto rb = grid.rowBounds (4);
            const Point<float> pos (rb.getX() + rb.getWidth() / 32.0f * 2.5f, rb.getBottom() - 2.0f);
            MouseEvent e (src, pos, ModifierKeys(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &grid, &grid, Time::getCurrentTime(), pos, Time::getCurrentTime(), 1, false);
            grid.mouseDown (e); grid.mouseUp (e);
            const int n = jmin (128, p.library.set (SampleLibrary::tie)->size());
            const float afterClick = P.value[LimeEngine::laneTie][2].load();
            grid.keyPressed (KeyPress (KeyPress::upKey)); grid.keyPressed (KeyPress (KeyPress::upKey));
            const float afterUp = P.value[LimeEngine::laneTie][2].load();
            grid.keyPressed (KeyPress (KeyPress::rightKey)); grid.keyPressed (KeyPress (KeyPress::upKey));
            const float next = P.value[LimeEngine::laneTie][3].load();
            grid.keyPressed (KeyPress (KeyPress::deleteKey));
            const float deleted = P.value[LimeEngine::laneTie][3].load();
            const int nClick = (int) std::round (afterClick * n), nUp = (int) std::round (afterUp * n);
            const bool ok = nUp == nClick + 2 && std::abs (next * n - 1.0f) < 0.01f && deleted == 0.0f;
            report ("keyboard arrows on Tie column", ok, "click notch " + String (nClick) + ", +2x Up -> " + String (nUp) + ", Right+Up -> next step notch "
                    + String ((int) std::round (next * n)) + ", Delete -> " + String (deleted));
        }
        {   // factory library upgraded (old v2 files removed, new v3 files installed)
            const auto k = SampleLibrary::folderFor (SampleLibrary::kick, SampleLibrary::twinbox);
            const bool ok = ! k.getChildFile ("K01 Rumble Core.wav").existsAsFile() && k.getChildFile ("K01 Tek Rumble Deep.wav").existsAsFile()
                         && SampleLibrary::namesIn (k).size() == 48;
            report ("factory library upgrade", ok, String (SampleLibrary::namesIn (k).size()) + " kicks, old files removed: " + String (! k.getChildFile ("K01 Rumble Core.wav").existsAsFile() ? "yes" : "no"));
        }
        {
            LimeSeqPanel panel (p, false);
            panel.setSize (650, 260);
            panel.setVisible (true);
            int ok = 0;
            for (auto* c : panel.getChildren())
                if (auto* tb = dynamic_cast<TextButton*> (c))
                    if (tb->getButtonText() == "BELT" || tb->getButtonText() == "CAPSULE" || tb->getButtonText() == "MOD")
                        if (panel.getComponentAt (tb->getBounds().getCentre()) == tb) ++ok;
            report ("seq lane buttons clickable", ok == 3, String (ok) + "/3 buttons on top of the grid");
        }
        ed.reset();
    }
    std::printf (ok ? "ALL TESTS PASSED\n" : "SOME TESTS FAILED\n");
    return ok ? 0 : 1;
}
