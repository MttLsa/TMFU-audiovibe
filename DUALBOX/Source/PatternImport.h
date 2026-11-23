#pragma once
#include <juce_core/juce_core.h>

class TwinboxProcessor;

// -------------------------------------------------------------------------------------------------
//  IMPORT PATTERN (Section A): reads drum-machine preset / pattern / drum-patch text files owned by the user
//  (.mtpreset, .mtpattern, .mtdrum) and loads them into Section A:
//   - a preset file   -> the 8 drum patches (with names) + all its patterns (slots A..L) + swing
//   - a pattern file  -> the current pattern slot (8 channels, length, triggers / accents / fills)
//   - a drum file     -> the drum patch of the selected channel
//  The files are a nested "Key: value" / "Key: { ... }" text format; unknown keys are ignored.
//  Nothing is shipped with DUALBOX: only files on the user's computer are read.
// -------------------------------------------------------------------------------------------------
struct PatternImport
{
    struct Node
    {
        juce::String key, value;                 // value: plain text (quotes removed); empty for blocks
        juce::OwnedArray<Node> children;
        const Node* child (const juce::String& k) const;          // case-insensitive
        juce::String get (const juce::String& k) const { auto* c = child (k); return c != nullptr ? c->value : juce::String(); }
    };
    static std::unique_ptr<Node> parse (const juce::String& text);

    struct Result { bool ok = false; int patterns = 0, patches = 0; juce::String message; };
    // targetPattern: Section A slot for a single pattern file; channel: selected channel for a single drum file
    static Result run (TwinboxProcessor& p, const juce::File& file, int targetPattern, int channel);
    static Result runText (TwinboxProcessor& p, const juce::String& text, int targetPattern, int channel);

    // peak (linear) of one accented hit of channel ch with its current parameters (offline render, voice level)
    static float channelPeak (TwinboxProcessor& p, int ch);
    static constexpr float maxChannelPeakDb = 3.0f;   // = the loudest channel of the default kit; imported channels louder than
                                                      // this (EQ boosts, high levels) get their Level lowered to it

    static juce::String fileFilter() { return "*.mtpreset;*.mtpattern;*.mtdrum;*.txt"; }
};
