// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "filter.h"
#include <array>

namespace scenarios {
struct Sample { Motion raw, intended; };
struct Scenario { const char *name; double amplitude, hz, maxErrorRatio; int kind; };
inline const std::array<Scenario, 9> cases{{
    {"baseline_4Hz_300px_span", 150, 4, .12, 0},
    {"large_4Hz_1200px_span", 600, 4, .12, 0},
    {"large_2Hz_1200px_span", 600, 2, .30, 0},
    {"slow_1Hz_600px_span", 300, 1, .65, 0},
    {"irregular_two_axes", 0, 0, .35, 1},
    {"reach_with_shake", 300, 4, .45, 2},
    {"reach_without_shake", 0, 0, 0, 2},
    {"shake_stops", 300, 4, .2, 3},
    {"fine_correction_with_shake", 300, 4, .2, 4}
}};
inline Sample sample(const Scenario &s, double t) {
    const auto wave = [t](double hz) { return std::sin(2 * std::acos(-1.) * hz * t); };
    Motion intended{};
    if (s.kind == 2) {
        const double u = std::clamp(t - 2., 0., 1.);
        intended = {300 * u*u*(3-2*u), 0};
    }
    if (s.kind == 4) intended.x = 20*std::clamp(t-2.,0.,1.);
    Motion shake{s.amplitude * wave(s.hz), 0};
    if (s.kind == 3 && t >= 3) shake = {};
    if (s.kind == 1) {
        // Amplitude modulation and unrelated frequencies on each axis.
        // An engineering stress case, not a recorded or diagnostic tremor.
        const double envelope = .65 + .35 * wave(.37);
        shake = {envelope * (380 * wave(4.3) + 180 * wave(2.1)),
                 envelope * (250 * wave(5.7) + 100 * wave(1.3))};
    }
    if (s.kind == 2 && s.amplitude > 0) {
        // Shaking increases as the prescribed reach approaches its target.
        const double envelope = .25 + .75 * std::clamp(t - 2., 0., 1.);
        shake = {envelope * s.amplitude * wave(s.hz), envelope * 150 * wave(6)};
    }
    return {{std::round(intended.x + shake.x), std::round(intended.y + shake.y)}, intended};
}
struct Metrics {
    int count = 0, dwellCount = 0, inside = 0;
    double rawSquared = 0, errorSquared = 0, peak = 0, sumX = 0, sumY = 0;
    void add(double t, Sample s, Motion actual) {
        if (t < 2) return;
        const double error = std::hypot(actual.x-s.intended.x, actual.y-s.intended.y);
        rawSquared += std::pow(std::hypot(s.raw.x-s.intended.x, s.raw.y-s.intended.y), 2);
        errorSquared += error*error; peak = std::max(peak, error);
        sumX += actual.x-s.intended.x; sumY += actual.y-s.intended.y; ++count;
        if (t >= 4) { ++dwellCount; if (error <= 12) ++inside; }
    }
    double rawRms() const { return std::sqrt(rawSquared / std::max(1, count)); }
    double errorRms() const { return std::sqrt(errorSquared / std::max(1, count)); }
    double ratio() const { return rawRms() > 0 ? errorRms()/rawRms() : 0; }
    double dwell() const { return double(inside)/std::max(1, dwellCount); }
};
}
