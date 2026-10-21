#pragma once
#include "scale_control.h"

namespace grounded::viewport_scale {
// Grounded's legacy fraction helper multiplies r.ScreenPercentage by
// GSystemResolution / borderless-window size. The latter can be stale at startup
// or after Alt+Enter, turning a DLSS preset into an unsupported render fraction.
// With preset snapping enabled, percentages refer to the actual temporal output.
inline std::atomic<int> state{0}; // 0 waiting; 1 normalized; 2 other exe; -1 refused; 3 disabled.
inline const char* refusal="";
inline constexpr uint32_t FunctionRva=0x38265d0,SiteRva=0x3826794,CallRva=0x3759ef7;
inline constexpr std::array<unsigned char,32> Prefix{
    0x48,0x83,0xec,0x48,0x0f,0x29,0x74,0x24,0x30,0xf3,0x0f,0x10,0x35,0x4b,0x1f,0x18,
    0x02,0x0f,0x29,0x7c,0x24,0x20,0x0f,0x28,0xfe,0x48,0x89,0x5c,0x24,0x58,0x48,0x89};
inline constexpr std::array<unsigned char,25> Context{
    0x0f,0x28,0xce,0x0f,0x28,0x74,0x24,0x30,0xf3,0x0f,0x59,0xcf,0x0f,0x28,0x7c,0x24,
    0x20,0x0f,0x28,0xc1,0x48,0x83,0xc4,0x48,0xc3};
inline constexpr uint32_t Multiply=0xcf590ff3,Nop=0x00401f0f;

inline bool Replace(void* address,uint32_t expected,uint32_t replacement) {
    if(reinterpret_cast<uintptr_t>(address)%4 || !scale_control::Readable(address,4))return false;
    DWORD protection=0,unused=0;
    if(!VirtualProtect(address,4,PAGE_EXECUTE_READWRITE,&protection))return false;
    const auto previous=InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address),static_cast<LONG>(replacement),static_cast<LONG>(expected));
    const bool restored=VirtualProtect(address,4,protection,&unused)!=0;
    FlushInstructionCache(GetCurrentProcess(),address,4);
    if(!restored && static_cast<uint32_t>(previous)==expected){
        InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address),static_cast<LONG>(expected),static_cast<LONG>(replacement));
        FlushInstructionCache(GetCurrentProcess(),address,4);
    }
    return restored && static_cast<uint32_t>(previous)==expected;
}
inline bool Refuse(const char* message){refusal=message;state=-1;return true;}
inline void Configure(bool enabled){if(!enabled)state=3;}
inline bool TryInstall() {
    if(state!=0)return false;
    const auto scaleState=scale_control::state.load(std::memory_order_acquire);
    if(scaleState==0)return false;
    if(scaleState!=1){state=scaleState;return true;}
    // scale_control already verified the executable, PE version and CVar objects.
    const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    const auto prefix=reinterpret_cast<const void*>(base+FunctionRva);
    const auto context=reinterpret_cast<const void*>(base+SiteRva-8);
    const std::array<unsigned char,5> call{0xe8,0xd4,0xc6,0x0c,0x00};
    if(!scale_control::Readable(prefix,Prefix.size()) || memcmp(prefix,Prefix.data(),Prefix.size()) ||
       !scale_control::Readable(context,Context.size()) || memcmp(context,Context.data(),Context.size()) ||
       !scale_control::Readable(reinterpret_cast<void*>(base+CallRva),call.size()) ||
       memcmp(reinterpret_cast<void*>(base+CallRva),call.data(),call.size()))
        return Refuse("Legacy viewport fraction signature differs");
    if(!Replace(reinterpret_cast<void*>(base+SiteRva),Multiply,Nop))return Refuse("Cannot normalize viewport fraction");
    state.store(1,std::memory_order_release);return true;
}
}
