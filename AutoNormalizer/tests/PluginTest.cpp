// Host-side smoke test: instantiates the real AudioProcessor, feeds audio + MIDI Program Change,
// checks state save/restore and renders the editor to a PNG (argv[1], optional).
#include "../Source/PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("[%s] %s\n", ok ? " OK " : "FAIL", what);
    if (! ok)
        ++failures;
}

static float peakEver = 0;

static float runSeconds (AutoNormalizerProcessor& p, double seconds, float amp, double freq, bool programChangeFirst = false)
{
    const double sr = 48000.0;
    const int block = 256;
    static double phase = 0;
    juce::AudioBuffer<float> buf (2, block);
    float peak = 0;
    for (int done = 0; done < (int) (seconds * sr); done += block)
    {
        juce::MidiBuffer midi;
        if (programChangeFirst && done == 0)
            midi.addEvent (juce::MidiMessage::programChange (1, 5), 0);
        for (int i = 0; i < block; ++i)
        {
            const float v = amp * (float) (2.0 * (phase - std::floor (phase)) - 1.0);
            phase += freq / sr;
            buf.setSample (0, i, v);
            buf.setSample (1, i, v);
        }
        p.processBlock (buf, midi);
        peakEver = std::max (peakEver, buf.getMagnitude (0, block));
        if (done > (int) ((seconds - 0.5) * sr))
            peak = std::max (peak, buf.getMagnitude (0, block));
    }
    return peak;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    AutoNormalizerProcessor p;
    p.setRateAndBufferSizeDetails (48000.0, 256);
    p.prepareToPlay (48000.0, 256);

    check (p.getLatencySamples() == p.engine.getLatencySamples() && p.getLatencySamples() > 0, "look-ahead latency reported to host");

    float pk = runSeconds (p, 3.0, 0.1f, 110.0);
    std::printf ("      first preset: %.2f LUFS, gain %.2f dB, out peak %.2f dBFS\n", p.engine.getLastMeasuredLufs(),
                 p.engine.getGainDb(), juce::Decibels::gainToDecibels (pk));
    check (p.engine.getState() == an::State::Locked && std::abs (p.engine.getGainDb() - (-14.0f - p.engine.getLastMeasuredLufs())) < 0.01f,
           "first signal normalized to -14 LUFS");

    // Turn off auto-detect to prove the MIDI path alone works
    p.apvts.getParameter ("auto")->setValueNotifyingHost (0.0f);
    runSeconds (p, 0.5, 0.0f, 110.0, true); // Program Change during silence
    check (p.engine.getState() == an::State::WaitingForSignal, "MIDI Program Change arms the normalizer");
    runSeconds (p, 3.0, 0.7f, 220.0);
    check (std::abs (p.engine.getGainDb() - (-14.0f - p.engine.getLastMeasuredLufs())) < 0.01f
               && p.engine.getLastTrigger() == an::TriggerSource::Midi,
           "new preset normalized after Program Change");

    // automation trigger (rising edge)
    p.apvts.getParameter ("trigger")->setValueNotifyingHost (1.0f);
    runSeconds (p, 0.05, 0.0f, 220.0);
    check (p.engine.getState() == an::State::WaitingForSignal, "'Normalize Now' parameter arms the normalizer");
    p.apvts.getParameter ("trigger")->setValueNotifyingHost (0.0f);
    runSeconds (p, 3.0, 0.3f, 220.0);

    // loud target: output must stay <= -1 dBFS
    p.apvts.getParameter ("target")->setValueNotifyingHost (p.apvts.getParameterRange ("target").convertTo0to1 (-5.0f));
    p.engine.requestNormalize (an::TriggerSource::Manual);
    runSeconds (p, 4.0, 0.9f, 55.0);
    std::printf ("      worst output peak: %.3f dBFS, limiter GR %.2f dB\n", juce::Decibels::gainToDecibels (peakEver),
                 p.engine.getGainReductionDb());
    check (peakEver <= juce::Decibels::decibelsToGain (-1.0f) * 1.000001f, "output never exceeds -1 dBFS");

    // state round-trip
    juce::MemoryBlock mb;
    p.getStateInformation (mb);
    AutoNormalizerProcessor p2;
    p2.setRateAndBufferSizeDetails (48000.0, 256);
    p2.setStateInformation (mb.getData(), (int) mb.getSize());
    p2.prepareToPlay (48000.0, 256);
    runSeconds (p2, 0.2, 0.3f, 220.0);
    check (p2.engine.getState() == an::State::Locked && std::abs (p2.engine.getGainDb() - p.engine.getGainDb()) < 0.01f,
           "project reload restores the locked gain (no re-measure)");
    check (p2.apvts.getParameter ("auto")->getValue() < 0.5f, "parameters restored");

    if (argc > 1)
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        // let the timer run once so the status panel shows real values
        juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
        out.deleteFile();
        juce::FileOutputStream os (out);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("      editor snapshot -> %s\n", out.getFullPathName().toRawUTF8());
    }

    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PLUGIN TESTS PASSED" : "PLUGIN TESTS FAILED", failures);
    return failures == 0 ? 0 : 1;
}
