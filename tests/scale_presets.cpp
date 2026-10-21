#include "../src/grounded/scale_control.h"
#include <cstdio>
#include <stdexcept>
#include <string>

namespace {
struct Variable {std::wstring text;uint32_t flags=0;unsigned calls=0;};
unsigned notices=0;
unsigned traces=0;DWORD tracedThread=0;
void __fastcall Original(void* object,const wchar_t* value,uint32_t flags){
    auto& v=*static_cast<Variable*>(object);v.text=value?value:L"<null>";v.flags=flags;++v.calls;
}
void Notice(const char*,double,unsigned){++notices;}
void RecordTrace(const char*,double,unsigned,uint32_t,uintptr_t,DWORD thread){++traces;tracedThread=thread;}
void Check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
}
int main()try{
    using namespace grounded;using namespace grounded::scale_control;
    const std::pair<double,unsigned> cases[]={{1,33},{25,33},{33,33},{34,33},{40,33},{42,50},{49,50},{50,50},{52,50},{53.999,50},{54,58},
        {56,58},{58,58},{62,58},{62.499,58},{62.5,67},{65,67},{66,67},{67,67},{69,67},{70,67},
        {83.499,67},{83.5,100},{90,100},{99,100},{100,100},{120,100}};
    for(auto [input,wanted]:cases)Check(NearestScalePreset(input)==wanted,"Nearest preset or tie rule differs");
    Check(NearestScalePreset((ScalePresetPercentage(33)+50)/2)==50,"Ultra midpoint should prefer Performance");
    Check(NearestScalePreset(33.333332)==33,"Ultra value written by earlier versions is not recognized");
    // Unreal's rounded-up viewport must equal NGX's fixed Ultra input size.
    const std::pair<unsigned,unsigned> ultra[]={{1920,640},{1080,360},{2560,853},{1440,480},{1200,400},{3440,1147},{3840,1280},{2160,720}};
    for(auto [size,ngx]:ultra)
        Check(unsigned(std::ceil(float(size)*(float(ScalePresetPercentage(33))/100.0f)))==ngx,"Ultra viewport differs from NVIDIA's fixed Ultra size");
    Variable first,second,other;Configure(true,Notice);
    original=&Original;screen=&first;group=&second;
    Set(&first,L"33",0x2000000);
    Check(first.text==L"33.32" && first.flags==0x2000000,"Ultra did not receive NVIDIA's one-third viewport and original flags");
    Set(&first,L"33.333332",0x2000000);
    Check(first.text==L"33.32","Earlier Ultra value was not normalized");
    Set(&second,L"69.000000",0x1000000);
    Check(second.text==L"67" && second.flags==0x1000000 && second.calls==1,"Group did not receive canonical scale and original flags");
    Set(&first,L"56",0x2000000);
    Check(first.text==L"58" && first.flags==0x2000000,"Screen percentage or precedence changed");
    Set(&other,L"69",0x3000000);
    Check(other.text==L"69" && other.flags==0x3000000,"Unrelated CVar changed");
    for(auto invalid:{L"0",L"-1",L"nan",L"inf",L"69suffix",L"",L"text"}){
        Set(&first,invalid,0x4000000);Check(first.text==invalid,"Engine sentinel or invalid value rewritten");
    }
    Set(&first,nullptr,0x5000000);Check(first.text==L"<null>","Null input changed");
    Set(&second,L"  66.5  ",0x1000000);Check(second.text==L"67","Finite whitespace input rejected");
    Set(&first,L"50",0x1000000);Check(first.text==L"50","Exact preset modified");
    Set(&first,L"50.000001",0x1000000);Check(first.text==L"50","Tiny fraction escaped canonicalization");
    const auto before=notices;Set(&second,L"69",0x1000000);const auto once=notices;
    for(unsigned i=0;i<10000;++i)Set(&second,L"69",0x1000000);
    Check(once==before+1 && notices==once,"Repeated requests produced repeated logs");
    Check(second.text==L"67" && requested==69 && effective==67,"Repeated request changed selected preset");
    Configure(true,Notice,RecordTrace);
    for(unsigned i=0;i<100;++i)Set(&first,L"67",0x1000000);
    Check(traces==64 && tracedThread==GetCurrentThreadId(),"Optional scale tracing exceeded its bound or changed the caller thread");
    Set(&other,L"67",0x1000000);Check(traces==64,"An unrelated variable was traced");
    Check(TryInstall() && state==2,"Standalone process did not skip game-only installation");
    printf("PASS: nearest/tie cases including Ultra one-third viewport; both scoped CVar arguments; original caller flags; unrelated variables; sentinels; 10000 deduplicated requests; non-game executable skipped.\n");
    return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
