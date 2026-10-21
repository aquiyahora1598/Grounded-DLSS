#pragma once
#include "focus_time.h"
#include "scale_control.h"
#include <intrin.h>
extern "C" void GroundedFocusStep();
namespace grounded::focus_control {
inline std::atomic<int> state{0};
inline std::atomic<uint64_t> updates{0};
inline std::atomic<double> lastInterval{0},lastCoefficient{.1};
inline double timeConstant=.25;
inline const char* refusal="";
inline LARGE_INTEGER frequency{};
inline bool Refuse(const char* why){refusal=why;state=-1;return false;}
inline void Configure(const wchar_t* ini) {
    if(!GetPrivateProfileIntW(L"DepthOfField",L"FixFocusTiming",1,ini)){state=3;return;}
    wchar_t text[128]{};GetPrivateProfileStringW(L"DepthOfField",L"TimeConstantSeconds",L"0.25",text,128,ini);
    wchar_t* end=nullptr;double parsed=std::wcstod(text,&end);
    if(end!=text&&!*end&&std::isfinite(parsed)&&parsed>=.05&&parsed<=2)timeConstant=parsed;
}
inline bool TryInstall() {
    if(state!=0)return state==1;
    auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));wchar_t path[32768]{};
    GetModuleFileNameW(nullptr,path,32768);auto name=std::wcsrchr(path,L'\\');
    if(_wcsicmp(name?name+1:path,L"Maine-Win64-Shipping.exe")){state=2;return false;}
    const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<=0||dos->e_lfanew>4096)return Refuse("DOS header differs");
    const auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.TimeDateStamp!=scale_control::Timestamp||nt->OptionalHeader.SizeOfImage!=scale_control::ImageSize)return Refuse("Game version differs");
    constexpr unsigned char signature[]={0xf3,0x0f,0x10,0xa7,0x40,0x19,0x00,0x00,0x0f,0x28,0xde,0xf3,0x0f,0x10,0x0d,0x31,0x25,0x36,0x03,0xf3,0x0f,0x5f,0xc1,0xf3,0x0f,0x11,0x7c,0x24,0x20,0xf3,0x0f,0x5c,0xc4,0xf3,0x0f,0x59,0x05,0xfb,0x2d,0x01,0x03,0xf3,0x0f,0x58,0xc4,0xf3,0x0f,0x11,0x87,0x40,0x19,0x00,0x00,0xe8,0x82,0x80,0xfe,0x01,0x80,0x8b,0x57,0x02,0x00,0x00,0x01};
    if(memcmp(reinterpret_cast<void*>(base+0x1825fd4),signature,sizeof(signature)))return Refuse("Native autofocus signature differs");
    if(!IsProcessorFeaturePresent(PF_COMPARE_EXCHANGE128)||!QueryPerformanceFrequency(&frequency))return Refuse("Atomic patch or monotonic clock unavailable");
    // All changed bytes fit inside one aligned 16-byte block. Publish in one
    // CMPXCHG16B rather than exposing a partly written instruction to threads.
    auto block=reinterpret_cast<unsigned char*>(base+0x1825ff0);
    alignas(16) long long expected[2]{},desired[2]{};memcpy(expected,block,16);memcpy(desired,block,16);
    void* relay=nullptr;
    for(uintptr_t distance=0x10000;distance<0x70000000&&!relay;distance+=0x10000){
        for(int sign:{1,-1}){auto hint=(base&~uintptr_t(0xffff))+sign*distance;
            relay=VirtualAlloc(reinterpret_cast<void*>(hint),0x1000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(relay)break;}
    }
    if(!relay)return Refuse("No nearby relay page available");
    unsigned char jump[14]={0xff,0x25,0,0,0,0};auto destination=reinterpret_cast<uintptr_t>(&GroundedFocusStep);memcpy(jump+6,&destination,8);memcpy(relay,jump,14);
    DWORD ignored=0;if(!VirtualProtect(relay,0x1000,PAGE_EXECUTE_READ,&ignored)){VirtualFree(relay,0,MEM_RELEASE);return Refuse("Cannot protect relay");}
    const int64_t delta=reinterpret_cast<intptr_t>(relay)-intptr_t(base+0x1825ffa);
    if(delta<INT32_MIN||delta>INT32_MAX){VirtualFree(relay,0,MEM_RELEASE);return Refuse("Relay outside call range");}
    auto replacement=reinterpret_cast<unsigned char*>(desired)+5;replacement[0]=0xe8;auto displacement=int32_t(delta);memcpy(replacement+1,&displacement,4);memset(replacement+5,0x90,3);
    DWORD protection=0;if(!VirtualProtect(block,16,PAGE_EXECUTE_READWRITE,&protection)){VirtualFree(relay,0,MEM_RELEASE);return Refuse("Cannot protect native autofocus code");}
    bool replaced=_InterlockedCompareExchange128(reinterpret_cast<volatile long long*>(block),desired[1],desired[0],expected)!=0;
    VirtualProtect(block,16,protection,&ignored);FlushInstructionCache(GetCurrentProcess(),block,16);FlushInstructionCache(GetCurrentProcess(),relay,14);
    if(!replaced){VirtualFree(relay,0,MEM_RELEASE);return Refuse("Native autofocus changed during installation");}
    state=1;return true;
}
}
extern "C" __declspec(noinline) float GroundedFocusBlend(float difference,void* camera) noexcept {
    namespace f=grounded::focus_control;
    struct Clock {void* camera=nullptr;int64_t tick=0;};static thread_local Clock clock;
    LARGE_INTEGER now{};QueryPerformanceCounter(&now);
    double elapsed=clock.camera==camera&&clock.tick?double(now.QuadPart-clock.tick)/double(f::frequency.QuadPart):0;
    clock={camera,now.QuadPart};double alpha=grounded::focus_time::Coefficient(elapsed,f::timeConstant);
    f::lastInterval=elapsed;f::lastCoefficient=alpha;++f::updates;
    return difference*static_cast<float>(alpha);
}
