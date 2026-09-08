// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "filter.h"
#include <istream>
inline bool readFilterConfig(std::istream &input, FilterConfig &output) {
    FilterConfig c;
    if (!(input>>c.strength>>c.speed) || !std::isfinite(c.strength) || !std::isfinite(c.speed)) return false;
    input>>std::ws;
    if (!input.eof()) {
        int center=0;
        if (!(input>>center) || (center!=0 && center!=1)) return false;
        c.centerTracking=center==1; input>>std::ws;
        if (!input.eof()) {
            int always=0;
            if (!(input>>always>>c.centerWindow) || (always!=0 && always!=1) || !std::isfinite(c.centerWindow)) return false;
            c.alwaysCenter=always==1; input>>std::ws;
        }
    }
    if (!input.eof()) return false;
    output=c; return true;
}
