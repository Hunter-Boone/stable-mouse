// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <cmath>

struct Motion { double x = 0; double y = 0; };
struct FilterConfig {
    double strength = 55; // 0..100
    double speed = 1;     // 0.25..2
};

// A time-based, relative-motion low-pass filter. Residual motion settles even
// when the device stops reporting. No stored absolute target can trap the
// pointer at a screen edge. All methods run on the owning input thread.
class Stabilizer {
public:
    void configure(FilterConfig c) {
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
        // Larger intentional movements catch up sooner. The strength control
        // still trades response time for steadiness; it cannot identify intent.
        const double tau = config_.strength == 0 ? 0 :
            (0.008 + 0.0014 * config_.strength) /
            (1 + std::hypot(pending_.x, pending_.y) / 100.0);
        const double alpha = tau == 0 ? 1 : -std::expm1(-std::min(seconds, 0.1) / tau);
        Motion out{pending_.x * alpha, pending_.y * alpha};
        pending_.x -= out.x; pending_.y -= out.y;
        return out;
    }
    void reset() { pending_ = {}; fraction_ = {}; }
    // Preserve sub-pixel movement instead of rounding every sample to zero.
    Motion pixels(double seconds) {
        auto m = step(seconds);
        fraction_.x += m.x; fraction_.y += m.y;
        Motion out{std::trunc(fraction_.x), std::trunc(fraction_.y)};
        fraction_.x -= out.x; fraction_.y -= out.y;
        return out;
    }
private:
    FilterConfig config_;
    Motion pending_, fraction_;
};
