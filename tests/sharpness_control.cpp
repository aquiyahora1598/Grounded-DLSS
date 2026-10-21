#include "../src/grounded/sharpness_control.h"
#include <cstdio>
#include <stdexcept>

void Check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main()try{
    using namespace grounded::sharpness_control;
    Values original{0.1f,0.2f};void* volatile cache=&original;
    Controller test;test.Configure(Settings{});
    Check(test.Bind(&cache,&original),"Cannot bind verified typed cache");
    Check(test.Publish(true,2) && cache==&original,"Default policy added sharpening");
    Settings explicitValues;explicitValues.enabled=true;
    explicitValues.quality=.3f;explicitValues.balanced=.5f;explicitValues.performance=.7f;
    explicitValues.ultraPerformance=.9f;
    test.Configure(explicitValues);
    auto verify=[&](bool active,unsigned mode,float wanted){
        Check(test.Publish(active,mode),"Cannot select sharpness");
        const auto* result=static_cast<Values*>(cache);
        Check(std::abs(result->render-wanted)<1e-6f,"Wrong render-thread sharpness");
        if(active)Check(result->game==result->render,"Cached copies differ");
        Check(original.game==0.1f && original.render==0.2f,"Original CVar was modified");
    };
    verify(true,2,0.3f);verify(true,1,0.5f);verify(true,0,0.7f);verify(true,5,0.0f);
    verify(true,3,0.9f);
    verify(true,2,0.3f);
    const auto before=test.changes.load();
    for(unsigned i=0;i<10000;++i)Check(test.Publish(true,2),"Repeated mode failed");
    Check(test.changes==before,"Unchanged mode caused repeated writes");
    verify(false,2,0.2f);Check(cache==&original,"F8 did not restore original cache");
    original.render=0.45f;
    Check(test.Publish(true,0) && test.Publish(false,0),"Cannot return after CVar update");
    Check(cache==&original && static_cast<Values*>(cache)->render==0.45f,"Updated original setting lost");
    Check(test.Publish(true,5) && test.Publish(true,4),"Unknown mode fallback failed");
    Check(cache==&original && test.mode==-1,"Unknown NGX mode received arbitrary sharpening");
    Settings custom;custom.enabled=true;custom.quality=0.4f;custom.dlaa=0.1f;
    test.Configure(custom);Check(test.Publish(true,2),"Custom quality failed");
    Check(static_cast<Values*>(cache)->render==0.4f,"Custom quality ignored");
    Check(test.Publish(true,5) && static_cast<Values*>(cache)->render==0.1f,"Custom DLAA ignored");
    Check(test.Publish(false,5),"Custom restore failed");
    Settings disabled;disabled.enabled=false;test.Configure(disabled);
    Check(test.Publish(true,2) && cache==&original,"Disabled policy changed image");
    test.Configure(explicitValues);Check(test.Publish(true,2),"Re-enable failed");
    Values foreign{};cache=&foreign;
    Check(!test.Publish(false,2) && cache==&foreign,"Foreign cache overwritten");
    float parsed=0.3f;
    for(auto bad:{L"-0.1",L"1.1",L"nan",L"inf",L"0.5suffix",L"",L"text"})
        Check(!ParseStrength(bad,parsed) && parsed==0.3f,"Invalid setting did not preserve default");
    Check(ParseStrength(L" 0.7 ",parsed) && parsed==0.7f,"Valid setting rejected");
    Check(ParseStrength(L"0",parsed) && parsed==0 && ParseStrength(L"1",parsed) && parsed==1,"Range endpoints rejected");
    Check(TryInstall() && state==2,"Non-game installation was not skipped");
    printf("PASS: all five NGX modes including Ultra 0.9 and returns; F8 restore; preserved live CVar updates; 10000 no-write repeats; custom/disabled settings; foreign cache refusal; invalid input; non-game guard.\n");
    return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL: %s\n",error.what());return 1;}
