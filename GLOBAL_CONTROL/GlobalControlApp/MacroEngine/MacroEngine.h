#pragma once
// MacroEngine: turns the position of one macro knob into a target raw value for every controlled instance.
//
// ABSOLUTE: every instance gets the same display value (e.g. Threshold = -12 dB), each clamped to its own range.
// RELATIVE: every instance keeps its own starting value and receives the same variation, computed in the
//           domain that matches the parameter's measured scale:
//             Linear      -> additive in display units      (-12 dB, -8 dB  + (-2 dB) -> -14, -10)
//             Logarithmic -> multiplicative (octave-like)   (10 ms, 40 ms  x 2      -> 20, 80)
//             Curved      -> multiplicative if the display is strictly positive, else additive
//             Discrete    -> integer step offset
//             Enumerated  -> integer step offset over the item list
//             Unknown     -> normalized raw offset (flagged as approximate)
// Per-instance clamping: when one instance reaches its limit the others keep moving; when the knob comes
// back the clamped instance returns exactly to its original offset (targets are always computed from the
// stored start values, never accumulated).
#include "../Core/ParameterScale.h"

#include <cstdint>
#include <string>
#include <vector>

namespace gc {

enum class MacroMode { Absolute, Relative };

struct InstanceTarget {
    int64_t paramId = 0;      // Live runtime id of the DeviceParameter (valid for the current session only)
    double startRaw = 0;      // raw value captured when the relative gesture started / when assigned
};

struct MacroSettings {
    std::string name;
    uint32_t colour = 0xffd08a2e;
    MacroMode mode = MacroMode::Absolute;
    // Absolute: display range mapped onto the knob. Relative: maximum variation at the knob ends
    // (display units for additive scales, octaves for multiplicative scales, steps for discrete).
    double rangeMin = 0, rangeMax = 1;
    double relativeSpan = 6;
    bool invert = false;
    int precision = 2;        // decimals of the display value sent (absolute); <0 = no rounding
    double fineFactor = 0.1;  // drag sensitivity multiplier while fine adjustment is held
    double defaultKnob = 0.5; // reset position
};

struct MacroOutput {
    int64_t paramId;
    double raw;
    bool clamped;             // the requested value was outside the parameter's range
    bool approximate;         // scale unknown: normalized control
};

class MacroEngine {
public:
    /** Knob position 0..1 -> per-instance raw targets. */
    static std::vector<MacroOutput> compute (const MacroSettings&, const ParameterScale&,
                                             const std::vector<InstanceTarget>&, double knob01);

    /** Relative variation produced by a knob position (0.5 = none), in the scale's relative domain. */
    static double relativeAmount (const MacroSettings&, double knob01);

    /** Mouse/encoder drag helper: new knob position for a drag delta (fine adjustment aware). */
    static double drag (const MacroSettings&, double knob01, double delta, bool fine);

    /** Absolute mode: knob position that corresponds to a display value (for feedback / initial state). */
    static double knobForDisplay (const MacroSettings&, double display);

    static bool multiplicative (const ParameterScale&);
};

} // namespace gc
