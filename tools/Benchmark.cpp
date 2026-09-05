#include "core/Engine.h"
#include <chrono>
#include <iostream>
#include <iomanip>
int main(){using namespace dopa;std::cout<<"sample_rate,wavetable,voices,seconds_for_one_second_audio,peak\n";for(double rate:{48000.,96000.,192000.})for(int wave:{0,1}){Engine e;e.prepare(rate);e.set(WaveMix,wave);e.set(WavePosition,.65);e.set(WaveMotion,.4);e.set(Punch,0);e.reset();for(int n=0;n<16;++n)e.noteOn(36+n,1,n);auto start=std::chrono::steady_clock::now();for(int i=0;i<int(rate);++i)e.tick();double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();std::cout<<int(rate)<<","<<wave<<",16,"<<std::fixed<<std::setprecision(6)<<elapsed<<","<<e.peak<<"\n";}}
