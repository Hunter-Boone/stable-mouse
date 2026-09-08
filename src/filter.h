// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <cmath>
#include "center_tracker.h"
#include "always_center.h"

struct Motion { double x = 0; double y = 0; };
struct FilterConfig {
    double strength = 55; // 0..100
    double speed = 1;     // 0.25..2
    bool centerTracking = false; // opt-in preview for repeated reversals
    bool alwaysCenter = false;   // midpoint first, without recognition
    double centerWindow = .25;   // .1.. .6 seconds
};

// A time-based, relative-motion low-pass filter. Residual motion settles even
// when the device stops reporting. No stored absolute target can trap the
// pointer at a screen edge. All methods run on the owning input thread.
class Stabilizer {
public:
    void configure(FilterConfig c) {
        c.centerWindow = std::isfinite(c.centerWindow) ? std::clamp(c.centerWindow, .1, .6) : .25;
        if (c.alwaysCenter != config_.alwaysCenter || (c.alwaysCenter && c.centerWindow != config_.centerWindow)) reset();
        if (c.centerTracking != config_.centerTracking || (config_.centerTracking && (c.strength == 0) != (config_.strength == 0))) reset();
        config_.centerTracking = c.centerTracking;
        config_.alwaysCenter = c.alwaysCenter;
        config_.centerWindow = c.centerWindow;
        config_.strength = std::isfinite(c.strength) ? std::clamp(c.strength, 0.0, 100.0) : 55;
        config_.speed = std::isfinite(c.speed) ? std::clamp(c.speed, 0.25, 2.0) : 1;
    }
    void add(double x, double y) {
        if (!std::isfinite(x) || !std::isfinite(y)) return;
        pending_.x += x * config_.speed;
        pending_.y += y * config_.speed;
    }
    Motion step(double seconds) {
        if (!std::isfinite(seconds) || seconds <= 0) return {};
        // Two equal low-pass stages. Do not shorten the time constant based on
        // movement size: a large tremor must not be mistaken for deliberate motion.
        // pending_ = target - stage 1; second_ = stage 1 - output.
        // The exact constant-input solution avoids sample-rate-dependent tuning.
        if (config_.strength == 0) {
            Motion out{pending_.x + second_.x, pending_.y + second_.y};
            pending_ = {}; second_ = {}; return out;
        }
        const double tau = 0.012 + 0.000022 * config_.strength * config_.strength;
        if (config_.centerTracking && config_.alwaysCenter) {
            Motion out{alwaysX_.step(pending_.x, std::min(seconds,.1), tau, config_.centerWindow),
                       alwaysY_.step(pending_.y, std::min(seconds,.1), tau, config_.centerWindow)};
            pending_ = {}; return out;
        }
        if (config_.centerTracking) {
            Motion out{centerX_.step(pending_.x, std::min(seconds,.1), tau),
                       centerY_.step(pending_.y, std::min(seconds,.1), tau)};
            pending_ = {}; return out;
        }
        const double h = std::min(seconds, 0.1) / tau;
        const double decay = std::exp(-h), alpha = -std::expm1(-h);
        Motion out{second_.x * alpha + pending_.x * (alpha - h * decay),
                   second_.y * alpha + pending_.y * (alpha - h * decay)};
        second_ = {(second_.x + pending_.x * h) * decay,
                   (second_.y + pending_.y * h) * decay};
        pending_.x *= decay; pending_.y *= decay;
        return out;
    }
    void reset() { pending_ = {}; second_ = {}; fraction_ = {}; centerX_ = {}; centerY_ = {}; alwaysX_ = {}; alwaysY_ = {}; }
    // Preserve sub-pixel movement instead of rounding every sample to zero.
    Motion pixels(double seconds) {
        auto m = step(seconds);
        fraction_.x += m.x; fraction_.y += m.y;
        Motion out{std::trunc(fraction_.x), std::trunc(fraction_.y)};
        fraction_.x -= out.x; fraction_.y -= out.y;
        return out;
    }
private:
    CenterTracker centerX_, centerY_;
    AlwaysCenterAxis alwaysX_, alwaysY_;
    FilterConfig config_;
    Motion pending_, second_, fraction_;
};
