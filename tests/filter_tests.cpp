// SPDX-License-Identifier: GPL-3.0-only
#include "filter.h"
#include <cstdlib>
#include <iostream>
#include <limits>
namespace {
void check(bool condition, const char *message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
double tremorRms(double strength, int hz, double amplitude = 8, double frequency = 8) {
    Stabilizer filter; filter.configure({strength, 1});
    const double dt = 1.0 / hz, pi = std::acos(-1.0);
    double previous = 0, position = 0, sum = 0; int count = 0;
    for (int i = 1; i <= hz * 5; ++i) {
        const double raw = amplitude * std::sin(2 * pi * frequency * i * dt);
        filter.add(0, raw - previous); previous = raw;
        position += filter.step(dt).y;
        if (i > hz * 2) { sum += position * position; ++count; }
    }
    return std::sqrt(sum / count);
}
}
int main() {
    Stabilizer filter;
    filter.configure({0, 1.5}); filter.add(10, -4);
    auto m = filter.step(.01);
    check(m.x == 15 && m.y == -6, "zero strength preserves movement and applies speed");
    filter.reset(); filter.configure({55, 1}); filter.add(100, -50);
    Motion sum;
    for (int i = 0; i < 1000; ++i) { m = filter.step(.01); sum.x += m.x; sum.y += m.y; }
    check(std::abs(sum.x - 100) < 1e-8 && std::abs(sum.y + 50) < 1e-8, "movement settles completely without further input");
    filter.add(20, 30); filter.reset(); m = filter.step(.01);
    check(m.x == 0 && m.y == 0, "pause or click discards the settling tail");
    filter.configure({0, .25});
    double pixels = 0;
    for (int i = 0; i < 40; ++i) { filter.add(1, 0); pixels += filter.pixels(.01).x; }
    check(pixels == 10, "slow pointer keeps subpixel motion");
    filter.reset();
    for (int i = 0; i < 40; ++i) { filter.add(-1, 0); pixels += filter.pixels(.01).x; }
    check(pixels == 0, "negative motion rounds symmetrically");
    filter.add(std::numeric_limits<double>::quiet_NaN(), 4);
    m = filter.step(.01); check(m.x == 0 && m.y == 0, "invalid input cannot poison the filter");
    filter.add(10, 10); m = filter.step(-1); check(m.x == 0 && m.y == 0, "invalid time does not advance movement");
    const double raw = tremorRms(0, 125), smooth = tremorRms(55, 125), strong = tremorRms(85, 125);
    check(smooth < raw * .35, "balanced smoothing attenuates synthetic 8 Hz oscillation");
    check(strong < smooth, "strong setting attenuates more than balanced");
    check(std::abs(tremorRms(55, 1000) - smooth) < .2, "smoothing is consistent across device sampling rates");
    for (double frequency : {2., 4., 8., 12.}) {
        const double small = tremorRms(85, 125, 8, frequency) / 8;
        const double large = tremorRms(85, 125, 300, frequency) / 300;
        check(std::abs(small - large) < 1e-8, "large shakes must not weaken smoothing");
    }
    const double largeRaw = tremorRms(0, 125, 300, 4);
    const double largeStrong = tremorRms(85, 125, 300, 4);
    check(largeStrong / largeRaw < .06, "Strong rejects at least 94% of a large 4 Hz oscillation");
    check(tremorRms(100, 125, 300, 2) / tremorRms(0, 125, 300, 2) < .12,
        "maximum strength also reduces slower shaking");
    filter.reset(); filter.configure({85, 1}); sum = {};
    for (int i = 0; i < 2000; ++i) {
        if (i < 125) filter.add(.8, -.4);
        m = filter.step(1. / 125); sum.x += m.x; sum.y += m.y;
    }
    check(std::abs(sum.x - 100) < 1e-7 && std::abs(sum.y + 50) < 1e-7,
        "slow deliberate motion still travels its full distance");
    std::cout << "PASS: conservation, settling, reset, speed, fractions, invalid data, sampling rate\n";
    std::cout << "Synthetic 8 Hz path RMS: raw=" << raw << ", balanced=" << smooth << ", strong=" << strong << "\n";
    std::cout << "Large 4 Hz Strong output/input RMS: " << largeStrong / largeRaw << "\n";
}
