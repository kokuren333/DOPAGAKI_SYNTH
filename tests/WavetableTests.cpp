#include "core/Wavetable.h"
#include "core/Engine.h"
#include "core/Presets.h"
#include <iostream>
#include <stdexcept>
#include <vector>
#include <iomanip>
using namespace dopa;
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
int main(){try{
    const auto& bank=WavetableBank::original();
    for(int f=0;f<16;++f)for(int l=0;l<10;++l){const auto& cycle=bank.cycle(f,l);double mean=0,peak=0;for(int i=0;i<2048;++i){check(std::isfinite(cycle[i]),"finite table");mean+=cycle[i];peak=std::max(peak,std::abs(double(cycle[i])));}check(std::abs(mean/2048)<1e-7,"table DC");check(peak<=1.1,"table amplitude");check(cycle[0]==cycle[2048],"periodic table boundary");}
    check(std::abs(bank.read(.25,0,.001)-.95)<1e-6,"frame zero must be sine");
    for(double phase:{.0,.123,.999999})check(std::abs(bank.read(phase,0.5,.001)-bank.read(phase+1,0.5,.001))<1e-12,"phase wrap");
    // Energy outside all physically permitted harmonics (including folded images).
    // Direct projections here are independent of the table constructor's IFFT.
    constexpr int n=8192;double worstDb=-300;
    for(int bin:{37,127,379,811})for(double position:{0.,.13,.5,1.}){
        std::vector<double> signal(n);double total=0,dc=0;
        for(int i=0;i<n;++i){signal[i]=bank.read(double(i)*bin/n,position,double(bin)/n);total+=signal[i]*signal[i];dc+=signal[i];}
        total/=n;double valid=0;for(int h=1;h<=int(.45*n/bin);++h){double a=0,b=0;for(int i=0;i<n;++i){a+=signal[i]*std::cos(2*pi*h*bin*i/n);b+=signal[i]*std::sin(2*pi*h*bin*i/n);}valid+=2*(a*a+b*b)/(double(n)*n);}
        double db=10*std::log10(std::max(1e-16,(total-valid)/std::max(total,1e-12)));worstDb=std::max(worstDb,db);check(db<-70,"wavetable high-register alias residual");check(std::abs(dc/n)<1e-7,"read DC");
    }
    // Both frame boundaries and guarded mip boundaries are continuous.
    for(int f=1;f<15;++f)for(int i=0;i<256;++i){double phase=i/256.;double pos=f/15.;check(std::abs(bank.read(phase,pos-1e-8,.004)-bank.read(phase,pos+1e-8,.004))<1e-5,"frame seam");}
    for(int level=1;level<9;++level){double delta=.225*std::exp2(level)/512;for(int i=0;i<256;++i)check(std::abs(bank.read(i/256.,.7,delta*(1-1e-8))-bank.read(i/256.,.7,delta*(1+1e-8)))<1e-5,"mip seam");}
    Engine engine,other;engine.prepare(48000);other.prepare(48000);engine.target=defaults();engine.set(WaveMix,1);engine.set(Drive,0);engine.set(Sub,0);engine.set(Punch,0);engine.set(DelayMix,0);other.target=engine.target;other.set(WavePosition,1);engine.reset();other.reset();engine.noteOn(69,1);other.noteOn(69,1);
    double difference=0;for(int i=0;i<4096;++i){double d=engine.tick()[0]-other.tick()[0];difference+=d*d;}check(difference>.01,"wave position has no audio effect");
    // v2 -> v3 adds bypassed table state; all old values must survive.
    std::ostringstream old;old<<"DOPAGAKI 2\n"<<std::setprecision(17);auto values=defaults();for(int i=0;i<41;++i)old<<registry[i].id<<" "<<values[i]<<"\n";Values migrated{};check(decode(old.str(),migrated),"v2 migration");check(migrated==values,"v2 values changed");
    check(factoryPresets().size()==96,"factory bank size");for(size_t i=64;i<96;++i)check(factoryPresets()[i].values[index(WaveMix)]==1,"new preset does not use table");
    std::cout<<"PASS: 160 tables DC/finite/wrap, sine reference, high-register alias residual "<<worstDb<<" dB worst, frame/mip continuity, audio morph, v2 migration, 96 presets\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
