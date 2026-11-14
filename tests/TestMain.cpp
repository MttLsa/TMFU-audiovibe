// Offline test harness: renders audio through the processor and snapshots the GUI.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>

using namespace juce;

static void setP (TwinboxProcessor& p, const String& id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

static bool render (TwinboxProcessor& p, const File& out, double seconds, float& peak, int& nans)
{
    const double sr = 48000.0; const int bs = 512;
    p.setPlayConfigDetails (0, 2, sr, bs);
    p.prepareToPlay (sr, bs);
    AudioBuffer<float> all (2, (int) (seconds * sr));
    AudioBuffer<float> buf (2, bs);
    MidiBuffer midi;
    peak = 0; nans = 0;
    for (int pos = 0; pos + bs <= all.getNumSamples(); pos += bs)
    {
        midi.clear();
        if (pos == 48000 * 7) midi.addEvent (MidiMessage::noteOn (1, 36, (uint8) 100), 10);   // MIDI trigger A kick
        if (pos == 48000 * 7) midi.addEvent (MidiMessage::noteOn (1, 48, (uint8) 100), 200);  // MIDI trigger B kick
        p.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < bs; ++i)
            {
                const float v = buf.getSample (c, i);
                if (! std::isfinite (v)) ++nans;
                peak = jmax (peak, std::abs (v));
                all.setSample (c, pos + i, v);
            }
    }
    WavAudioFormat wav;
    out.deleteFile();
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (out), sr, 2, 24, {}, 0));
    if (w) w->writeFromAudioSampleBuffer (all, 0, all.getNumSamples());
    return nans == 0;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const File outDir (argc > 1 ? argv[1] : ".");
    bool ok = true;

    {   // ---------------- audio: A only, B only, mix, filter sweep, state round-trip
        struct Case { const char* name; float xfade; float cutoff; int mode; float res; };
        const Case cases[] = { { "A_only", 0, 20000, 2, 0 }, { "B_only", 100, 20000, 2, 0 }, { "mix50", 50, 20000, 2, 0 },
                               { "lp_800_res70", 50, 800, 2, 70 }, { "hp_2k", 50, 2000, 0, 30 }, { "bp_1k_res100", 50, 1000, 1, 100 } };
        for (auto& c : cases)
        {
            TwinboxProcessor p;
            setP (p, params::gRun, 1); setP (p, params::gXfade, c.xfade); setP (p, params::gCutoff, c.cutoff);
            setP (p, params::gMode, (float) c.mode); setP (p, params::gRes, c.res);
            setP (p, params::bLfoFM, 40); setP (p, params::bLfoDM, 30); setP (p, params::bDirt, 30); setP (p, params::bNorm, 1);
            setP (p, params::bModDest, (float) params::mdTieTune); setP (p, params::bTieStr, 1); setP (p, params::bTieLen, 6);
            setP (p, params::aFill, 1);
            float peak; int nans;
            const bool r = render (p, outDir.getChildFile (String ("render_") + c.name + ".wav"), 8.0, peak, nans);
            std::printf ("%-14s peak=%.3f (%.1f dBFS) nans=%d %s\n", c.name, peak, Decibels::gainToDecibels (peak), nans, r && peak > 0.01f ? "OK" : "FAIL");
            ok = ok && r && peak > 0.01f;
        }
        // state round trip
        TwinboxProcessor a, b;
        setP (a, params::a (2, params::mt::oscFreq), 777.0f);
        a.micro.pattern (5).steps[3][7].store (3);
        a.lime.pattern (4).mod[2].store (0.9f);
        MemoryBlock mb; a.getStateInformation (mb); b.setStateInformation (mb.getData(), (int) mb.getSize());
        const bool st = std::abs (b.apvts.getRawParameterValue (params::a (2, params::mt::oscFreq))->load() - 777.0f) < 0.5f
                     && b.micro.pattern (5).steps[3][7].load() == 3 && std::abs (b.lime.pattern (4).mod[2].load() - 0.9f) < 0.01f;
        std::printf ("state round-trip %s (%d bytes, %d params)\n", st ? "OK" : "FAIL", (int) mb.getSize(), a.getParameters().size());
        ok = ok && st;
    }

    {   // ---------------- GUI snapshots
        TwinboxProcessor p;
        setP (p, params::gRun, 1);
        p.prepareToPlay (48000, 512);
        std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (ed->getWidth(), ed->getHeight());
        ed->setVisible (true);
        MessageManager::getInstance()->runDispatchLoopUntil (300);
        auto shot = [&] (Component& c, const String& name, float scale = 1.0f)
        {
            auto img = c.createComponentSnapshot (c.getLocalBounds(), true, scale);
            PNGImageFormat png; FileOutputStream os (outDir.getChildFile (name));
            os.setPosition (0); os.truncate(); png.writeImageToStream (img, os);
        };
        auto* ed2 = dynamic_cast<TwinboxEditor*> (ed.get());
        jassert (ed2 != nullptr);
        ed->setSize ((int) (ed->getWidth() / 0.8f), (int) (ed->getHeight() / 0.8f));   // 100 % scale
        MessageManager::getInstance()->runDispatchLoopUntil (200);
        shot (*ed, "gui_main.png");
        // collapsed global panel
        if (auto* mv = ed->getChildComponent (0))
            if (auto* view = dynamic_cast<MainView*> (mv)) { view->global.setExpanded (false); }
        MessageManager::getInstance()->runDispatchLoopUntil (200);
        shot (*ed, "gui_collapsed.png");
        // expanded windows
        auto snapWin = [&] (Component* c, int w, int h, const String& name)
        {
            std::unique_ptr<Component> owner (c);
            c->setLookAndFeel (&ed->getLookAndFeel());
            c->setSize (w, h);
            MessageManager::getInstance()->runDispatchLoopUntil (100);
            shot (*c, name);
        };
        snapWin (new MicroPatternWindowContent (p, {}), 1280, 870, "win_A_pattern.png");
        snapWin (new MicroPatchWindowContent (p, 0, {}), 1300, 610, "win_A_patch.png");
        snapWin (new LimeSeqWindowContent (p), 1280, 790, "win_B_sequencer.png");
        snapWin (new LimeSamplerPanel (p, params::hat, true), 900, 590, "win_B_hat.png");
        snapWin (new LimeSamplerPanel (p, params::tie, true), 900, 590, "win_B_tie.png");
        snapWin (new LimeMixerView (p), 1300, 690, "win_B_mixer.png");
        ed.reset();
    }
    std::printf (ok ? "ALL TESTS PASSED\n" : "SOME TESTS FAILED\n");
    return ok ? 0 : 1;
}
