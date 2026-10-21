#pragma once
// Bridge only a verified temporal pass's newly recorded dispatch packet.
#include "native_memory.h"
#include "native_temporal_signatures.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <string>
#include <vector>
#include <intrin.h>
namespace grounded::native_temporal {
using Microsoft::WRL::ComPtr;
using PassExecute=void(__fastcall*)(void*,void*);
using PacketExecute=void(__fastcall*)(void*,void*);
struct Layout {uintptr_t base=0,graphTable=0,rhiTable=0,packetTable=0;};
struct Frame {
    std::array<ComPtr<ID3D12Resource>,5> resources{}; // exposure,color,depth,velocity,output
    std::array<D3D12_RESOURCE_DESC,5> descriptions{};
    std::array<uintptr_t,5> graph{},rhi{},locations{},candidates{};
    std::array<UINT,3> groups{};
    unsigned sample=0;DWORD recordedThread=0;
    uintptr_t packet=0,command=0;
    ULONGLONG taggedTick=0;
    bool resourcesValid=false;
    const char* reason="";
    std::string detail;
};
using Allow=bool(*)(unsigned&);
using Notify=void(*)(const Frame&,void*,bool);
inline Layout layout{};
inline PassExecute originalPass=nullptr;
inline PacketExecute originalPacket=nullptr;
inline Allow allow=nullptr;
inline Notify notify=nullptr;
inline std::atomic<bool> installed{false};
inline void** passSlot=nullptr;
inline void* shadowPacketTable[1]{};
// Process-resident: abandoned engine packets must not release driver resources
// through a global destructor under the loader lock at process termination.
inline auto& pendingMutex=*new std::mutex;
inline auto& pending=*new std::unordered_map<void*,std::shared_ptr<Frame>>;
inline thread_local const Frame* current=nullptr;
inline std::atomic<uint64_t> tagged{0},executed{0},refused{0},stale{0};
inline uintptr_t Word(const unsigned char* memory,size_t offset){uintptr_t value=0;memcpy(&value,memory+offset,8);return value;}
inline bool DirectXAddress(uintptr_t address){
    HMODULE module=nullptr;if(!address || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(address),&module))return false;
    wchar_t path[32768]{};if(!GetModuleFileNameW(module,path,32768))return false;auto slash=wcsrchr(path,L'\\');auto name=slash?slash+1:path;
    return !_wcsicmp(name,L"d3d12.dll") || !_wcsicmp(name,L"D3D12Core.dll") || !_wcsicmp(name,L"nvwgf2umx.dll");
}
inline bool Acquire(uintptr_t graph,ComPtr<ID3D12Resource>& resource,Frame& frame,unsigned index){
    frame.graph[index]=graph;std::array<unsigned char,0x60> g{};std::array<unsigned char,0xc0> r{};
    if(!native_memory::Copy(g.data(),reinterpret_cast<void*>(graph),g.size()) || Word(g.data(),0)!=layout.graphTable){frame.reason="Graph texture table differs";return false;}
    auto rhi=Word(g.data(),0x10);frame.rhi[index]=rhi;
    if(!native_memory::Copy(r.data(),reinterpret_cast<void*>(rhi),r.size()) || Word(r.data(),0)!=layout.rhiTable){frame.reason="Native texture table differs";return false;}
    uint32_t flags=0;memcpy(&flags,r.data()+0x38,4);
    if(flags&0x20000){frame.reason="Special texture path not yet verified";return false;}
    auto location=Word(r.data(),0xb0);frame.locations[index]=location;uintptr_t candidate=0,table=0,query=0;
    if(!native_memory::Read(location+0x20,candidate) || !native_memory::Read(candidate,table) ||
       !native_memory::Read(table,query) || !DirectXAddress(table) || !DirectXAddress(query)){
        frame.reason="Native COM pointer not in a verified DirectX module";return false;
    }
    frame.candidates[index]=candidate;
    if(FAILED(reinterpret_cast<IUnknown*>(candidate)->QueryInterface(IID_PPV_ARGS(&resource)))){frame.reason="Native pointer is not ID3D12Resource";return false;}
    frame.descriptions[index]=resource->GetDesc();const auto& desc=frame.descriptions[index];
    const DXGI_FORMAT formats[]={DXGI_FORMAT_R32G32B32A32_FLOAT,DXGI_FORMAT_R16G16B16A16_FLOAT,
        DXGI_FORMAT_R32G8X24_TYPELESS,DXGI_FORMAT_R16G16B16A16_UNORM,DXGI_FORMAT_R16G16B16A16_FLOAT};
    uint32_t width=0,height=0;memcpy(&width,g.data()+0x48,4);memcpy(&height,g.data()+0x4c,4);
    if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D || desc.Width!=width || desc.Height!=height ||
       desc.Format!=formats[index] || desc.MipLevels!=1 || desc.DepthOrArraySize!=1 || desc.SampleDesc.Count!=1){
        frame.detail="resource="+std::to_string(index)+" graph="+std::to_string(width)+"x"+std::to_string(height)+
            " texture="+std::to_string(desc.Width)+"x"+std::to_string(desc.Height)+
            " format="+std::to_string(desc.Format)+" expected="+std::to_string(formats[index])+
            " mips="+std::to_string(desc.MipLevels)+" slices="+std::to_string(desc.DepthOrArraySize)+" samples="+std::to_string(desc.SampleDesc.Count);
        frame.reason="Native texture description differs from verified pass";return false;
    }
    return true;
}
inline bool ReadFrame(void* self,Frame& frame){
    std::array<unsigned char,0x118> pass{};std::array<unsigned char,0x2a0> parameters{};
    if(!native_memory::Copy(pass.data(),self,pass.size()) || Word(pass.data(),0)!=layout.base+0x4fc1f60 ||
       !native_memory::Copy(parameters.data(),reinterpret_cast<void*>(Word(pass.data(),0xf0)),parameters.size())){frame.reason="Temporal pass parameters unavailable";return false;}
    memcpy(frame.groups.data(),pass.data()+0x108,12);constexpr unsigned offsets[]={0x1a0,0x1b0,0x210,0x220};
    for(unsigned i=0;i<4;++i)if(!Acquire(Word(parameters.data(),offsets[i]),frame.resources[i],frame,i))return false;
    // 0x1d0 is the previous TemporalAA history. The graph builder puts the
    // first writable output view at 0x268. Its verified creator (0x2376e60)
    // returns allocation+8, with its parent graph texture at returned+0x28.
    std::array<unsigned char,0x38> outputView{};
    if(!native_memory::Copy(outputView.data(),reinterpret_cast<void*>(Word(parameters.data(),0x268)),outputView.size()) ||
       Word(outputView.data(),0)!=layout.base+0x4f5f5b0){frame.reason="Temporal output UAV view differs";return false;}
    // The builder initializes the two one-byte selectors, not the padding.
    uint16_t viewSlice=0;memcpy(&viewSlice,outputView.data()+0x30,2);
    if(viewSlice!=0){frame.reason="Temporal output UAV requires a different mip or plane";return false;}
    if(!Acquire(Word(outputView.data(),0x28),frame.resources[4],frame,4))return false;
    const auto& output=frame.descriptions[4];
    if(frame.descriptions[0].Width!=1 || frame.descriptions[0].Height!=1 || frame.groups[0]!=(output.Width+7)/8 ||
       frame.groups[1]!=(output.Height+7)/8 || frame.groups[2]!=1){frame.reason="Native dispatch dimensions differ";return false;}
    ComPtr<IUnknown> identity;
    for(auto& resource:frame.resources){ComPtr<ID3D12Device> device;ComPtr<IUnknown> next;
        if(FAILED(resource->GetDevice(IID_PPV_ARGS(&device))) || FAILED(device.As(&next)) || (identity && identity.Get()!=next.Get())){frame.reason="Native resources belong to different devices";return false;}
        identity=next;
    }
    frame.resourcesValid=true;return true;
}
inline void __fastcall Packet(void* self,void* command){
    std::shared_ptr<Frame> frame;
    {std::lock_guard<std::mutex> lock(pendingMutex);auto found=pending.find(self);if(found!=pending.end()){frame=std::move(found->second);pending.erase(found);}}
    // Restore before forwarding: any original virtual access sees the engine table.
    InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(self),reinterpret_cast<void*>(layout.packetTable),shadowPacketTable);
    auto previous=current;current=frame.get();
    if(frame && notify)notify(*frame,command,true);
    originalPacket(self,command);
    if(frame){++executed;if(notify)notify(*frame,command,false);}
    current=previous;
}
inline bool Tag(void* command,uintptr_t oldTail,const std::shared_ptr<Frame>& frame){
    uintptr_t first=0,endTail=0;
    if(!native_memory::Read(oldTail,first) || !native_memory::Read(reinterpret_cast<uintptr_t>(command)+8,endTail))return false;
    uintptr_t selected=0,cursor=first;unsigned matches=0,count=0;
    while(cursor && count++<256){
        std::array<uintptr_t,2> packet{};if(!native_memory::Copy(packet.data(),reinterpret_cast<void*>(cursor),sizeof(packet)))return false;
        if(packet[0]==layout.packetTable){std::array<UINT,3> groups{};if(!native_memory::Copy(groups.data(),reinterpret_cast<void*>(cursor+0x10),12))return false;
            if(groups==frame->groups){selected=cursor;++matches;}}
        if(cursor+8==endTail)break;cursor=packet[1];
    }
    if(!cursor || cursor+8!=endTail || matches!=1)return false;
    frame->packet=selected;frame->taggedTick=GetTickCount64();
    // Recorded packets normally execute within the same frame. One that never
    // ran (culled or discarded work) must not occupy the table forever, and a
    // new packet at a reused address replaces its dead predecessor. A purged
    // packet that still runs finds no frame and simply executes the original.
    std::vector<std::shared_ptr<Frame>> retired;
    {std::lock_guard<std::mutex> lock(pendingMutex);
        for(auto it=pending.begin();it!=pending.end();){
            if(it->first==reinterpret_cast<void*>(selected) || frame->taggedTick-it->second->taggedTick>2000){retired.push_back(std::move(it->second));it=pending.erase(it);}
            else ++it;
        }
        if(pending.size()>=64)return false;
        pending.emplace(reinterpret_cast<void*>(selected),frame);
    }
    stale.fetch_add(retired.size(),std::memory_order_relaxed);
    retired.clear(); // Release retained resources outside the table lock.
    auto prior=InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(selected),shadowPacketTable,reinterpret_cast<void*>(layout.packetTable));
    if(prior!=reinterpret_cast<void*>(layout.packetTable)){std::lock_guard<std::mutex> lock(pendingMutex);pending.erase(reinterpret_cast<void*>(selected));return false;}
    ++tagged;return true;
}
inline void __fastcall Pass(void* self,void* command){
    unsigned sample=0;
    if(allow && !allow(sample)){originalPass(self,command);return;}
    auto frame=std::make_shared<Frame>();frame->sample=sample;frame->recordedThread=GetCurrentThreadId();frame->command=reinterpret_cast<uintptr_t>(command);
    ReadFrame(self,*frame);uintptr_t tail=0;native_memory::Read(frame->command+8,tail);
    originalPass(self,command);
    if(!tail || !Tag(command,tail,frame)){++refused;if(notify)notify(*frame,nullptr,false);}
}
template<size_t N>inline bool Matches(uintptr_t address,const std::array<unsigned char,N>& signature){std::array<unsigned char,N> value{};return native_memory::Copy(value.data(),reinterpret_cast<void*>(address),N)&&value==signature;}
inline bool Install(Allow permission,Notify callback){
    if(installed)return true;auto module=GetModuleHandleW(nullptr);wchar_t path[32768]{};GetModuleFileNameW(module,path,32768);auto slash=wcsrchr(path,L'\\');
    if(_wcsicmp(slash?slash+1:path,L"Maine-Win64-Shipping.exe"))return false;
    auto base=reinterpret_cast<uintptr_t>(module);IMAGE_DOS_HEADER dos{};IMAGE_NT_HEADERS64 nt{};
    if(!native_memory::Read(base,dos) || dos.e_magic!=IMAGE_DOS_SIGNATURE || dos.e_lfanew<0 || dos.e_lfanew>4096 || !native_memory::Read(base+dos.e_lfanew,nt) ||
       nt.Signature!=IMAGE_NT_SIGNATURE || nt.FileHeader.TimeDateStamp!=1745511144 || nt.FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64 || nt.OptionalHeader.SizeOfImage!=111910912)return false;
    uintptr_t passMethod=0,packetMethod=0;
    if(!native_memory::Read(base+0x4fc1f68,passMethod) || passMethod!=base+0x269f400 || !native_memory::Read(base+0x4b02108,packetMethod) || packetMethod!=base+0x15020a0 ||
       !Matches(passMethod,PassSignature) || !Matches(base+0x2331d60,TextureSignature) || !Matches(packetMethod,PacketThunkSignature) || !Matches(base+0x2f867f0,PacketSignature))return false;
    HMODULE pinned=nullptr;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&Pass),&pinned))return false;
    layout={base,base+0x5091960,base+0x4f4ee80,base+0x4b02108};originalPass=reinterpret_cast<PassExecute>(passMethod);originalPacket=reinterpret_cast<PacketExecute>(packetMethod);
    allow=permission;notify=callback;shadowPacketTable[0]=reinterpret_cast<void*>(&Packet);passSlot=reinterpret_cast<void**>(base+0x4fc1f68);
    DWORD protection=0,unused=0;if(!VirtualProtect(passSlot,8,PAGE_READWRITE,&protection))return false;
    auto previous=InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(passSlot),reinterpret_cast<void*>(&Pass),reinterpret_cast<void*>(passMethod));
    bool restored=VirtualProtect(passSlot,8,protection,&unused)!=0;
    if(previous!=reinterpret_cast<void*>(passMethod))return false;
    if(!restored){InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(passSlot),reinterpret_cast<void*>(passMethod),reinterpret_cast<void*>(&Pass));return false;}
    installed=true;return true;
}
inline bool Remove(){
    if(!installed)return false;DWORD protection=0,unused=0;if(!VirtualProtect(passSlot,8,PAGE_READWRITE,&protection))return false;
    auto previous=InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(passSlot),reinterpret_cast<void*>(originalPass),reinterpret_cast<void*>(&Pass));
    bool restored=VirtualProtect(passSlot,8,protection,&unused)!=0;installed=false;
    return previous==reinterpret_cast<void*>(&Pass)&&restored;
}
}
