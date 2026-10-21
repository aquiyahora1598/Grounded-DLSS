#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>
namespace grounded::native_memory {
inline bool Copy(void* destination,const void* source,size_t bytes){
    if(!source)return false;MEMORY_BASIC_INFORMATION region{};
    if(!VirtualQuery(source,&region,sizeof(region)) || region.State!=MEM_COMMIT || (region.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
    auto begin=reinterpret_cast<uintptr_t>(source),end=begin+bytes;
    if(end<begin || end>reinterpret_cast<uintptr_t>(region.BaseAddress)+region.RegionSize)return false;
    __try{memcpy(destination,source,bytes);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
template<class T>inline bool Read(uintptr_t address,T& value){return Copy(&value,reinterpret_cast<void*>(address),sizeof(value));}
}
