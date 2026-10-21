#pragma once
#include <cstdint>

namespace grounded {
inline bool SupportsSecondGenerationTransformer(uint32_t architecture) {
    // NVAPI architecture IDs: Turing=0x160, Ampere=0x170,
    // Ada=0x190, Blackwell=0x1b0. Exclude unknown/Tegra sentinel IDs.
    return architecture>=0x190 && architecture<0x10000;
}
inline bool ValidPresetOverride(unsigned value) {return value==5 || value==11 || value==12 || value==13;}
inline unsigned RecommendedRenderPreset(unsigned ngxMode,uint32_t architecture,unsigned overridePreset=0) {
    if(ValidPresetOverride(overridePreset))return overridePreset;
    if(!SupportsSecondGenerationTransformer(architecture))return 11; // K, including unknown GPU.
    return ngxMode==0?13:ngxMode==3?12:11; // Performance M; Ultra L; other modes K.
}
}
