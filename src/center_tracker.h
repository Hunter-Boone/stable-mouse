// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <cmath>

// One relative axis. Positions are rebased to the emitted output every step,
// so no absolute desktop target is retained. Only recent reversals are kept.
class CenterTracker {
public:
    double step(double delta, double dt, double tau) {
        time_ += dt; raw_ += delta;
        const double h = dt/tau, decay = std::exp(-h), alpha = -std::expm1(-h);
        const double pending = raw_-stage_, second = stage_-fallback_;
        fallback_ += second*alpha + pending*(alpha-h*decay);
        stage_ += pending*alpha;
        if (!direction_ && std::abs(raw_-peak_) >= 1) direction_ = raw_ > peak_ ? 1 : -1;
        if ((direction_ > 0 && raw_ > peak_) || (direction_ < 0 && raw_ < peak_)) peak_ = raw_;
        // Require a meaningful retreat from the extremum. One-pixel reversals
        // were treating sampling noise as extra half-cycles, especially at 1 kHz.
        const double retreat = std::max(4., std::min(12., previousSpan_*.06));
        if (direction_ && (raw_-peak_)*direction_ <= -retreat) {
            const double interval = time_-lastTurn_, span = std::abs(peak_-previousPeak_);
            const double candidate = (peak_+previousPeak_)/2;
            const double centerStep = candidate-priorCenter_;
            const bool plausible = turns_ >= 3 && interval >= .03 && interval <= .6
                && interval/previousInterval_ >= .4 && interval/previousInterval_ <= 2.5
                && span >= 2 && span/previousSpan_ >= .35 && span/previousSpan_ <= 2.8
                && std::abs(centerStep-priorCenterStep_) <= std::max(4.,span*.3);
            // Acquire with three supporting reversals, but tolerate one poor
            // reversal after confidence has built. Never update from a rejected
            // midpoint. Smooth accepted midpoints so unequal swings do not move
            // the estimated center as much as their individual extrema do.
            const bool wasLocked = locked_;
            consistent_ = plausible ? std::min(consistent_+1, 5) : std::max(0, consistent_-2);
            locked_ = consistent_ >= (wasLocked ? 2 : 3);
            if (locked_ && plausible) center_ = wasLocked ? center_+.3*(candidate-center_) : candidate;
            priorCenter_ = candidate; priorCenterStep_ = centerStep;
            previousPeak_ = peak_; previousInterval_ = interval; previousSpan_ = span;
            lastTurn_ = time_; turns_ = std::min(turns_+1, 4);
            direction_ = -direction_; peak_ = raw_;
        }
        if (locked_ && time_-lastTurn_ > std::min(.7,previousInterval_*2.5)) {
            locked_ = false; consistent_ = 0;
        }
        // Fade between estimates rather than assigning the fallback position on
        // release. The old prototype jumped when their positions differed.
        blend_ += (-std::expm1(-dt/.08))*((locked_ ? 1. : 0.)-blend_);
        centerSmooth_ += (-std::expm1(-dt/.08))*((locked_ ? center_ : fallback_)-centerSmooth_);
        const double output = fallback_*(1-blend_) + centerSmooth_*blend_;
        raw_ -= output; stage_ -= output; fallback_ -= output;
        peak_ -= output; previousPeak_ -= output; priorCenter_ -= output;
        center_ -= output; centerSmooth_ -= output;
        return output;
    }
private:
    double time_ = 0, raw_ = 0, stage_ = 0, fallback_ = 0;
    double peak_ = 0, previousPeak_ = 0, priorCenter_ = 0, priorCenterStep_ = 0;
    double lastTurn_ = 0, previousInterval_ = 0, previousSpan_ = 0;
    double center_ = 0, centerSmooth_ = 0, blend_ = 0;
    int direction_ = 0, turns_ = 0, consistent_ = 0;
    bool locked_ = false;
};
