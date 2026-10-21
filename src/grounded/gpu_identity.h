#pragma once
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <string>
#include "../../external/nvapi/nvapi.h"

namespace grounded {
struct GpuIdentity {std::wstring name;unsigned architecture=0,vendor=0,device=0;};
inline GpuIdentity IdentifyGpu(ID3D12Device* device) {
    GpuIdentity result;const auto wanted=device->GetAdapterLuid();
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    if(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) &&
       SUCCEEDED(factory->EnumAdapterByLuid(wanted,IID_PPV_ARGS(&adapter)))) {
        DXGI_ADAPTER_DESC1 desc{};
        if(SUCCEEDED(adapter->GetDesc1(&desc))){result.name=desc.Description;result.vendor=desc.VendorId;result.device=desc.DeviceId;}
    }
    if(result.vendor!=0x10de)return result;
    // Match NVAPI's physical GPU to the D3D12 device LUID, not the first GPU
    // enumerated or a marketing name. Query only; never change driver profiles.
    HMODULE module=LoadLibraryExW(L"nvapi64.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!module)return result;
    using Query=void*(__cdecl*)(unsigned);
    auto query=reinterpret_cast<Query>(GetProcAddress(module,"nvapi_QueryInterface"));
    auto initialize=query?reinterpret_cast<decltype(&NvAPI_Initialize)>(query(0x0150e828)):nullptr;
    auto enumerate=query?reinterpret_cast<decltype(&NvAPI_EnumPhysicalGPUs)>(query(0xe5ac921f)):nullptr;
    auto adapterId=query?reinterpret_cast<decltype(&NvAPI_GPU_GetAdapterIdFromPhysicalGpu)>(query(0x0ff07fde)):nullptr;
    auto architecture=query?reinterpret_cast<decltype(&NvAPI_GPU_GetArchInfo)>(query(0xd8265d24)):nullptr;
    auto unload=query?reinterpret_cast<decltype(&NvAPI_Unload)>(query(0xd22bdd7e)):nullptr;
    const bool initialized=initialize && initialize()==NVAPI_OK;
    if(initialized && enumerate && adapterId && architecture) {
        NvPhysicalGpuHandle handles[NVAPI_MAX_PHYSICAL_GPUS]{};NvU32 count=0;
        if(enumerate(handles,&count)==NVAPI_OK)for(NvU32 i=0;i<count && i<NVAPI_MAX_PHYSICAL_GPUS;++i){
            LUID found{};
            if(adapterId(handles[i],&found)!=NVAPI_OK || found.LowPart!=wanted.LowPart || found.HighPart!=wanted.HighPart)continue;
            NV_GPU_ARCH_INFO info{};info.version=NV_GPU_ARCH_INFO_VER;
            auto status=architecture(handles[i],&info);
            if(status==NVAPI_INCOMPATIBLE_STRUCT_VERSION){info.version=NV_GPU_ARCH_INFO_VER_1;status=architecture(handles[i],&info);}
            if(status==NVAPI_OK)result.architecture=info.architecture;
            break;
        }
    }
    if(initialized && unload)unload();FreeLibrary(module);return result;
}
}
