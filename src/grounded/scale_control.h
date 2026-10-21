#pragma once
#include "scale_presets.h"
#include <windows.h>
#include <array>
#include <atomic>
#include <cstring>
#include <cstdint>
#include <intrin.h>

namespace grounded::scale_control {
using SetText=void(__fastcall*)(void*,const wchar_t*,uint32_t);
using Notification=void(*)(const char*,double,unsigned);
using Trace=void(*)(const char*,double,unsigned,uint32_t,uintptr_t,DWORD);
inline std::atomic<SetText> original{nullptr};
inline std::atomic<void*> screen{nullptr},group{nullptr};
inline std::atomic<int> state{0}; // 0 awaiting objects; 1 installed; 2 other exe; -1 refused; 3 disabled.
inline std::atomic<double> requested{0};
inline std::atomic<unsigned> effective{0};
inline std::atomic<uint64_t> corrections{0};
inline std::atomic<uint64_t> lastEvent{0};
inline Notification notify=nullptr;
inline Trace trace=nullptr;
inline std::atomic<unsigned> traceCalls{0};
inline const char* refusal="";
inline constexpr uint32_t ScreenSlot=0x6215200,GroupSlot=0x6420330;
inline constexpr uint32_t VtableRva=0x4ea7b08,SetRva=0x1d1ba10,TypedDataRva=0xa90810;
inline constexpr uint32_t Timestamp=1745511144,ImageSize=111910912;
inline constexpr unsigned SetSlot=16;
inline constexpr std::array<unsigned char,32> SetterPrefix{
    0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x48,0x83,0xec,0x20,0x48,
    0x8b,0xea,0x41,0x8b,0xf0,0x41,0x8b,0xd0,0x48,0x8b,0xf9,0xe8,0xa0,0xc7,0xfe,0xff};
inline constexpr std::array<unsigned char,5> TypedDataPrefix{0x48,0x8d,0x41,0x48,0xc3};

inline bool Readable(const void* address,size_t bytes) {
    if(!address)return false;MEMORY_BASIC_INFORMATION info{};
    if(!VirtualQuery(address,&info,sizeof(info)) || info.State!=MEM_COMMIT ||
       (info.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
    const auto begin=reinterpret_cast<uintptr_t>(address),end=begin+bytes;
    return end>=begin && end<=reinterpret_cast<uintptr_t>(info.BaseAddress)+info.RegionSize;
}
inline void __fastcall Set(void* self,const wchar_t* value,uint32_t flags) {
    auto targetScreen=screen.load(std::memory_order_acquire),targetGroup=group.load(std::memory_order_acquire);
    auto invoke=original.load(std::memory_order_acquire);
    if(!invoke)return; // Published before installing the slot; unreachable in a valid installation.
    const bool ours=self==targetScreen || self==targetGroup;
    double number=0;
    if(ours && ParseUserScale(value,number)){
        const unsigned preset=NearestScalePreset(number);
        if(trace && traceCalls.fetch_add(1,std::memory_order_relaxed)<64)
            trace(self==targetGroup?"sg.ResolutionQuality":"r.ScreenPercentage",number,preset,flags,reinterpret_cast<uintptr_t>(_ReturnAddress()),GetCurrentThreadId());
        requested.store(number,std::memory_order_relaxed);effective.store(preset,std::memory_order_relaxed);
        if(std::abs(number-ScalePresetPercentage(preset))>1e-5){
            ++corrections;
            // Avoid log traffic if another engine component repeats the same request.
            float single=static_cast<float>(number);uint32_t bits=0;memcpy(&bits,&single,sizeof(bits));
            const uint64_t event=(uint64_t(bits)<<8)|preset;
            if(lastEvent.exchange(event,std::memory_order_relaxed)!=event && notify)
                notify(self==targetGroup?"sg.ResolutionQuality":"r.ScreenPercentage",number,preset);
        }
        value=ScalePresetText(preset); // Canonical even for a tiny fractional difference.
    }
    // Runs on the original caller's thread. Unreal still handles precedence,
    // callbacks and its game/render-thread copies of the value.
    invoke(self,value,flags);
}
inline void Configure(bool enabled,Notification callback,Trace traceCallback=nullptr) {
    notify=callback;trace=traceCallback;traceCalls=0;if(!enabled)state=3;
}
inline bool Refuse(const char* reason){refusal=reason;state=-1;return true;}
inline bool TryInstall() {
    if(state.load(std::memory_order_acquire)!=0)return false;
    auto module=GetModuleHandleW(nullptr);wchar_t path[32768]{};
    if(!GetModuleFileNameW(module,path,32768))return Refuse("Cannot identify executable");
    const auto slash=std::wcsrchr(path,L'\\');
    if(_wcsicmp(slash?slash+1:path,L"Maine-Win64-Shipping.exe")){state=2;return true;}
    const auto base=reinterpret_cast<uintptr_t>(module);
    auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if(!Readable(dos,sizeof(*dos)) || dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<0 || dos->e_lfanew>4096)
        return Refuse("Unrecognized DOS header");
    auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    if(!Readable(nt,sizeof(*nt)) || nt->Signature!=IMAGE_NT_SIGNATURE ||
       nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64 || nt->FileHeader.TimeDateStamp!=Timestamp ||
       nt->OptionalHeader.SizeOfImage!=ImageSize)return Refuse("Unrecognized Grounded executable version");
    auto table=reinterpret_cast<void**>(base+VtableRva);
    if(!Readable(table,20*sizeof(void*)) || table[SetSlot]!=reinterpret_cast<void*>(base+SetRva) ||
       table[12]!=reinterpret_cast<void*>(base+TypedDataRva))return Refuse("Console variable vtable differs");
    if(!Readable(reinterpret_cast<void*>(base+SetRva),SetterPrefix.size()) ||
       memcmp(reinterpret_cast<void*>(base+SetRva),SetterPrefix.data(),SetterPrefix.size()) ||
       memcmp(reinterpret_cast<void*>(base+TypedDataRva),TypedDataPrefix.data(),TypedDataPrefix.size()))
        return Refuse("Console variable method signature differs");
    auto first=*reinterpret_cast<void**>(base+ScreenSlot),second=*reinterpret_cast<void**>(base+GroupSlot);
    if(!first || !second)return false;
    if(!Readable(first,0x50) || !Readable(second,0x50) ||
       *reinterpret_cast<void***>(first)!=table || *reinterpret_cast<void***>(second)!=table)
        return Refuse("Observed console objects differ");
    DWORD protection=0;
    if(!VirtualProtect(table+SetSlot,sizeof(void*),PAGE_READWRITE,&protection))return Refuse("Cannot update console variable slot");
    original.store(reinterpret_cast<SetText>(base+SetRva),std::memory_order_release);
    screen.store(first,std::memory_order_release);group.store(second,std::memory_order_release);
    auto previous=InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(table+SetSlot),
        reinterpret_cast<void*>(&Set),reinterpret_cast<void*>(base+SetRva));
    DWORD unused=0;const bool restored=VirtualProtect(table+SetSlot,sizeof(void*),protection,&unused)!=0;
    if(previous!=reinterpret_cast<void*>(base+SetRva))return Refuse("Console slot changed during installation");
    if(!restored){
        InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(table+SetSlot),
            reinterpret_cast<void*>(base+SetRva),reinterpret_cast<void*>(&Set));
        return Refuse("Console slot protection could not be restored");
    }
    state.store(1,std::memory_order_release);return true;
}
}
