#include "Params.h"
#include <map>

namespace params
{
    static const Def defs[] = {
       #define TS_DEF(e, id, name, kind, mn, mx, df, ctr, unit, ch) { id, name, kind, (float) (mn), (float) (mx), (float) (df), (float) (ctr), unit, ch },
        TS_PARAMS (TS_DEF)
       #undef TS_DEF
    };
    static_assert (sizeof (defs) / sizeof (defs[0]) == NUM, "parameter table mismatch");

    const Def& def (int i) { return defs[i]; }

    int indexOf (const juce::String& id)
    {
        static const std::map<juce::String, int> m = [] {
            std::map<juce::String, int> r;
            for (int i = 0; i < NUM; ++i) r[defs[i].id] = i;
            return r;
        }();
        auto it = m.find (id);
        return it == m.end() ? -1 : it->second;
    }

    juce::StringArray choicesOf (int i) { return juce::StringArray::fromTokens (defs[i].choices, "|", ""); }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        for (int i = 0; i < NUM; ++i)
        {
            const auto& d = defs[i];
            const juce::ParameterID pid { d.id, 1 };
            switch (d.kind)
            {
                case F:
                {
                    juce::NormalisableRange<float> r (d.mn, d.mx);
                    if (d.centre > d.mn && d.centre < d.mx) r.setSkewForCentre (d.centre);
                    const juce::String unit (d.unit);
                    auto attr = juce::AudioParameterFloatAttributes().withLabel (unit)
                        .withStringFromValueFunction ([unit] (float v, int) {
                            if (unit == "Hz") return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (v, v < 100 ? 1 : 0) + " Hz";
                            if (unit == "ms") return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s" : juce::String (v, v < 10 ? 1 : 0) + " ms";
                            if (unit.isEmpty()) return juce::String (v, 2);
                            return juce::String (v, 1) + " " + unit; });
                    layout.add (std::make_unique<juce::AudioParameterFloat> (pid, d.name, r, d.def, attr));
                    break;
                }
                case I: layout.add (std::make_unique<juce::AudioParameterInt> (pid, d.name, (int) d.mn, (int) d.mx, (int) d.def)); break;
                case B: layout.add (std::make_unique<juce::AudioParameterBool> (pid, d.name, d.def > 0.5f)); break;
                case C: layout.add (std::make_unique<juce::AudioParameterChoice> (pid, d.name, choicesOf (i), (int) d.def)); break;
            }
        }
        return layout;
    }
}
