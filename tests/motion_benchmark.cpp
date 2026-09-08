// SPDX-License-Identifier: GPL-3.0-only
#include "motion_scenarios.h"
#include <fstream>
#include <iostream>

int main(int argc, char **argv) {
    std::ofstream trace;
    if (argc > 1) {
        trace.open(argv[1]);
        if (!trace) return 1;
        trace << "scenario,strength,t,raw_x,raw_y,intended_x,intended_y,output_x,output_y\n";
    }
    std::cout << "scenario,strength,raw_error_rms_px,output_error_rms_px,error_ratio,peak_error_px,target_dwell_fraction\n";
    bool ok = true;
    for (double strength : {0., 55., 85., 100.}) for (auto s : scenarios::cases) {
        Stabilizer filter; filter.configure({strength, 1});
        Motion previous{}, output{}; scenarios::Metrics m;
        for (int i = 0; i <= 750; ++i) {
            const double t = i * .008;
            const auto input = scenarios::sample(s, t);
            filter.add(input.raw.x-previous.x, input.raw.y-previous.y); previous = input.raw;
            const auto delta = filter.pixels(.008); output.x += delta.x; output.y += delta.y;
            m.add(t, input, output);
            if (strength == 0) ok &= output.x == input.raw.x && output.y == input.raw.y;
            if (trace) trace << s.name << ',' << strength << ',' << t << ',' << input.raw.x << ',' << input.raw.y << ',' << input.intended.x << ',' << input.intended.y << ',' << output.x << ',' << output.y << '\n';
        }
        std::cout << s.name << ',' << strength << ',' << m.rawRms() << ',' << m.errorRms() << ',' << (s.kind == 2 && s.amplitude == 0 ? -1 : m.ratio()) << ',' << m.peak << ',' << m.dwell() << '\n';
        // Compare to bounds only at the specified Strong setting. The noiseless
        // reach deliberately measures the cost of smoothing; it is not attenuation.
        if (strength == 85) ok &= m.count > 400 && (s.amplitude || s.kind == 1 ? m.ratio() < s.maxErrorRatio : m.errorRms() < 70);
    }
    return ok ? 0 : 1;
}
