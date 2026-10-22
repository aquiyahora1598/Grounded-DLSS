#pragma once
// DXGI hooks and bounded presentation metadata shared by the FSR backend.
// This observer itself generates no GPU work; fsr_runtime.h owns interpolation.
#include <windows.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <intrin.h>

namespace grounded::framegen::probe {
using Microsoft::WRL::ComPtr;
using Logger=void(*)(const char*,...);
struct Chain {
    uintptr_t queue=0;
    DXGI_SWAP_CHAIN_DESC desc{};
    uint64_t serial=0, presents=0, temporal=0;
    LONGLONG lastPresent=0;
};
struct Temporal {
    uintptr_t list=0, queue=0;
    uint64_t recorded=0, submitted=0;
    unsigned inputWidth=0,inputHeight=0,outputWidth=0,outputHeight=0;
};
struct State {
    std::mutex hooksMutex,mutex;
    std::map<std::pair<void**,UINT>,void*> originals;
    std::map<uintptr_t,Chain> chains;
    Temporal temporal{};
    uint64_t serial=0, samples=0;
    std::atomic<bool> terminating{false};
    Logger log=nullptr;
    void(*chainNotification)(IDXGISwapChain*)=nullptr;
    bool(*createOverride)(IDXGIFactory*,IUnknown*,DXGI_SWAP_CHAIN_DESC*,IDXGISwapChain**,HRESULT&)=nullptr;
    bool(*hwndOverride)(IDXGIFactory2*,IUnknown*,HWND,const DXGI_SWAP_CHAIN_DESC1*,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*,IDXGIOutput*,IDXGISwapChain1**,HRESULT&)=nullptr;
    HRESULT(*presentOverride)(IDXGISwapChain*,UINT,UINT,const DXGI_PRESENT_PARAMETERS*,bool,void*)=nullptr;
    void(*beforeResize)(IDXGISwapChain*)=nullptr;
    void(*afterResize)(IDXGISwapChain*,HRESULT)=nullptr;
    uintptr_t image=0;size_t imageBytes=0;
};
inline State& state=*new State; // Hooks live until process exit.
inline uintptr_t Identity(IUnknown* object){
    ComPtr<IUnknown> identity;
    return object && SUCCEEDED(object->QueryInterface(IID_PPV_ARGS(&identity)))?reinterpret_cast<uintptr_t>(identity.Get()):0;
}
inline uintptr_t Rva(void* address){
    const auto p=reinterpret_cast<uintptr_t>(address);
    return p>=state.image && p-state.image<state.imageBytes?p-state.image:0;
}
inline void Stack(const char* event){
    if(!state.log)return;
    void* addresses[24]{};auto count=CaptureStackBackTrace(1,24,addresses,nullptr);
    unsigned kept=0;
    for(unsigned i=0;i<count && kept<10;++i)if(auto rva=Rva(addresses[i])){
        state.log("FG probe stack event=%s frame=%u game_rva=0x%llx",event,kept++,static_cast<unsigned long long>(rva));
    }
}
template<class T,UINT Slot> inline T Original(IUnknown* object){
    auto key=std::make_pair(*reinterpret_cast<void***>(object),Slot);
    if(state.terminating.load(std::memory_order_acquire))return reinterpret_cast<T>(state.originals.at(key));
    std::lock_guard<std::mutex> lock(state.hooksMutex);
    return reinterpret_cast<T>(state.originals.at(key));
}
inline bool Hook(IUnknown* object,UINT slot,void* replacement){
    std::lock_guard<std::mutex> lock(state.hooksMutex);
    auto table=*reinterpret_cast<void***>(object);auto key=std::make_pair(table,slot);
    if(state.originals.count(key))return true;
    DWORD protection=0;
    if(!VirtualProtect(table+slot,sizeof(void*),PAGE_READWRITE,&protection))return false;
    state.originals.emplace(key,table[slot]);
    InterlockedExchangePointer(reinterpret_cast<void* volatile*>(table+slot),replacement);
    DWORD ignored=0;VirtualProtect(table+slot,sizeof(void*),protection,&ignored);
    return true;
}
inline void RecordTemporal(ID3D12GraphicsCommandList* list,unsigned iw,unsigned ih,unsigned ow,unsigned oh){
    if(state.terminating.load(std::memory_order_relaxed))return;
    std::lock_guard<std::mutex> lock(state.mutex);
    auto& t=state.temporal;t.list=reinterpret_cast<uintptr_t>(list);t.queue=0;
    ++t.recorded;t.inputWidth=iw;t.inputHeight=ih;t.outputWidth=ow;t.outputHeight=oh;
}
inline void Discard(ID3D12GraphicsCommandList* list){
    std::lock_guard<std::mutex> lock(state.mutex);
    if(state.temporal.list==reinterpret_cast<uintptr_t>(list))state.temporal.list=0;
}
inline void Submitted(ID3D12CommandQueue* queue,UINT count,ID3D12CommandList*const* lists){
    std::lock_guard<std::mutex> lock(state.mutex);
    auto& t=state.temporal;
    for(UINT i=0;i<count;++i)if(t.list==reinterpret_cast<uintptr_t>(lists[i])){
        t.submitted=t.recorded;t.queue=Identity(queue);t.list=0;break;
    }
}
inline void PresentSample(IDXGISwapChain* chain,UINT flags,void* caller){
    if(flags&DXGI_PRESENT_TEST)return;
    bool stack=false;
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        auto hit=state.chains.find(Identity(chain));
        if(hit==state.chains.end())return;
        auto& c=hit->second;const auto& t=state.temporal;
        LARGE_INTEGER now{},frequency{};QueryPerformanceCounter(&now);QueryPerformanceFrequency(&frequency);
        ++c.presents;
        const auto previous=c.lastPresent;c.lastPresent=now.QuadPart;
        const bool paired=t.queue && t.queue==c.queue && t.submitted>c.temporal;
        if(paired)c.temporal=t.submitted;
        const bool selected=c.presents<=2 || (paired && (state.samples<18 || c.presents%120==0));
        if(!selected || state.samples>=64)return;
        ++state.samples;stack=state.samples<=4;
        if(state.log)state.log("FG probe present chain=%llu present=%llu paired_temporal=%u temporal_seq=%llu input=%ux%u output=%ux%u cpu_interval_ms=%.3f caller_rva=0x%llx observer_generates_frames=0",
            static_cast<unsigned long long>(c.serial),static_cast<unsigned long long>(c.presents),unsigned(paired),
            static_cast<unsigned long long>(paired?t.submitted:0),t.inputWidth,t.inputHeight,t.outputWidth,t.outputHeight,
            previous?1000.0*double(now.QuadPart-previous)/double(frequency.QuadPart):0.0,static_cast<unsigned long long>(Rva(caller)));
    }
    if(stack)Stack("present");
}
using Present=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT);
inline HRESULT STDMETHODCALLTYPE OnPresent(IDXGISwapChain* chain,UINT interval,UINT flags){
    auto fn=Original<Present,8>(chain);
    if(state.presentOverride && !state.terminating.load())return state.presentOverride(chain,interval,flags,nullptr,false,reinterpret_cast<void*>(fn));
    if(!state.terminating.load(std::memory_order_acquire))PresentSample(chain,flags,_ReturnAddress());
    return fn(chain,interval,flags);
}
using Present1=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain1*,UINT,UINT,const DXGI_PRESENT_PARAMETERS*);
inline HRESULT STDMETHODCALLTYPE OnPresent1(IDXGISwapChain1* chain,UINT interval,UINT flags,const DXGI_PRESENT_PARAMETERS* parameters){
    auto fn=Original<Present1,22>(chain);
    if(state.presentOverride && !state.terminating.load())return state.presentOverride(chain,interval,flags,parameters,true,reinterpret_cast<void*>(fn));
    if(!state.terminating.load(std::memory_order_acquire))PresentSample(chain,flags,_ReturnAddress());
    return fn(chain,interval,flags,parameters);
}
using Resize=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT,UINT,DXGI_FORMAT,UINT);
inline HRESULT STDMETHODCALLTYPE OnResize(IDXGISwapChain* chain,UINT buffers,UINT width,UINT height,DXGI_FORMAT format,UINT flags){
    auto fn=Original<Resize,13>(chain);
    if(state.terminating.load(std::memory_order_acquire))return fn(chain,buffers,width,height,format,flags);
    if(state.beforeResize)state.beforeResize(chain);
    const auto hr=fn(chain,buffers,width,height,format,flags);
    if(state.afterResize)state.afterResize(chain,hr);
    DXGI_SWAP_CHAIN_DESC desc{};chain->GetDesc(&desc);
    {std::lock_guard<std::mutex> lock(state.mutex);auto hit=state.chains.find(Identity(chain));
        if(hit!=state.chains.end()){hit->second.desc=desc;hit->second.temporal=0;hit->second.lastPresent=0;
            state.temporal.list=0;state.temporal.queue=0;state.temporal.submitted=0;}}
    if(state.log)state.log("FG probe resize result=0x%08lx size=%ux%u format=%u observer_generates_frames=0",hr,desc.BufferDesc.Width,desc.BufferDesc.Height,unsigned(desc.BufferDesc.Format));
    if(SUCCEEDED(hr) && state.chainNotification)state.chainNotification(chain);
    return hr;
}
inline bool Attach(IUnknown* creationDevice,IDXGISwapChain* chain,void* caller){
    ComPtr<ID3D12CommandQueue> queue;
    if(!creationDevice || FAILED(creationDevice->QueryInterface(IID_PPV_ARGS(&queue))) || queue->GetDesc().Type!=D3D12_COMMAND_LIST_TYPE_DIRECT)return false;
    DXGI_SWAP_CHAIN_DESC desc{};if(FAILED(chain->GetDesc(&desc)))return false;
    const auto key=Identity(chain);const auto queueKey=Identity(queue.Get());
    uint64_t serial=0;
    {std::lock_guard<std::mutex> lock(state.mutex);
        // No COM references are retained. Re-created objects may reuse an address.
        if(!state.chains.count(key) && state.chains.size()>=8)state.chains.erase(state.chains.begin());
        Chain c;c.queue=queueKey;c.desc=desc;c.serial=serial=++state.serial;state.chains.insert_or_assign(key,c);}
    bool ok=Hook(chain,8,reinterpret_cast<void*>(&OnPresent)) && Hook(chain,13,reinterpret_cast<void*>(&OnResize));
    ComPtr<IDXGISwapChain1> one;if(SUCCEEDED(chain->QueryInterface(IID_PPV_ARGS(&one))))ok=Hook(one.Get(),22,reinterpret_cast<void*>(&OnPresent1))&&ok;
    // No Release hook: COM interfaces may use different identity pointers and
    // teardown may run under loader lock. Entries are bounded and replaced at creation.
    if(state.log)state.log("FG probe swapchain chain=%llu hooks=%u size=%ux%u format=%u buffers=%u effect=%u flags=0x%x creation_caller_rva=0x%llx observer_generates_frames=0",
        static_cast<unsigned long long>(serial),unsigned(ok),desc.BufferDesc.Width,desc.BufferDesc.Height,unsigned(desc.BufferDesc.Format),desc.BufferCount,unsigned(desc.SwapEffect),desc.Flags,static_cast<unsigned long long>(Rva(caller)));
    if(state.chainNotification)state.chainNotification(chain);
    Stack("create-swapchain");return ok;
}
using Make=HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory*,IUnknown*,DXGI_SWAP_CHAIN_DESC*,IDXGISwapChain**);
inline HRESULT STDMETHODCALLTYPE Create(IDXGIFactory* factory,IUnknown* device,DXGI_SWAP_CHAIN_DESC* desc,IDXGISwapChain** out){
    HRESULT intercepted=E_FAIL;
    if(state.createOverride && state.createOverride(factory,device,desc,out,intercepted)){
        if(SUCCEEDED(intercepted) && out && *out)Attach(device,*out,_ReturnAddress());return intercepted;}
    const auto hr=Original<Make,10>(factory)(factory,device,desc,out);
    if(!state.terminating.load(std::memory_order_acquire) && SUCCEEDED(hr) && out && *out)Attach(device,*out,_ReturnAddress());
    return hr;
}
using MakeHwnd=HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory2*,IUnknown*,HWND,const DXGI_SWAP_CHAIN_DESC1*,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*,IDXGIOutput*,IDXGISwapChain1**);
inline HRESULT STDMETHODCALLTYPE CreateHwnd(IDXGIFactory2* factory,IUnknown* device,HWND hwnd,const DXGI_SWAP_CHAIN_DESC1* desc,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen,IDXGIOutput* restrictOutput,IDXGISwapChain1** out){
    HRESULT intercepted=E_FAIL;
    if(state.hwndOverride && state.hwndOverride(factory,device,hwnd,desc,fullscreen,restrictOutput,out,intercepted)){
        if(SUCCEEDED(intercepted) && out && *out)Attach(device,*out,_ReturnAddress());return intercepted;}
    const auto hr=Original<MakeHwnd,15>(factory)(factory,device,hwnd,desc,fullscreen,restrictOutput,out);
    if(!state.terminating.load(std::memory_order_acquire) && SUCCEEDED(hr) && out && *out)Attach(device,*out,_ReturnAddress());
    return hr;
}
inline bool Install(IDXGIFactory1* factory,Logger log,uintptr_t image=0,size_t imageBytes=0){
    state.log=log;state.image=image;state.imageBytes=imageBytes;
    ComPtr<IDXGIFactory2> two;
    const bool ok=Hook(factory,10,reinterpret_cast<void*>(&Create)) && SUCCEEDED(factory->QueryInterface(IID_PPV_ARGS(&two))) && Hook(two.Get(),15,reinterpret_cast<void*>(&CreateHwnd));
    if(log)log("FG presentation probe installed=%u; CPU metadata only; no GPU work; observer_generates_frames=0",unsigned(ok));
    return ok;
}
inline void Terminating(){state.terminating.store(true,std::memory_order_release);}
}
