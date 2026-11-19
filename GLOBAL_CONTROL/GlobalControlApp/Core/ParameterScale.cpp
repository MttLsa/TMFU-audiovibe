#include "ParameterScale.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace gc {

const char* toString (ScaleKind k)
{
    switch (k) {
        case ScaleKind::Linear: return "linear";
        case ScaleKind::Logarithmic: return "logarithmic";
        case ScaleKind::Curved: return "curved";
        case ScaleKind::Discrete: return "discrete";
        case ScaleKind::Enumerated: return "enumerated";
        case ScaleKind::Unknown: break;
    }
    return "unknown";
}

static std::string trim (const std::string& s)
{
    size_t a = 0, b = s.size();
    while (a < b && std::isspace ((unsigned char) s[a])) ++a;
    while (b > a && std::isspace ((unsigned char) s[b - 1])) --b;
    return s.substr (a, b - a);
}

ParsedDisplay parseDisplay (const std::string& textIn)
{
    ParsedDisplay r;
    const std::string text = trim (textIn);
    std::string lower = text;
    std::transform (lower.begin(), lower.end(), lower.begin(), [] (unsigned char c) { return (char) std::tolower (c); });

    const auto infPos = lower.find ("inf");
    const auto symPos = text.find ("\xE2\x88\x9E");   // UTF-8 infinity sign
    if (infPos != std::string::npos || symPos != std::string::npos) {
        const auto pos = infPos != std::string::npos ? infPos : symPos;
        const bool neg = pos > 0 && text[pos - 1] == '-';
        r.infinite = true;
        r.value = neg ? -INFINITY : INFINITY;
        r.unit = trim (text.substr (pos + (infPos != std::string::npos ? 3 : 3)));
        return r;
    }

    // first number: [-+]digits[.,digits][e[-+]digits]
    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        const bool startsNum = std::isdigit ((unsigned char) c)
                            || ((c == '-' || c == '+' || c == '.') && i + 1 < text.size() && std::isdigit ((unsigned char) text[i + 1]));
        if (! startsNum) continue;
        std::string num;
        size_t j = i;
        if (text[j] == '-' || text[j] == '+') num += text[j++];
        bool dot = false;
        while (j < text.size()) {
            const char d = text[j];
            if (std::isdigit ((unsigned char) d)) num += d;
            else if ((d == '.' || d == ',') && ! dot && j + 1 < text.size() && std::isdigit ((unsigned char) text[j + 1])) { num += '.'; dot = true; }
            else break;
            ++j;
        }
        if (j + 1 < text.size() && (text[j] == 'e' || text[j] == 'E')
            && (std::isdigit ((unsigned char) text[j + 1]) || ((text[j + 1] == '-' || text[j + 1] == '+') && j + 2 < text.size() && std::isdigit ((unsigned char) text[j + 2])))) {
            num += 'e'; ++j;
            if (text[j] == '-' || text[j] == '+') num += text[j++];
            while (j < text.size() && std::isdigit ((unsigned char) text[j])) num += text[j++];
        }
        double v = std::strtod (num.c_str(), nullptr);
        std::string unit = trim (text.substr (j));
        if (unit.rfind ("kHz", 0) == 0) { v *= 1000.0; unit = "Hz"; }
        else if (unit == "s" || unit == "sec") { v *= 1000.0; unit = "ms"; }
        r.value = v;
        r.unit = unit;
        return r;
    }
    r.unit = text;
    return r;
}

// max deviation from the least-squares line, relative to the span of y
static double linearityError (const std::vector<double>& x, const std::vector<double>& y)
{
    const size_t n = x.size();
    double mx = 0, my = 0;
    for (size_t i = 0; i < n; ++i) { mx += x[i]; my += y[i]; }
    mx /= (double) n; my /= (double) n;
    double sxx = 0, sxy = 0;
    for (size_t i = 0; i < n; ++i) { sxx += (x[i] - mx) * (x[i] - mx); sxy += (x[i] - mx) * (y[i] - my); }
    const double k = sxx > 0 ? sxy / sxx : 0;
    const auto [lo, hi] = std::minmax_element (y.begin(), y.end());
    const double span = std::max (1e-12, *hi - *lo);
    double err = 0;
    for (size_t i = 0; i < n; ++i) err = std::max (err, std::abs (y[i] - (my + k * (x[i] - mx))));
    return err / span;
}

ParameterScale ParameterScale::fromSamples (double min, double max, bool quantized, const std::vector<ScaleSample>& samples)
{
    ParameterScale s;
    s.min_ = min; s.max_ = max;

    std::vector<std::pair<double, double>> pts;
    std::vector<std::string> units;
    size_t nonNumeric = 0;
    for (const auto& smp : samples) {
        const auto p = parseDisplay (smp.text);
        if (p.value && ! p.infinite) { pts.emplace_back (smp.raw, *p.value); units.push_back (p.unit); }
        else if (! p.infinite) ++nonNumeric;
    }
    std::sort (pts.begin(), pts.end());

    if (quantized) {
        if (nonNumeric > 0 || pts.size() < 2) {
            s.kind_ = ScaleKind::Enumerated;
            for (const auto& smp : samples) s.labels_.push_back (trim (smp.text));
            return s;
        }
        s.kind_ = ScaleKind::Discrete;
    }

    if (! units.empty()) {   // majority unit
        std::string best; size_t bestN = 0;
        for (const auto& u : units) { const auto n = (size_t) std::count (units.begin(), units.end(), u); if (n > bestN) { bestN = n; best = u; } }
        s.unit_ = best;
    }

    bool inc = true, dec = true;
    for (size_t i = 1; i < pts.size(); ++i) {
        if (pts[i].second < pts[i - 1].second) inc = false;
        if (pts[i].second > pts[i - 1].second) dec = false;
    }
    const bool monotonic = pts.size() >= 2 && (inc || dec) && pts.front().second != pts.back().second;
    if (! monotonic) {
        if (s.kind_ != ScaleKind::Discrete) s.kind_ = ScaleKind::Unknown;
        if (s.kind_ == ScaleKind::Unknown) return s;
    }
    s.increasing_ = inc;
    // a plateau inside a monotonic curve (e.g. display rounding) would make the inverse ambiguous:
    // keep the first point of every run of equal displays
    for (const auto& p : pts)
        if (s.pts_.empty() || p.second != s.pts_.back().second) s.pts_.push_back (p);

    if (s.kind_ == ScaleKind::Discrete) return s;
    if (s.pts_.size() < 3) { s.kind_ = ScaleKind::Linear; return s; }

    std::vector<double> x, y, ly;
    bool allPositive = true;
    for (const auto& p : s.pts_) { x.push_back (p.first); y.push_back (p.second); allPositive = allPositive && p.second > 0; }
    if (linearityError (x, y) < 0.01) s.kind_ = ScaleKind::Linear;
    else if (allPositive) {
        for (double v : y) ly.push_back (std::log (v));
        s.kind_ = linearityError (x, ly) < 0.02 ? ScaleKind::Logarithmic : ScaleKind::Curved;
    } else s.kind_ = ScaleKind::Curved;
    return s;
}

std::optional<double> ParameterScale::toDisplay (double raw) const
{
    if (pts_.empty()) return std::nullopt;
    if (kind_ == ScaleKind::Discrete) {          // nearest step at or below
        const double r = std::round (raw);
        const auto it = std::min_element (pts_.begin(), pts_.end(), [r] (auto& a, auto& b) { return std::abs (a.first - r) < std::abs (b.first - r); });
        return it->second;
    }
    if (raw <= pts_.front().first) return pts_.front().second;
    if (raw >= pts_.back().first) return pts_.back().second;
    const auto hi = std::upper_bound (pts_.begin(), pts_.end(), raw, [] (double v, const auto& p) { return v < p.first; });
    const auto lo = hi - 1;
    const double t = (raw - lo->first) / std::max (1e-15, hi->first - lo->first);
    if (logDomain()) return std::exp (std::log (lo->second) + t * (std::log (hi->second) - std::log (lo->second)));
    return lo->second + t * (hi->second - lo->second);
}

std::optional<double> ParameterScale::toRaw (double display) const
{
    if (pts_.size() < 2 || kind_ == ScaleKind::Unknown || kind_ == ScaleKind::Enumerated) return std::nullopt;
    if (kind_ == ScaleKind::Discrete) {
        const auto it = std::min_element (pts_.begin(), pts_.end(), [display] (auto& a, auto& b) { return std::abs (a.second - display) < std::abs (b.second - display); });
        return it->first;
    }
    // work on a view ordered by increasing display
    auto at = [this] (size_t i) -> const std::pair<double, double>& { return increasing_ ? pts_[i] : pts_[pts_.size() - 1 - i]; };
    const size_t n = pts_.size();
    if (display <= at (0).second) return at (0).first;
    if (display >= at (n - 1).second) return at (n - 1).first;
    size_t a = 0, b = n - 1;                      // binary search on display
    while (b - a > 1) { const size_t m = (a + b) / 2; (at (m).second <= display ? a : b) = m; }
    const auto& p0 = at (a); const auto& p1 = at (b);
    double t;
    if (logDomain()) t = (std::log (display) - std::log (p0.second)) / std::max (1e-15, std::log (p1.second) - std::log (p0.second));
    else t = (display - p0.second) / std::max (1e-15, p1.second - p0.second);
    return std::clamp (p0.first + t * (p1.first - p0.first), min_, max_);
}

std::optional<std::pair<double, double>> ParameterScale::displayRange() const
{
    if (pts_.empty()) return std::nullopt;
    return std::make_pair (std::min (pts_.front().second, pts_.back().second), std::max (pts_.front().second, pts_.back().second));
}

} // namespace gc
