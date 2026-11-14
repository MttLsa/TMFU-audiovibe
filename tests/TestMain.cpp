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
        ed.reset();
    }
    std::printf (ok ? "ALL TESTS PASSED\n" : "SOME TESTS FAILED\n");
    return ok ? 0 : 1;
}
