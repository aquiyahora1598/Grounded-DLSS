#pragma once
#include "scale_control.h"
#include <array>
#include <atomic>
#include <cmath>

namespace grounded::sharpness_control {
// Unreal caches two floats (game/render) returned by the tonemapper CVar's
// GetTypedData. Redirect only that verified cache to immutable pairs. The real
// CVar, its priority, callbacks, and both original values remain untouched.
struct Values {float game=0,render=0;};
struct Settings {
    bool enabled=false;
    float quality=0,balanced=0,performance=0,dlaa=0,ultraPerformance=0;
};
inline bool ParseStrength(const wchar_t* text,float& value) {
    if(!text)return false;
    wchar_t* end=nullptr;const double number=std::wcstod(text,&end);
    if(end==text)return false;
    while(*end && std::iswspace(*end))++end;
    if(*end || !std::isfinite(number) || number<0 || number>1)return false;
    value=static_cast<float>(number);return true;
}
inline Settings ReadSettings(const wchar_t* ini) {
    Settings result;result.enabled=GetPrivateProfileIntW(L"Sharpening",L"Enabled",0,ini)!=0;
    const wchar_t* keys[]={L"Quality",L"Balanced",L"Performance",L"DLAA",L"UltraPerformance"};
    float* outputs[]={&result.quality,&result.balanced,&result.performance,&result.dlaa,&result.ultraPerformance};
    for(unsigned i=0;i<5;++i){wchar_t text[128]{};
        if(GetPrivateProfileStringW(L"Sharpening",keys[i],L"",text,128,ini))ParseStrength(text,*outputs[i]);
    }
    return result;
}
class Controller {
    void* volatile* slot=nullptr;
    void* baseline=nullptr;
    void* current=nullptr;
    std::array<Values,5> values{}; // NGX Performance, Balanced, Quality, DLAA, Ultra.
    bool enabled=true;
public:
    std::atomic<int> mode{-1};
    std::atomic<float> strength{-1};
    std::atomic<uint64_t> changes{0};
    // Configure and Bind run before publishing state=1. Publish is serialized
    // by runtimeMutex together with DLSS evaluations, F8 and test controls.
    void Configure(const Settings& s) {
        enabled=s.enabled;
        const float ordered[]={s.performance,s.balanced,s.quality,s.dlaa,s.ultraPerformance};
        for(unsigned i=0;i<5;++i)values[i]={ordered[i],ordered[i]};
    }
    bool Bind(void* volatile* destination,void* expected) {
        if(!destination || !expected || InterlockedCompareExchangePointer(destination,nullptr,nullptr)!=expected)return false;
        slot=destination;baseline=current=expected;return true;
    }
    bool Publish(bool active,unsigned quality) {
        if(!slot)return true;
        const int index=quality<=2?int(quality):quality==5?3:quality==3?4:-1;
        const bool use=enabled && active && index>=0;
        void* desired=use?static_cast<void*>(&values[index]):baseline;
        if(current!=desired){
            // A foreign change must never be overwritten. Stop publishing if
            // another mod replaces this cache after our verified binding.
            if(InterlockedCompareExchangePointer(slot,desired,current)!=current)return false;
            current=desired;++changes;
        }
        mode=use?int(quality):-1;strength=use?values[index].render:-1;return true;
    }
};
inline Controller controller;
inline std::atomic<int> state{0}; // 0 waiting, 1 bound, 2 other exe, 3 disabled, -1 refused.
inline const char* refusal="";
inline constexpr uint32_t ObjectSlot=0x634b0f0,DataSlot=0x634b0f8,RegistrationRva=0x741c99;
inline constexpr std::array<unsigned char,62> Registration{
    0xc7,0x44,0x24,0x20,0x60,0x00,0x00,0x00,0x48,0x8d,0x15,0x00,0xcc,0x86,0x04,0xff,
    0x50,0x10,0x48,0x8d,0x0d,0x9e,0x3b,0x0f,0x04,0x48,0x89,0x05,0x37,0x94,0xc0,0x05,
    0x48,0x89,0x0d,0x28,0x94,0xc0,0x05,0x48,0x8b,0xc8,0x48,0x8b,0x10,0xff,0x52,0x60,
    0x48,0x8d,0x0d,0xe0,0xdc,0x01,0x04,0x48,0x89,0x05,0x21,0x94,0xc0,0x05};
inline bool Refuse(const char* reason){refusal=reason;state=-1;return true;}
inline void Configure(const Settings& settings) {
    controller.Configure(settings);if(!settings.enabled)state=3;
}
inline bool TryInstall() {
    if(state.load(std::memory_order_acquire)!=0)return false;
    const auto module=GetModuleHandleW(nullptr);wchar_t path[32768]{};
    if(!GetModuleFileNameW(module,path,32768))return Refuse("Cannot identify executable");
    const auto slash=std::wcsrchr(path,L'\\');
    if(_wcsicmp(slash?slash+1:path,L"Maine-Win64-Shipping.exe")){state=2;return true;}
    const auto base=reinterpret_cast<uintptr_t>(module);
    const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    using scale_control::Readable;
    if(!Readable(dos,sizeof(*dos)) || dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<0 || dos->e_lfanew>4096)
        return Refuse("Unrecognized DOS header");
    const auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    if(!Readable(nt,sizeof(*nt)) || nt->Signature!=IMAGE_NT_SIGNATURE || nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64 ||
       nt->FileHeader.TimeDateStamp!=scale_control::Timestamp || nt->OptionalHeader.SizeOfImage!=scale_control::ImageSize)
        return Refuse("Unrecognized Grounded executable version");
    if(!Readable(reinterpret_cast<void*>(base+RegistrationRva),Registration.size()) ||
       memcmp(reinterpret_cast<void*>(base+RegistrationRva),Registration.data(),Registration.size()))
        return Refuse("Tonemapper registration signature differs");
    const auto object=*reinterpret_cast<uintptr_t*>(base+ObjectSlot);
    if(!object)return false;
    const auto data=*reinterpret_cast<uintptr_t*>(base+DataSlot);
    // Registration publishes the object before publishing its data cache.
    if(!data)return false;
    const auto table=reinterpret_cast<void**>(base+scale_control::VtableRva);
    if(!Readable(reinterpret_cast<void*>(object),0x50) || *reinterpret_cast<void***>(object)!=table ||
       !Readable(table,20*sizeof(void*)) || table[12]!=reinterpret_cast<void*>(base+scale_control::TypedDataRva) ||
       memcmp(table[12],scale_control::TypedDataPrefix.data(),scale_control::TypedDataPrefix.size()) || data!=object+0x48)
        return Refuse("Tonemapper typed data differs");
    MEMORY_BASIC_INFORMATION info{};
    if(!VirtualQuery(reinterpret_cast<void*>(base+DataSlot),&info,sizeof(info)) || info.State!=MEM_COMMIT ||
       (info.Protect&(PAGE_GUARD|PAGE_NOACCESS)) || (info.Protect&0xff)!=PAGE_READWRITE)
        return Refuse("Tonemapper cache is not writable data");
    if(!controller.Bind(reinterpret_cast<void* volatile*>(base+DataSlot),reinterpret_cast<void*>(data)))
        return Refuse("Tonemapper cache changed during binding");
    state.store(1,std::memory_order_release);return true;
}
inline void Publish(bool active,unsigned quality) {
    if(state.load(std::memory_order_acquire)==1 && !controller.Publish(active,quality))Refuse("Tonemapper cache replaced externally");
}
}
