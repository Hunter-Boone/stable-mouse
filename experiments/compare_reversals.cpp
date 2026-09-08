// SPDX-License-Identifier: GPL-3.0-only
#include "reversal_filter.h"
#include "motion_scenarios.h"
#include <fstream>
#include <iostream>
#include <vector>

struct Case { const char *name; int index; };
scenarios::Sample sample(Case c, double t) {
    if (c.index < 7) return scenarios::sample(scenarios::cases[c.index],t);
    const double pi = std::acos(-1.);
    const auto wave = [=](double hz) {return std::sin(2*pi*hz*t);};
    if (c.index == 7) {
        // Same observed path as unwanted shaking, but intended in this task.
        const Motion intended{150*wave(4),0}; return {intended,intended};
    }
    if (c.index == 8) {
        const double u = std::clamp(t-2.,0.,1.);
        const Motion intended{12*u,0}; return {intended,intended};
    }
    if (c.index == 9) {
        // Modest intentional repositioning superposed on large oscillation.
        const double u = std::clamp(t-2.,0.,1.);
        Motion intended{20*u,0}; return {{intended.x+300*wave(4),0},intended};
    }
    if (c.index == 10) {
        const double amp = 100+400*std::clamp((t-2.)/2,0.,1.);
        return {{amp*wave(4),0},{}};
    }
    if (c.index == 11) {
        // Instantaneous frequency sweeps from 2 to 8 Hz over six seconds.
        return {{300*std::sin(2*pi*(2*t+.5*t*t)),0},{}};
    }
    if (c.index == 12) {
        // Alternating deliberate corrections, slower than the shake cases.
        Motion intended{40*wave(.8),0}; return {intended,intended};
    }
    // Tremor stops at a zero crossing, then the hand remains still.
    return {{t<3 ? 300*wave(4) : 0,0},{}};
}

template<class Filter>
void run(Filter filter, const char *name, Case c, std::ofstream &trace) {
    Motion previous{},out{}; scenarios::Metrics metrics;
    double peakStartup = 0, maxStep = 0, lateError = 0;
    for (int i=0;i<=750;++i) {
        const double t=i*.008; const auto s=sample(c,t);
        filter.add(s.raw.x-previous.x,s.raw.y-previous.y);previous=s.raw;
        const auto d=filter.step(.008);out.x+=d.x;out.y+=d.y;
        metrics.add(t,s,out);
        if(t<2)peakStartup=std::max(peakStartup,std::hypot(out.x-s.intended.x,out.y-s.intended.y));
        maxStep=std::max(maxStep,std::hypot(d.x,d.y));
        if(t>=4)lateError=std::max(lateError,std::hypot(out.x-s.intended.x,out.y-s.intended.y));
        trace<<c.name<<','<<name<<','<<t<<','<<s.raw.x<<','<<s.raw.y<<','<<s.intended.x<<','<<s.intended.y<<','<<out.x<<','<<out.y<<'\n';
    }
    std::cout<<c.name<<','<<name<<','<<metrics.errorRms()<<','<<metrics.dwell()<<','<<peakStartup<<','<<maxStep<<','<<lateError<<'\n';
}
int main(int argc,char **argv) {
    if(argc!=2)return 1;
    std::ofstream trace(argv[1]);if(!trace)return 1;
    trace<<"scenario,strength,t,raw_x,raw_y,intended_x,intended_y,output_x,output_y\n";
    std::cout<<"scenario,algorithm,rms_error,within_12px_fraction,startup_peak_error,max_step,late_peak_error\n";
    std::vector<Case> cases;
    for(int i=0;i<7;++i)cases.push_back({scenarios::cases[i].name,i});
    for(auto c : {Case{"intentional_same_4Hz_path",7},Case{"clean_12px_nudge",8},Case{"20px_nudge_with_shake",9},Case{"amplitude_ramp",10},Case{"frequency_sweep",11},Case{"intentional_slow_corrections",12},Case{"shake_stops",13}})cases.push_back(c);
    for(auto c:cases){
        Stabilizer current;current.configure({85,1});run(current,"current_Strong",c,trace);
        run(ReversalFilter{},"midpoint",c,trace);
        run(ReversalFilter{6},"midpoint_hold_6px",c,trace);
        run(ReversalFilter{0,true},"Strong_plus_gated_midpoint",c,trace);
    }
}
