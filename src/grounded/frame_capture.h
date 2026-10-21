#pragma once
// Explicit one-frame diagnostic. Compiled only into the capture build.
// Caller serializes every method. No GPU waits; disk writes only after Poll
// observes the submission fence, on the background control thread.
#include "integration.h"
#include <array>
#include <fstream>
#include <iomanip>
#include <vector>
namespace grounded::frame_capture {
using Microsoft::WRL::ComPtr;
struct Image {
    ComPtr<ID3D12Resource> source,readback;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT rows=0;UINT64 rowBytes=0,size=0;
    const char* name="";
};
class Capture {
    std::array<Image,3> images{};
    ComPtr<ID3D12Fence> fence;
    ID3D12CommandList* recording=nullptr;
    UINT64 sequence=0,submittedValue=0;
    bool armed=false,dlss=false,recorded=false;
    Constants constants{};
    std::string error;
    bool Fail(const char* message){error=message;armed=false;return false;}
public:
    bool Armed()const{return armed;}
    bool Busy()const{return armed||recorded;}
    bool Pending(ID3D12CommandList* list)const{return recorded&&recording==list&&!submittedValue;}
    const std::string& Error()const{return error;}
    bool Arm(UINT64 number){
        if(!number || Busy())return false;
        sequence=number;armed=true;dlss=false;submittedValue=0;error.clear();return true;
    }
    void CancelArmed(){if(armed){armed=false;error="No matching temporal pass within capture deadline";}}
    bool Record(ID3D12GraphicsCommandList* list,const PassInput& input,bool usedDlss){
        if(!armed || !list)return false;
        ComPtr<ID3D12Device> device;
        if(FAILED(list->GetDevice(IID_PPV_ARGS(&device))))return Fail("Capture device unavailable");
        const std::array<ID3D12Resource*,3> sources{input.color,input.output,input.exposure};
        const std::array<const char*,3> names{"input.rgba16f","output.rgba16f","exposure.rgba32f"};
        std::array<Image,3> next{};
        for(unsigned i=0;i<3;++i){
            if(!sources[i])return Fail("Capture resource missing");
            auto desc=sources[i]->GetDesc();
            if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D || desc.DepthOrArraySize!=1 ||
               desc.MipLevels!=1 || desc.SampleDesc.Count!=1 || desc.Width>8192 || desc.Height>8192 ||
               desc.Format!=(i==2?DXGI_FORMAT_R32G32B32A32_FLOAT:DXGI_FORMAT_R16G16B16A16_FLOAT))return Fail("Capture resource contract differs");
            auto& image=next[i];image.name=names[i];image.source=sources[i];
            device->GetCopyableFootprints(&desc,0,1,0,&image.footprint,&image.rows,&image.rowBytes,&image.size);
            if(!image.size || image.size>512ull*1024*1024)return Fail("Capture exceeds bounded memory budget");
            D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_READBACK;
            D3D12_RESOURCE_DESC rd{};rd.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;rd.Width=image.size;
            rd.Height=1;rd.DepthOrArraySize=rd.MipLevels=1;rd.SampleDesc.Count=1;rd.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            if(FAILED(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&rd,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&image.readback))))return Fail("Capture readback allocation failed");
        }
        ComPtr<ID3D12Fence> nextFence;
        if(FAILED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&nextFence))))return Fail("Capture fence creation failed");
        // Allocate/validate everything before recording any GPU command.
        for(unsigned i=0;i<3;++i){
            auto& image=next[i];D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            b.Transition={image.source.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                i==1?D3D12_RESOURCE_STATE_UNORDERED_ACCESS:D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_SOURCE};
            list->ResourceBarrier(1,&b);
            D3D12_TEXTURE_COPY_LOCATION src{},dst{};src.pResource=image.source.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dst.pResource=image.readback.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=image.footprint;
            list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);
            std::swap(b.Transition.StateBefore,b.Transition.StateAfter);list->ResourceBarrier(1,&b);
        }
        images=std::move(next);fence=std::move(nextFence);constants=input.constants;
        dlss=usedDlss;recording=list;recorded=true;armed=false;return true;
    }
    bool Submitted(ID3D12CommandQueue* queue,UINT count,ID3D12CommandList* const* lists){
        if(!recorded || submittedValue)return false;
        bool found=false;for(UINT i=0;i<count;++i)found|=lists[i]==recording;
        if(!found)return false;
        if(FAILED(queue->Signal(fence.Get(),1))){error="Capture queue signal failed";return false;}
        submittedValue=1;recording=nullptr;return true;
    }
    void Discarded(ID3D12CommandList* list){
        if(Pending(list)){images={};fence.Reset();recording=nullptr;recorded=false;error="Capture recording discarded before submission";}
    }
    // Returns 0 while not ready, 1 after durable output, -1 after an error.
    int Poll(const std::filesystem::path& directory){
        if(!recorded || !submittedValue || fence->GetCompletedValue()<submittedValue)return 0;
        recorded=false; // The GPU is done; a failed disk write cannot block future captures.
        if(fence->GetCompletedValue()==UINT64_MAX){error="Capture device removed";return -1;}
        const auto target=directory/std::to_wstring(sequence);
        try{
            if(std::filesystem::exists(target)){error="Capture destination already exists";return -1;}
            std::filesystem::create_directories(target);
            std::ofstream meta(target/L"frame.json");meta<<std::setprecision(9)<<"{\"sequence\":"<<sequence<<",\"pid\":"<<GetCurrentProcessId()
                <<",\"dlss\":"<<(dlss?"true":"false")<<",\"active\":["<<constants.activeWidth<<','<<constants.activeHeight
                <<"],\"output\":["<<constants.outputWidth<<','<<constants.outputHeight<<"],\"jitter\":["<<constants.jitterX<<','<<constants.jitterY<<"],\"images\":[";
            for(unsigned i=0;i<3;++i){
                auto& image=images[i];void* pointer=nullptr;D3D12_RANGE range{0,SIZE_T(image.size)};
                if(FAILED(image.readback->Map(0,&range,&pointer))){error="Capture map failed";return -1;}
                std::ofstream file(target/image.name,std::ios::binary);
                for(UINT row=0;row<image.rows;++row)file.write(static_cast<const char*>(pointer)+image.footprint.Offset+SIZE_T(row)*image.footprint.Footprint.RowPitch,std::streamsize(image.rowBytes));
                file.close();const bool saved=bool(file);D3D12_RANGE written{0,0};image.readback->Unmap(0,&written);
                if(!saved){error="Capture image write failed";return -1;}
                if(i)meta<<',';
                meta<<"{\"file\":\""<<image.name<<"\",\"width\":"<<image.footprint.Footprint.Width<<",\"height\":"<<image.footprint.Footprint.Height
                    <<",\"format\":"<<unsigned(image.footprint.Footprint.Format)<<",\"row_bytes\":"<<image.rowBytes<<'}';
            }
            meta<<"],\"pass\":[";for(size_t i=0;i<constants.pass.size();++i){if(i)meta<<',';if(std::isfinite(constants.pass[i]))meta<<constants.pass[i];else meta<<"null";}
            meta<<"],\"view\":[";for(size_t i=0;i<constants.view.size();++i){if(i)meta<<',';if(std::isfinite(constants.view[i]))meta<<constants.view[i];else meta<<"null";}meta<<"]}\n";
            meta.close();if(!meta){error="Capture metadata write failed";return -1;}
            images={};fence.Reset();recorded=false;return 1;
        }catch(const std::exception& e){error=e.what();return -1;}
    }
};
}
