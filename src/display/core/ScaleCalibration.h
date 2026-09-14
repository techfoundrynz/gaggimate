#pragma once
#include <cmath>

// Calibrate a tray on two supports: the same mass is placed at two distinct
// positions. Solve both sensitivities, allowing inverted load-cell polarity.
struct ScaleCalibration {
    double left = 0, right = 0; // grams per ADC count
    bool valid() const {
        return std::isfinite(left) && std::isfinite(right) && std::abs(left) > 1e-9 &&
               std::abs(right) > 1e-9 && std::abs(left) < 1 && std::abs(right) < 1;
    }
    static bool solve(double a, double b, double c, double d, double mass, ScaleCalibration &out) {
        if (!std::isfinite(mass) || mass < 10 || mass > 500) return false;
        const double det = a * d - b * c;
        const double magnitude = std::abs(a * d) + std::abs(b * c);
        if (!std::isfinite(det) || magnitude < 1 || std::abs(det) < magnitude * 0.05) return false;
        ScaleCalibration candidate{mass * (d - b) / det, mass * (a - c) / det};
        if (!candidate.valid()) return false;
        out = candidate;
        return true;
    }
    double weight(double a, double b) const { return a * left + b * right; }
};
