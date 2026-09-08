// SPDX-License-Identifier: GPL-3.0-only
#include "filter.h"
#include <cstdint>
#include <cstdlib>
#include <iostream>

void check(bool condition, const char *message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}

// Generated engineering inputs, not recordings or models of a medical condition.
// Vary duration and amplitude independently on each half-cycle, then make a
// deliberate 160px reach and hold still. Test quantization and sampling noise too.
int main() {
    const char *names[] = {"timing +/-40%", "amplitude +/-40%", "both +/-35%", "2px noise", "mixed frequencies"};
    const double aggregateBounds[] = {.3, .95, .9, .4, .9};
    for (int kind=0; kind<5; ++kind) {
        double totalNormal=0, totalCenter=0;
        for (int rate : {60,125,250,1000}) for (std::uint32_t seed=1; seed<=20; ++seed) {
            auto state=seed;
            auto random=[&] { state=state*1664525u+1013904223u; return double(state)/4294967296.; };
            Stabilizer normal, center; normal.configure({85,1}); center.configure({85,1,true});
            double time=0, duration=.125, from=0, to=65, previous=0;
            double outNormal=0, outCenter=0, sumNormal=0, sumCenter=0;
            const double timing=kind==0 ? .4 : kind==2 ? .35 : 0;
            const double amplitude=kind==1 ? .4 : kind==2 ? .35 : 0;
            for (int i=0; i<rate*10; ++i) {
                const double t=double(i)/rate;
                while (t>=time+duration) {
                    time+=duration; from=to;
                    to=(to>0 ? -1 : 1)*65*(1+amplitude*(2*random()-1));
                    duration=.125*(1+timing*(2*random()-1));
                }
                const double target=t>=6 ? 160*std::min(1.,(t-6)/.4) : 0;
                double raw=t<6 ? from+(to-from)*(.5-.5*std::cos(std::acos(-1.)*(t-time)/duration)) : 0;
                if (kind==3 && t<6) raw+=random()*4-2;
                if (kind==4 && t<6) raw=65*std::sin(t*2*std::acos(-1.)*4.3)+30*std::sin(t*2*std::acos(-1.)*2.1);
                raw=std::round(raw+target);
                normal.add(raw-previous,0); center.add(raw-previous,0); previous=raw;
                outNormal+=normal.step(1./rate).x; outCenter+=center.step(1./rate).x;
                check(std::isfinite(outCenter), "variable input remains finite");
                if (t>=2 && t<6) { sumNormal+=outNormal*outNormal; sumCenter+=outCenter*outCenter; }
                if (t>=7.5) check(std::abs(outCenter-target)<2, "tracking releases for deliberate travel");
            }
            check(std::abs(outCenter-160)<.01, "no permanent offset after travel");
            // Do not allow the aggregate to hide a large individual regression.
            check(std::sqrt(sumCenter/sumNormal)<1.1, "individual case stays within 10% of ordinary smoothing");
            totalNormal+=std::sqrt(sumNormal/(rate*4)); totalCenter+=std::sqrt(sumCenter/(rate*4));
        }
        std::cout<<names[kind]<<": ordinary RMS="<<totalNormal/80<<", center RMS="<<totalCenter/80<<'\n';
        check(totalCenter/totalNormal<aggregateBounds[kind], "center tracking improves variable shaking across rates and seeds");
    }
}
