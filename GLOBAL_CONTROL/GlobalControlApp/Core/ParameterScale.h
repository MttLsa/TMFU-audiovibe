#pragma once
// ParameterScale: learns how a Live DeviceParameter's raw value maps to the value the user sees, using
// ONLY samples of the parameter's own display function (DeviceParameter.str_for_value, requested from the
// bridge with /global/parameter/describe). Third-party plug-ins are exposed by Live as raw 0..1 values;
// their real scale (dB linear, ms logarithmic, ratio exponential, enumerations) is never invented here,
// it is measured.
#include <optional>
#include <string>
#include <vector>

namespace gc {

enum class ScaleKind {
    Linear,       // display is affine in raw (e.g. Threshold in dB)
    Logarithmic,  // log(display) is affine in raw (e.g. Attack in ms, frequency in Hz)
    Curved,       // monotonic but neither of the above (e.g. a ratio with a custom taper): piecewise
    Discrete,     // quantized parameter (is_quantized = 1) with numeric steps
    Enumerated,   // quantized parameter whose steps are labels (value_items)
    Unknown       // non-numeric or non-monotonic display: only normalized control is possible
};

const char* toString (ScaleKind k);

struct ParsedDisplay {
    std::optional<double> value;   // nullopt when the text carries no number ("Off", "Clean")
    std::string unit;              // normalized unit: kHz -> Hz, s -> ms
    bool infinite = false;
};

/** "-12.0 dB" -> -12 dB, "1.2 kHz" -> 1200 Hz, "950 ms"/"1.20 s" -> ms, "-inf dB" -> infinite. */
ParsedDisplay parseDisplay (const std::string& text);

struct ScaleSample { double raw; std::string text; };

class ParameterScale {
public:
    /** samples ordered by raw value, as returned by the bridge. */
    static ParameterScale fromSamples (double min, double max, bool quantized, const std::vector<ScaleSample>& samples);

    ScaleKind kind() const noexcept { return kind_; }
    const std::string& unit() const noexcept { return unit_; }
    double minRaw() const noexcept { return min_; }
    double maxRaw() const noexcept { return max_; }
    bool hasNumericDisplay() const noexcept { return pts_.size() >= 2; }
    bool increasing() const noexcept { return increasing_; }

    /** Display value for a raw value (interpolated in the scale's own domain). */
    std::optional<double> toDisplay (double raw) const;
    /** Raw value that produces the given display value; clamped to the parameter range. */
    std::optional<double> toRaw (double display) const;
    /** Display range actually reachable [lo, hi]. */
    std::optional<std::pair<double, double>> displayRange() const;

    /** Labels of an enumerated parameter (index = raw step). */
    const std::vector<std::string>& labels() const noexcept { return labels_; }

private:
    double min_ = 0, max_ = 1;
    ScaleKind kind_ = ScaleKind::Unknown;
    bool increasing_ = true;
    std::string unit_;
    std::vector<std::pair<double, double>> pts_;   // (raw, display) numeric & finite, monotonic
    std::vector<std::string> labels_;
    bool logDomain() const noexcept { return kind_ == ScaleKind::Logarithmic; }
};

} // namespace gc
