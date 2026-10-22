// Isolated analytical FSR interpolation test; synthetic scene, no game or presents.
#include <windows.h>
#include <wintrust.h>
#include <softpub.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstring>
#include <cmath>
#include <ffx_api.h>
#include <dx12/ffx_api_dx12.h>
#include <ffx_framegeneration.h>
#pragma comment(lib,"wintrust.lib")
using Microsoft::WRL::ComPtr;
namespace fs=std::filesystem;
constexpr UINT W=640,H=360;
void Check(HRESULT hr){if(FAILED(hr))throw std::runtime_error("D3D12 failure "+std::to_string(unsigned(hr)));}
void Api(ffxReturnCode_t rc,const char* stage){if(rc)throw std::runtime_error(std::string(stage)+" failed: "+std::to_string(rc));}
bool Trusted(const fs::path& path){
    WINTRUST_FILE_INFO file{};file.cbStruct=sizeof(file);file.pcwszFilePath=path.c_str();
    WINTRUST_DATA data{};data.cbStruct=sizeof(data);data.dwUIChoice=WTD_UI_NONE;data.fdwRevocationChecks=WTD_REVOKE_NONE;
    data.dwUnionChoice=WTD_CHOICE_FILE;data.pFile=&file;data.dwStateAction=WTD_STATEACTION_VERIFY;data.dwProvFlags=WTD_CACHE_ONLY_URL_RETRIEVAL;
    GUID action=WINTRUST_ACTION_GENERIC_VERIFY_V2;auto rc=WinVerifyTrust(nullptr,&action,&data);
    data.dwStateAction=WTD_STATEACTION_CLOSE;WinVerifyTrust(nullptr,&action,&data);return rc==ERROR_SUCCESS;
}
struct Texture{ComPtr<ID3D12Resource> resource;D3D12_RESOURCE_STATES state=D3D12_RESOURCE_STATE_COPY_DEST;};
struct Gpu{
    ComPtr<ID3D12Device> device;ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;ComPtr<ID3D12Fence> fence;
    HANDLE event=nullptr;UINT64 value=0;std::vector<ComPtr<ID3D12Resource>> uploads;
    Gpu(){
        ComPtr<IDXGIFactory4> factory;ComPtr<IDXGIAdapter1> adapter;Check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
        for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> a;if(factory->EnumAdapters1(i,&a)==DXGI_ERROR_NOT_FOUND)break;
            DXGI_ADAPTER_DESC1 d{};a->GetDesc1(&d);if(d.VendorId==0x10de){adapter=a;break;}}
        if(!adapter)throw std::runtime_error("Fixture requires the NVIDIA test adapter");
        Check(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)));
        D3D12_COMMAND_QUEUE_DESC qd{};Check(device->CreateCommandQueue(&qd,IID_PPV_ARGS(&queue)));
        Check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));
        Check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)));
        Check(list->Close());Check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));
        event=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!event)throw std::runtime_error("Fence event failed");
    }
    ~Gpu(){if(event)CloseHandle(event);}
    void Begin(){uploads.clear();Check(allocator->Reset());Check(list->Reset(allocator.Get(),nullptr));}
    void Submit(){Check(list->Close());ID3D12CommandList* batch[]={list.Get()};queue->ExecuteCommandLists(1,batch);
        Check(queue->Signal(fence.Get(),++value));Check(fence->SetEventOnCompletion(value,event));
        if(WaitForSingleObject(event,20000)!=WAIT_OBJECT_0)throw std::runtime_error("GPU wait timed out");Check(device->GetDeviceRemovedReason());}
    ComPtr<ID3D12Resource> Buffer(UINT64 size,D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state){
        D3D12_HEAP_PROPERTIES hp{};hp.Type=type;D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
        d.Width=size;d.Height=1;d.DepthOrArraySize=d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ComPtr<ID3D12Resource> result;Check(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,state,nullptr,IID_PPV_ARGS(&result)));return result;
    }
    Texture Make(DXGI_FORMAT format,bool output=false){
        Texture t;D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;D3D12_RESOURCE_DESC d{};
        d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=W;d.Height=H;d.DepthOrArraySize=d.MipLevels=1;d.SampleDesc.Count=1;d.Format=format;
        if(output){d.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;t.state=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;}
        Check(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,t.state,nullptr,IID_PPV_ARGS(&t.resource)));return t;
    }
    void Transition(Texture& t,D3D12_RESOURCE_STATES state){if(state==t.state)return;D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition={t.resource.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,t.state,state};list->ResourceBarrier(1,&b);t.state=state;}
    void Upload(Texture& t,const void* data,UINT pixelBytes){
        auto d=t.resource->GetDesc();D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};UINT64 total=0;device->GetCopyableFootprints(&d,0,1,0,&fp,nullptr,nullptr,&total);
        auto upload=Buffer(total,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);void* mapped=nullptr;D3D12_RANGE empty{};Check(upload->Map(0,&empty,&mapped));
        for(UINT y=0;y<H;++y)memcpy(static_cast<char*>(mapped)+fp.Offset+SIZE_T(y)*fp.Footprint.RowPitch,static_cast<const char*>(data)+SIZE_T(y)*W*pixelBytes,SIZE_T(W)*pixelBytes);
        upload->Unmap(0,nullptr);Transition(t,D3D12_RESOURCE_STATE_COPY_DEST);
        D3D12_TEXTURE_COPY_LOCATION src{},dst{};src.pResource=upload.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;src.PlacedFootprint=fp;
        dst.pResource=t.resource.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);
        Transition(t,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);uploads.push_back(std::move(upload));
    }
};
#ifndef GROUNDED_FSR_FIXTURE_LIBRARY
int Run(const fs::path& sdk,const fs::path& out){
    if(!sdk.is_absolute() || fs::exists(out))throw std::runtime_error("Use an absolute SDK path and a new output directory");
    const auto base=sdk/L"Kits/FidelityFX/signedbin";auto loaderPath=base/L"amd_fidelityfx_loader_dx12.dll",effectPath=base/L"amd_fidelityfx_framegeneration_dx12.dll";
    if(!Trusted(loaderPath)||!Trusted(effectPath))throw std::runtime_error("SDK signatures failed");
    auto effect=LoadLibraryExW(effectPath.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    auto module=LoadLibraryExW(loaderPath.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!effect||!module)throw std::runtime_error("Signed SDK load failed");
    const auto create=reinterpret_cast<PfnFfxCreateContext>(GetProcAddress(module,"ffxCreateContext"));
    const auto configure=reinterpret_cast<PfnFfxConfigure>(GetProcAddress(module,"ffxConfigure"));
    const auto dispatch=reinterpret_cast<PfnFfxDispatch>(GetProcAddress(module,"ffxDispatch"));
    const auto query=reinterpret_cast<PfnFfxQuery>(GetProcAddress(module,"ffxQuery"));
    const auto destroy=reinterpret_cast<PfnFfxDestroyContext>(GetProcAddress(module,"ffxDestroyContext"));
    if(!create||!configure||!dispatch||!query||!destroy)throw std::runtime_error("API exports missing");
    Gpu gpu;uint64_t count=0;ffxQueryDescGetVersions versions{};versions.header.type=FFX_API_QUERY_DESC_TYPE_GET_VERSIONS;
    versions.createDescType=FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION;versions.device=gpu.device.Get();versions.outputCount=&count;
    Api(query(nullptr,&versions.header),"version count");if(count>32 || !count)throw std::runtime_error("Provider count invalid");
    std::vector<uint64_t> ids(count);std::vector<const char*> names(count);versions.versionIds=ids.data();versions.versionNames=names.data();Api(query(nullptr,&versions.header),"versions");
    uint64_t selected=0;for(size_t i=0;i<count;++i)if(std::string(names[i]?names[i]:"").find("3.1.6")!=std::string::npos)selected=ids[i];
    if(!selected)throw std::runtime_error("Analytical 3.1.6 provider unavailable");
    ffxOverrideVersion version{};version.header.type=FFX_API_DESC_TYPE_OVERRIDE_VERSION;version.versionId=selected;
    ffxCreateContextDescFrameGenerationVersion api{};api.header.type=FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION_VERSION;api.version=FFX_FRAMEGENERATION_VERSION;api.header.pNext=&version.header;
    ffxCreateBackendDX12Desc backend{};backend.header.type=FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12;backend.device=gpu.device.Get();backend.header.pNext=&api.header;
    ffxCreateContextDescFrameGeneration desc{};desc.header.type=FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION;desc.header.pNext=&backend.header;
    desc.displaySize=desc.maxRenderSize={W,H};desc.backBufferFormat=ffxApiGetSurfaceFormatDX12(DXGI_FORMAT_R8G8B8A8_UNORM);
    desc.flags=FFX_FRAMEGENERATION_ENABLE_DEPTH_INVERTED|FFX_FRAMEGENERATION_ENABLE_DEPTH_INFINITE;
    ffxContext context=nullptr;Api(create(&context,&desc.header,nullptr),"create");
    auto color=gpu.Make(DXGI_FORMAT_R8G8B8A8_UNORM),depth=gpu.Make(DXGI_FORMAT_R32_FLOAT),motion=gpu.Make(DXGI_FORMAT_R32G32_FLOAT),output=gpu.Make(DXGI_FORMAT_R8G8B8A8_UNORM,true);
    auto od=output.resource->GetDesc();D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT64 total=0;
    gpu.device->GetCopyableFootprints(&od,0,1,0,&footprint,nullptr,nullptr,&total);auto readback=gpu.Buffer(total,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
    fs::create_directories(out);std::ofstream report(out/L"results.json");report<<"{\"synthetic_scene\":true,\"in_game\":false,\"presented_frames\":0,\"width\":"<<W<<",\"height\":"<<H<<",\"provider\":\"3.1.6\",\"frames\":[\n";
    unsigned interpolated=0;bool allCorrect=true;
    for(unsigned frame=0;frame<12;++frame){
        const UINT left=100+frame*8;const UINT64 frameId=frame+(frame>=8?2:0);const bool reset=frame==0 || frame==6;
        std::vector<unsigned char> rgba(SIZE_T(W)*H*4);std::vector<float> z(SIZE_T(W)*H),mv(SIZE_T(W)*H*2);
        for(UINT y=0;y<H;++y)for(UINT x=0;x<W;++x){const auto i=SIZE_T(y)*W+x;bool object=x>=left && x<left+120 && y>=120 && y<240;
            rgba[i*4]=object?220:static_cast<unsigned char>(20+((x/17+y/19)&1)*15);rgba[i*4+1]=object?70:40;rgba[i*4+2]=object?40:70;rgba[i*4+3]=255;
            z[i]=object?.05f:.01f;mv[i*2]=object?-8.f:0.f;mv[i*2+1]=0;}
        ffxConfigureDescFrameGeneration cfg{};cfg.header.type=FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION;cfg.frameID=frameId;
        cfg.frameGenerationEnabled=true;cfg.flags=FFX_FRAMEGENERATION_FLAG_NO_SWAPCHAIN_CONTEXT_NOTIFY;cfg.generationRect={0,0,W,H};Api(configure(&context,&cfg.header),"configure");
        gpu.Begin();gpu.Upload(color,rgba.data(),4);gpu.Upload(depth,z.data(),4);gpu.Upload(motion,mv.data(),8);
        ffxDispatchDescFrameGenerationPrepareV2 prep{};prep.header.type=FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION_PREPARE_V2;
        prep.commandList=gpu.list.Get();prep.frameID=frameId;prep.renderSize={W,H};prep.motionVectorScale={1,1};prep.frameTimeDelta=1000.f/60;
        prep.reset=reset;prep.cameraNear=.1f;prep.cameraFar=1000;prep.cameraFovAngleVertical=1.57079632679f;prep.viewSpaceToMetersFactor=1;
        prep.cameraUp[1]=1;prep.cameraRight[0]=1;prep.cameraForward[2]=1;prep.depth=ffxApiGetResourceDX12(depth.resource.Get());prep.motionVectors=ffxApiGetResourceDX12(motion.resource.Get());
        Api(dispatch(&context,&prep.header),"prepare");
        ffxDispatchDescFrameGeneration generate{};generate.header.type=FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION;generate.commandList=gpu.list.Get();generate.frameID=frameId;
        generate.presentColor=ffxApiGetResourceDX12(color.resource.Get());generate.outputs[0]=ffxApiGetResourceDX12(output.resource.Get(),FFX_API_RESOURCE_STATE_UNORDERED_ACCESS);
        generate.numGeneratedFrames=1;generate.reset=reset;generate.backbufferTransferFunction=FFX_API_BACKBUFFER_TRANSFER_FUNCTION_SRGB;generate.minMaxLuminance[1]=80;generate.generationRect={0,0,W,H};
        Api(dispatch(&context,&generate.header),"interpolate");
        gpu.Transition(output,D3D12_RESOURCE_STATE_COPY_SOURCE);D3D12_TEXTURE_COPY_LOCATION src{},dst{};src.pResource=output.resource.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.pResource=readback.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=footprint;gpu.list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);gpu.Transition(output,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);gpu.Submit();
        D3D12_RANGE range{0,SIZE_T(total)};void* mapped=nullptr;Check(readback->Map(0,&range,&mapped));double sum=0;UINT64 red=0;
        std::ofstream image(out/(L"frame-"+std::to_wstring(frame)+L".rgba8"),std::ios::binary);
        for(UINT y=0;y<H;++y){const auto row=static_cast<const unsigned char*>(mapped)+footprint.Offset+SIZE_T(y)*footprint.Footprint.RowPitch;
            image.write(reinterpret_cast<const char*>(row),W*4);for(UINT x=0;x<W;++x)if(row[x*4]>150 && row[x*4+1]<100 && row[x*4+2]<100){sum+=x;++red;}}
        image.close();D3D12_RANGE written{};readback->Unmap(0,&written);if(!image || !red)throw std::runtime_error("Output image/object missing");
        const double center=sum/double(red),current=left+59.5,offset=current-center;
        const bool between=!reset && frame!=8 && offset>1 && offset<7;if(between)++interpolated;
        allCorrect=allCorrect && std::abs(offset-((reset || frame==8)?0.0:4.0))<.25 && red>=12000 && red<=16000;
        if(frame)report<<",\n";report<<"{\"id\":"<<frameId<<",\"reset\":"<<(reset?"true":"false")<<",\"red_pixels\":"<<red<<",\"current_center\":"<<current<<",\"generated_center\":"<<center<<",\"offset\":"<<offset<<",\"between_inputs\":"<<(between?"true":"false")<<"}";
        std::cout<<"frame="<<frame<<" id="<<frameId<<" reset="<<reset<<" current="<<current<<" generated="<<center<<" offset="<<offset<<"\n";
    }
    Api(destroy(&context,nullptr),"destroy");allCorrect=allCorrect && interpolated==9;report<<"\n],\"intermediate_positions_verified\":"<<interpolated<<",\"passed\":"<<(allCorrect?"true":"false")<<"}\n";report.close();
    FreeLibrary(module);FreeLibrary(effect);return allCorrect && report?0:10;
}
int wmain(int argc,wchar_t** argv){if(argc!=3)return 2;try{return Run(fs::absolute(argv[1]),fs::path(argv[2]));}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
#endif
