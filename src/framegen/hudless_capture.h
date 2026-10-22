#pragma once
// One-shot development capture, not a frame-generation backend. The caller must
// verify the bound swapchain RT and that no D3D12 render pass is active. Source
// commands are inserted in the same list as the host's RT draw; its normal GPU
// lifetime covers this extra use. No backbuffer reference survives Record().
#include "presentation_probe.h"
#include <filesystem>
#include <fstream>
namespace grounded::framegen {
class HudlessCapture {
    std::mutex mutex;
    probe::ComPtr<ID3D12Resource> color,readback;
    probe::ComPtr<ID3D12Fence> fence;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT rows=0;UINT64 rowBytes=0,bytes=0;
    ID3D12CommandList* recording=nullptr;
    bool started=false,submitted=false,saved=false;
    std::string error;
public:
    bool Record(ID3D12GraphicsCommandList* list,ID3D12Resource* source){
        std::lock_guard<std::mutex> lock(mutex);
        if(started || !list || !source || list->GetType()!=D3D12_COMMAND_LIST_TYPE_DIRECT)return false;
        started=true;
        auto desc=source->GetDesc();
        if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D || desc.DepthOrArraySize!=1 || desc.MipLevels!=1 ||
           desc.SampleDesc.Count!=1 || desc.Width<32 || desc.Width>8192 || desc.Height<32 || desc.Height>8192 ||
           desc.Format!=DXGI_FORMAT_R10G10B10A2_UNORM || !(desc.Flags&D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)){
            error="HUD-less source contract differs";return false;}
        probe::ComPtr<ID3D12Device> device;
        if(FAILED(list->GetDevice(IID_PPV_ARGS(&device)))){error="HUD-less device unavailable";return false;}
        D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;
        desc.Flags=D3D12_RESOURCE_FLAG_NONE;desc.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;
        if(FAILED(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&color)))){error="HUD-less color allocation failed";return false;}
        device->GetCopyableFootprints(&desc,0,1,0,&footprint,&rows,&rowBytes,&bytes);
        if(bytes>256ull*1024*1024){error="HUD-less capture exceeds memory bound";return false;}
        hp.Type=D3D12_HEAP_TYPE_READBACK;D3D12_RESOURCE_DESC buffer{};buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
        buffer.Width=bytes;buffer.Height=1;buffer.DepthOrArraySize=buffer.MipLevels=1;buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if(FAILED(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback))) ||
           FAILED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)))){error="HUD-less readback/fence allocation failed";return false;}
        D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition={source,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE};
        list->ResourceBarrier(1,&barrier);list->CopyResource(color.Get(),source);
        std::swap(barrier.Transition.StateBefore,barrier.Transition.StateAfter);list->ResourceBarrier(1,&barrier);
        barrier.Transition={color.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_COPY_SOURCE};
        list->ResourceBarrier(1,&barrier);
        D3D12_TEXTURE_COPY_LOCATION src{},dst{};src.pResource=color.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.pResource=readback.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=footprint;
        list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);
        barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_SOURCE;barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        list->ResourceBarrier(1,&barrier);recording=list;return true;
    }
    void Submitted(ID3D12CommandQueue* queue,UINT count,ID3D12CommandList* const* lists){
        std::lock_guard<std::mutex> lock(mutex);if(!recording || submitted)return;
        for(UINT i=0;i<count;++i)if(lists[i]==recording){
            // Called after the host's original ExecuteCommandLists. Failure keeps
            // the allocations alive: never guess that an untracked GPU use ended.
            if(FAILED(queue->Signal(fence.Get(),1)))error="HUD-less submission fence failed";
            else submitted=true;return;
        }
    }
    void Discarded(ID3D12CommandList* list){
        std::lock_guard<std::mutex> lock(mutex);
        if(recording==list && !submitted && error.empty()){
            color.Reset();readback.Reset();fence.Reset();recording=nullptr;started=false;
        }
    }
    int Poll(const std::filesystem::path& directory){
        std::lock_guard<std::mutex> lock(mutex);if(saved)return 0;
        if(!error.empty()){saved=true;if(probe::state.log)probe::state.log("FG HUD-less capture failed: %s",error.c_str());return -1;}
        if(!submitted || fence->GetCompletedValue()<1)return 0;
        saved=true;
        if(fence->GetCompletedValue()==UINT64_MAX){error="HUD-less device removed";return -1;}
        try{
            if(std::filesystem::exists(directory)){error="Preserving existing HUD-less capture";return -1;}
            std::filesystem::create_directories(directory);
            void* pointer=nullptr;D3D12_RANGE range{0,SIZE_T(bytes)};
            if(FAILED(readback->Map(0,&range,&pointer))){error="HUD-less map failed";return -1;}
            std::ofstream raw(directory/L"hudless.r10g10b10a2",std::ios::binary);
            for(UINT row=0;row<rows;++row)raw.write(static_cast<const char*>(pointer)+footprint.Offset+SIZE_T(row)*footprint.Footprint.RowPitch,std::streamsize(rowBytes));
            raw.close();D3D12_RANGE written{0,0};readback->Unmap(0,&written);
            if(!raw){error="HUD-less write failed";return -1;}
            std::ofstream meta(directory/L"capture.json");
            meta<<"{\"pid\":"<<GetCurrentProcessId()<<",\"width\":"<<footprint.Footprint.Width<<",\"height\":"<<footprint.Footprint.Height
                <<",\"format\":24,\"row_bytes\":"<<rowBytes<<",\"gpu_fence_completed\":true,\"fg_enabled\":false}\n";
            meta.close();if(!meta){error="HUD-less metadata write failed";return -1;}
            color.Reset();readback.Reset();fence.Reset();recording=nullptr;return 1;
        }catch(const std::exception& e){error=e.what();return -1;}
    }
};
}
