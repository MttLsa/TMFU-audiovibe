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

double MacroEngine::knobForRaw (const MacroSettings& m, const ParameterScale& s, double raw)
{
    double k = 0.5;
    if (s.kind() == ScaleKind::Enumerated || s.kind() == ScaleKind::Unknown) {
        const double span = m.rangeMax - m.rangeMin;
        k = span != 0 ? (raw - m.rangeMin) / span : 0.5;
    } else {
        const auto d = s.toDisplay (raw);
        if (! d) return 0.5;
        if (multiplicative (s) && m.rangeMin > 0 && m.rangeMax > 0 && *d > 0)
            k = (std::log (*d) - std::log (m.rangeMin)) / std::max (1e-12, std::log (m.rangeMax) - std::log (m.rangeMin));
        else {
            const double span = m.rangeMax - m.rangeMin;
            k = span != 0 ? (*d - m.rangeMin) / span : 0.5;
        }
    }
    k = std::clamp (k, 0.0, 1.0);
    return m.invert ? 1.0 - k : k;
}

double MacroEngine::rebaseStart (const MacroSettings& m, const ParameterScale& s, double currentRaw, double knob)
{
    const double amount = relativeAmount (m, knob);
    const double lo = s.minRaw(), hi = s.maxRaw();
    switch (s.kind()) {
        case ScaleKind::Discrete:
        case ScaleKind::Enumerated:
            return std::clamp (std::round (currentRaw - std::round (amount)), lo, hi);
        case ScaleKind::Unknown: {
            const double frac = amount / std::max (1e-12, 2.0 * m.relativeSpan);
            return std::clamp (currentRaw - frac * (hi - lo), lo, hi);
        }
        default: {
            const auto cur = s.toDisplay (currentRaw);
            if (! cur) return currentRaw;
            const double start = multiplicative (s) ? *cur / std::pow (2.0, amount) : *cur - amount;
            return s.toRaw (start).value_or (currentRaw);
        }
    }
}

void MacroEngine::applyDefaults (MacroSettings& m, const ParameterScale& s)
{
    const auto r = s.displayRange();
    switch (s.kind()) {
        case ScaleKind::Enumerated:
        case ScaleKind::Discrete: {
            const double steps = std::max (1.0, std::round (s.maxRaw() - s.minRaw()));
            m.rangeMin = s.kind() == ScaleKind::Discrete && r ? r->first : s.minRaw();
            m.rangeMax = s.kind() == ScaleKind::Discrete && r ? r->second : s.maxRaw();
            m.relativeSpan = std::max (1.0, std::round (steps / 2.0));
            m.precision = 0;
            break;
        }
        case ScaleKind::Unknown:
            m.rangeMin = s.minRaw(); m.rangeMax = s.maxRaw(); m.relativeSpan = 1; m.precision = -1;
            break;
        default:
            if (r) { m.rangeMin = r->first; m.rangeMax = r->second; }
            if (multiplicative (s)) m.relativeSpan = 2;                           // +/- 2 octaves
            else if (r) {
                const double span = (r->second - r->first) / 4.0;
                const double mag = std::pow (10.0, std::floor (std::log10 (std::max (1e-9, span))));
                m.relativeSpan = std::max (mag, std::round (span / mag) * mag);    // e.g. 60 dB -> +/-15 dB
            }
            if (r) {
                const double span = r->second - r->first;
                m.precision = span >= 1000 ? 0 : span >= 100 ? 1 : 2;
            }
    }
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
