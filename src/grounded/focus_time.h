#pragma once
#include <cmath>
namespace grounded::focus_time {
// This replaces only the native autofocus's frame-count dependent 0.1 blend.
// The game's depth sample, target, clamping and depth-to-blur mapping stay native.
inline double Coefficient(double elapsed,double timeConstant=.25) noexcept {
    if(!std::isfinite(elapsed)||elapsed<=0||elapsed>.5||!std::isfinite(timeConstant)||timeConstant<.05||timeConstant>2)return .1;
    return -std::expm1(-elapsed/timeConstant);
}
}
