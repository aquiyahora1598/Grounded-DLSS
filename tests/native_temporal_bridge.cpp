#include "../src/grounded/native_temporal_bridge.h"
#include <dxgi1_6.h>
#include <cassert>
#include <cstdio>
namespace n=grounded::native_temporal;
using Microsoft::WRL::ComPtr;
struct Packet {uintptr_t table=0;Packet* next=nullptr;std::array<UINT,3> groups{240,135,1};UINT padding=0;};
struct Queue {Packet* head=nullptr;Packet** tail=&head;};
Packet shader,dispatch,unset;unsigned packetCalls=0,passCalls=0;DWORD recordThread=0;
void __fastcall OriginalPacket(void* self,void*){assert(reinterpret_cast<Packet*>(self)->table==n::layout.packetTable);assert(n::current && n::current->resourcesValid);++packetCalls;}
void Append(Queue& q,Packet& packet){*q.tail=&packet;q.tail=&packet.next;}
void __fastcall OriginalPass(void*,void* memory){assert(GetCurrentThreadId()==recordThread);++passCalls;auto& queue=*static_cast<Queue*>(memory);Append(queue,shader);Append(queue,dispatch);Append(queue,unset);}
void Put(unsigned char* bytes,unsigned offset,uintptr_t value){memcpy(bytes+offset,&value,8);}
int main(){
    ComPtr<IDXGIFactory4> factory;assert(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))));ComPtr<IDXGIAdapter1> adapter;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> candidate;if(factory->EnumAdapters1(i,&candidate)==DXGI_ERROR_NOT_FOUND)break;
        DXGI_ADAPTER_DESC1 desc{};candidate->GetDesc1(&desc);if(desc.VendorId==0x10de){adapter=candidate;break;}}
    ComPtr<ID3D12Device> device;assert(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device))));
    n::layout={0x10000000,0x15091960,0x14f4ee80,0x14b02108};n::originalPass=&OriginalPass;n::originalPacket=&OriginalPacket;n::shadowPacketTable[0]=reinterpret_cast<void*>(&n::Packet);
    shader.table=0x111;dispatch.table=n::layout.packetTable;unset.table=0x222;
    std::array<ComPtr<ID3D12Resource>,5> resources;
    std::array<std::array<unsigned char,0x60>,5> graph{};
    std::array<std::array<unsigned char,0xc0>,5> rhi{};
    std::array<std::array<unsigned char,0x28>,5> location{};
    const DXGI_FORMAT formats[]={DXGI_FORMAT_R32G32B32A32_FLOAT,DXGI_FORMAT_R16G16B16A16_FLOAT,DXGI_FORMAT_R32G8X24_TYPELESS,DXGI_FORMAT_R16G16B16A16_UNORM,DXGI_FORMAT_R16G16B16A16_FLOAT};
    std::array<unsigned char,0x118> pass{};std::array<unsigned char,0x2a0> parameters{};
    Put(pass.data(),0,n::layout.base+0x4fc1f60);Put(pass.data(),0xf0,reinterpret_cast<uintptr_t>(parameters.data()));
    memcpy(pass.data()+0x108,dispatch.groups.data(),12);constexpr unsigned offsets[]={0x1a0,0x1b0,0x210,0x220};
    std::array<unsigned char,0x38> outputView{};
    Put(outputView.data(),0,n::layout.base+0x4f5f5b0);Put(outputView.data(),0x28,reinterpret_cast<uintptr_t>(graph[4].data()));
    Put(parameters.data(),0x268,reinterpret_cast<uintptr_t>(outputView.data()));
    // The similarly named history at 0x1d0 is deliberately the wrong resource.
    Put(parameters.data(),0x1d0,reinterpret_cast<uintptr_t>(graph[1].data()));
    for(unsigned i=0;i<5;++i){
        D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;D3D12_RESOURCE_DESC desc{};
        desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;desc.Width=i==0?1:i==4?1920:1288;desc.Height=i==0?1:i==4?1080:724;
        desc.DepthOrArraySize=desc.MipLevels=1;desc.SampleDesc.Count=1;desc.Format=formats[i];
        desc.Flags=i==2?D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL:D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        assert(SUCCEEDED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&resources[i]))));
        Put(graph[i].data(),0,n::layout.graphTable);Put(graph[i].data(),0x10,reinterpret_cast<uintptr_t>(rhi[i].data()));
        uint32_t width=static_cast<uint32_t>(desc.Width);memcpy(graph[i].data()+0x48,&width,4);memcpy(graph[i].data()+0x4c,&desc.Height,4);
        Put(rhi[i].data(),0,n::layout.rhiTable);Put(rhi[i].data(),0xb0,reinterpret_cast<uintptr_t>(location[i].data()));
        Put(location[i].data(),0x20,reinterpret_cast<uintptr_t>(resources[i].Get()));if(i<4)Put(parameters.data(),offsets[i],reinterpret_cast<uintptr_t>(graph[i].data()));
    }
    n::Frame frame;assert(n::ReadFrame(pass.data(),frame) && frame.resourcesValid);
    for(unsigned i=0;i<5;++i)assert(frame.resources[i].Get()==resources[i].Get());
    uint32_t special=0x20000;memcpy(rhi[1].data()+0x38,&special,4);n::Frame invalid;assert(!n::ReadFrame(pass.data(),invalid));
    special=0;memcpy(rhi[1].data()+0x38,&special,4);
    Queue queue;recordThread=GetCurrentThreadId();n::Pass(pass.data(),&queue);
    assert(passCalls==1 && n::tagged==1 && dispatch.table==reinterpret_cast<uintptr_t>(n::shadowPacketTable));
    frame={};invalid={};resources={}; // Only queued frame references now keep the native objects alive.
    auto execute=reinterpret_cast<n::PacketExecute>(*reinterpret_cast<void**>(dispatch.table));
    execute(&dispatch,&queue);assert(packetCalls==1 && n::executed==1 && !n::current && dispatch.table==n::layout.packetTable && n::pending.empty());
    auto test=std::make_shared<n::Frame>();test->groups=dispatch.groups;Queue duplicate;Packet a,b;a.table=b.table=n::layout.packetTable;
    auto oldTail=reinterpret_cast<uintptr_t>(duplicate.tail);Append(duplicate,a);Append(duplicate,b);
    assert(!n::Tag(&duplicate,oldTail,test) && n::pending.empty() && a.table==n::layout.packetTable && b.table==n::layout.packetTable);
    puts("PASS: real ID3D12Resource conversion and descriptions, five resource lifetimes retained, special path refused, exact newly recorded packet selected, original calls/thread/table preserved, duplicate dispatches refused.");
}
