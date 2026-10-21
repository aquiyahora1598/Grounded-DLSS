#include <cstdint>
#include <cstdio>
extern "C" uint64_t focusTestCamera=0;
extern "C" void RunFocusAbi(void*);
int main(){
 alignas(16) uint64_t result[22]{};RunFocusAbi(result);
 bool valid=focusTestCamera==8&&(result[8]&0x841)==0x40;
 for(unsigned i=0;i<8;++i)valid&=result[i]==i+1;
 const uint32_t expected[]={0x40800000,0x11223344,0x55667788,0x99aabbcc};
 auto values=reinterpret_cast<uint32_t*>(result+9);
 for(unsigned i=0;i<24;++i)valid&=values[i]==(i==0?0x3f800000:expected[i%4]);
 if(!valid){for(unsigned i=0;i<9;++i)printf("register[%u]=%llx\n",i,result[i]);for(unsigned i=0;i<24;++i)printf("lane[%u]=%08x\n",i,values[i]);printf("camera=%llu\n",focusTestCamera);}
 printf("%s: autofocus instruction bridge preserves volatile registers, SIMD lanes, flags, stack and camera identity.\n",valid?"PASS":"FAIL");return valid?0:1;
}
