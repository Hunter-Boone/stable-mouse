// SPDX-License-Identifier: GPL-3.0-only
#include "filter.h"
#include "motion_scenarios.h"
#include <cstdlib>
#include <iostream>
#include <limits>

void check(bool ok, const char *message) {
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
int main() {
    // Check acquisition and stationary hold across timer rates, with quantized
    // input. The fallback must remain usable if coarse timing prevents a lock.
    for (int rate : {60,125,250,1000}) {
        Stabilizer filter; filter.configure({85,1,true});
        double previous=0,output=0,latePeak=0,maxReleaseSpeed=0;
        for (int i=0;i<rate*8;++i) {
            double t=double(i)/rate;
            double raw=t<6 ? std::round(600*std::sin(2*std::acos(-1.)*4*t)) : 0;
            filter.add(raw-previous,0);previous=raw;
            const auto d=filter.step(1./rate);output+=d.x;
            check(std::isfinite(output),"output stays finite");
            if(t>=4 && t<6)latePeak=std::max(latePeak,std::abs(output));
            if(t>=6)maxReleaseSpeed=std::max(maxReleaseSpeed,std::abs(d.x)*rate);
        }
        std::cout<<rate<<" Hz: late peak="<<latePeak<<", release speed="<<maxReleaseSpeed<<", settled="<<output<<'\n';
        check(latePeak<6,"center tracking holds regular large shaking inside a small target");
        check(maxReleaseSpeed<2200,"release has no prototype-sized position jump");
        check(std::abs(output)<.2,"released motion settles without a lasting offset");
    }
    // Clean travel uses exactly the ordinary filter. Irregular oscillation can
    // now engage tracking, so test its output instead of requiring it to stay off.
    for(auto scenario : {scenarios::cases[4],scenarios::cases[6]}) {
        Stabilizer normal,center;normal.configure({85,1});center.configure({85,1,true});
        Motion previous{},a{},b{};
        scenarios::Metrics normalMetrics, centerMetrics;
        for(int i=0;i<750;++i){
            auto s=scenarios::sample(scenario,i*.008);
            normal.add(s.raw.x-previous.x,s.raw.y-previous.y);center.add(s.raw.x-previous.x,s.raw.y-previous.y);previous=s.raw;
            auto da=normal.step(.008),db=center.step(.008);a.x+=da.x;a.y+=da.y;b.x+=db.x;b.y+=db.y;
            if(scenario.kind==2) check(std::hypot(a.x-b.x,a.y-b.y)<1e-6,"clean travel preserves ordinary filtering");
            normalMetrics.add(i*.008,s,a);centerMetrics.add(i*.008,s,b);
        }
        if(scenario.kind==1) check(centerMetrics.errorRms()<normalMetrics.errorRms(),"mixed-axis irregular input improves on ordinary smoothing");
    }
    Stabilizer filter;filter.configure({85,1,true});
    double total=0;
    for(int i=0;i<1000;++i){if(i<125)filter.add(.096,0);total+=filter.step(.008).x;}
    check(std::abs(total-12)<1e-6,"12-pixel correction is preserved");
    filter.add(100,30);filter.step(.008);filter.reset();
    for(int i=0;i<100;++i){auto d=filter.step(.008);check(d.x==0 && d.y==0,"click/pause reset leaves no center or settling tail");}
    filter.add(std::numeric_limits<double>::quiet_NaN(),3);
    check(filter.step(.008).x==0,"invalid input is ignored");
    filter.configure({0,.25,true});total=0;
    for(int i=0;i<40;++i){filter.add(1,0);total+=filter.pixels(.008).x;}
    check(total==10,"bypass and fractional speed work in center mode");
    filter.configure({85,1,true});filter.add(12,0);total=0;
    for(int i=0;i<1000;++i)total+=filter.step(.008).x;
    check(std::abs(total-12)<1e-6,"leaving bypass cannot restore old center state");
    std::cout<<"PASS: center acquisition, release, fallback, small corrections, reset and bypass\n";
}
