// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>

// One axis of the website's always-center experiment, including its ordering:
// rolling range midpoint first, then the ordinary two-stage smoother.
class AlwaysCenterAxis {
public:
    double step(double delta, double dt, double tau, double window) {
        time_ += dt; raw_ += delta;
        samples_.push_back({time_, raw_});
        while (samples_.size()>1 && samples_[1].time<=time_-window) samples_.pop_front();
        double low=std::numeric_limits<double>::infinity(), high=-low;
        for (const auto &sample : samples_) { low=std::min(low,sample.position); high=std::max(high,sample.position); }
        const double center=(low+high)/2;
        pending_ += center-previousCenter_; previousCenter_=center;
        const double h=dt/tau, decay=std::exp(-h), alpha=-std::expm1(-h);
        const double out=second_*alpha+pending_*(alpha-h*decay);
        second_=(second_+pending_*h)*decay; pending_*=decay;
        return out;
    }
private:
    struct Sample { double time, position; };
    std::deque<Sample> samples_{{0,0}};
    double time_=0, raw_=0, previousCenter_=0, pending_=0, second_=0;
};
