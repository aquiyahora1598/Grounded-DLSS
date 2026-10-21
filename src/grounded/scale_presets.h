#pragma once
#include <array>
#include <cmath>
#include <cwchar>
#include <cwctype>

namespace grounded {
// Public labels are integers; Ultra uses NVIDIA's fixed one-third viewport.
// Unreal rounds the viewport up, while NGX rounds W/3 to nearest: 33.333332
// gave 854 columns at 2560 against NGX's 853, forcing a full-image resample.
// 33.32 rounds up to NGX's size at 1920, 2560, 3440 and 3840 wide and at
// 1080, 1200, 1440 and 2160 high. At an exact midpoint prefer the higher
// resolution.
inline constexpr std::array<unsigned,5> ScalePresets{33,50,58,67,100};
inline double ScalePresetPercentage(unsigned preset) {return preset==33?double(33.32f):double(preset);}
inline unsigned NearestScalePreset(double requested) {
    unsigned selected=ScalePresets.front();
    double distance=std::abs(requested-ScalePresetPercentage(selected));
    for(auto preset:ScalePresets){
        const auto next=std::abs(requested-ScalePresetPercentage(preset));
        if(next<=distance){distance=next;selected=preset;}
    }
    return selected;
}
inline bool ParseUserScale(const wchar_t* text,double& requested) {
    if(!text)return false;
    wchar_t* end=nullptr;requested=std::wcstod(text,&end);
    if(end==text)return false;
    while(*end && std::iswspace(*end))++end;
    // Nonpositive engine values select automatic/default scaling. Keep those
    // sentinels intact; the user-facing slider emits positive percentages.
    return !*end && std::isfinite(requested) && requested>0;
}
inline const wchar_t* ScalePresetText(unsigned preset) {
    switch(preset){case 33:return L"33.32";case 50:return L"50";case 58:return L"58";case 67:return L"67";default:return L"100";}
}
}
