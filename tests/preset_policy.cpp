#include "../src/grounded/preset_policy.h"
#include <cstdio>
#include <stdexcept>
#include <initializer_list>
void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main()try{
    using namespace grounded;
    for(unsigned architecture:{0u,0x160u,0x170u,0xe0000020u,0xffffffffu})
        for(unsigned mode:{0u,1u,2u,3u,5u})Check(RecommendedRenderPreset(mode,architecture)==11,"Legacy/unknown GPU did not choose K");
    for(unsigned architecture:{0x190u,0x1b0u,0x1d0u})
        for(unsigned mode:{0u,1u,2u,3u,5u})Check(RecommendedRenderPreset(mode,architecture)==(mode==0?13u:mode==3?12u:11u),"Modern GPU recommendation differs");
    for(unsigned preset:{5u,11u,12u,13u})for(unsigned mode:{0u,1u,2u,3u,5u})
        Check(RecommendedRenderPreset(mode,0x190,preset)==preset,"Explicit override ignored");
    Check(RecommendedRenderPreset(0,0x190,999)==13,"Invalid override defeated automatic policy");
    puts("PASS: Turing/Ampere K for all modes; Ada/Blackwell/future K/M/L; unknown architecture K; per-mode policy and explicit overrides.");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
