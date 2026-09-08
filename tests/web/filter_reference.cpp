// SPDX-License-Identifier: GPL-3.0-only
#include "filter.h"
#include <iomanip>
#include <iostream>
#include <string>

// A small stdin protocol lets the browser parity test use the actual app filter.
int main() {
    Stabilizer filter;
    std::string operation;
    std::cout << std::setprecision(17);
    while (std::cin >> operation) {
        if (operation == "always") {
            FilterConfig c; c.centerTracking=true; c.alwaysCenter=true;
            std::cin >> c.strength >> c.speed >> c.centerWindow;
            filter.configure(c);
        } else if (operation == "config") {
            FilterConfig c;
            std::cin >> c.strength >> c.speed >> c.centerTracking;
            filter.configure(c);
        } else if (operation == "add") {
            double x, y;
            std::cin >> x >> y;
            filter.add(x, y);
        } else if (operation == "reset") {
            filter.reset();
        } else if (operation == "step" || operation == "pixels") {
            double dt;
            std::cin >> dt;
            const auto m = operation == "step" ? filter.step(dt) : filter.pixels(dt);
            std::cout << m.x << ' ' << m.y << '\n';
        } else {
            return 2;
        }
        if (std::cin.fail()) return 3;
    }
}
