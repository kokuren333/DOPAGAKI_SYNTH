#pragma once
#include "Parameters.h"
#include <vector>
#include <sstream>
#include <iomanip>
#include <locale>
#include <set>

namespace dopa {
// Envelope parameters are logarithmic: author durations in seconds, not percentages.
inline double envelopeTime(double secondsValue){return clean(std::log(std::max(.001,secondsValue)/.001)/std::log(5000.));}
struct Preset { std::string name,category; Values values=defaults(); };
inline const std::vector<Preset>& factoryPresets() {
    // Independently authored recipes. Each family changes its envelope, register,
    // oscillator and motion, not just its name or master gain.
    static const auto bank=[] {
        std::vector<Preset> out;
        const char* families[]={"NEON LEAD","RUSH BASS","PRISM PLUCK","AFTERGLOW PAD","LASER KEY","CHAOS MOTION","SUB DROP","CHROME REESE"};
        const char* variants[]={"Ignition","Voltage","Overdrive","Mirage","Starlight","Jackpot","Hyperdrive","Finale"};
        for(int f=0;f<8;++f)for(int v=0;v<8;++v) {
            Preset p{std::string(families[f])+" / "+variants[v],families[f]};
            auto set=[&](Param id,double n){p.values[index(id)]=n;};
            const double t=v/7.;
            set(Detune,.12+.65*t);set(Cutoff,.5+.45*t);set(Drive,.08+.48*t);
            set(DelayTime,.16+.5*t);set(Punch,.15+.5*t);
            switch(f) {
            case 0:set(Shape,v%2/3.);set(Release,.18+.2*t);set(DelayMix,.18+.14*t);break;
            case 1:set(Octave,.25);set(Sub,.65);set(Cutoff,.3+.24*t);set(Decay,.25);set(Sustain,.35);set(DelayMix,0);set(LfoDepth,.25+.6*t);set(LfoRate,.55+.3*t);break;
            case 2:set(Shape,(v%3)/3.);set(Decay,.15+.18*t);set(Sustain,0);set(Release,.15);set(DelayMix,.3);set(Punch,.7);break;
            case 3:set(Attack,.72+.2*t);set(Release,.7+.23*t);set(Punch,0);set(Drive,.05);set(DelayMix,.32);set(Feedback,.55);set(LfoDepth,.13);set(LfoRate,.1+.2*t);break;
            case 4:set(Shape,1);set(Octave,.75);set(Decay,.18+.3*t);set(Sustain,.15);set(Sub,0);set(DelayMix,.26);break;
            case 5:set(Motion,.35+.6*t);set(LfoRate,.62+.3*t);set(LfoDepth,.65);set(Shape,(v%4)/3.);set(Feedback,.4);break;
            case 6:set(Shape,2./3);set(Octave,0);set(Sub,.8);set(Detune,0);set(Spread,0);set(DelayMix,0);set(Decay,.22+.3*t);set(Sustain,0);set(Punch,.7);break;
            case 7:set(Octave,.25);set(Detune,.35+.6*t);set(Cutoff,.25+.38*t);set(Sub,.45);set(DelayMix,.04);set(LfoDepth,.3);set(LfoRate,.2);break;
            }
            out.push_back(p);
        }
        const char* waveFamilies[]={"GLASS VECTOR","FORMANT RUSH","MORPH BLOOM","TABLE SPARK"};
        for(int family=0;family<4;++family)for(int variant=0;variant<8;++variant){
            Preset p{std::string(waveFamilies[family])+" / "+variants[variant],waveFamilies[family]};
            auto set=[&](Param id,double value){p.values[index(id)]=value;};double t=variant/7.;
            set(WaveMix,1);set(WavePosition,.08+.84*t);set(WaveMotion,.1+.5*t);set(Punch,0);set(Sub,.08);set(Drive,.08);set(Cutoff,.85);set(Detune,.25);set(DelayMix,.22);
            if(family==0){set(Octave,.75);set(Decay,.38);set(Sustain,.32);set(Release,.38);set(PhaseStart,.25);}
            if(family==1){set(Octave,.25);set(Sub,.4);set(WaveMotion,.6);set(LfoRate,.55+.3*t);set(Drive,.28);set(DelayMix,.03);set(Detune,.1);}
            if(family==2){set(Attack,.65);set(Release,.7);set(Detune,.6);set(LfoRate,.18);set(DelayMix,.35);set(Feedback,.5);}
            if(family==3){set(Decay,.3);set(Sustain,0);set(Release,.2);set(Punch,.18);set(DelayMix,.3);set(WaveMotion,.2);}
            out.push_back(p);
        }
        // v0.4 voicing: preserve names/IDs, replace accidental 4-20 ms decays.
        for(size_t i=0;i<out.size();++i){
            auto& values=out[i].values;const int family=int(i/8);double t=(i%8)/7.;
            auto set=[&](Param id,double value){values[index(id)]=clean(value);};
            auto envelope=[&](double a,double d,double s,double r){set(Attack,envelopeTime(a));set(Decay,envelopeTime(d));set(Sustain,s);set(Release,envelopeTime(r));};
            set(Punch,.025+.075*t);set(Resonance,.12);set(Gain,.72);
            switch(family){
            case 0: envelope(.003,.28,.82,.18+.12*t);set(Cutoff,.78+.17*t);set(Sub,.28);set(Detune,.28+.35*t);set(Drive,.16+.12*t);break;
            case 1: envelope(.004,.2,.68,.085);set(Cutoff,.56+.24*t);set(LfoDepth,.1+.22*t);set(Detune,.08+.18*t);set(Drive,.2+.18*t);break;
            case 2: envelope(.002,.22+.26*t,0,.16);set(Cutoff,.75+.2*t);set(Shape,(i%2)/3.);break;
            case 3: envelope(.18+.42*t,.8,.88,1.1+1.2*t);set(Cutoff,.67+.22*t);set(Sub,.3);break;
            case 4: envelope(.003,.32+.25*t,.42,.24);set(Octave,.5);set(Sub,.2);set(Cutoff,.78+.17*t);break;
            case 5: envelope(.004,.3,.82,.18);set(Motion,.25+.4*t);set(LfoDepth,.1+.24*t);set(Cutoff,.77+.18*t);set(Sub,.35);break;
            case 6: envelope(.003,.6+.5*t,.3,.2);set(Octave,.25);set(Punch,.13+.14*t);set(Cutoff,.7);break;
            case 7: envelope(.005,.35,.85,.16);set(Cutoff,.56+.22*t);set(LfoDepth,.16);set(Drive,.2);break;
            case 8: envelope(.003,.42+.25*t,.38,.3);set(Octave,.5);set(Sub,.23);break;
            case 9: envelope(.004,.25,.78,.12);set(Sub,.5);set(WaveMotion,.2+.25*t);break;
            case 10: envelope(.15+.3*t,.7,.9,1.2);set(Sub,.25);break;
            case 11: envelope(.002,.2+.3*t,0,.19);set(Sub,.23);break;
            }
        }
        return out;
    }();return bank;
}
// Human-readable, bounded strict format; never evaluate code from a preset.
inline std::string encode(const Values& v) {
    std::ostringstream s;s.imbue(std::locale::classic());s<<"DOPAGAKI 3\n"<<std::setprecision(17);
    for(size_t i=0;i<v.size();++i)s<<registry[i].id<<" "<<v[i]<<"\n";
    return s.str();
}
inline bool decode(const std::string& text,Values& destination) {
    if(text.size()>16384)return false;
    std::istringstream s(text);s.imbue(std::locale::classic());std::string magic;int version=0;
    if(!(s>>magic>>version)||magic!="DOPAGAKI"||(version<1||version>3))return false;
    Values v=defaults();std::set<uint32_t> seen;uint32_t id;double n;
    while(s>>id) { if(!(s>>n)||!std::isfinite(n)||n<0||n>1||index(id)<0||!seen.insert(id).second)return false;v[index(id)]=n; }
    const size_t expected=version==1?22:version==2?41:parameterCount;
    if(!s.eof()||seen.size()!=expected)return false;
    for(size_t i=0;i<expected;++i)if(!seen.count(registry[i].id))return false;
    destination=v;return true;
}
}
