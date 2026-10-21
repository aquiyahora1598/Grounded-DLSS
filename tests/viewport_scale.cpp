#include "../src/grounded/viewport_scale.h"
#include <cstdio>
#include <stdexcept>

void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main()try{
    using namespace grounded::viewport_scale;
    // Same scalar operation as the verified helper, in a separately owned page.
    // float fraction(float percentage, float borderlessRatio): return percentage*ratio.
    const unsigned char code[]={0xf3,0x0f,0x59,0xc1,0xc3};
    auto memory=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    Check(memory!=nullptr,"Allocation failed");memcpy(memory,code,sizeof(code));DWORD old=0;
    Check(VirtualProtect(memory,4096,PAGE_EXECUTE_READ,&old)!=0,"Executable fixture failed");
    auto function=reinterpret_cast<float(*)(float,float)>(memory);
    const uint32_t fixtureMultiply=0xc1590ff3;
    Check(std::abs(function(.67f,.8f)-.536f)<1e-6f,"Fixture did not reproduce startup mismatch");
    Check(!Replace(memory+1,fixtureMultiply,Nop),"Unaligned instruction accepted");
    Check(!Replace(memory,Multiply,Nop),"Different instruction accepted");
    Check(Replace(memory,fixtureMultiply,Nop),"Atomic instruction replacement failed");
    for(float ratio:{.5f,.75f,.8f,1.0f})for(float preset:{1.f,.67f,.58f,.5f,1.f/3.f})
        Check(std::abs(function(preset,ratio)-preset)<1e-6f,"Preset changed with borderless base resolution");
    MEMORY_BASIC_INFORMATION info{};VirtualQuery(memory,&info,sizeof(info));
    Check(info.Protect==PAGE_EXECUTE_READ,"Code page permissions not restored");
    Check(Replace(memory,Nop,fixtureMultiply),"Fixture restoration failed");
    Check(std::abs(function(.67f,.75f)-.5025f)<1e-6f,"Original behavior not restored");
    VirtualFree(memory,0,MEM_RELEASE);
    grounded::scale_control::state=2;
    Check(TryInstall() && state==2,"Non-game process was not skipped");
    printf("PASS: stale 80%% factor reproduced; five presets ignore old window sizes; atomic/unaligned/signature guards; original page protection and behavior restored; non-game skip.\n");
    return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
