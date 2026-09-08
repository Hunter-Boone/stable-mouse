// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "filter.h"

// Research prototype ONLY. Not linked into the app. Each axis estimates the
// midpoint of alternating turning points, then holds small center changes.
// This cannot distinguish intentional oscillation from unwanted oscillation.
class ReversalFilter {
    struct Axis {
        double raw = 0, peak = 0, previousPeak = 0, lastTurn = 0;
        double previousInterval = 0, previousSpan = 0, center = 0;
        double output = 0, fallback = 0, fallbackStage = 0;
        double priorCenter = 0, priorCenterStep = 0;
        int consistent = 0;
        int direction = 0, turns = 0;
        bool locked = false;
        double step(double delta, double dt, double now, double holdRadius, bool conservative) {
            raw += delta;
            const double h = dt/(conservative ? .012+.000022*85*85 : .04);
            const double decay = std::exp(-h), alpha = -std::expm1(-h);
            const double pending = raw-fallbackStage, second = fallbackStage-fallback;
            fallback += second*alpha + pending*(alpha-h*decay);
            fallbackStage += alpha*pending;
            if (!direction && std::abs(raw-peak) >= 1) direction = raw > peak ? 1 : -1;
            if ((direction > 0 && raw > peak) || (direction < 0 && raw < peak)) peak = raw;
            if (direction && (raw-peak)*direction <= -1) {
                const double interval = now-lastTurn, span = std::abs(peak-previousPeak);
                const bool plausible = turns >= 2 && interval >= .03 && interval <= .6
                    && interval/previousInterval >= .7 && interval/previousInterval <= 1.3
                    && span >= 2 && span/previousSpan >= .5 && span/previousSpan <= 2;
                const double candidate = (peak+previousPeak)/2;
                const double centerStep = candidate-priorCenter;
                const bool steadyCenter = turns>=3 && std::abs(centerStep-priorCenterStep) <= std::max(3.,span*.02);
                consistent = plausible && steadyCenter ? consistent+1 : 0;
                locked = plausible && (!conservative || consistent>=3);
                if (locked) center = candidate;
                priorCenter = candidate; priorCenterStep = centerStep;
                previousPeak = peak; previousInterval = interval; previousSpan = span;
                lastTurn = now; ++turns; direction = -direction; peak = raw;
            }
            // A one-way departure or missing reversal releases the center estimate.
            if (locked && now-lastTurn > std::min(.7, previousInterval*1.6)) locked = false;
            double target = locked ? center : fallback;
            // Hysteresis on the estimated center, not on every raw mouse delta.
            // Apply it only while reversals still support an oscillation estimate.
            if (locked) {
                const double difference = target-output;
                target = output + std::copysign(std::max(0., std::abs(difference)-holdRadius), difference);
            }
            const double before = output;
            output += (locked ? -std::expm1(-dt/.02) : 1) * (target-output);
            return output-before;
        }
    } x_, y_;
    Motion pending_{};
    double time_ = 0, holdRadius_;
    bool conservative_;
public:
    explicit ReversalFilter(double holdRadius = 0, bool conservative = false): holdRadius_(holdRadius), conservative_(conservative) {}
    void add(double x, double y) { pending_.x += x; pending_.y += y; }
    Motion step(double dt) {
        time_ += dt;
        Motion out{x_.step(pending_.x,dt,time_,holdRadius_,conservative_), y_.step(pending_.y,dt,time_,holdRadius_,conservative_)};
        pending_ = {}; return out;
    }
};
