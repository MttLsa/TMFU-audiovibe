#include "MacroEngine.h"

#include <algorithm>
#include <cmath>

namespace gc {

static double roundTo (double v, int decimals)
{
    if (decimals < 0) return v;
    const double k = std::pow (10.0, decimals);
    return std::round (v * k) / k;
}

bool MacroEngine::multiplicative (const ParameterScale& s)
{
    if (s.kind() == ScaleKind::Logarithmic) return true;
    if (s.kind() == ScaleKind::Curved) {
        const auto r = s.displayRange();
        return r && r->first > 0;
    }
    return false;
}

double MacroEngine::relativeAmount (const MacroSettings& m, double knob)
{
    knob = std::clamp (knob, 0.0, 1.0);
    if (m.invert) knob = 1.0 - knob;
    return (knob - 0.5) * 2.0 * m.relativeSpan;
}

double MacroEngine::drag (const MacroSettings& m, double knob, double delta, bool fine)
{
    return std::clamp (knob + delta * (fine ? m.fineFactor : 1.0), 0.0, 1.0);
}

double MacroEngine::knobForDisplay (const MacroSettings& m, double display)
{
    const double span = m.rangeMax - m.rangeMin;
    double k = span != 0 ? (display - m.rangeMin) / span : 0.5;
    k = std::clamp (k, 0.0, 1.0);
    return m.invert ? 1.0 - k : k;
}

std::vector<MacroOutput> MacroEngine::compute (const MacroSettings& m, const ParameterScale& s,
                                               const std::vector<InstanceTarget>& inst, double knob)
{
    std::vector<MacroOutput> out;
    out.reserve (inst.size());
    knob = std::clamp (knob, 0.0, 1.0);
    const double lo = s.minRaw(), hi = s.maxRaw();

    auto clampRaw = [&] (int64_t id, double raw, bool approx) {
        const double c = std::clamp (raw, lo, hi);
        out.push_back ({ id, c, std::abs (c - raw) > 1e-12, approx });
    };

    if (m.mode == MacroMode::Absolute) {
        const double k = m.invert ? 1.0 - knob : knob;
        if (s.kind() == ScaleKind::Enumerated || s.kind() == ScaleKind::Unknown) {
            // no numeric display: the knob covers [rangeMin, rangeMax] in raw units (steps for enums)
            double raw = m.rangeMin + k * (m.rangeMax - m.rangeMin);
            const bool approx = s.kind() == ScaleKind::Unknown;
            if (! approx) raw = std::round (raw);
            for (const auto& t : inst) clampRaw (t.paramId, raw, approx);
            return out;
        }
        double display = m.rangeMin + k * (m.rangeMax - m.rangeMin);
        if (multiplicative (s) && m.rangeMin > 0 && m.rangeMax > 0)        // log knob for log parameters
            display = std::exp (std::log (m.rangeMin) + k * (std::log (m.rangeMax) - std::log (m.rangeMin)));
        display = roundTo (display, m.precision);
        const auto range = s.displayRange();
        const bool outside = range && (display < range->first - 1e-9 || display > range->second + 1e-9);
        const double raw = s.toRaw (display).value_or (lo);
        for (const auto& t : inst) out.push_back ({ t.paramId, raw, outside, false });
        return out;
    }

    // RELATIVE
    const double amount = relativeAmount (m, knob);
    for (const auto& t : inst) {
        switch (s.kind()) {
            case ScaleKind::Discrete:
            case ScaleKind::Enumerated: {
                clampRaw (t.paramId, std::round (t.startRaw + std::round (amount)), false);
                break;
            }
            case ScaleKind::Unknown: {
                const double frac = amount / std::max (1e-12, 2.0 * m.relativeSpan);  // -0.5..+0.5 of the range
                clampRaw (t.paramId, t.startRaw + frac * (hi - lo), true);
                break;
            }
            default: {
                const auto start = s.toDisplay (t.startRaw);
                if (! start) { clampRaw (t.paramId, t.startRaw, true); break; }
                const double target = multiplicative (s) ? *start * std::pow (2.0, amount) : *start + amount;
                const auto range = s.displayRange();
                const bool outside = range && (target < range->first - 1e-9 || target > range->second + 1e-9);
                const double raw = s.toRaw (target).value_or (t.startRaw);
                out.push_back ({ t.paramId, raw, outside, false });
            }
        }
    }
    return out;
}

} // namespace gc
