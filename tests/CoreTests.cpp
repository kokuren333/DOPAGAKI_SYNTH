#include "core/Engine.h"
#include "core/Presets.h"
#include "ui/Piano.h"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <chrono>
#include <filesystem>

void require(bool test,const char* message){if(!test)throw std::runtime_error(message);}
void wav(const std::filesystem::path& path,const std::vector<float>& samples){std::ofstream s(path,std::ios::binary);auto u16=[&](uint16_t x){s.put(char(x));s.put(char(x>>8));};auto u32=[&](uint32_t x){u16(uint16_t(x));u16(uint16_t(x>>16));};s.write("RIFF",4);u32(36+uint32_t(samples.size()*2));s.write("WAVEfmt ",8);u32(16);u16(1);u16(2);u32(48000);u32(192000);u16(4);u16(16);s.write("data",4);u32(uint32_t(samples.size()*2));for(float v:samples)u16(uint16_t(int16_t(std::clamp(v,-1.f,1.f)*32767)));}
int main(int argc,char** argv){try{
    using namespace dopa;
    Values parsed{};auto init=defaults();require(decode(encode(init),parsed)&&parsed==init,"state roundtrip");
    require(!decode("DOPAGAKI 99\n",parsed),"unknown version accepted");require(!decode("DOPAGAKI 1\n100 nan",parsed),"NaN accepted");require(!decode(encode(init)+"100 0.5\n",parsed),"duplicate accepted");
    {std::ostringstream old;old<<"DOPAGAKI 1\n";for(int i=0;i<22;++i)old<<registry[i].id<<" "<<init[i]<<"\n";require(decode(old.str(),parsed),"v1 migration");require(parsed[index(SeqRun)]==0&&parsed[index(Step01)]==defaults()[index(Step01)],"migration sequence defaults");}
    Engine a,b;a.prepare(48000);b.prepare(48000);a.noteOn(69,1);b.noteOn(69,1);double energy=0;
    for(int i=0;i<48000;++i){auto x=a.tick(),y=b.tick();require(x==y,"nondeterministic engine");energy+=x[0]*x[0];}require(energy>1,"silent note");
    a.noteOff(69);for(int i=0;i<48000*6;++i)a.tick();require(a.activeVoices()==0,"stuck release");
    a.reset();a.set(Pedal,1);a.noteOn(60,1);for(int i=0;i<1000;++i)a.tick();a.noteOff(60);for(int i=0;i<48000;++i)a.tick();require(a.activeVoices()==1,"pedal failed");a.set(Pedal,0);for(int i=0;i<48000*6;++i)a.tick();require(a.activeVoices()==0,"pedal stuck");
    a.reset();for(int n=0;n<32;++n)a.noteOn(40+n,1,n);require(a.activeVoices()==16,"voice bound");
    // Fundamental check on clean sine, no pitch transient or detune.
    a.target=defaults();a.set(Shape,2./3);a.set(Detune,0);a.set(Sub,0);a.set(Punch,0);a.set(DelayMix,0);a.set(Drive,0);a.reset();a.noteOn(69,1);for(int i=0;i<48000;++i)a.tick();int crossings=0;float prev=0;for(int i=0;i<48000;++i){float x=a.tick()[0];if(prev<=0&&x>0)++crossings;prev=x;}require(std::abs(crossings-440)<=1,"frequency error");
    // No cancellation at zero detune: clean centered sine must reach useful level.
    a.set(Spread,0);a.set(Gain,1);a.set(Sustain,1);a.reset();a.noteOn(69,1);for(int i=0;i<48000;++i)a.tick();double rms=0;for(int i=0;i<48000;++i){auto x=a.tick();rms+=x[0]*x[0];require(std::abs(x[0]-x[1])<1e-7,"center stereo mismatch");}rms=std::sqrt(rms/48000);require(rms>.3&&rms<.4,"unison cancellation or bad gain staging");
    a.target=defaults();a.set(SeqRun,1);a.reset();for(int i=0;i<6000;++i)a.tick();require(a.sequenceStep==0,"sequencer first step");a.tick();require(a.sequenceStep==1,"sequencer tempo");a.set(SeqRun,0);a.tick();require(a.sequenceStep==-1,"sequencer stop");
    a.reset();a.audition(60);require(a.activeVoices()==1,"audition note");a.audition(-1);for(int i=0;i<48000;++i)a.tick();require(a.activeVoices()==0,"audition release");
    // Hit black keys before the white keys behind them; lower white regions remain playable.
    require(piano::hit(200,660)==49&&piano::hit(200,735)==48,"piano black-key overlap");
    require(piano::hit(1150,735)==84&&piano::hit(1200,735)==-1,"piano range");
    a.reset();a.auditionKeys((1ull<<0)|(1ull<<4)|(1ull<<7));require(a.activeVoices()==3,"preview chord");
    a.auditionKeys(1ull<<4);for(int i=0;i<48000;++i)a.tick();require(a.activeVoices()==1,"preview partial release");
    a.noteOff(52,-1,15);require(a.voices[1].held,"host note off stopped preview");a.auditionKeys(0);for(int i=0;i<48000;++i)a.tick();require(a.activeVoices()==0,"preview chord stuck");
    // Plucks must have a musical body beyond the first 100 ms, with echo disabled.
    for(int family:{2,11})for(int variant=0;variant<8;++variant){a.target=factoryPresets()[family*8+variant].values;a.set(DelayMix,0);a.reset();a.noteOn(60,1);double body=0;for(int i=0;i<7200;++i){auto v=a.tick();if(i>=4800)body+=v[0]*v[0]+v[1]*v[1];}require(body/4800>1e-5,"pluck loses its body before 150 ms");}
    for(double x:{-.7,-.1,0.,.1,.7}){Saturation sat;require(sat.process(x,0)==x,"drive zero must be dry");require(outputCeiling(x)==x,"master colors quiet signal");}
    // Coherent high-frequency sine: folded odd harmonics above Nyquist are aliases.
    {constexpr int n=4096,bin=407;std::array<double,n> aa{},naive{};Saturation sat;sat.previous=.7*std::sin(2*pi*bin*(n-1)/n);
        for(int i=0;i<n;++i){double x=.7*std::sin(2*pi*bin*i/n);aa[i]=sat.process(x,1);naive[i]=std::tanh(11*x)/std::sqrt(11.);}
        auto alias=[&](const auto& signal){double total=0;for(int harmonic=7;harmonic<=31;harmonic+=2){int k=(bin*harmonic)%n;double real=0,imag=0;for(int i=0;i<n;++i){real+=signal[i]*std::cos(2*pi*k*i/n);imag+=signal[i]*std::sin(2*pi*k*i/n);}total+=real*real+imag*imag;}return total;};
        double reduction=10*std::log10(alias(aa)/alias(naive));std::cout<<"Saturation alias energy vs naive (407/4096 Fs): "<<reduction<<" dB\n";require(reduction<-3,"ADAA alias reduction regression");}
    auto start=std::chrono::steady_clock::now();double bankEnergy=0;
    for(double rate:{44100.,48000.,88200.,96000.,176400.,192000.}){
        a.prepare(rate);
        for(auto& preset:factoryPresets()){
            require(decode(encode(preset.values),parsed)&&parsed==preset.values,"preset roundtrip");a.target=preset.values;a.reset();a.noteOn(60,1);double e=0;
            for(int i=0;i<int(rate*.16);++i){auto x=a.tick();for(float v:x){require(std::isfinite(v)&&std::abs(v)<=1,"unstable output");e+=v*v;}}
            require(e>1e-10,"silent factory preset");bankEnergy+=e;
        }
    }
    // Worst controls and dense chords remain finite.
    a.prepare(48000);a.target.fill(1);a.set(Pedal,0);a.reset();for(int n=0;n<16;++n)a.noteOn(80+n,1,n);for(int i=0;i<96000;++i)for(float x:a.tick())require(std::isfinite(x)&&std::abs(x)<=1,"extreme instability");
    if(argc>1){std::filesystem::path output=argv[1];std::filesystem::create_directories(output);std::vector<float> song;for(int f=0;f<8;++f){a.prepare(48000);a.target=factoryPresets()[size_t(f*8+3)].values;a.reset();for(int i=0;i<144000;++i){if(i%24000==0){int n=std::array<int,6>{48,55,60,63,60,55}[i/24000];a.noteOn(n,1,i/24000);}if(i%24000==15000){int n=std::array<int,6>{48,55,60,63,60,55}[i/24000];a.noteOff(n,i/24000);}auto x=a.tick();song.insert(song.end(),x.begin(),x.end());}}wav(output/"factory-showcase.wav",song);
        std::vector<float> waveSong;
        for(int family=0;family<4;++family){a.prepare(48000);a.target=factoryPresets()[size_t(64+family*8+4)].values;a.reset();a.noteOn(60,.9);for(int i=0;i<144000;++i){if(i==96000)a.noteOff(60);auto x=a.tick();waveSong.insert(waveSong.end(),x.begin(),x.end());}}
        wav(output/"wavetable-showcase.wav",waveSong);
        for(size_t i=0;i<factoryPresets().size();++i){auto& p=factoryPresets()[i];std::ofstream s(output/("preset-"+std::to_string(i+1)+".dopa"));s<<encode(p.values);}
    }
    std::cout<<"PASS: state, 96 presets x 6 sample rates, deterministic audio, release, pedal, 16 voice limit, A440, finite extremes. Energy="<<bankEnergy<<"; bank test seconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<"\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}

