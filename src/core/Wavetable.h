#pragma once
#include <array>
#include <complex>
#include <cmath>
#include <algorithm>
#include <numbers>
#if (defined(_M_X64) || defined(__SSE__)) && !defined(DOPA_FORCE_SCALAR_WAVETABLE)
#include <xmmintrin.h>
#endif

namespace dopa {
// Immutable, shared, original harmonic frames. Construct on a non-audio thread.
// Each octave level removes upper partials BEFORE playback; a guarded crossfade
// between levels prevents timbre steps without blending an unsafe upper level.
class WavetableBank {
public:
    static constexpr int size=2048,frames=16,levels=10,maxHarmonic=512;
    using Cycle=std::array<float,size+1>;
    struct Reader {
        const Cycle *a,*b,*c,*d;double frameMix,levelMix;
        double operator()(double phase)const noexcept {
            const double x=phase*size;const int i=int(x);const double t=x-i;
#if (defined(_M_X64) || defined(__SSE__)) && !defined(DOPA_FORCE_SCALAR_WAVETABLE)
            // Four neighboring frame/mip cycles are independent: vectorize the
            // same cubic polynomial across them, not across unrelated voices.
            const int im=(i+size-1)&(size-1),ip=(i+1)&(size-1),ipp=(i+2)&(size-1);
            const auto y0=_mm_set_ps((*d)[im],(*c)[im],(*b)[im],(*a)[im]);
            const auto y1=_mm_set_ps((*d)[i],(*c)[i],(*b)[i],(*a)[i]);
            const auto y2=_mm_set_ps((*d)[ip],(*c)[ip],(*b)[ip],(*a)[ip]);
            const auto y3=_mm_set_ps((*d)[ipp],(*c)[ipp],(*b)[ipp],(*a)[ipp]);
            const auto u=_mm_set1_ps(float(t));
            const auto c0=_mm_sub_ps(y2,y0);
            const auto c1=_mm_sub_ps(_mm_add_ps(_mm_sub_ps(_mm_mul_ps(_mm_set1_ps(2),y0),_mm_mul_ps(_mm_set1_ps(5),y1)),_mm_mul_ps(_mm_set1_ps(4),y2)),y3);
            const auto c2=_mm_sub_ps(_mm_add_ps(_mm_mul_ps(_mm_set1_ps(3),_mm_sub_ps(y1,y2)),y3),y0);
            const auto value=_mm_add_ps(y1,_mm_mul_ps(_mm_mul_ps(_mm_set1_ps(.5f),u),_mm_add_ps(c0,_mm_mul_ps(u,_mm_add_ps(c1,_mm_mul_ps(u,c2))))));
            std::array<float,4> s{};_mm_storeu_ps(s.data(),value);
            const double lower=s[0]+(double(s[1])-s[0])*frameMix,upper=s[2]+(double(s[3])-s[2])*frameMix;
#else
            const auto interpolate=[&](const Cycle& table){
                const double y0=table[(i+size-1)&(size-1)],y1=table[i],y2=table[(i+1)&(size-1)],y3=table[(i+2)&(size-1)];
                return y1+.5*t*(y2-y0+t*(2*y0-5*y1+4*y2-y3+t*(3*(y1-y2)+y3-y0)));
            };
            const double p=interpolate(*a),q=interpolate(*c);
            const double lower=p+(interpolate(*b)-p)*frameMix,upper=q+(interpolate(*d)-q)*frameMix;
#endif
            return lower+(upper-lower)*levelMix;
        }
    };
    Reader reader(double position,double mip)const noexcept {
        const double frame=std::clamp(position,0.,1.)*(frames-1),level=std::clamp(mip,0.,double(levels-1));
        const int f=int(frame),g=std::min(f+1,frames-1),l=int(level),m=std::min(l+1,levels-1);
        return {&data[f][l],&data[g][l],&data[f][m],&data[g][m],frame-f,level-l};
    }
    const Cycle& cycle(int frame,int level)const {return data[frame][level];}
    static const WavetableBank& original(){static const WavetableBank bank;return bank;}
    double read(double phase,double position,double delta)const noexcept {
        phase-=std::floor(phase);
        return reader(position,std::log2(std::max(1.,delta*maxHarmonic/.225)))(phase);
    }
private:
    std::array<std::array<Cycle,levels>,frames> data{};
    static void inverse(std::array<std::complex<double>,size>& x) {
        for(int i=1,j=0;i<size;++i){int bit=size>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(x[i],x[j]);}
        for(int length=2;length<=size;length<<=1){auto wlen=std::polar(1.,2*std::numbers::pi/length);for(int start=0;start<size;start+=length){std::complex<double> w=1.;for(int j=0;j<length/2;++j){auto a=x[start+j],b=x[start+j+length/2]*w;x[start+j]=a+b;x[start+j+length/2]=a-b;w*=wlen;}}}
        for(auto& s:x)s/=size;
    }
    WavetableBank(){
        for(int f=0;f<frames;++f){
            std::array<double,maxHarmonic+1> amplitudes{};
            const double t=f/double(frames-1);
            amplitudes[1]=1;
            for(int h=2;h<=maxHarmonic;++h){
                const double body=std::pow(t,.65)/std::pow(double(h),1.1+.65*(1-t));
                const double formant=std::exp(-std::pow((h-(3+26*t))/(1.5+5*t),2)*.5);
                const double comb=.25+.75*std::pow(std::sin(h*(.35+1.2*t)),2);
                amplitudes[h]=body*((1-t)*comb+t)+.12*std::sin(t*std::numbers::pi)*formant;
            }
            double scale=1;
            for(int l=0;l<levels;++l){std::array<std::complex<double>,size> spectrum{};
                for(int h=1;h<=maxHarmonic>>l;++h){spectrum[h]={0,-amplitudes[h]*size*.5};spectrum[size-h]=std::conj(spectrum[h]);}
                inverse(spectrum);
                if(l==0){double peak=0;for(auto s:spectrum)peak=std::max(peak,std::abs(s.real()));scale=.95/std::max(.95,peak);}
                for(int i=0;i<size;++i)data[f][l][i]=float(spectrum[i].real()*scale);
                data[f][l][size]=data[f][l][0];
            }
        }
    }
};
}
