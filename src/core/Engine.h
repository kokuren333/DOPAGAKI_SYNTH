#pragma once
#include "Parameters.h"
#include "Saturation.h"
#include "Wavetable.h"
#include <vector>
#include <numbers>

namespace dopa {
inline constexpr double pi=std::numbers::pi;
struct Voice {
    bool active=false,held=false,deferred=false;
    int note=0,id=-1,channel=0,stage=0;uint64_t age=0;
    double velocity=0,env=0,releaseStep=0,time=0,baseFrequency=440;
    std::array<double,5> phase{};
    double subPhase=0;
    std::array<double,2> low{},band{};
};
class Engine {
public:
    Values target=defaults(),current=defaults();
    std::array<Voice,16> voices{};
    double sampleRate=48000,peak=0;
    int sequenceStep=-1;
    void prepare(double rate) {
        sampleRate=std::isfinite(rate)?std::clamp(rate,1000.,2000000.):48000.;
        delayL.assign(size_t(sampleRate*1.0)+8,0);delayR.assign(delayL.size(),0);
        table=&WavetableBank::original();smoothing=1-std::exp(-1/(.008*sampleRate));
        reset();
    }
    void reset() { voices={};std::fill(delayL.begin(),delayL.end(),0);std::fill(delayR.begin(),delayR.end(),0);write=0;lfoPhase=0;age=0;current=target;peak=0;saturators={};sequenceStep=-1;sequencePhase=0;sequenceNote=-1;previewNote=-1;previewMask=0; }
    // GUI sends the complete held-key set, so a rapid release cannot overflow a queue.
    void auditionKeys(uint64_t mask) {
        for(int i=0;i<37;++i)if((mask^previewMask)&(uint64_t(1)<<i)) {
            if(mask&(uint64_t(1)<<i))noteOn(48+i,.9,-200-i,15);
            else stopInternal(-200-i);
        }
        previewMask=mask;
    }
    void audition(int note) {
        if(note==previewNote)return;
        stopInternal(-100);
        previewNote=note;
        if(note>=0)noteOn(note,.9,-100,15);
    }
    void set(uint32_t id,double v) { int i=index(id);if(i>=0)target[i]=clean(v); }
    void noteOn(int note,double velocity,int id=-1,int channel=0) {
        if(note<0||note>127)return;
        if(velocity<=0){noteOff(note,id,channel);return;}
        Voice* selected=nullptr;
        for(auto& v:voices)if(!v.active){selected=&v;break;}
        if(!selected) {selected=&voices[0];for(auto& v:voices)if(v.age<selected->age)selected=&v;}
        *selected={};auto& v=*selected;v.active=v.held=true;v.note=note;v.id=id;v.channel=channel;v.velocity=clean(velocity);v.age=++age;
        v.baseFrequency=440*std::exp2((note-69)/12.);
        // Align reset phases. The old evenly-offset bank cancelled its fundamental
        // at zero detune; increasing gain could not restore those missing harmonics.
        for(int j=0;j<5;++j)v.phase[j]=target[index(PhaseStart)]==1?0:target[index(PhaseStart)];
    }
    void noteOff(int note,int id=-1,int channel=0) {
        for(auto& v:voices)if(v.active&&v.held&&v.id>-100&&v.channel==channel&&(id>=0?v.id==id:v.note==note)) {
            v.held=false;if(target[index(Pedal)]>=.5)v.deferred=true;else release(v);
        }
    }
    int activeVoices()const {int n=0;for(auto& v:voices)n+=v.active;return n;}
    std::array<float,2> tick() {
        sequenceTick();
        // One-pole smoothing is independent of host block size.
        for(size_t i=0;i<current.size();++i)current[i]+=smoothing*(target[i]-current[i]);
        auto p=[&](Param id){return current[index(id)];};
        lfoPhase+=.1*std::pow(200.,p(LfoRate))/sampleRate;lfoPhase-=std::floor(lfoPhase);
        const double lfo=std::sin(2*pi*lfoPhase);
        const double tablePosition=std::clamp(p(WavePosition)+lfo*p(WaveMotion)*.5,0.,1.);
        const double attackIncrement=1/(seconds(p(Attack))*sampleRate),decayIncrement=(1-p(Sustain))/(seconds(p(Decay))*sampleRate);
        const double pitchScale=std::exp2(int(target[index(Octave)]*4+.5)-2+4*(p(Bend)-.5)/12.);
        const double detuneOctaves=p(Detune)*.32/12,detune=std::exp2(detuneOctaves);
        const std::array<double,5> tuning{1/(detune*detune),1/detune,1,detune,detune*detune};
        std::array<double,5> panL{},panR{};
        for(int j=0;j<5;++j){double pan=(j-2)*.5*p(Spread);panL[j]=std::sqrt((1-pan)*.5);panR[j]=std::sqrt((1+pan)*.5);}
        const double cutoffBase=hz(p(Cutoff))*std::exp2(lfo*p(LfoDepth)*3),resonance=2-1.8*p(Resonance);
        const double fixedFilterG=std::tan(pi*std::clamp(cutoffBase,20.,sampleRate*.2)/sampleRate);
        const double gateGain=.48*(1-p(Motion)*(.5+.5*std::cos(2*pi*lfoPhase)));
        const int shape=int(target[index(Shape)]*3+.5);
        double left=0,right=0;
        for(auto& v:voices)if(v.active) {
            if(v.deferred&&target[index(Pedal)]<.5){v.deferred=false;release(v);}
            if(v.stage==0) {v.env+=attackIncrement;if(v.env>=1){v.env=1;v.stage=1;}}
            else if(v.stage==1) {v.env-=decayIncrement;if(v.env<=p(Sustain)){v.env=p(Sustain);v.stage=2;}}
            else if(v.stage==2) v.env=p(Sustain);
            else {v.env-=v.releaseStep;if(v.env<=0){v={};continue;}}
            v.time+=1/sampleRate;
            double frequency=v.baseFrequency*pitchScale;
            if(p(Punch)>0)frequency*=std::exp2(p(Punch)*std::exp(-v.time*65));
            const double baseDelta=frequency/sampleRate;
            const double baseMip=table&&p(WaveMix)>0?std::log2(std::max(1e-10,baseDelta*512/.225)):0;
            double a=0,b=0;
            for(int j=0;j<5;++j) {
                double dt=std::min(.45,baseDelta*tuning[j]);
                double phase=v.phase[j];
                double wave=0;
                if(p(WaveMix)<1||!table)wave=shape==0 ? 2*phase-1-blep(phase,dt) :
                    shape==1 ? (phase<.5?1.:-1.)+blep(phase,dt)-blep(std::fmod(phase+.5,1.),dt) :
                    shape==2 ? std::sin(2*pi*phase) : metallic(phase,dt);
                if(table&&p(WaveMix)>0)wave+=(table->reader(tablePosition,baseMip+(j-2)*detuneOctaves)(phase)-wave)*p(WaveMix);
                v.phase[j]=phase+dt;v.phase[j]-=std::floor(v.phase[j]);
                // Keep a coherent center while the four side voices provide width.
                const double weight=j==2?2.:.75;
                a+=wave*panL[j]*weight;b+=wave*panR[j]*weight;
            }
            v.subPhase+=std::min(.45,frequency*.5/sampleRate);v.subPhase-=std::floor(v.subPhase);
            double sub=std::sin(2*pi*v.subPhase)*p(Sub)*.5;
            a=a*.2+sub;b=b*.2+sub;
            const double cutoff=p(Punch)>0?std::clamp(cutoffBase*std::exp2(p(Punch)*v.env*2),20.,sampleRate*.2):0;
            // Topology-preserving state variable lowpass; bounded resonance.
            const double g=p(Punch)>0?std::tan(pi*cutoff/sampleRate):fixedFilterG,k=resonance;
            auto filter=[&](double x,int c) {
                double v1=(v.band[c]+g*(x-v.low[c]))/(1+g*(g+k));
                double v2=v.low[c]+g*v1;v.band[c]=2*v1-v.band[c];v.low[c]=2*v2-v.low[c];
                if(std::abs(v.band[c])<1e-20)v.band[c]=0;if(std::abs(v.low[c])<1e-20)v.low[c]=0;return v2;
            };
            const double amp=v.env*v.velocity*gateGain;
            left+=filter(a,0)*amp;right+=filter(b,1)*amp;
        }
        left=saturators[0].process(left,p(Drive));right=saturators[1].process(right,p(Drive));
        if(!delayL.empty()) {
            double delaySamples=(.06+p(DelayTime)*.69)*sampleRate;
            auto read=[&](const std::vector<double>& buffer,double offset){double pos=double(write)-offset;while(pos<0)pos+=buffer.size();size_t i=size_t(pos);double t=pos-i;return buffer[i]*(1-t)+buffer[(i+1)%buffer.size()]*t;};
            double dl=read(delayL,delaySamples),dr=read(delayR,delaySamples*1.013);
            delayL[write]=left+dr*p(Feedback)*.82;delayR[write]=right+dl*p(Feedback)*.82;write=(write+1)%delayL.size();
            left+=dl*p(DelayMix);right+=dr*p(DelayMix);
        }
        left=outputCeiling(left*p(Gain)*1.5);right=outputCeiling(right*p(Gain)*1.5);
        if(!std::isfinite(left)||!std::isfinite(right)){left=right=0;}
        peak=std::max(std::max(std::abs(left),std::abs(right)),peak*.9995);
        return {float(left),float(right)};
    }
private:
    const WavetableBank* table=nullptr;
    double smoothing=1-std::exp(-1/(.008*48000));
    std::array<Saturation,2> saturators{};
    double sequencePhase=0;int sequenceNote=-1,previewNote=-1;uint64_t previewMask=0;
    void stopInternal(int id){for(auto& v:voices)if(v.active&&v.channel==15&&v.id==id){v.held=v.deferred=false;release(v);}}
    void sequenceTick(){
        if(target[index(SeqRun)]<.5){if(sequenceStep>=0)stopInternal(-101);sequenceStep=-1;sequenceNote=-1;sequencePhase=0;return;}
        if(sequenceStep<0||sequencePhase>=1-1e-12){
            if(sequencePhase>=1-1e-12)sequencePhase=std::max(0.,sequencePhase-1);
            stopInternal(-101);sequenceStep=(sequenceStep+1)%16;
            const int code=int(target[index(Step01)+sequenceStep]*12+.5);
            sequenceNote=code>0?47+code:-1;
            if(sequenceNote>=0)noteOn(sequenceNote,.9,-101,15);
        }
        if(sequenceNote>=0&&sequencePhase>=.05+.9*target[index(SeqGate)]){stopInternal(-101);sequenceNote=-1;}
        sequencePhase+=(60+180*target[index(SeqTempo)])/(15*sampleRate);
    }
    std::vector<double> delayL,delayR;size_t write=0;uint64_t age=0;double lfoPhase=0;
    void release(Voice& v){v.stage=3;v.releaseStep=std::max(v.env,1e-9)/(seconds(target[index(Release)])*sampleRate);}
    static double blep(double t,double dt){if(t<dt){t/=dt;return t+t-t*t-1;}if(t>1-dt){t=(t-1)/dt;return t*t+t+t+1;}return 0;}
    static double metallic(double phase,double dt){double x=0;for(int h=1;h<=7;++h)if(h*dt<.45)x+=std::sin(2*pi*phase*h)/(h%2?double(h):double(h)*2);return x*.7;}
};
}
