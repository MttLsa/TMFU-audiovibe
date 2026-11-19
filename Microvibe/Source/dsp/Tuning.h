#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <array>

struct MTSClient;

// =====================================================================================================
//  Microtonality: built-in scales, Scala .scl / .kbm import, MTS-ESP client.
//  Every pitched element of the plug-in asks  tuning.snap (semitones from middle C, amount)
//  and gets the nearest pitch of the current scale (blended by the Snap knob).
// =====================================================================================================
namespace tuning
{
    const juce::StringArray& builtinNames();     // last entry = "User (.scl)"

    struct Scale
    {
        std::array<double, 256> cents {};       // degrees in [0, period), ascending, cents[0] = 0
        int size = 12;
        double period = 1200.0;
        double tonicSemis = 0.0;                // tonic offset from middle C (semitones), set by the kbm or the root
        bool tonicFromKbm = false;
        juce::String name;
    };

    Scale builtin (int index);
    bool parseScl (const juce::String& text, Scale& out, juce::String& error);
    bool parseKbm (const juce::String& text, const Scale& scl, double& tonicSemis, juce::String& error);

    class Tuning
    {
    public:
        Tuning();
        ~Tuning();

        // message thread
        bool loadUser (const juce::String& sclText, const juce::String& kbmText, juce::String& error);
        juce::String userName() const { return userScaleName; }

        // audio thread (once per block)
        void update (int builtinIndex, int root, bool useMts);
        double snap (double semis, float amount) const;             // semitones from middle C
        static double toHz (double semis) { return 261.6255653 * std::exp2 (semis / 12.0); }
        bool mtsActive() const { return mtsOn; }
        int currentSize() const { return cur.size; }

    private:
        double nearest (double semis) const;
        Scale cur, user;
        juce::SpinLock lock;
        std::atomic<int> userVersion { 0 };
        int seenVersion = -1, curBuiltin = -1, curRoot = -1;
        juce::String userScaleName;
        MTSClient* mts = nullptr;
        bool mtsOn = false;
        std::array<double, 128> mtsPitch {};
        int mtsCount = 0;
        int mtsCounter = 0;
    };
}
