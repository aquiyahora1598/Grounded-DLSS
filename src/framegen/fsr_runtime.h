#pragma once
// Experimental synchronous FSR 3 FG. SR remains direct NGX. The AMD proxy owns
// pacing; the HUD-less texture is read only on the game GFX queue (async=false).
#include "scene_probe.h"
#include "camera.h"
#include <ffx_api.h>
#include <dx12/ffx_api_dx12.h>
#include <ffx_framegeneration.h>
#include <dx12/ffx_api_framegeneration_dx12.h>
#include <wintrust.h>
#include <softpub.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <cfloat>
#pragma comment(lib,"wintrust.lib")
namespace grounded::framegen::fsr {
namespace p=grounded::framegen::probe;
using p::ComPtr;
inline thread_local bool creating=false;
struct Runtime {
    std::recursive_mutex mutex;
    p::Logger log=nullptr;
    HMODULE loader=nullptr,effect=nullptr;
    PfnFfxCreateContext create=nullptr;PfnFfxDestroyContext destroy=nullptr;
    PfnFfxConfigure configure=nullptr;PfnFfxQuery query=nullptr;PfnFfxDispatch dispatch=nullptr;
    ffxContext swapContext=nullptr,fgContext=nullptr;
    IDXGISwapChain4* proxy=nullptr;std::atomic<uintptr_t> proxyKey{0};
    ULONG privateRefFloor=1;
    std::atomic<uintptr_t> nativeKey{0};
    ComPtr<ID3D12Device> device;ComPtr<ID3D12CommandQueue> queue;
    std::atomic<ID3D12CommandQueue*> queuePointer{nullptr};
    ComPtr<ID3D12Fence> fence;UINT64 fenceValue=0;
    ComPtr<ID3D12Resource> hudless;
    DXGI_SWAP_CHAIN_DESC desc{};UINT fgWidth=0,fgHeight=0;
    uint64_t provider=0,frameId=0;
    std::atomic<ID3D12CommandList*> prepareList{nullptr},toneList{nullptr};
    bool desired=true,active=false,failed=false,dirty=true,destroying=false,closed=false;
    bool validFrame=false,preparedSubmitted=false,toneSubmitted=false,copied=false,resetFrame=true,haveCamera=false;
    bool pausedLast=false;
    Camera camera{},previousCamera{};DWORD presentThread=0;
    DXGI_COLOR_SPACE_TYPE colorSpace=DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
    std::atomic<UINT64> renderedPresents{0},nativePresents{0},generatedDispatches{0},skipped{0};
    std::atomic<UINT64> callbackErrors{0};
    bool Trusted(const std::filesystem::path& path){
        WINTRUST_FILE_INFO file{};file.cbStruct=sizeof(file);file.pcwszFilePath=path.c_str();
        WINTRUST_DATA data{};data.cbStruct=sizeof(data);data.dwUIChoice=WTD_UI_NONE;data.fdwRevocationChecks=WTD_REVOKE_NONE;
        data.dwUnionChoice=WTD_CHOICE_FILE;data.pFile=&file;data.dwStateAction=WTD_STATEACTION_VERIFY;data.dwProvFlags=WTD_CACHE_ONLY_URL_RETRIEVAL;
        GUID action=WINTRUST_ACTION_GENERIC_VERIFY_V2;auto rc=WinVerifyTrust(nullptr,&action,&data);
        data.dwStateAction=WTD_STATEACTION_CLOSE;WinVerifyTrust(nullptr,&action,&data);return rc==ERROR_SUCCESS;
    }
    bool Load(const std::filesystem::path& folder,p::Logger logger){
        log=logger;if(loader)return true;
        auto lib=folder/L"amd_fidelityfx_loader_dx12.dll",fx=folder/L"amd_fidelityfx_framegeneration_dx12.dll";
        if(!folder.is_absolute() || !Trusted(lib) || !Trusted(fx)){if(log)log("FSR FG unavailable: signed AMD libraries not found in %ls",folder.c_str());return false;}
        effect=LoadLibraryExW(fx.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        loader=LoadLibraryExW(lib.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!loader || !effect)return false;
        create=reinterpret_cast<PfnFfxCreateContext>(GetProcAddress(loader,"ffxCreateContext"));
        destroy=reinterpret_cast<PfnFfxDestroyContext>(GetProcAddress(loader,"ffxDestroyContext"));
        configure=reinterpret_cast<PfnFfxConfigure>(GetProcAddress(loader,"ffxConfigure"));
        query=reinterpret_cast<PfnFfxQuery>(GetProcAddress(loader,"ffxQuery"));
        dispatch=reinterpret_cast<PfnFfxDispatch>(GetProcAddress(loader,"ffxDispatch"));
        return create && destroy && configure && query && dispatch;
    }
    bool Result(ffxReturnCode_t rc,const char* stage){if(!rc)return true;failed=true;if(log)log("FSR FG error: %s result=%u; DLSS remains available",stage,rc);return false;}
    uint64_t Provider(uint64_t type,const char* version){
        uint64_t count=0;ffxQueryDescGetVersions q{};q.header.type=FFX_API_QUERY_DESC_TYPE_GET_VERSIONS;q.createDescType=type;q.device=device.Get();q.outputCount=&count;
        if(query(nullptr,&q.header) || !count || count>32)return 0;
        std::vector<uint64_t> ids(count);std::vector<const char*> names(count);q.versionIds=ids.data();q.versionNames=names.data();
        if(query(nullptr,&q.header))return 0;
        for(size_t i=0;i<count;++i)if(names[i] && std::string(names[i]).find(version)!=std::string::npos)return ids[i];return 0;
    }
    bool CanCreate(IUnknown* object){
        if(creating || !create || proxy || closed || failed || !object)return false;
        ComPtr<ID3D12CommandQueue> q;if(FAILED(object->QueryInterface(IID_PPV_ARGS(&q))) || q->GetDesc().Type!=D3D12_COMMAND_LIST_TYPE_DIRECT)return false;
        queue=q;queuePointer=queue.Get();if(FAILED(queue->GetDevice(IID_PPV_ARGS(&device))))return false;
        provider=Provider(FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION,"3.1.6");return provider!=0;
    }
    bool Flush(){
        if(!queue)return true;if(!fence && FAILED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence))))return false;
        if(FAILED(queue->Signal(fence.Get(),++fenceValue)))return false;
        HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!event)return false;
        auto hr=fence->SetEventOnCompletion(fenceValue,event);auto result=SUCCEEDED(hr)?WaitForSingleObject(event,10000):WAIT_FAILED;CloseHandle(event);
        return result==WAIT_OBJECT_0 && fence->GetCompletedValue()!=UINT64_MAX;
    }
    void Disable(){
        if(!fgContext || !proxy){active=false;return;}
        if(active){ffxConfigureDescFrameGeneration cfg{};cfg.header.type=FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION;
            cfg.swapChain=proxy;cfg.frameGenerationEnabled=false;cfg.frameID=frameId;configure(&fgContext,&cfg.header);}
        active=false;
    }
    bool Ensure(){
        if(!proxy || FAILED(proxy->GetDesc(&desc)))return false;
        if(fgContext && !dirty && fgWidth==desc.BufferDesc.Width && fgHeight==desc.BufferDesc.Height)return true;
        if(prepareList.load() || toneList.load())return false;
        Disable();if(!Flush()){failed=true;return false;}
        if(fgContext){if(!Result(destroy(&fgContext,nullptr),"destroy before resize"))return false;}
        hudless.Reset();
        ffxOverrideVersion selected{};selected.header.type=FFX_API_DESC_TYPE_OVERRIDE_VERSION;selected.versionId=provider;
        ffxCreateContextDescFrameGenerationVersion version{};version.header.type=FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION_VERSION;version.version=FFX_FRAMEGENERATION_VERSION;version.header.pNext=&selected.header;
        ffxCreateBackendDX12Desc backend{};backend.header.type=FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12;backend.device=device.Get();backend.header.pNext=&version.header;
        ffxCreateContextDescFrameGeneration config{};config.header.type=FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION;config.header.pNext=&backend.header;
        config.displaySize=config.maxRenderSize={desc.BufferDesc.Width,desc.BufferDesc.Height};config.backBufferFormat=ffxApiGetSurfaceFormatDX12(desc.BufferDesc.Format);
        config.flags=FFX_FRAMEGENERATION_ENABLE_DEPTH_INVERTED|FFX_FRAMEGENERATION_ENABLE_DEPTH_INFINITE;
        if(!Result(create(&fgContext,&config.header,nullptr),"create FG"))return false;
        D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;D3D12_RESOURCE_DESC texture{};texture.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texture.Width=desc.BufferDesc.Width;texture.Height=desc.BufferDesc.Height;texture.Format=desc.BufferDesc.Format;texture.DepthOrArraySize=texture.MipLevels=1;texture.SampleDesc.Count=1;
        if(FAILED(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&texture,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,nullptr,IID_PPV_ARGS(&hudless)))){failed=true;return false;}
        fgWidth=desc.BufferDesc.Width;fgHeight=desc.BufferDesc.Height;dirty=false;haveCamera=false;
        if(log)log("FSR FG context ready: provider=3.1.6 swapchain=3.1.7 output=%ux%u format=%u async=0",fgWidth,fgHeight,unsigned(desc.BufferDesc.Format));return true;
    }
    static ffxReturnCode_t Generate(ffxDispatchDescFrameGeneration* params,void* user){
        auto& s=*static_cast<Runtime*>(user);
        if(!params || GetCurrentThreadId()!=s.presentThread || params->frameID!=s.frameId){++s.callbackErrors;return FFX_API_RETURN_ERROR;}
        params->reset=s.resetFrame;params->backbufferTransferFunction=FFX_API_BACKBUFFER_TRANSFER_FUNCTION_SRGB;
        params->minMaxLuminance[0]=0;params->minMaxLuminance[1]=80;
        auto rc=s.dispatch(&s.fgContext,&params->header);if(!rc)s.generatedDispatches+=params->numGeneratedFrames;else ++s.callbackErrors;return rc;
    }
    bool Prepare(ID3D12GraphicsCommandList* list,const Camera& current,float jitterX,float jitterY,ID3D12Resource* depth,ID3D12Resource* motion,UINT width,UINT height,bool reset,bool checkPause=true){
        std::lock_guard<std::recursive_mutex> lock(mutex);
        if(!desired || failed || !proxy || closed || colorSpace!=DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709)return false;
        if(validFrame || prepareList.load() || toneList.load()){validFrame=false;++skipped;return false;}
        if(!Ensure() || width>fgWidth || height>fgHeight || !width || !height)return false;
        bool cameraCut=reset || !haveCamera;float distance=0,dot=0;
        for(UINT i=0;i<3;++i){auto d=current.position[i]-previousCamera.position[i];distance+=d*d;dot+=current.forward[i]*previousCamera.forward[i];}
        cameraCut|=haveCamera && (distance>2000.f*2000.f || dot<.5f || std::abs(current.fov-previousCamera.fov)>.05f || current.worldTime<previousCamera.worldTime);
        const bool paused=checkPause && haveCamera && current.worldTime==previousCamera.worldTime;
        previousCamera=current;haveCamera=true;
        if(paused || current.deltaMs<.1f || current.deltaMs>200){pausedLast=true;++skipped;return false;}
        cameraCut|=pausedLast;pausedLast=false;
        camera=current;resetFrame=cameraCut;copied=false;preparedSubmitted=toneSubmitted=false;++frameId;
        ffxConfigureDescFrameGeneration cfg{};cfg.header.type=FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION;cfg.swapChain=proxy;
        cfg.frameGenerationEnabled=true;cfg.allowAsyncWorkloads=false;cfg.frameGenerationCallback=Generate;cfg.frameGenerationCallbackUserContext=this;
        cfg.HUDLessColor=ffxApiGetResourceDX12(hudless.Get());cfg.frameID=frameId;cfg.generationRect={0,0,int(fgWidth),int(fgHeight)};
        if(!Result(configure(&fgContext,&cfg.header),"configure frame"))return false;active=true;
        D3D12_RESOURCE_BARRIER barriers[2]{};ID3D12Resource* resources[]={depth,motion};
        for(UINT i=0;i<2;++i){barriers[i].Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barriers[i].Transition={resources[i],D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE};}
        list->ResourceBarrier(2,barriers);
        ffxDispatchDescFrameGenerationPrepareV2 prep{};prep.header.type=FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION_PREPARE_V2;prep.frameID=frameId;prep.commandList=list;
        prep.renderSize={width,height};prep.jitterOffset={jitterX,jitterY};prep.motionVectorScale={1,1};prep.frameTimeDelta=current.deltaMs;prep.reset=resetFrame;
        prep.cameraNear=current.nearPlane;prep.cameraFar=FLT_MAX;prep.cameraFovAngleVertical=current.fov;prep.viewSpaceToMetersFactor=.01f;
        memcpy(prep.cameraPosition,current.position,sizeof(current.position));memcpy(prep.cameraUp,current.up,sizeof(current.up));memcpy(prep.cameraRight,current.right,sizeof(current.right));memcpy(prep.cameraForward,current.forward,sizeof(current.forward));
        prep.depth=ffxApiGetResourceDX12(depth);prep.motionVectors=ffxApiGetResourceDX12(motion);
        auto rc=dispatch(&fgContext,&prep.header);
        for(auto& b:barriers)std::swap(b.Transition.StateBefore,b.Transition.StateAfter);list->ResourceBarrier(2,barriers);
        prepareList=list;validFrame=Result(rc,"prepare frame");
        if(frameId<=2 && log)log("FSR FG camera: near=%.4f fov=%.6f delta_ms=%.4f world_time=%.6f render=%ux%u",current.nearPlane,current.fov,current.deltaMs,current.worldTime,width,height);
        return validFrame;
    }
    void CopyHudless(ID3D12GraphicsCommandList* list,ID3D12Resource* source){
        std::lock_guard<std::recursive_mutex> lock(mutex);if(!validFrame || copied || !source)return;
        auto td=source->GetDesc();if(td.Width!=fgWidth || td.Height!=fgHeight || td.Format!=desc.BufferDesc.Format || td.MipLevels!=1 || td.DepthOrArraySize!=1 || td.SampleDesc.Count!=1)return;
        D3D12_RESOURCE_BARRIER b[2]{};for(auto& v:b)v.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b[0].Transition={source,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE};
        b[1].Transition={hudless.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_DEST};
        list->ResourceBarrier(2,b);list->CopyResource(hudless.Get(),source);for(auto& v:b)std::swap(v.Transition.StateBefore,v.Transition.StateAfter);list->ResourceBarrier(2,b);
        copied=true;toneList=list;
    }
    void Submitted(ID3D12CommandQueue* q,UINT count,ID3D12CommandList*const* lists){
        if(q!=queuePointer.load())return;std::lock_guard<std::recursive_mutex> lock(mutex);
        for(UINT i=0;i<count;++i){if(prepareList.load()==lists[i]){prepareList=nullptr;preparedSubmitted=true;}
            if(toneList.load()==lists[i]){toneList=nullptr;toneSubmitted=true;}}
    }
    void Discarded(ID3D12CommandList* list){
        if(prepareList.load()!=list && toneList.load()!=list)return;std::lock_guard<std::recursive_mutex> lock(mutex);
        if(prepareList.load()==list){prepareList=nullptr;validFrame=false;}if(toneList.load()==list){toneList=nullptr;validFrame=false;}
    }
    // The control thread only queues the choice. Configure/flush stay on the
    // presentation thread, avoiding a lock inversion with the host SR recorder.
    void SetEnabled(bool enabled){std::lock_guard<std::recursive_mutex> lock(mutex);desired=enabled;if(!enabled)validFrame=false;haveCamera=false;
        if(log)log("F9: FSR Frame Generation %s; DLSS unchanged",enabled?"requested":"OFF");}
    void BeforeResize(){std::lock_guard<std::recursive_mutex> lock(mutex);validFrame=false;Disable();dirty=true;haveCamera=false;}
    void Shutdown(){
        std::lock_guard<std::recursive_mutex> lock(mutex);if(destroying)return;destroying=true;validFrame=false;Disable();
        if(Flush() && !prepareList.load() && !toneList.load()){if(fgContext)destroy(&fgContext,nullptr);hudless.Reset();}
        else if(log)log("FSR FG teardown: retaining unsubmitted GPU allocations until process exit");
        if(swapContext)destroy(&swapContext,nullptr);closed=true;active=false;queuePointer=nullptr;
        if(log)log("FSR FG shutdown: rendered_presents=%llu native_presents=%llu generation_dispatches=%llu callback_errors=%llu",renderedPresents.load(),nativePresents.load(),generatedDispatches.load(),callbackErrors.load());
    }
    void WriteStatus(const std::filesystem::path& path){std::lock_guard<std::recursive_mutex> lock(mutex);
        std::ofstream f(path);f<<"requested="<<desired<<"\nactive="<<active<<"\nfailed="<<failed<<"\nframe_id="<<frameId<<"\nrendered_presents="<<renderedPresents<<"\nnative_presents="<<nativePresents
            <<"\ngeneration_dispatches="<<generatedDispatches<<"\ncallback_errors="<<callbackErrors<<"\nskipped="<<skipped<<"\noutput_width="<<fgWidth<<"\noutput_height="<<fgHeight<<"\n";}
};
inline Runtime& runtime=*new Runtime; // Never destroy driver objects under loader lock.
inline ULONG STDMETHODCALLTYPE Released(IUnknown* object){
    const auto fn=p::Original<ULONG(STDMETHODCALLTYPE*)(IUnknown*),2>(object);auto count=fn(object);
    // Never QueryInterface from Release: the temporary identity's Release would
    // recursively enter this hook when only our private reference remains.
    if(count==runtime.privateRefFloor && !runtime.destroying && object==static_cast<IUnknown*>(runtime.proxy)){runtime.Shutdown();runtime.proxy=nullptr;runtime.proxyKey=0;return fn(object);}return count;
}
inline HRESULT STDMETHODCALLTYPE SetColor(IDXGISwapChain3* chain,DXGI_COLOR_SPACE_TYPE color){
    const auto fn=p::Original<HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain3*,DXGI_COLOR_SPACE_TYPE),38>(chain);
    std::lock_guard<std::recursive_mutex> lock(runtime.mutex);auto hr=fn(chain,color);if(SUCCEEDED(hr)){runtime.colorSpace=color;
        if(color!=DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709){runtime.validFrame=false;runtime.Disable();if(runtime.log)runtime.log("FSR FG disabled for unsupported HDR color space=%u",unsigned(color));}}return hr;
}
inline HRESULT STDMETHODCALLTYPE Resize1(IDXGISwapChain3* chain,UINT count,UINT w,UINT h,DXGI_FORMAT format,UINT flags,const UINT* masks,IUnknown*const* queues){
    const auto fn=p::Original<HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain3*,UINT,UINT,UINT,DXGI_FORMAT,UINT,const UINT*,IUnknown*const*),39>(chain);
    runtime.BeforeResize();auto hr=fn(chain,count,w,h,format,flags,masks,queues);if(SUCCEEDED(hr))scene_probe::Chain(chain);return hr;
}
inline HRESULT STDMETHODCALLTYPE Fullscreen(IDXGISwapChain* chain,BOOL full,IDXGIOutput* output){
    const auto fn=p::Original<HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,BOOL,IDXGIOutput*),10>(chain);
    runtime.BeforeResize();return fn(chain,full,output);
}
inline void Adopt(IDXGISwapChain4* chain){
    runtime.proxy=chain;runtime.proxyKey=p::Identity(chain);
    // The pinned AMD proxy context also owns a COM reference. Record that floor
    // plus our private reference, excluding the one returned to the application.
    runtime.privateRefFloor=chain->AddRef()-1;
    p::Hook(chain,2,reinterpret_cast<void*>(Released));p::Hook(chain,38,reinterpret_cast<void*>(SetColor));p::Hook(chain,39,reinterpret_cast<void*>(Resize1));p::Hook(chain,10,reinterpret_cast<void*>(Fullscreen));
    if(runtime.log)runtime.log("FSR FG proxy installed; private/context reference floor=%lu; F9 toggles FG; direct DLSS SR preserved",runtime.privateRefFloor);
}
inline bool CreateLegacy(IDXGIFactory* factory,IUnknown* object,DXGI_SWAP_CHAIN_DESC* desc,IDXGISwapChain** output,HRESULT& hr){
    if(creating || !desc || !output)return false;std::lock_guard<std::recursive_mutex> lock(runtime.mutex);if(!runtime.CanCreate(object))return false;
    ffxCreateContextDescFrameGenerationSwapChainVersionDX12 version{};version.header.type=FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_VERSION_DX12;version.version=FFX_FRAMEGENERATION_SWAPCHAIN_DX12_VERSION;
    IDXGISwapChain4* chain=nullptr;ffxCreateContextDescFrameGenerationSwapChainNewDX12 create{};create.header.type=FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_NEW_DX12;create.header.pNext=&version.header;
    create.swapchain=&chain;create.desc=desc;create.dxgiFactory=factory;create.gameQueue=runtime.queue.Get();creating=true;auto rc=runtime.create(&runtime.swapContext,&create.header,nullptr);creating=false;
    if(rc || !chain){if(runtime.log)runtime.log("FSR FG proxy creation failed result=%u; using native swapchain",rc);return false;}
    Adopt(chain);hr=chain->QueryInterface(IID_PPV_ARGS(output));chain->Release();return true;
}
inline bool CreateHwnd(IDXGIFactory2* factory,IUnknown* object,HWND hwnd,const DXGI_SWAP_CHAIN_DESC1* desc,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen,IDXGIOutput* restricted,IDXGISwapChain1** output,HRESULT& hr){
    if(creating || !desc || !output || restricted)return false;std::lock_guard<std::recursive_mutex> lock(runtime.mutex);if(!runtime.CanCreate(object))return false;
    ffxCreateContextDescFrameGenerationSwapChainVersionDX12 version{};version.header.type=FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_VERSION_DX12;version.version=FFX_FRAMEGENERATION_SWAPCHAIN_DX12_VERSION;
    IDXGISwapChain4* chain=nullptr;auto copy=*desc;DXGI_SWAP_CHAIN_FULLSCREEN_DESC fs{};if(fullscreen)fs=*fullscreen;
    ffxCreateContextDescFrameGenerationSwapChainForHwndDX12 create{};create.header.type=FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_FOR_HWND_DX12;create.header.pNext=&version.header;
    create.swapchain=&chain;create.hwnd=hwnd;create.desc=&copy;create.fullscreenDesc=fullscreen?&fs:nullptr;create.dxgiFactory=factory;create.gameQueue=runtime.queue.Get();creating=true;auto rc=runtime.create(&runtime.swapContext,&create.header,nullptr);creating=false;
    if(rc || !chain){if(runtime.log)runtime.log("FSR FG HWND proxy creation failed result=%u",rc);return false;}
    Adopt(chain);hr=chain->QueryInterface(IID_PPV_ARGS(output));chain->Release();return true;
}
inline HRESULT Present(IDXGISwapChain* chain,UINT interval,UINT flags,const DXGI_PRESENT_PARAMETERS* parameters,bool one,void* fn){
    auto forward=[&]{return one?reinterpret_cast<p::Present1>(fn)(static_cast<IDXGISwapChain1*>(chain),interval,flags,parameters):reinterpret_cast<p::Present>(fn)(chain,interval,flags);};
    const auto key=p::Identity(chain);
    if(key!=runtime.proxyKey){const auto hr=forward();if(key==runtime.nativeKey.load() && hr==S_OK && !(flags&DXGI_PRESENT_TEST))++runtime.nativePresents;return hr;}
    if(flags&DXGI_PRESENT_TEST)return forward();std::lock_guard<std::recursive_mutex> lock(runtime.mutex);
    const bool eligible=runtime.desired && !runtime.failed && runtime.validFrame && runtime.copied && runtime.preparedSubmitted && runtime.toneSubmitted;
    if(!eligible){runtime.Disable();++runtime.skipped;}
    runtime.presentThread=GetCurrentThreadId();auto hr=forward();runtime.presentThread=0;runtime.validFrame=false;
    const auto n=++runtime.renderedPresents;
    if(runtime.callbackErrors.load()){runtime.failed=true;runtime.Disable();}
    if(runtime.log && (n%120==0 || (eligible && n<30)))runtime.log("FSR FG present: enabled=%u rendered=%llu native=%llu generation_dispatches=%llu errors=%llu result=0x%08lx",unsigned(eligible),n,runtime.nativePresents.load(),runtime.generatedDispatches.load(),runtime.callbackErrors.load(),hr);
    return hr;
}
inline bool Install(IDXGIFactory1* factory,const std::filesystem::path& folder,p::Logger log){
    if(!runtime.Load(folder,log))return false;
    p::state.createOverride=CreateLegacy;p::state.hwndOverride=CreateHwnd;p::state.presentOverride=Present;
    p::state.beforeResize=[](IDXGISwapChain* chain){if(p::Identity(chain)==runtime.proxyKey)runtime.BeforeResize();};
    p::state.chainNotification=[](IDXGISwapChain* chain){if(creating){runtime.nativeKey=p::Identity(chain);return;}
        if(p::Identity(chain)==runtime.proxyKey)scene_probe::Chain(chain);};
    scene_probe::state.continuous=true;scene_probe::state.temporalReady=true;
    scene_probe::state.verifiedDraw=[](ID3D12GraphicsCommandList* list,ID3D12Resource* source){runtime.CopyHudless(list,source);};
    return p::Install(factory,log);
}
inline void RecordPrepared(ID3D12GraphicsCommandList* list,const Constants& c,ID3D12Resource* depth,ID3D12Resource* motion,UINT w,UINT h,bool reset){
    Camera cam;if(!DecodeCamera(c,cam))return;runtime.Prepare(list,cam,c.jitterX*w/c.activeWidth,c.jitterY*h/c.activeHeight,depth,motion,w,h,reset);
}
}
