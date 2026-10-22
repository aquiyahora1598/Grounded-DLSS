#pragma once
// Metadata only. Matches a PS observed in the historical original-game capture,
// then verifies its destination against the actual swapchain buffers at runtime.
// Does not copy images, insert barriers, dispatch work, or enable interpolation.
#include "presentation_probe.h"
#include <array>
#include <unordered_map>
#include <unordered_set>
namespace grounded::framegen::scene_probe {
namespace p=grounded::framegen::probe;
struct List {bool backbuffer=false,renderPass=false;uintptr_t resource=0;};
struct State {
    std::mutex mutex;
    std::unordered_set<uintptr_t> buffers;
    std::unordered_map<SIZE_T,uintptr_t> views;
    std::unordered_map<void*,List> lists;
    std::atomic<void*> tone{nullptr};
    std::atomic<unsigned> remaining{8};
    std::atomic<bool> temporalReady{false};
    bool continuous=false;
    unsigned rtvStride=0;
    void(*verifiedDraw)(ID3D12GraphicsCommandList*,ID3D12Resource*)=nullptr;
};
inline State& state=*new State;
inline void Chain(IDXGISwapChain* chain){
    DXGI_SWAP_CHAIN_DESC desc{};if(FAILED(chain->GetDesc(&desc)) || desc.BufferCount>16)return;
    std::unordered_set<uintptr_t> buffers;
    for(UINT i=0;i<desc.BufferCount;++i){p::ComPtr<ID3D12Resource> texture;
        if(SUCCEEDED(chain->GetBuffer(i,IID_PPV_ARGS(&texture))))buffers.insert(reinterpret_cast<uintptr_t>(texture.Get()));}
    std::lock_guard<std::mutex> lock(state.mutex);state.buffers=std::move(buffers);state.lists.clear();
}
inline void View(ID3D12Device* device,ID3D12Resource* resource,D3D12_CPU_DESCRIPTOR_HANDLE handle){
    if(!state.continuous && !state.remaining.load(std::memory_order_relaxed))return;
    std::lock_guard<std::mutex> lock(state.mutex);
    if(state.views.size()>=8192)state.views.clear();
    state.views[handle.ptr]=reinterpret_cast<uintptr_t>(resource);
    state.rtvStride=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
}
inline void Targets(ID3D12GraphicsCommandList* list,UINT count,const D3D12_CPU_DESCRIPTOR_HANDLE* handles,BOOL single,bool renderPass){
    if(!state.continuous && !state.remaining.load(std::memory_order_relaxed))return;
    std::lock_guard<std::mutex> lock(state.mutex);
    if(state.lists.size()>=4096)state.lists.clear();
    List value;value.renderPass=renderPass;
    for(UINT i=0;handles && i<count && i<8;++i){
        const auto handle=single?handles[0].ptr+SIZE_T(i)*state.rtvStride:handles[i].ptr;
        auto view=state.views.find(handle);
        if(view!=state.views.end() && state.buffers.count(view->second)){value.backbuffer=true;value.resource=view->second;break;}
    }
    state.lists[list]=value;
}
inline void End(ID3D12GraphicsCommandList* list){
    if(!state.continuous && !state.remaining.load(std::memory_order_relaxed))return;
    std::lock_guard<std::mutex> lock(state.mutex);auto hit=state.lists.find(list);
    if(hit!=state.lists.end())hit->second.renderPass=false;
}
inline void Reset(ID3D12GraphicsCommandList* list){
    std::lock_guard<std::mutex> lock(state.mutex);state.lists.erase(list);
}
inline void Released(void* object){void* expected=object;state.tone.compare_exchange_strong(expected,nullptr);}
inline bool Pipeline(void* object,const D3D12_SHADER_BYTECODE& code){
    if(!object || !code.pShaderBytecode || code.BytecodeLength!=2276)return false;
    uint64_t hash=14695981039346656037ull;auto bytes=static_cast<const unsigned char*>(code.pShaderBytecode);
    for(size_t i=0;i<code.BytecodeLength;++i)hash=(hash^bytes[i])*1099511628211ull;
    if(hash!=0x4d65421511290023ull)return false;
    state.tone.store(object,std::memory_order_release);
    if(p::state.log)p::state.log("FG scene probe historical tone PS matched hash=%016llx bytes=%zu pipeline=%p; destination not yet verified",hash,code.BytecodeLength,object);
    return true;
}
inline void Draw(ID3D12GraphicsCommandList* list,void* pipeline,UINT elements,bool indexed){
    if(!state.temporalReady.load(std::memory_order_relaxed) || (!state.continuous && !state.remaining.load(std::memory_order_relaxed)) || !pipeline || state.tone.load(std::memory_order_acquire)!=pipeline)return;
    List target;
    {std::lock_guard<std::mutex> lock(state.mutex);auto hit=state.lists.find(list);if(hit!=state.lists.end())target=hit->second;}
    unsigned pending=state.remaining.load();while(pending && !state.remaining.compare_exchange_weak(pending,pending-1)){}
    if(state.continuous && target.backbuffer && !target.renderPass && state.verifiedDraw)state.verifiedDraw(list,reinterpret_cast<ID3D12Resource*>(target.resource));
    if(!pending)return;
    if(p::state.log)p::state.log("FG scene probe tone draw sample=%u backbuffer_verified=%u inside_render_pass=%u resource=%p elements=%u indexed=%u observer_generates_frames=0",
        9-pending,unsigned(target.backbuffer),unsigned(target.renderPass),reinterpret_cast<void*>(target.resource),elements,unsigned(indexed));
    if(pending>=7)p::Stack("tone-draw");
    if(!state.continuous && target.backbuffer && !target.renderPass && state.verifiedDraw)state.verifiedDraw(list,reinterpret_cast<ID3D12Resource*>(target.resource));
}
}
