// Offline test harness: renders audio through the processor, checks presets / FX / Legacy import, snapshots the GUI.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/Presets.h"
#include <juce_audio_formats/juce_audio_formats.h>

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
    if (argc > 5 && String (argv[3]) == "calibrate")
    {
        // For every Section B preset: limit the level increase that each FX (Delay, xFx, Magic) adds over the dry
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
            const double dry = measure (pv, [] (TwinboxProcessor& p) { setP (p, params::bFxMix, 0); setP (p, params::bMagMix, 0); });
            String log;
            struct Fx { const char* id; bool isMix; std::function<void (TwinboxProcessor&)> isolate; };
            const Fx fxs[] = {
                { params::bDlyGain, false, [] (TwinboxProcessor& p) { setP (p, params::bXGain, -60); setP (p, params::bMagMix, 0); } },
                { params::bXGain,   false, [] (TwinboxProcessor& p) { setP (p, params::bDlyGain, -60); setP (p, params::bMagMix, 0); } },
                { params::bMagMix,  true,  [] (TwinboxProcessor& p) { setP (p, params::bDlyGain, -60); setP (p, params::bXGain, -60); } },
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
        setP (p, params::bFxMix, 70); setP (p, params::bMagMix, 40); setP (p, params::bMagPitch, 7); setP (p, params::bDirt, 30);
        setP (p, params::bNorm, 1); setP (p, params::bEqAmp, 6); setP (p, params::bFltTrk, 40); setP (p, params::bFltCut, 3000);
        setP (p, params::bRemix, 37); setP (p, params::bRemixRate, 1); setP (p, params::bChainOn, 1);
        setP (p, params::b (params::pulp, params::lv::on), 1); setP (p, params::b (params::pulp, params::lv::comb), 50);
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

    // ---------------------------------------------------------------- B presets: FX returns <= -16 dB, knobs reach 0 dB
    {
        int bad = 0;
        for (auto& it : PresetLibrary::get().items (1))
            if (auto* prm = it.data.getProperty ("params", var()).getDynamicObject())
                for (auto* id : { params::bDlyGain, params::bXGain })
                    if ((float) prm->getProperty (id) > -16.0f + 1.0e-3f) ++bad;
        for (auto& it : PresetLibrary::get().items (1))
            if (auto* prm = it.data.getProperty ("params", var()).getDynamicObject())
                if ((float) prm->getProperty (params::bDlyGain) > -59.9f) ++bad;
        TwinboxProcessor p;
        auto* d = p.apvts.getParameter (params::bDlyGain); auto* x = p.apvts.getParameter (params::bXGain);
        const bool range = std::abs (d->getNormalisableRange().end) < 1.0e-6f && std::abs (x->getNormalisableRange().end) < 1.0e-6f
                        && std::abs (d->convertFrom0to1 (d->getDefaultValue()) + 60.0f) < 0.01f && std::abs (x->convertFrom0to1 (x->getDefaultValue()) + 16.0f) < 0.01f;
        report ("B preset FX gain <= -16 dB", bad == 0 && range, String (bad) + " values above -16 dB; knob max 0 dB, default -16 dB: " + (range ? "yes" : "no"));
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
        int pitched = 0, onC = 0;
        for (auto& smp : tieSet->samples) if (smp->pitch >= 0) { ++pitched; const int pc = ((int) std::round (smp->pitch) % 12 + 12) % 12; if (pc == 0) ++onC; }
        report ("Tie factory samples analysed", pitched > tieSet->size() / 2, String (pitched) + "/" + String (tieSet->size()) + " pitched, " + String (onC) + " detected on C");

        // 3) SCALE: play a pitched sample, measure the output pitch
        auto renderAudition = [&] (int idx, double seconds) -> float
        {
            p.setPlayConfigDetails (0, 2, 44100, 512); p.prepareToPlay (44100, 512);
            setP (p, params::gRun, 0); setP (p, params::gXfade, 100); setP (p, params::bFxMix, 0);
            for (int v : { params::kick, params::snare, params::hat, params::pod, params::pulp }) setP (p, params::b (v, params::lv::on), 0);
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
        for (int i = 0; i < tieSet->size(); ++i) if (tieSet->names[i].contains ("Sine Ping")) idx = i;
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

        // 4) loading a preset keeps every module's on/off state (incl. Pulp)
        setP (p, params::b (params::kick, params::lv::on), 0); setP (p, params::b (params::pulp, params::lv::on), 1);
        PresetLibrary::applyB (p, PresetLibrary::get().items (1)[5], true, true, false);
        const bool keep = p.apvts.getRawParameterValue (params::b (params::kick, params::lv::on))->load() < 0.5f
                       && p.apvts.getRawParameterValue (params::b (params::pulp, params::lv::on))->load() > 0.5f;
        report ("preset load keeps module on/off", keep, keep ? String ("Kick stays off, Pulp stays on") : String ("state changed"));
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
        ed.reset();
    }
    std::printf (ok ? "ALL TESTS PASSED\n" : "SOME TESTS FAILED\n");
    return ok ? 0 : 1;
}
