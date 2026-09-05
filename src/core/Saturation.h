#pragma once
#include <cmath>

namespace dopa {
// First-order antiderivative antialiasing for a tanh waveshaper.
// F(x)=log(cosh(g*x))/g; divided differences integrate over each sample interval.
// Stable log-cosh avoids overflow at extreme signals. Mix=0 is exactly dry.
struct Saturation {
    double previous=0;
    static double primitive(double x,double gain) {
        const double a=std::abs(gain*x);
        return (a+std::log1p(std::exp(-2*a))-std::log(2.))/gain;
    }
    double process(double x,double amount) {
        const double g=1+amount*10,d=x-previous;
        const double wet=std::abs(d)<1e-7?std::tanh(g*(x+previous)*.5):
            (primitive(x,g)-primitive(previous,g))/d;
        previous=x;
        return x*(1-amount)+amount*wet/std::sqrt(g);
    }
};
inline double outputCeiling(double x) {
    // Unity transfer up to -1.4 dBFS; no always-on coloration of quiet sounds.
    const double a=std::abs(x);
    return a<=.85?x:std::copysign(.85+.149*std::tanh((a-.85)/.149),x);
}
}
