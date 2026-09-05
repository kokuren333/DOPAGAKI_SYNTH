#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <cstdio>

namespace dopa {
// IDs are a public compatibility contract. Append only; never reuse an ID.
enum Param : uint32_t { Gain=100, Shape, Detune, Spread, Sub, Cutoff, Resonance,
    Attack, Decay, Sustain, Release, Drive, LfoRate, LfoDepth, DelayMix,
    DelayTime, Feedback, Octave, Punch, Motion, Bend, Pedal,
    Step01, Step02, Step03, Step04, Step05, Step06, Step07, Step08,
    Step09, Step10, Step11, Step12, Step13, Step14, Step15, Step16,
    SeqRun, SeqTempo, SeqGate, WaveMix, WavePosition, WaveMotion, PhaseStart, Count };
struct Parameter { Param id; const char* name; double initial; int steps=0; };
inline constexpr std::array<Parameter,45> registry {{
    {Gain,"OUTPUT",.65},{Shape,"SHAPE",0,3},{Detune,"UNISON",.3},
    {Spread,"WIDTH",.8},{Sub,"SUB",.25},{Cutoff,"CUTOFF",.8},
    {Resonance,"RESONANCE",.2},{Attack,"ATTACK",.05},{Decay,"DECAY",.35},
    {Sustain,"SUSTAIN",.75},{Release,"RELEASE",.25},{Drive,"HEAT",.2},
    {LfoRate,"PULSE RATE",.35},{LfoDepth,"WOBBLE",0},{DelayMix,"ECHO",.2},
    {DelayTime,"ECHO TIME",.3},{Feedback,"FEEDBACK",.35},{Octave,"OCTAVE",.5,4},
    {Punch,"IMPACT",.2},{Motion,"GATE",0},{Bend,"PITCH BEND",.5},{Pedal,"SUSTAIN PEDAL",0,1},
    {Step01,"STEP 01",1./12,12},{Step02,"STEP 02",0,12},{Step03,"STEP 03",8./12,12},{Step04,"STEP 04",0,12},
    {Step05,"STEP 05",4./12,12},{Step06,"STEP 06",0,12},{Step07,"STEP 07",11./12,12},{Step08,"STEP 08",0,12},
    {Step09,"STEP 09",1./12,12},{Step10,"STEP 10",0,12},{Step11,"STEP 11",8./12,12},{Step12,"STEP 12",0,12},
    {Step13,"STEP 13",6./12,12},{Step14,"STEP 14",0,12},{Step15,"STEP 15",4./12,12},{Step16,"STEP 16",0,12},
    {SeqRun,"SEQUENCE PLAY",0,1},{SeqTempo,"SEQUENCE TEMPO",1./3},{SeqGate,"NOTE LENGTH",.7},
    {WaveMix,"TABLE MIX",0},{WavePosition,"FRAME",0},{WaveMotion,"FRAME MOTION",0},{PhaseStart,"START PHASE",0}
}};
inline constexpr size_t parameterCount=registry.size();
inline int index(uint32_t id) { return id>=Gain && id<Count ? int(id-Gain) : -1; }
inline double clean(double v) { return std::isfinite(v)?std::clamp(v,0.,1.):0.; }
using Values=std::array<double,parameterCount>;
inline Values defaults() { Values v{}; for(size_t i=0;i<v.size();++i)v[i]=registry[i].initial;return v; }
inline double seconds(double n) { return .001*std::pow(5000.,n); }
inline double hz(double n) { return 30.*std::pow(600.,n); }
inline std::string display(Param id,double n) {
    char s[64]{};
    if(id==Shape) { return std::array<const char*,4>{"SAW","PULSE","SINE","METAL"}[std::clamp(int(n*3+.5),0,3)]; }
    if(id==Octave) return std::to_string(int(n*4+.5)-2);
    if(id==WavePosition){std::snprintf(s,sizeof(s),"%.2f / 15",n*15);return s;}
    if(id==PhaseStart){std::snprintf(s,sizeof(s),"%.0f deg",n*360);return s;}
    if(id==Cutoff) std::snprintf(s,sizeof(s),"%.0f Hz",hz(n));
    else if(id>=Attack && id<=Release && id!=Sustain) std::snprintf(s,sizeof(s),"%.0f ms",1000*seconds(n));
    else if(id==LfoRate) std::snprintf(s,sizeof(s),"%.2f Hz",.1*std::pow(200.,n));
    else if(id==DelayTime) std::snprintf(s,sizeof(s),"%.0f ms",60+n*690);
    else std::snprintf(s,sizeof(s),"%.0f %%",100*n);
    return s;
}
}
