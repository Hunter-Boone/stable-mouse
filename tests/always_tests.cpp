// SPDX-License-Identifier: GPL-3.0-only
#include "config_codec.h"
#include <sstream>
#include <iostream>
#include <cstdlib>
void check(bool ok,const char *message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
int main(){
    for(const char *text:{"55 1", "55 1 1", "55 1 1 1 .45"}){
        std::istringstream stream(text);FilterConfig c;check(readFilterConfig(stream,c),"valid helper configuration");
        if(std::string(text)=="55 1 1 1 .45")check(c.alwaysCenter && c.centerTracking && c.centerWindow==.45,"always mode survives helper protocol");
    }
    for(const char *text:{"", "55", "55 1 3", "55 1 1 2 .25", "55 1 1 1", "55 1 1 1 nan", "55 1 1 1 .25 garbage"}){
        std::istringstream stream(text);FilterConfig c;check(!readFilterConfig(stream,c),"invalid helper configuration rejected");
    }
    for(int rate:{60,125,1000})for(double window:{.1,.25,.6})for(double distance:{1.,12.,160.}){
        Stabilizer f;f.configure({85,1,true,true,window});f.add(distance,-distance/2);Motion total;
        for(int i=0;i<rate*4;i++){auto d=f.step(1./rate);total.x+=d.x;total.y+=d.y;}
        check(std::abs(total.x-distance)<.001 && std::abs(total.y+distance/2)<.001,"full travel after center expires");
        f.add(200,0);f.step(.008);f.reset();check(f.step(.008).x==0,"click/pause clears always center");
        f.configure({0,.25,true,true,window});double pixels=0;
        for(int i=0;i<40;i++){f.add(1,0);pixels+=f.pixels(.008).x;}
        check(pixels==10,"fractional bypass");
        f.configure({85,1,true,true,window});check(f.step(.008).x==0,"bypass does not restore old center");
    }
    std::cout<<"PASS: helper protocol, always-center travel, reset, bypass\n";
}
