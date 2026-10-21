// Grounded shader-specific DLSS replacement. The diagnostic probe is not loaded.
#include "integration.h"
#include "native_ngx.h"
#include "local_dlss.h"
#include "scale_control.h"
#include "viewport_scale.h"
#include "sharpness_control.h"
#include "focus_control.h"
#include "native_temporal_bridge.h"
#include "../render_probe/pipeline_stream.h"
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <array>
#include <algorithm>
#include <atomic>
#include <fstream>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdarg>
#include <tlhelp32.h>
#include <intrin.h>
#ifdef GROUNDED_FRAME_CAPTURE
#include "frame_capture.h"
#endif

namespace {
using Microsoft::WRL::ComPtr;
HMODULE gModule=nullptr;
thread_local bool internal=false;
struct Internal { bool old=internal; Internal(){internal=true;} ~Internal(){internal=old;} };
// Set at DLL_PROCESS_DETACH, which only happens at process exit because the
// module is pinned. Other threads are gone by then and some may have died
// holding our locks, so from then on hooks forward without locks or caches.
std::atomic<bool> terminating{false};
bool Bypass(){return internal || terminating.load(std::memory_order_relaxed);}
// Hooked vtables keep calling into this module until the process is gone, so
// everything they touch is process-resident. With the static CRT, ordinary
// globals are destroyed at DLL_PROCESS_DETACH while late D3D12 Release calls
// from other modules can still arrive; leaked objects stay valid until exit.
auto& hookMutex=*new std::mutex;
auto& runtimeMutex=*new std::mutex;
auto& logMutex=*new std::mutex;
auto& metadataMutex=*new std::shared_mutex;
auto& commandsMutex=*new std::shared_mutex;
auto& originals=*new std::map<std::pair<void**,UINT>,void*>;
std::atomic<int> ready{0};
std::atomic<bool> enabled{true}, evaluated{false}, failed{false};
#ifdef GROUNDED_FRAME_CAPTURE
grounded::frame_capture::Capture* frameCapture=new grounded::frame_capture::Capture;
std::atomic<bool> captureArmed{false};
std::atomic<ID3D12CommandList*> capturePendingList{nullptr};
void CaptureFrame(ID3D12GraphicsCommandList* list,const grounded::PassInput& input,bool dlss) {
    if(!captureArmed.load(std::memory_order_acquire))return;
    if(frameCapture->Record(list,input,dlss))capturePendingList.store(list,std::memory_order_release);
    captureArmed.store(false,std::memory_order_release);
}
#endif
std::atomic<UINT> renderPresetRequested{0};
std::atomic<UINT> gpuArchitecture{0};
std::atomic<UINT> qualityMode{2};
std::atomic<UINT64> featureGenerations{0};
std::atomic<bool> historyDirty{true};
std::atomic<UINT64> successes{0}, fallbacks{0}, candidates{0}, commandEpoch{0}, pipelineEpoch{0};
// 0=descriptor fallback, 1=compare native resources against descriptors, 2=native only.
std::atomic<UINT> nativeBridgeMode{0};
std::atomic<UINT64> nativePasses{0};
std::atomic<UINT64> nativeScopes{0},nativeCompared{0},nativeMismatches{0},nativeMissing{0},nativeRejected{0},nativeUnclassified{0};
bool AllowNativePass(unsigned& sample) {
    ++nativePasses;
    if(ready!=1)return false;
    if(!enabled.load(std::memory_order_relaxed)
#ifdef GROUNDED_FRAME_CAPTURE
       && !captureArmed.load(std::memory_order_acquire)
#endif
    )return false;
    sample=unsigned(nativeScopes.fetch_add(1,std::memory_order_relaxed));return true;
}
auto& directory=*new std::filesystem::path;
// Hooks and GPU state are process-resident. Never run NGX/driver teardown from
// a C++ global destructor under the Windows loader lock at process termination.
grounded::Integration* integration=nullptr;
auto& lastError=*new std::string;
std::atomic<UINT> activeWidth{0}, activeHeight{0}, outputWidth{0}, outputHeight{0};
DWORD ownerThread=0;
ULONGLONG lastEvaluation=0;
std::atomic<bool> profiling{false};
// Diagnostic comparison only; startup always uses the full thread-local index.
std::atomic<bool> legacyHeapCache{false};
std::atomic<bool> treeHeapCache{false}; // Diagnostic A/B against the v11.12 index.
struct ProfileCounter {std::atomic<UINT64> calls{0},ticks{0},samples{0};};
std::array<ProfileCounter,15> profileCounters;
struct Profile {
    struct Local {UINT64 calls=0,ticks=0,samples=0;};
    static Local& Counter(UINT group){thread_local std::array<Local,15> local{};return local[group];}
    UINT index;LARGE_INTEGER begin{};bool active=false;
    explicit Profile(UINT group):index(group) {
        if(!profiling.load(std::memory_order_relaxed))return;
        active=true;
        auto& p=Counter(index);
        const UINT stride=index<6 || index==8 || index==9 || index>=12?1024:index==7?1:64;
        if((++p.calls&(stride-1))==0)QueryPerformanceCounter(&begin);
    }
    ~Profile(){
        if(!active)return;
        auto& local=Counter(index);
        if(begin.QuadPart){LARGE_INTEGER end;QueryPerformanceCounter(&end);local.ticks+=end.QuadPart-begin.QuadPart;++local.samples;}
        const UINT stride=index<6 || index==8 || index==9 || index>=12?1024:index==7?1:64;
        if(local.calls>=stride){auto& p=profileCounters[index];
            p.calls.fetch_add(local.calls,std::memory_order_relaxed);p.ticks.fetch_add(local.ticks,std::memory_order_relaxed);
            p.samples.fetch_add(local.samples,std::memory_order_relaxed);local={};}
    }
};

void Log(const char* format,...) {
    std::lock_guard<std::mutex> lock(logMutex);
    // Bound one session's output; startup rotation bounds the file across sessions.
    static UINT64 lines=0;constexpr UINT64 maximum=20000;
    if(lines>maximum)return;
    FILE* f=nullptr; _wfopen_s(&f,(directory/L"GroundedDLSS-runtime.log").c_str(),L"a");
    if (!f) return;
    fprintf(f,"[%llu] ",GetTickCount64());
    if(++lines>maximum){fputs("Log limit reached for this session; later messages are omitted\n",f);fclose(f);return;}
    va_list args; va_start(args,format); vfprintf(f,format,args); va_end(args);
    fputc('\n',f); fclose(f);
}
void TryGameScalePresets() {
    if(!grounded::scale_control::TryInstall())return;
    const auto result=grounded::scale_control::state.load();
    if(result==1)Log("Game scale presets installed: verified Grounded console variables; %ls/50/58/67/100; before viewport allocation",grounded::ScalePresetText(33));
    else if(result==-1)Log("Game scale presets refused: %s",grounded::scale_control::refusal);
}
void TryGameSharpness() {
    if(!grounded::sharpness_control::TryInstall())return;
    const auto result=grounded::sharpness_control::state.load();
    if(result==1)Log("Per-mode sharpening bound: verified Unreal tonemapper typed cache; original CVar retained");
    else if(result==-1)Log("Per-mode sharpening refused: %s",grounded::sharpness_control::refusal);
}
template<class T,UINT Slot> T Original(IUnknown* object) {
    if(terminating.load(std::memory_order_acquire)) {
        // Exit path: no other thread runs, and thread-local caches may already
        // be destroyed. The leaked map stays valid; read it without the lock.
        auto found=originals.find({*reinterpret_cast<void***>(object),Slot});
        return found==originals.end()?nullptr:reinterpret_cast<T>(found->second);
    }
    Profile profile(0);
    // Vtables are immutable after their one-time hook. No global mutex on repeat calls.
    thread_local std::unordered_map<void**,T> cache;
    thread_local void** recentTable=nullptr;
    thread_local T recentMethod=nullptr;
    auto table=*reinterpret_cast<void***>(object);
    if(table==recentTable && recentMethod)return recentMethod;
    auto hit=cache.find(table); if(hit!=cache.end()) {recentTable=table;recentMethod=hit->second;return recentMethod;}
    std::lock_guard<std::mutex> lock(hookMutex);
    auto found=originals.find({table,Slot});
    auto method=found==originals.end()?nullptr:reinterpret_cast<T>(found->second);
    if(method) {cache.emplace(table,method);recentTable=table;recentMethod=method;} return method;
}
bool Hook(IUnknown* object,UINT slot,void* method) {
    std::lock_guard<std::mutex> lock(hookMutex);
    auto table=*reinterpret_cast<void***>(object); auto key=std::make_pair(table,slot);
    if(originals.count(key)) return true;
    DWORD protection=0;
    if(!VirtualProtect(table+slot,sizeof(void*),PAGE_READWRITE,&protection)) return false;
    originals.emplace(key,table[slot]); InterlockedExchangePointer(reinterpret_cast<void* volatile*>(table+slot),method);
    DWORD unused=0; VirtualProtect(table+slot,sizeof(void*),protection,&unused); return true;
}

struct ResourceRef {ID3D12Resource* pointer=nullptr;UINT64 generation=0;};
struct Resource : ResourceRef {D3D12_RESOURCE_DESC desc{};};
// Views are immutable and live until their resource retires. Heap slots contain
// only atomic IDs; copies do not lock shared source heaps or copy full views.
struct Descriptor {
    ResourceRef resource;D3D12_TEX2D_SRV texture2D{};UINT mapping=0;
    DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN;bool uav=false;
    D3D12_SHADER_RESOURCE_VIEW_DESC View()const {
        D3D12_SHADER_RESOURCE_VIEW_DESC view{};view.Format=format;view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
        view.Shader4ComponentMapping=mapping;view.Texture2D=texture2D;return view;
    }
};
static_assert(std::is_trivially_copyable_v<Descriptor> && sizeof(Descriptor)<=64,"Descriptor metadata must remain compact");
struct DescriptorStorage {
    explicit DescriptorStorage(UINT count):entries(count){for(auto& id:entries)id.store(0,std::memory_order_relaxed);}
    std::atomic<bool> valid{true};
    std::vector<std::atomic<UINT64>> entries;
};
static_assert(std::atomic<UINT64>::is_always_lock_free,"Descriptor IDs require lock-free 64-bit atomics");
struct ResourceRecord : Resource {std::vector<UINT64> views;};
struct Heap { ID3D12DescriptorHeap* pointer=nullptr; UINT64 cpu=0,gpu=0; UINT count=0,step=0; D3D12_DESCRIPTOR_HEAP_TYPE type{}; std::shared_ptr<DescriptorStorage> storage; };
struct Mapping { ID3D12Resource* resource=nullptr; const unsigned char* cpu=nullptr; UINT64 size=0; UINT depth=0; };
auto& resources=*new std::unordered_map<ID3D12Resource*,ResourceRecord>;
auto& descriptorViews=*new std::unordered_map<UINT64,Descriptor>;
auto& heaps=*new std::unordered_map<ID3D12DescriptorHeap*,Heap>;
auto& cpuHeaps=*new std::map<UINT64,Heap>;
auto& mappings=*new std::map<UINT64,Mapping>;
auto& targets=*new std::unordered_map<ID3D12PipelineState*,grounded::PassVariant>;
auto& roots=*new std::unordered_set<ID3D12RootSignature*>;
UINT64 nextResource=0,nextDescriptor=0;

struct Argument {
    enum Type { Unset,Table,CBV,SRV,UAV,Constants } type=Unset;
    UINT64 generation=0,tableGeneration=0;
    UINT64 value=0,mask=0;
    std::array<UINT,64> constants{};
};
struct RootState {
    ID3D12RootSignature* root=nullptr; std::array<Argument,64> args{};
    UINT64 generation=1,tableGeneration=1;
    void Invalidate(ID3D12RootSignature* signature=nullptr) {
        root=signature;
        // Changing a signature invalidates bindings without touching 64 large
        // argument records on every root-signature switch.
        if(!++generation){for(auto& a:args)a.generation=0;generation=1;}
    }
    void InvalidateTables() {
        // A heap change invalidates descriptor tables, but preserves root
        // descriptors and constants as required by D3D12.
        if(!++tableGeneration){for(auto& a:args)a.tableGeneration=0;tableGeneration=1;}
    }
    bool Valid(UINT index)const {
        const auto& a=args[index];
        return a.generation==generation && a.type!=Argument::Unset &&
            (a.type!=Argument::Table || a.tableGeneration==tableGeneration);
    }
    Argument& Bind(UINT index,Argument::Type type) {
        auto& a=args[index];
        if(a.generation!=generation || a.type!=type){a.type=type;a.mask=0;}
        a.generation=generation;
        if(type==Argument::Table)a.tableGeneration=tableGeneration;
        return a;
    }
};
struct Command {
    // Remains set after submission, including legal re-execution of the list.
    // ClearState changes bindings but does not discard recorded GPU commands.
    std::atomic<bool> dlssRecorded{false};
    ID3D12PipelineState* pso=nullptr;
    RootState compute,graphics;
    std::array<ID3D12DescriptorHeap*,2> heaps{};
    UINT heapCount=0;
    grounded::PassVariant target=grounded::PassVariant::Unknown;
    bool direct=true;
    void Reset(ID3D12PipelineState* pipeline, bool isDirect) {
        pso=pipeline;compute.Invalidate();graphics.Invalidate();
        heapCount=0;heaps={};target=grounded::PassVariant::Unknown;direct=isDirect;
    }
};
auto& commands=*new std::unordered_map<ID3D12GraphicsCommandList*,std::shared_ptr<Command>>;
Command& State(ID3D12GraphicsCommandList* list) {
    Profile profile(1);
    thread_local ID3D12GraphicsCommandList* cachedList=nullptr;
    thread_local std::shared_ptr<Command> cached;
    thread_local std::unordered_map<ID3D12GraphicsCommandList*,std::shared_ptr<Command>> cache;
    thread_local UINT64 epoch=~UINT64(0);
    auto now=commandEpoch.load(std::memory_order_acquire);
    if(cachedList==list && epoch==now && cached) return *cached;
    if(epoch!=now){cache.clear();epoch=now;}
    auto hit=cache.find(list);
    if(hit!=cache.end()){cachedList=list;cached=hit->second;return *cached;}
    Profile miss(9);
    std::unique_lock<std::shared_mutex> lock(commandsMutex);
    auto& state=commands[list]; if(!state) {state=std::make_shared<Command>();state->direct=list->GetType()==D3D12_COMMAND_LIST_TYPE_DIRECT;}
    if(cache.size()>=256)cache.clear();
    cache.emplace(list,state);cached=state; cachedList=list; return *cached;
}
grounded::PassVariant IsTarget(ID3D12PipelineState* pso) {
    thread_local UINT64 epoch=~UINT64(0);
    thread_local std::unordered_map<ID3D12PipelineState*,grounded::PassVariant> cache;
    auto now=pipelineEpoch.load(std::memory_order_acquire);
    if(epoch!=now) { cache.clear(); epoch=now; }
    auto hit=cache.find(pso); if(hit!=cache.end()) return hit->second;
    std::shared_lock<std::shared_mutex> lock(metadataMutex);
    auto found=targets.find(pso);
    auto value=found==targets.end()?grounded::PassVariant::Unknown:found->second;
    cache[pso]=value; return value;
}
using Release=ULONG(STDMETHODCALLTYPE*)(IUnknown*);
ULONG STDMETHODCALLTYPE ResourceRelease(IUnknown* object) {
    Profile profile(2);
    auto original=Original<Release,2>(object);
    auto resource=static_cast<ID3D12Resource*>(object);
    ULONG count=original(object);
    if(!count && !terminating.load(std::memory_order_relaxed)) {std::unique_lock<std::shared_mutex> lock(metadataMutex);
        auto record=resources.find(resource);
        if(record!=resources.end()){for(auto id:record->second.views)descriptorViews.erase(id);resources.erase(record);}
        for(auto it=mappings.begin();it!=mappings.end();) {
        if(it->second.resource==resource) it=mappings.erase(it); else ++it;
    }}
    return count;
}
using Map=HRESULT(STDMETHODCALLTYPE*)(ID3D12Resource*,UINT,const D3D12_RANGE*,void**);
HRESULT STDMETHODCALLTYPE ResourceMap(ID3D12Resource* r,UINT sub,const D3D12_RANGE* range,void** data) {
    auto original=Original<Map,8>(r); HRESULT hr=original(r,sub,range,data);
    if(!Bypass() && SUCCEEDED(hr) && sub==0 && data && *data) {
        auto d=r->GetDesc(); D3D12_HEAP_PROPERTIES hp{}; D3D12_HEAP_FLAGS flags{};
        if(d.Dimension==D3D12_RESOURCE_DIMENSION_BUFFER && SUCCEEDED(r->GetHeapProperties(&hp,&flags)) && hp.Type==D3D12_HEAP_TYPE_UPLOAD) {
            auto gpu=r->GetGPUVirtualAddress(); std::unique_lock<std::shared_mutex> lock(metadataMutex);
            auto& m=mappings[gpu]; m.resource=r; m.cpu=static_cast<const unsigned char*>(*data); m.size=d.Width; ++m.depth;
        }
    }
    return hr;
}
using Unmap=void(STDMETHODCALLTYPE*)(ID3D12Resource*,UINT,const D3D12_RANGE*);
void STDMETHODCALLTYPE ResourceUnmap(ID3D12Resource* r,UINT sub,const D3D12_RANGE* range) {
    auto original=Original<Unmap,9>(r);
    if(!Bypass() && sub==0) {
        std::unique_lock<std::shared_mutex> lock(metadataMutex);
        for(auto it=mappings.begin();it!=mappings.end();++it) if(it->second.resource==r) {
            if(--it->second.depth==0) mappings.erase(it); break;
        }
    }
    original(r,sub,range);
}
bool HookResource(ID3D12Resource* r) {
    return Hook(r,2,reinterpret_cast<void*>(ResourceRelease)) && Hook(r,8,reinterpret_cast<void*>(ResourceMap)) && Hook(r,9,reinterpret_cast<void*>(ResourceUnmap));
}
Resource ObserveResource(ID3D12Resource* r) {
    if(!r) return {};
    {std::shared_lock<std::shared_mutex> lock(metadataMutex); auto hit=resources.find(r); if(hit!=resources.end()) return hit->second;}
    auto d=r->GetDesc(); HookResource(r);
    std::unique_lock<std::shared_mutex> lock(metadataMutex);
    auto hit=resources.find(r); if(hit!=resources.end()) return hit->second;
    Resource record{{r,++nextResource},d}; resources.emplace(r,ResourceRecord{record,{}}); return record;
}
ULONG STDMETHODCALLTYPE HeapRelease(IUnknown* object) {
    Profile profile(3);
    auto original=Original<Release,2>(object);
    auto heap=static_cast<ID3D12DescriptorHeap*>(object);
    ULONG count=original(object);
    if(!count && !terminating.load(std::memory_order_relaxed)) {std::unique_lock<std::shared_mutex> lock(metadataMutex);auto hit=heaps.find(heap); if(hit!=heaps.end()) {
        const auto h=hit->second;
        if(h.storage)h.storage->valid=false;
        cpuHeaps.erase(h.cpu);
        heaps.erase(hit);
    }} return count;
}
Heap ObserveHeap(ID3D12DescriptorHeap* heap) {
    if(!heap) return {};
    {std::shared_lock<std::shared_mutex> lock(metadataMutex); auto hit=heaps.find(heap); if(hit!=heaps.end()) return hit->second;}
    Hook(heap,2,reinterpret_cast<void*>(HeapRelease)); auto d=heap->GetDesc();
    Heap h; h.pointer=heap; h.cpu=heap->GetCPUDescriptorHandleForHeapStart().ptr; h.count=d.NumDescriptors; h.type=d.Type;
    if(d.Type==D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)h.storage=std::make_shared<DescriptorStorage>(d.NumDescriptors);
    if(d.Flags&D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) h.gpu=heap->GetGPUDescriptorHandleForHeapStart().ptr;
    ComPtr<ID3D12Device> device; if(SUCCEEDED(heap->GetDevice(IID_PPV_ARGS(&device)))) h.step=device->GetDescriptorHandleIncrementSize(d.Type);
    std::unique_lock<std::shared_mutex> lock(metadataMutex);
    auto hit=heaps.find(heap);if(hit!=heaps.end())return hit->second;
    heaps[heap]=h;if(h.storage)cpuHeaps[h.cpu]=h;return h;
}
const Heap& LegacyDescriptorHeap(UINT64 handle,const Heap* pinned) {
    // Descriptor addresses stay within a heap. Cache its range rather than
    // hashing and locking one global metadata map for every copied descriptor.
    thread_local std::array<Heap,4> cache;
    thread_local UINT next=0;
    static const Heap empty;
    auto contains=[&](const Heap& h){return h.storage && h.storage->valid.load(std::memory_order_acquire) &&
        h.step && handle>=h.cpu && handle-h.cpu<UINT64(h.count)*h.step && (handle-h.cpu)%h.step==0;};
    for(const auto& h:cache)if(contains(h))return h;
    Profile miss(8);
    std::shared_lock<std::shared_mutex> lock(metadataMutex);
    auto found=cpuHeaps.upper_bound(handle);if(found==cpuHeaps.begin())return empty;--found;
    if(!contains(found->second))return empty;
    auto slot=next++%cache.size();
    // A copy borrows the destination before looking up its source. Keep that
    // owning cache slot alive if the second lookup needs to replace an entry.
    // No driver callbacks or further lookups occur while these references live.
    if(&cache[slot]==pinned)slot=next++%cache.size();
    cache[slot]=found->second;return cache[slot];
}
const Heap& TreeDescriptorHeap(UINT64 handle,const Heap* pinned) {
    thread_local std::map<UINT64,Heap> cache;
    thread_local std::array<const Heap*,2> recent{};
    thread_local UINT next=0;
    static const Heap empty;
    const auto contains=[&](const Heap& h){return h.storage && h.storage->valid.load(std::memory_order_acquire) &&
        h.step && handle>=h.cpu && handle-h.cpu<UINT64(h.count)*h.step && (handle-h.cpu)%h.step==0;};
    for(auto h:recent)if(h && contains(*h))return *h;
    auto local=cache.upper_bound(handle);
    if(local!=cache.begin()){--local;if(contains(local->second)){
        recent[next++%recent.size()]=&local->second;return local->second;
    }}
    Profile miss(8);
    std::shared_lock<std::shared_mutex> lock(metadataMutex);
    auto found=cpuHeaps.upper_bound(handle);if(found==cpuHeaps.begin())return empty;--found;
    if(!contains(found->second))return empty;
    recent={};
    auto replaced=cache.find(found->first);
    if(replaced!=cache.end()){
        if(&replaced->second==pinned)return empty;
        cache.erase(replaced);
    }
    if(cache.size()>=256){
        for(auto it=cache.begin();it!=cache.end();) {
            if(&it->second!=pinned && !it->second.storage->valid.load(std::memory_order_acquire))it=cache.erase(it);else ++it;
        }
        if(cache.size()>=256){auto victim=cache.begin();if(&victim->second==pinned)++victim;cache.erase(victim);}
    }
    auto inserted=cache.emplace(found->first,found->second).first;
    recent[next++%recent.size()]=&inserted->second;return inserted->second;
}
const Heap& DescriptorHeap(UINT64 handle,const Heap* pinned=nullptr) {
    if(legacyHeapCache.load(std::memory_order_relaxed))return LegacyDescriptorHeap(handle,pinned);
    if(treeHeapCache.load(std::memory_order_relaxed))return TreeDescriptorHeap(handle,pinned);
    // Keep search keys contiguous. Heap objects remain at stable addresses so
    // a borrowed destination survives vector insertions and source lookups.
    using CacheEntry=std::pair<UINT64,std::unique_ptr<Heap>>;
    thread_local auto cache=[](){std::vector<CacheEntry> entries;entries.reserve(256);return entries;}();
    thread_local std::array<const Heap*,2> recent{};
    thread_local UINT next=0;
    static const Heap empty;
    const auto contains=[&](const Heap& h){return h.storage && h.storage->valid.load(std::memory_order_acquire) &&
        h.step && handle>=h.cpu && handle-h.cpu<UINT64(h.count)*h.step && (handle-h.cpu)%h.step==0;};
    for(auto h:recent)if(h && contains(*h))return *h;
    auto local=std::upper_bound(cache.begin(),cache.end(),handle,
        [](UINT64 value,const CacheEntry& entry){return value<entry.first;});
    if(local!=cache.begin()){--local;if(contains(*local->second)){
        recent[next++%recent.size()]=local->second.get();return *local->second;
    }}
    Profile miss(8);
    std::shared_lock<std::shared_mutex> lock(metadataMutex);
    auto found=cpuHeaps.upper_bound(handle);if(found==cpuHeaps.begin())return empty;--found;
    if(!contains(found->second))return empty;
    // Bound retained retired heap storage. Owned heaps remain stable on insert;
    // neither pruning nor address reuse may erase a borrowed destination.
    recent={};
    auto replaced=std::lower_bound(cache.begin(),cache.end(),found->first,
        [](const CacheEntry& entry,UINT64 value){return entry.first<value;});
    if(replaced!=cache.end() && replaced->first==found->first){
        if(replaced->second.get()==pinned)return empty;
        cache.erase(replaced);
    }
    if(cache.size()>=256){
        for(auto it=cache.begin();it!=cache.end();) {
            if(it->second.get()!=pinned && !it->second->storage->valid.load(std::memory_order_acquire))it=cache.erase(it);else ++it;
        }
        if(cache.size()>=256){auto victim=cache.begin();if(victim->second.get()==pinned)++victim;cache.erase(victim);}
    }
    auto position=std::lower_bound(cache.begin(),cache.end(),found->first,
        [](const CacheEntry& entry,UINT64 value){return entry.first<value;});
    auto inserted=cache.emplace(position,found->first,std::make_unique<Heap>(found->second));
    recent[next++%recent.size()]=inserted->second.get();return *inserted->second;
}
void StoreDescriptor(UINT64 handle,const Descriptor& value) {
    const auto& h=DescriptorHeap(handle);if(!h.storage)return;
    auto index=SIZE_T((handle-h.cpu)/h.step);
    UINT64 id=0;
    if(value.resource.pointer){
        std::unique_lock<std::shared_mutex> lock(metadataMutex);
        auto resource=resources.find(value.resource.pointer);
        if(resource!=resources.end() && resource->second.generation==value.resource.generation){
            // Repeated creation of the same view reuses its record. Records are
            // reclaimed on ResourceRelease; IDs never recycle into stale slots.
            const auto equal=[&](const Descriptor& other){return other.format==value.format && other.uav==value.uav &&
                other.mapping==value.mapping && other.texture2D.MostDetailedMip==value.texture2D.MostDetailedMip &&
                other.texture2D.MipLevels==value.texture2D.MipLevels && other.texture2D.PlaneSlice==value.texture2D.PlaneSlice &&
                other.texture2D.ResourceMinLODClamp==value.texture2D.ResourceMinLODClamp;};
            for(auto candidate:resource->second.views)if(equal(descriptorViews.at(candidate))){id=candidate;break;}
            if(!id){id=++nextDescriptor;descriptorViews.emplace(id,value);resource->second.views.push_back(id);}
        }
    }
    if(h.storage->valid)h.storage->entries[index].store(id,std::memory_order_relaxed);
}
ULONG STDMETHODCALLTYPE PipelineRelease(IUnknown* object) {
    auto original=Original<Release,2>(object);
    ULONG count=original(object);
    if(!count && !terminating.load(std::memory_order_relaxed)) {std::unique_lock<std::shared_mutex> lock(metadataMutex);if(targets.erase(static_cast<ID3D12PipelineState*>(object)))++pipelineEpoch;}return count;
}
ULONG STDMETHODCALLTYPE RootRelease(IUnknown* object) {
    auto original=Original<Release,2>(object);
    ULONG count=original(object);
    if(!count && !terminating.load(std::memory_order_relaxed)){std::unique_lock<std::shared_mutex> lock(metadataMutex);roots.erase(static_cast<ID3D12RootSignature*>(object));}return count;
}
void ObservePipeline(void* object,const D3D12_SHADER_BYTECODE& code) {
    if(!object || !code.pShaderBytecode || (code.BytecodeLength!=10292 && code.BytecodeLength!=10864)) return;
    UINT64 hash=14695981039346656037ull; auto bytes=static_cast<const unsigned char*>(code.pShaderBytecode);
    for(size_t i=0;i<code.BytecodeLength;++i) hash=(hash^bytes[i])*1099511628211ull;
    const auto variant=grounded::ClassifyShader(hash,code.BytecodeLength);
    if(variant==grounded::PassVariant::Unknown) return;
    ComPtr<ID3D12PipelineState> pso;
    if(FAILED(static_cast<IUnknown*>(object)->QueryInterface(IID_PPV_ARGS(&pso)))) return;
    Hook(pso.Get(),2,reinterpret_cast<void*>(PipelineRelease));
    {std::unique_lock<std::shared_mutex> lock(metadataMutex); targets[pso.Get()]=variant; ++pipelineEpoch;}
    Log("Verified Grounded %s shader %016llx PSO=%p",variant==grounded::PassVariant::Native100?"native 100% TAA":"TAAU",hash,pso.Get());
}
using Compute=HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*,const D3D12_COMPUTE_PIPELINE_STATE_DESC*,REFIID,void**);
HRESULT STDMETHODCALLTYPE CreateCompute(ID3D12Device* d,const D3D12_COMPUTE_PIPELINE_STATE_DESC* desc,REFIID iid,void** out) {
    HRESULT hr=Original<Compute,11>(d)(d,desc,iid,out);
    if(!Bypass() && SUCCEEDED(hr) && out && desc) ObservePipeline(*out,desc->CS); return hr;
}
using Stream=HRESULT(STDMETHODCALLTYPE*)(ID3D12Device2*,const D3D12_PIPELINE_STATE_STREAM_DESC*,REFIID,void**);
HRESULT STDMETHODCALLTYPE CreateStream(ID3D12Device2* d,const D3D12_PIPELINE_STATE_STREAM_DESC* desc,REFIID iid,void** out) {
    HRESULT hr=Original<Stream,47>(d)(d,desc,iid,out); D3D12_SHADER_BYTECODE code{};
    if(!Bypass() && SUCCEEDED(hr) && out && ProbeComputeBytecode(desc,code)) ObservePipeline(*out,code); return hr;
}
using LoadCompute=HRESULT(STDMETHODCALLTYPE*)(ID3D12PipelineLibrary*,LPCWSTR,const D3D12_COMPUTE_PIPELINE_STATE_DESC*,REFIID,void**);
HRESULT STDMETHODCALLTYPE LibraryCompute(ID3D12PipelineLibrary* l,LPCWSTR name,const D3D12_COMPUTE_PIPELINE_STATE_DESC* desc,REFIID iid,void** out) {
    HRESULT hr=Original<LoadCompute,10>(l)(l,name,desc,iid,out);
    if(!Bypass() && SUCCEEDED(hr) && out && desc) ObservePipeline(*out,desc->CS); return hr;
}
using LoadStream=HRESULT(STDMETHODCALLTYPE*)(ID3D12PipelineLibrary1*,LPCWSTR,const D3D12_PIPELINE_STATE_STREAM_DESC*,REFIID,void**);
HRESULT STDMETHODCALLTYPE LibraryStream(ID3D12PipelineLibrary1* l,LPCWSTR name,const D3D12_PIPELINE_STATE_STREAM_DESC* desc,REFIID iid,void** out) {
    HRESULT hr=Original<LoadStream,13>(l)(l,name,desc,iid,out); D3D12_SHADER_BYTECODE code{};
    if(!Bypass() && SUCCEEDED(hr) && out && ProbeComputeBytecode(desc,code)) ObservePipeline(*out,code); return hr;
}
bool HookLibrary(ID3D12PipelineLibrary* library) {
    bool ok=Hook(library,10,reinterpret_cast<void*>(LibraryCompute)); ComPtr<ID3D12PipelineLibrary1> extended;
    if(SUCCEEDED(library->QueryInterface(IID_PPV_ARGS(&extended)))) ok=Hook(extended.Get(),13,reinterpret_cast<void*>(LibraryStream))&&ok;
    return ok;
}
using MakeLibrary=HRESULT(STDMETHODCALLTYPE*)(ID3D12Device1*,const void*,SIZE_T,REFIID,void**);
HRESULT STDMETHODCALLTYPE CreateLibrary(ID3D12Device1* d,const void* data,SIZE_T size,REFIID iid,void** out) {
    HRESULT hr=Original<MakeLibrary,44>(d)(d,data,size,iid,out);
    if(!Bypass() && SUCCEEDED(hr) && out && *out) { ComPtr<ID3D12PipelineLibrary> library;
        if(SUCCEEDED(static_cast<IUnknown*>(*out)->QueryInterface(IID_PPV_ARGS(&library)))) HookLibrary(library.Get()); }
    return hr;
}
bool RootMatches(const void* data,SIZE_T size) {
    ComPtr<ID3D12VersionedRootSignatureDeserializer> reader;
    if(FAILED(D3D12CreateVersionedRootSignatureDeserializer(data,size,IID_PPV_ARGS(&reader)))) return false;
    const D3D12_VERSIONED_ROOT_SIGNATURE_DESC* desc=nullptr;
    if(FAILED(reader->GetRootSignatureDescAtVersion(D3D_ROOT_SIGNATURE_VERSION_1_0,&desc)) || desc->Desc_1_0.NumParameters!=5) return false;
    auto p=desc->Desc_1_0.pParameters;
    for(UINT i=0;i<3;++i) {
        if(p[i].ParameterType!=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE || p[i].DescriptorTable.NumDescriptorRanges!=1) return false;
        const auto& r=p[i].DescriptorTable.pDescriptorRanges[0];
        auto type=i==0?D3D12_DESCRIPTOR_RANGE_TYPE_SRV:i==1?D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        if(r.RangeType!=type || r.BaseShaderRegister!=0 || r.RegisterSpace!=0 ||
           r.NumDescriptors!=(i==0?64u:16u) || (r.OffsetInDescriptorsFromTableStart!=0 && r.OffsetInDescriptorsFromTableStart!=D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND)) return false;
    }
    for(UINT i=3;i<5;++i) if(p[i].ParameterType!=D3D12_ROOT_PARAMETER_TYPE_CBV || p[i].Descriptor.ShaderRegister!=i-3 || p[i].Descriptor.RegisterSpace!=0) return false;
    return true;
}
using CreateRoot=HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*,UINT,const void*,SIZE_T,REFIID,void**);
HRESULT STDMETHODCALLTYPE MakeRoot(ID3D12Device* d,UINT node,const void* data,SIZE_T size,REFIID iid,void** out) {
    HRESULT hr=Original<CreateRoot,16>(d)(d,node,data,size,iid,out);
    if(!Bypass() && SUCCEEDED(hr) && out && *out && RootMatches(data,size)) {
        ComPtr<ID3D12RootSignature> root; if(SUCCEEDED(static_cast<IUnknown*>(*out)->QueryInterface(IID_PPV_ARGS(&root)))) {
            Hook(root.Get(),2,reinterpret_cast<void*>(RootRelease));
            std::unique_lock<std::shared_mutex> lock(metadataMutex); roots.insert(root.Get());
        }
    } return hr;
}
using CreateSRV=void(STDMETHODCALLTYPE*)(ID3D12Device*,ID3D12Resource*,const D3D12_SHADER_RESOURCE_VIEW_DESC*,D3D12_CPU_DESCRIPTOR_HANDLE);
void STDMETHODCALLTYPE MakeSRV(ID3D12Device* d,ID3D12Resource* r,const D3D12_SHADER_RESOURCE_VIEW_DESC* desc,D3D12_CPU_DESCRIPTOR_HANDLE dest) {
    Original<CreateSRV,18>(d)(d,r,desc,dest); if(Bypass()) return;
    if(r && desc && desc->ViewDimension==D3D12_SRV_DIMENSION_TEXTURE2D) {
        Descriptor value;value.resource=ObserveResource(r);value.texture2D=desc->Texture2D;
        value.mapping=desc->Shader4ComponentMapping;value.format=desc->Format;
        StoreDescriptor(dest.ptr,value);
    } else StoreDescriptor(dest.ptr,{});
}
using CreateUAV=void(STDMETHODCALLTYPE*)(ID3D12Device*,ID3D12Resource*,ID3D12Resource*,const D3D12_UNORDERED_ACCESS_VIEW_DESC*,D3D12_CPU_DESCRIPTOR_HANDLE);
void STDMETHODCALLTYPE MakeUAV(ID3D12Device* d,ID3D12Resource* r,ID3D12Resource* counter,const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc,D3D12_CPU_DESCRIPTOR_HANDLE dest) {
    Original<CreateUAV,19>(d)(d,r,counter,desc,dest); if(Bypass()) return;
    if(r && desc && desc->ViewDimension==D3D12_UAV_DIMENSION_TEXTURE2D && desc->Texture2D.MipSlice==0) {
        Descriptor value;value.resource=ObserveResource(r);value.uav=true;value.format=desc->Format;
        StoreDescriptor(dest.ptr,value);
    } else StoreDescriptor(dest.ptr,{});
}
using CreateCBV=void(STDMETHODCALLTYPE*)(ID3D12Device*,const D3D12_CONSTANT_BUFFER_VIEW_DESC*,D3D12_CPU_DESCRIPTOR_HANDLE);
void STDMETHODCALLTYPE MakeCBV(ID3D12Device* d,const D3D12_CONSTANT_BUFFER_VIEW_DESC* desc,D3D12_CPU_DESCRIPTOR_HANDLE dest) {
    Original<CreateCBV,17>(d)(d,desc,dest); if(Bypass()) return;
    StoreDescriptor(dest.ptr,{});
}
void CopyInto(const Heap& dst,SIZE_T di,UINT64 source,UINT count) {
    const auto step=dst.step;
    const auto& src=DescriptorHeap(source,&dst);
    count=std::min(count,dst.count-UINT(di));
    const bool known=src.storage && src.step==step && count<=src.count-UINT((source-src.cpu)/step);
    if(!known) {
        for(UINT i=0;i<count;++i)dst.storage->entries[di+i].store(0,std::memory_order_relaxed);
        return;
    }
    auto si=SIZE_T((source-src.cpu)/step);
    if(dst.storage==src.storage && !(di+count<=si || si+count<=di))return;
    // Callers synchronize conflicting API ranges. Atomic slots let our reader
    // observe a complete ID without racing other recording threads. Immutable
    // view records are read and reclaimed only under metadataMutex.
    for(UINT i=0;i<count;++i)dst.storage->entries[di+i].store(
        src.storage->entries[si+i].load(std::memory_order_relaxed),std::memory_order_relaxed);
}
void CopyDescriptorSpan(UINT64 destination,UINT64 source,UINT count,UINT step) {
    const auto& dst=DescriptorHeap(destination);if(!dst.storage || (step && dst.step!=step))return;
    CopyInto(dst,SIZE_T((destination-dst.cpu)/dst.step),source,count);
}
void CopyMetadata(UINT dc,const D3D12_CPU_DESCRIPTOR_HANDLE* dst,const UINT* ds,UINT sc,const D3D12_CPU_DESCRIPTOR_HANDLE* src,const UINT* ss,UINT step) {
    Profile profile(4);
    Profile variant(12);
    if(!dc || !dst || !src) return;
    if(!step)step=DescriptorHeap(dst[0].ptr).step;
    if(!step)return;
    UINT64 totalD=0,totalS=0; for(UINT i=0;i<dc;++i) totalD+=ds?ds[i]:1; for(UINT i=0;i<sc;++i) totalS+=ss?ss[i]:1;
    if(totalD!=totalS) {
        for(UINT i=0;i<dc;++i)CopyDescriptorSpan(dst[i].ptr,0,ds?ds[i]:1,step);
        return;
    }
    // D3D12 forbids overlapping descriptor copy ranges. Stream metadata in the same order.
    UINT sourceRange=0,sourceOffset=0;
    for(UINT i=0;i<dc;++i) {
        // UE can gather many source ranges into one destination range. Borrow
        // that destination once, and pin it throughout every source lookup.
        const auto& destination=DescriptorHeap(dst[i].ptr);
        const bool known=destination.storage && destination.step==step;
        const auto base=known?SIZE_T((dst[i].ptr-destination.cpu)/step):0;
        for(UINT j=0;j<(ds?ds[i]:1);) {
            while(sourceRange<sc && sourceOffset>=(ss?ss[sourceRange]:1)) {++sourceRange;sourceOffset=0;}
            if(sourceRange==sc)return;
            const UINT count=std::min((ds?ds[i]:1)-j,(ss?ss[sourceRange]:1)-sourceOffset);
            if(known && base+j<destination.count)
                CopyInto(destination,base+j,src[sourceRange].ptr+SIZE_T(sourceOffset)*step,count);
            j+=count;sourceOffset+=count;
        }
    }
}
using Copy=void(STDMETHODCALLTYPE*)(ID3D12Device*,UINT,const D3D12_CPU_DESCRIPTOR_HANDLE*,const UINT*,UINT,const D3D12_CPU_DESCRIPTOR_HANDLE*,const UINT*,D3D12_DESCRIPTOR_HEAP_TYPE);
void STDMETHODCALLTYPE CopyDescriptors(ID3D12Device* d,UINT dc,const D3D12_CPU_DESCRIPTOR_HANDLE* dst,const UINT* ds,UINT sc,const D3D12_CPU_DESCRIPTOR_HANDLE* src,const UINT* ss,D3D12_DESCRIPTOR_HEAP_TYPE type) {
    Original<Copy,23>(d)(d,dc,dst,ds,sc,src,ss,type);
    if(!Bypass() && type==D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) CopyMetadata(dc,dst,ds,sc,src,ss,0);
}
using Simple=void(STDMETHODCALLTYPE*)(ID3D12Device*,UINT,D3D12_CPU_DESCRIPTOR_HANDLE,D3D12_CPU_DESCRIPTOR_HANDLE,D3D12_DESCRIPTOR_HEAP_TYPE);
void STDMETHODCALLTYPE CopySimple(ID3D12Device* d,UINT count,D3D12_CPU_DESCRIPTOR_HANDLE dst,D3D12_CPU_DESCRIPTOR_HANDLE src,D3D12_DESCRIPTOR_HEAP_TYPE type) {
    Original<Simple,24>(d)(d,count,dst,src,type);
    if(!Bypass() && type==D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) {Profile profile(4);Profile variant(13);CopyDescriptorSpan(dst.ptr,src.ptr,count,0);}
}
using MakeHeap=HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*,const D3D12_DESCRIPTOR_HEAP_DESC*,REFIID,void**);
HRESULT STDMETHODCALLTYPE CreateHeap(ID3D12Device* d,const D3D12_DESCRIPTOR_HEAP_DESC* desc,REFIID iid,void** out) {
    HRESULT hr=Original<MakeHeap,14>(d)(d,desc,iid,out);
    if(!Bypass() && SUCCEEDED(hr) && out && *out) { ComPtr<ID3D12DescriptorHeap> h;
        if(SUCCEEDED(static_cast<IUnknown*>(*out)->QueryInterface(IID_PPV_ARGS(&h)))) ObserveHeap(h.Get()); }
    return hr;
}
using Reset=HRESULT(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,ID3D12CommandAllocator*,ID3D12PipelineState*);
HRESULT STDMETHODCALLTYPE ResetList(ID3D12GraphicsCommandList* list,ID3D12CommandAllocator* a,ID3D12PipelineState* pso) {
    HRESULT hr=Original<Reset,10>(list)(list,a,pso); if(Bypass() || FAILED(hr)) return hr;
    auto& state=State(list);
    if(state.dlssRecorded.load(std::memory_order_acquire)
#ifdef GROUNDED_FRAME_CAPTURE
       || capturePendingList.load(std::memory_order_acquire)==list
#endif
    ){
        std::lock_guard<std::mutex> lock(runtimeMutex); Internal guard; if(integration) integration->Discarded(list);
#ifdef GROUNDED_FRAME_CAPTURE
        frameCapture->Discarded(list);if(capturePendingList==list)capturePendingList=nullptr;
#endif
        state.dlssRecorded.store(false,std::memory_order_release);
    }
    state.Reset(pso,list->GetType()==D3D12_COMMAND_LIST_TYPE_DIRECT);state.target=IsTarget(pso);return hr;
}
using Clear=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,ID3D12PipelineState*);
void STDMETHODCALLTYPE ClearList(ID3D12GraphicsCommandList* list,ID3D12PipelineState* pso) {
    if(!Bypass()) {auto& s=State(list);s.Reset(pso,list->GetType()==D3D12_COMMAND_LIST_TYPE_DIRECT);s.target=IsTarget(pso);}
    Original<Clear,11>(list)(list,pso);
}
using SetPSO=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,ID3D12PipelineState*);
void STDMETHODCALLTYPE SetPipeline(ID3D12GraphicsCommandList* l,ID3D12PipelineState* p) {
    if(!Bypass()) {auto& s=State(l); s.pso=p; s.target=IsTarget(p);} Original<SetPSO,25>(l)(l,p);
}
using SetRoot=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,ID3D12RootSignature*);
template<UINT Slot,bool Graphics> void STDMETHODCALLTYPE SetSignature(ID3D12GraphicsCommandList* l,ID3D12RootSignature* r) {
    Profile profile(5);
    if(!Bypass()) {auto& state=State(l); auto& s=Graphics?state.graphics:state.compute;
        if(s.root!=r)s.Invalidate(r);}
    Original<SetRoot,Slot>(l)(l,r);
}
using SetTable=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,UINT,D3D12_GPU_DESCRIPTOR_HANDLE);
template<UINT Slot,bool Graphics> void STDMETHODCALLTYPE SetDescriptorTable(ID3D12GraphicsCommandList* l,UINT index,D3D12_GPU_DESCRIPTOR_HANDLE handle) {
    if(!Bypass() && index<64) {auto& state=State(l); auto& s=Graphics?state.graphics:state.compute;
        s.Bind(index,Argument::Table).value=handle.ptr;}
    Original<SetTable,Slot>(l)(l,index,handle);
}
using SetAddress=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,UINT,D3D12_GPU_VIRTUAL_ADDRESS);
template<UINT Slot,bool Graphics,Argument::Type Type> void STDMETHODCALLTYPE SetGPUAddress(ID3D12GraphicsCommandList* l,UINT index,D3D12_GPU_VIRTUAL_ADDRESS address) {
    if(!Bypass() && index<64) {auto& state=State(l); auto& s=Graphics?state.graphics:state.compute;
        s.Bind(index,Type).value=address;}
    Original<SetAddress,Slot>(l)(l,index,address);
}
using SetConstant=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,UINT,UINT,UINT);
template<UINT Slot,bool Graphics> void STDMETHODCALLTYPE SetOneConstant(ID3D12GraphicsCommandList* l,UINT index,UINT value,UINT offset) {
    if(!Bypass() && index<64 && offset<64) {auto& state=State(l);auto& a=(Graphics?state.graphics:state.compute).Bind(index,Argument::Constants);
        a.constants[offset]=value;a.mask|=UINT64(1)<<offset;}
    Original<SetConstant,Slot>(l)(l,index,value,offset);
}
using SetConstants=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,UINT,UINT,const void*,UINT);
template<UINT Slot,bool Graphics> void STDMETHODCALLTYPE SetManyConstants(ID3D12GraphicsCommandList* l,UINT index,UINT count,const void* values,UINT offset) {
    if(!Bypass() && index<64 && offset<64 && count<=64-offset && values) {auto& state=State(l);auto& a=(Graphics?state.graphics:state.compute).Bind(index,Argument::Constants);
        memcpy(a.constants.data()+offset,values,count*4);for(UINT i=0;i<count;++i)a.mask|=UINT64(1)<<(offset+i);}
    Original<SetConstants,Slot>(l)(l,index,count,values,offset);
}
using SetHeaps=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,UINT,ID3D12DescriptorHeap* const*);
void STDMETHODCALLTYPE SetDescriptorHeaps(ID3D12GraphicsCommandList* l,UINT count,ID3D12DescriptorHeap* const* values) {
    Profile profile(14);
    if(!Bypass() && count<=2) {
        auto& s=State(l); bool different=count!=s.heapCount;
        for(UINT i=0;i<count;++i) {different|=s.heaps[i]!=values[i];if(nativeBridgeMode!=2)ObserveHeap(values[i]);}
        if(different) {s.compute.InvalidateTables();s.graphics.InvalidateTables();}
        s.heapCount=count; s.heaps={}; for(UINT i=0;i<count;++i)s.heaps[i]=values[i];
    }
    Original<SetHeaps,28>(l)(l,count,values);
}
void Restore(ID3D12GraphicsCommandList* l,const Command& s) {
    l->SetDescriptorHeaps(s.heapCount,s.heaps.data());
    if(s.compute.root) l->SetComputeRootSignature(s.compute.root);
    if(s.graphics.root) l->SetGraphicsRootSignature(s.graphics.root);
    for(bool graphics:{false,true}) {
        const auto& root=graphics?s.graphics:s.compute;
        if(!root.root)continue;
        for(UINT i=0;i<64;++i) {if(!root.Valid(i))continue;const auto& a=root.args[i];switch(a.type) {
            case Argument::Table: if(graphics)l->SetGraphicsRootDescriptorTable(i,{a.value});else l->SetComputeRootDescriptorTable(i,{a.value});break;
            case Argument::CBV: if(graphics)l->SetGraphicsRootConstantBufferView(i,a.value);else l->SetComputeRootConstantBufferView(i,a.value);break;
            case Argument::SRV: if(graphics)l->SetGraphicsRootShaderResourceView(i,a.value);else l->SetComputeRootShaderResourceView(i,a.value);break;
            case Argument::UAV: if(graphics)l->SetGraphicsRootUnorderedAccessView(i,a.value);else l->SetComputeRootUnorderedAccessView(i,a.value);break;
            case Argument::Constants: for(UINT j=0;j<64;++j)if(a.mask&(UINT64(1)<<j)) {
                if(graphics)l->SetGraphicsRoot32BitConstant(i,a.constants[j],j);else l->SetComputeRoot32BitConstant(i,a.constants[j],j);
            }break;
            default:break;
        }}
    }
    if(s.pso)l->SetPipelineState(s.pso);
}
bool SafeCopy(void* to,const void* from,size_t bytes) {
    __try {memcpy(to,from,bytes);return true;} __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool ConstantBytes(UINT64 address,void* to,size_t bytes) {
    auto it=mappings.upper_bound(address);if(it==mappings.begin())return false;--it;
    const auto& m=it->second;auto offset=address-it->first;
    return m.cpu && offset<=m.size && bytes<=m.size-offset && SafeCopy(to,m.cpu+offset,bytes);
}
bool Resolve(const Command& s,UINT rootIndex,UINT reg,bool uav,Descriptor& value) {
    const auto& a=s.compute.args[rootIndex];if(!s.compute.Valid(rootIndex) || a.type!=Argument::Table)return false;
    for(UINT i=0;i<s.heapCount;++i) {auto hit=heaps.find(s.heaps[i]);if(hit==heaps.end())continue;const auto& h=hit->second;
        if(h.type!=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || !h.gpu || a.value<h.gpu || !h.step)continue;
        UINT64 delta=a.value-h.gpu;if(delta%h.step || delta/h.step+reg>=h.count)continue;
        if(!h.storage || !h.storage->valid)return false;
        {const auto index=SIZE_T(delta/h.step+reg);
            const auto id=h.storage->entries[index].load(std::memory_order_relaxed);
            const auto view=descriptorViews.find(id);if(view==descriptorViews.end())return false;value=view->second;}
        auto resource=resources.find(value.resource.pointer);
        return value.uav==uav && resource!=resources.end() && resource->second.generation==value.resource.generation;
    }return false;
}
// Caller holds metadataMutex for the upload mapping and root signature registry.
bool ReadConstants(const Command& state,grounded::PassInput& input,std::string& reason) {
    auto reject=[&](const std::string& message){reason=message;return false;};
    if(!roots.count(state.compute.root))return reject(roots.empty()?
        "Unrecognized temporal root signature; none was observed since startup (created before hooks were ready?)":
        "Unrecognized temporal root signature");
    if(!state.compute.Valid(3) || !state.compute.Valid(4) || state.compute.args[3].type!=Argument::CBV || state.compute.args[4].type!=Argument::CBV)return reject("Temporal constants are not bound");
    std::array<float,116> pass{};std::array<float,580> view{};
    if(!ConstantBytes(state.compute.args[3].value,pass.data(),sizeof(pass)))return reject("Temporal b0 upload mapping unavailable");
    if(!ConstantBytes(state.compute.args[4].value,view.data(),sizeof(view)))return reject("View b1 upload mapping unavailable");
    if(!grounded::DecodeConstants(pass.data(),sizeof(pass),view.data(),sizeof(view),input.constants))return reject("Temporal constant contract mismatch; normalization="+std::to_string(view[135*4+2]));
    if(state.target==grounded::PassVariant::Native100 &&
        (input.constants.activeWidth!=input.constants.outputWidth || input.constants.activeHeight!=input.constants.outputHeight))
        return reject("Native TAA contract requires a 100% viewport");
    return true;
}
bool DescriptorInput(const Command& state,grounded::PassInput& input,std::array<ComPtr<ID3D12Resource>,7>& retained,std::string& reason) {
    std::shared_lock<std::shared_mutex> lock(metadataMutex);
    if(!ReadConstants(state,input,reason))return false;
    auto reject=[&](const std::string& message){reason=message;return false;};
    std::array<Descriptor,7> d{};const DXGI_FORMAT formats[]={DXGI_FORMAT_R32G32B32A32_FLOAT,DXGI_FORMAT_R16G16B16A16_FLOAT,
        DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,DXGI_FORMAT_R16G16B16A16_UNORM,DXGI_FORMAT_X32_TYPELESS_G8X24_UINT,DXGI_FORMAT_R16G16B16A16_FLOAT,DXGI_FORMAT_R16G16B16A16_FLOAT};
    for(UINT i=0;i<7;++i) {
        const auto slot=i==6?std::string("u0"):std::string("t")+std::to_string(i);
        if(!Resolve(state,i==6?2u:0u,i==6?0u:i,i==6,d[i]))return reject("Unresolved live descriptor "+slot);
        if(d[i].format!=formats[i])return reject("Unexpected format at "+slot+": "+std::to_string(d[i].format));
        if(i<6 && d[i].texture2D.MostDetailedMip!=0)return reject("Nonzero first mip at "+slot);
        // Only channels read by our preparation shader must preserve their identity.
        const UINT channels=i==0 || i==2?1u:i==1?3u:i==3?2u:0u;
        for(UINT channel=0;channel<channels;++channel)
            if(UINT(D3D12_DECODE_SHADER_4_COMPONENT_MAPPING(channel,d[i].mapping))!=channel)
                return reject("Unsupported component mapping at "+slot+": "+std::to_string(d[i].mapping));
        retained[i]=d[i].resource.pointer;
    }
    input.exposure=d[0].resource.pointer;input.color=d[1].resource.pointer;input.depth=d[2].resource.pointer;
    input.velocity=d[3].resource.pointer;input.output=d[6].resource.pointer;input.depthView=d[2].View();input.velocityView=d[3].View();
    return true;
}
bool NativeInput(const Command& state,grounded::PassInput& input,std::array<ComPtr<ID3D12Resource>,7>& retained,std::string& reason) {
    const auto* frame=grounded::native_temporal::current;
    if(!frame){++nativeMissing;reason="No verified native temporal dispatch scope";return false;}
    if(!frame->resourcesValid){++nativeRejected;reason=frame->reason;if(!frame->detail.empty())reason+="; "+frame->detail;return false;}
    {std::shared_lock<std::shared_mutex> lock(metadataMutex);if(!ReadConstants(state,input,reason)){++nativeRejected;return false;}}
    if(frame->groups!=std::array<UINT,3>{(input.constants.outputWidth+7)/8,(input.constants.outputHeight+7)/8,1}){
        ++nativeRejected;reason="Native frame and temporal constants differ";return false;
    }
    for(UINT i=0;i<5;++i)retained[i]=frame->resources[i];
    input.exposure=retained[0].Get();input.color=retained[1].Get();input.depth=retained[2].Get();
    input.velocity=retained[3].Get();input.output=retained[4].Get();
    input.depthView={};input.depthView.Format=DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    input.depthView.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
    input.depthView.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;input.depthView.Texture2D.MipLevels=1;
    input.velocityView=input.depthView;input.velocityView.Format=DXGI_FORMAT_R16G16B16A16_UNORM;
    return true;
}
bool SameView(const D3D12_SHADER_RESOURCE_VIEW_DESC& a,const D3D12_SHADER_RESOURCE_VIEW_DESC& b) {
    return a.Format==b.Format && a.ViewDimension==b.ViewDimension && a.Shader4ComponentMapping==b.Shader4ComponentMapping &&
        a.Texture2D.MostDetailedMip==b.Texture2D.MostDetailedMip && a.Texture2D.MipLevels==b.Texture2D.MipLevels &&
        a.Texture2D.PlaneSlice==b.Texture2D.PlaneSlice && a.Texture2D.ResourceMinLODClamp==b.Texture2D.ResourceMinLODClamp;
}
bool Input(const Command& state,grounded::PassInput& input,std::array<ComPtr<ID3D12Resource>,7>& retained,std::string& reason) {
    const auto mode=nativeBridgeMode.load(std::memory_order_relaxed);
    if(mode==2)return NativeInput(state,input,retained,reason);
    if(!DescriptorInput(state,input,retained,reason))return false;
    if(mode!=1)return true;
    grounded::PassInput native;std::array<ComPtr<ID3D12Resource>,7> nativeRetained;
    if(!NativeInput(state,native,nativeRetained,reason))return false;
    ++nativeCompared;
    if(input.exposure!=native.exposure || input.color!=native.color || input.depth!=native.depth || input.velocity!=native.velocity ||
       input.output!=native.output || !SameView(input.depthView,native.depthView) || !SameView(input.velocityView,native.velocityView)){
        ++nativeMismatches;
        char detail[1024]{};sprintf_s(detail,"Native audit differs: resources E/C/D/V/O=%u/%u/%u/%u/%u; depth mapping=%u mip=%u count=%u plane=%u lod=%.8g; velocity mapping=%u mip=%u count=%u plane=%u lod=%.8g",
            unsigned(input.exposure==native.exposure),unsigned(input.color==native.color),unsigned(input.depth==native.depth),unsigned(input.velocity==native.velocity),unsigned(input.output==native.output),
            input.depthView.Shader4ComponentMapping,input.depthView.Texture2D.MostDetailedMip,input.depthView.Texture2D.MipLevels,input.depthView.Texture2D.PlaneSlice,input.depthView.Texture2D.ResourceMinLODClamp,
            input.velocityView.Shader4ComponentMapping,input.velocityView.Texture2D.MostDetailedMip,input.velocityView.Texture2D.MipLevels,input.velocityView.Texture2D.PlaneSlice,input.velocityView.Texture2D.ResourceMinLODClamp);
        reason=detail;return false;
    }
    input=native;retained=std::move(nativeRetained);return true;
}
using Dispatch=void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*,UINT,UINT,UINT);
HMODULE FindNGXModule() {
    // Use the NVIDIA driver API directly; no scaling intermediary is required.
    return grounded::NativeNgxModule();
}
auto& localRuntime=*new grounded::local_dlss::Selection;
std::filesystem::path FeatureDirectory(){return localRuntime.directory;}

void STDMETHODCALLTYPE DispatchList(ID3D12GraphicsCommandList* list,UINT x,UINT y,UINT z) {
    auto original=Original<Dispatch,14>(list);
    if(Bypass() || ready!=1 || (!enabled.load(std::memory_order_relaxed)
#ifdef GROUNDED_FRAME_CAPTURE
       && !captureArmed.load(std::memory_order_acquire)
#endif
    )) {original(list,x,y,z);return;}
    auto& state=State(list);
    if(state.target==grounded::PassVariant::Unknown && grounded::native_temporal::current) {
        // A verified temporal pass ran a pipeline we did not classify; Unreal
        // handles that dispatch. It is counted only: 11.23.1 also reset DLSS
        // history here, but in real play it fired continually and
        // left DLSS with almost no accumulation.
        if(nativeUnclassified.fetch_add(1,std::memory_order_relaxed)==0){
            size_t known=0;{std::shared_lock<std::shared_mutex> lock(metadataMutex);known=targets.size();}
            Log("Verified temporal pass used an unclassified pipeline %p (classified temporal pipelines=%zu); Unreal handles it; DLSS history is kept",state.pso,known);
        }
    }
    if(!state.direct || state.target==grounded::PassVariant::Unknown || z!=1) {original(list,x,y,z);return;}
    ++candidates;
    Profile dispatchProfile(7);
    grounded::PassInput input;std::array<ComPtr<ID3D12Resource>,7> retained;
    std::string reason;
    bool valid=Input(state,input,retained,reason);
    if(!valid || x!=(input.constants.outputWidth+7)/8 || y!=(input.constants.outputHeight+7)/8) {
        if(valid)reason="Dispatch dimensions differ from temporal output";
        {std::lock_guard<std::mutex> lock(runtimeMutex);grounded::sharpness_control::Publish(false,0);if(lastError!=reason){lastError=reason;Log("Native fallback: %s",reason.c_str());}}
        ++fallbacks;historyDirty=true;original(list,x,y,z);return;
    }
    bool replaced=false;
#ifdef GROUNDED_FRAME_CAPTURE
    if(!enabled.load(std::memory_order_relaxed)) {
        original(list,x,y,z);
        std::lock_guard<std::mutex> lock(runtimeMutex);Internal guard;CaptureFrame(list,input,false);return;
    }
#endif
    {
        std::lock_guard<std::mutex> lock(runtimeMutex);Internal guard;
        if(enabled) {
        DWORD thread=GetCurrentThreadId();
        if(!ownerThread)ownerThread=thread;
        if(ownerThread!=thread) {
            if(lastError!="Multiple render recording threads") {lastError="Multiple render recording threads";Log("Native fallback: %s",lastError.c_str());}
        } else {
            if(!integration) {
                ComPtr<ID3D12Device> device;list->GetDevice(IID_PPV_ARGS(&device));
                HMODULE module=FindNGXModule();
                wchar_t runtimePath[32768]{};if(module)GetModuleFileNameW(module,runtimePath,32768);
                auto featureDirectory=FeatureDirectory();
                Log("NVIDIA NGX module: %ls; feature directory: %ls",runtimePath,featureDirectory.c_str());
                auto candidate=std::make_unique<grounded::Integration>();
                if(device && module && !featureDirectory.empty() && candidate->Initialize(device.Get(),module,directory/L"GroundedDLSS-data",featureDirectory)) {
                    renderPresetRequested=candidate->RenderPresetRequested();
                    gpuArchitecture=candidate->GpuArchitecture();
                    Log("DLSS GPU: %ls; architecture=0x%x; automatic policy K for RTX 20/30 or unknown, K/M/L for Ada or later",candidate->GpuName().c_str(),candidate->GpuArchitecture());
                    Log("DLSS render presets by mode: Quality=%u Balanced=%u Performance=%u UltraPerformance=%u DLAA=%u (5=E, 11=K, 12=L, 13=M); scale follows game viewport",
                        candidate->RenderPresetFor(2),candidate->RenderPresetFor(1),candidate->RenderPresetFor(0),candidate->RenderPresetFor(3),candidate->RenderPresetFor(5));
                    integration=candidate.release();
                }
                else {lastError=!module?"NVIDIA NGX runtime not loaded":featureDirectory.empty()?"No compatible NVIDIA-signed production DLSS library was found":candidate->Error();Log("Initialization failed: %s",lastError.c_str());failed=true;enabled=false;}
            }
            if(integration) {
                const auto now=GetTickCount64();
                integration->SetProfiling(profiling.load(std::memory_order_relaxed));
                replaced=integration->Record(list,input,historyDirty.load() || !evaluated.load() || now-lastEvaluation>500);
                state.dlssRecorded.store(integration->HasPending(list),std::memory_order_release);
                Restore(list,state);
                if(replaced) {
#ifdef GROUNDED_FRAME_CAPTURE
                    CaptureFrame(list,input,true);
#endif
                    const bool changed=featureGenerations.load()!=integration->FeatureGenerations();
                    qualityMode=integration->QualityMode();featureGenerations=integration->FeatureGenerations();
                    renderPresetRequested=integration->RenderPresetRequested();
                    grounded::sharpness_control::Publish(true,qualityMode.load());
                    lastEvaluation=now;activeWidth=input.constants.activeWidth;activeHeight=input.constants.activeHeight;
                    historyDirty=false;
                    outputWidth=input.constants.outputWidth;outputHeight=input.constants.outputHeight;
                    auto count=++successes;
                    if(!evaluated.exchange(true) || changed)Log("DLSS ACTIVE: %ux%u (allocation %ux%u) -> %ux%u; native exposure, decoded motion, jitter=(%.8g,%.8g)",
                        input.constants.activeWidth,input.constants.activeHeight,input.constants.width,input.constants.height,
                        input.constants.outputWidth,input.constants.outputHeight,input.constants.jitterX,input.constants.jitterY);
                    if(changed)Log("DLSS scale changed: %.4f%% x %.4f%%; quality mode=%u (0=Performance, 1=Balanced, 2=Quality, 3=UltraPerformance, 5=DLAA); feature generation=%llu",
                        100.0*input.constants.activeWidth/input.constants.outputWidth,100.0*input.constants.activeHeight/input.constants.outputHeight,
                        qualityMode.load(),featureGenerations.load());
                    if(changed)Log("DLSS model requested: preset=%u; architecture=0x%x",renderPresetRequested.load(),integration->GpuArchitecture());
                    if(changed)Log("DLSS prepared viewport: %ux%u -> %ux%u; output %ux%u",input.constants.activeWidth,input.constants.activeHeight,
                        integration->InputWidth(),integration->InputHeight(),input.constants.outputWidth,input.constants.outputHeight);
                    if(count%300==0)Log("DLSS evaluations=%llu; native fallbacks=%llu",count,fallbacks.load());
                } else if(lastError!=integration->Error()) {lastError=integration->Error();Log("Native fallback: %s",lastError.c_str());}
                if(integration->Disabled()){failed=true;enabled=false;evaluated=false;}
            }
            if(!replaced)grounded::sharpness_control::Publish(false,0);
        }
        }
    }
    if(!replaced) {++fallbacks;original(list,x,y,z);}
}
using Execute=void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*,UINT,ID3D12CommandList* const*);
void STDMETHODCALLTYPE ExecuteLists(ID3D12CommandQueue* queue,UINT count,ID3D12CommandList* const* lists) {
    Profile profile(6);
    auto original=Original<Execute,10>(queue);
    if(Bypass()) {original(queue,count,lists);return;}
    bool hasDLSS=false;
    for(UINT i=0;i<count;++i)hasDLSS|=State(static_cast<ID3D12GraphicsCommandList*>(lists[i])).dlssRecorded.load(std::memory_order_acquire);
    bool hasCapture=false;
#ifdef GROUNDED_FRAME_CAPTURE
    const auto captureList=capturePendingList.load(std::memory_order_acquire);
    if(captureList)for(UINT i=0;i<count;++i)hasCapture|=lists[i]==captureList;
#endif
    if(!hasDLSS && !hasCapture){
        // Unrelated queues/lists do not access NGX or its frame/fence state.
        // Their submission may proceed while another thread records DLSS.
        Profile plain(10);Internal guard;original(queue,count,lists);return;
    }
    Profile withDLSS(11);
    std::lock_guard<std::mutex> lock(runtimeMutex);Internal guard;
    bool pending=false;if(integration)for(UINT i=0;i<count;++i)pending|=integration->HasPending(static_cast<ID3D12GraphicsCommandList*>(lists[i]));
    if(pending)integration->BeforeSubmit(queue);
    original(queue,count,lists);
    if(integration)integration->Submitted(queue,count,lists);
#ifdef GROUNDED_FRAME_CAPTURE
    if(hasCapture){frameCapture->Submitted(queue,count,lists);capturePendingList=nullptr;}
#endif
}
ULONG STDMETHODCALLTYPE CommandRelease(IUnknown* object) {
    auto original=Original<Release,2>(object);ULONG count=original(object);
    if(!Bypass() && !count) {
        auto list=static_cast<ID3D12GraphicsCommandList*>(object);
        {std::lock_guard<std::mutex> lock(runtimeMutex);Internal guard;if(integration)integration->Discarded(list);}
#ifdef GROUNDED_FRAME_CAPTURE
        {std::lock_guard<std::mutex> lock(runtimeMutex);Internal guard;frameCapture->Discarded(list);if(capturePendingList==list)capturePendingList=nullptr;}
#endif
        std::unique_lock<std::shared_mutex> lock(commandsMutex);commands.erase(list);++commandEpoch;
    }return count;
}
bool HookCommand(ID3D12GraphicsCommandList* l) {
    return Hook(l,2,reinterpret_cast<void*>(CommandRelease)) && Hook(l,10,reinterpret_cast<void*>(ResetList)) && Hook(l,11,reinterpret_cast<void*>(ClearList)) &&
        Hook(l,14,reinterpret_cast<void*>(DispatchList)) && Hook(l,25,reinterpret_cast<void*>(SetPipeline)) && Hook(l,28,reinterpret_cast<void*>(SetDescriptorHeaps)) &&
        Hook(l,29,reinterpret_cast<void*>(SetSignature<29,false>)) && Hook(l,30,reinterpret_cast<void*>(SetSignature<30,true>)) &&
        Hook(l,31,reinterpret_cast<void*>(SetDescriptorTable<31,false>)) && Hook(l,32,reinterpret_cast<void*>(SetDescriptorTable<32,true>)) &&
        Hook(l,33,reinterpret_cast<void*>(SetOneConstant<33,false>)) && Hook(l,34,reinterpret_cast<void*>(SetOneConstant<34,true>)) &&
        Hook(l,35,reinterpret_cast<void*>(SetManyConstants<35,false>)) && Hook(l,36,reinterpret_cast<void*>(SetManyConstants<36,true>)) &&
        Hook(l,37,reinterpret_cast<void*>(SetGPUAddress<37,false,Argument::CBV>)) && Hook(l,38,reinterpret_cast<void*>(SetGPUAddress<38,true,Argument::CBV>)) &&
        Hook(l,39,reinterpret_cast<void*>(SetGPUAddress<39,false,Argument::SRV>)) && Hook(l,40,reinterpret_cast<void*>(SetGPUAddress<40,true,Argument::SRV>)) &&
        Hook(l,41,reinterpret_cast<void*>(SetGPUAddress<41,false,Argument::UAV>)) && Hook(l,42,reinterpret_cast<void*>(SetGPUAddress<42,true,Argument::UAV>));
}
using MakeList=HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*,UINT,D3D12_COMMAND_LIST_TYPE,ID3D12CommandAllocator*,ID3D12PipelineState*,REFIID,void**);
HRESULT STDMETHODCALLTYPE CreateList(ID3D12Device* d,UINT node,D3D12_COMMAND_LIST_TYPE type,ID3D12CommandAllocator* a,ID3D12PipelineState* p,REFIID iid,void** out) {
    HRESULT hr=Original<MakeList,12>(d)(d,node,type,a,p,iid,out);
    if(!Bypass() && SUCCEEDED(hr) && type==D3D12_COMMAND_LIST_TYPE_DIRECT && out && *out) {
        ComPtr<ID3D12GraphicsCommandList> l;if(SUCCEEDED(static_cast<IUnknown*>(*out)->QueryInterface(IID_PPV_ARGS(&l)))) {
            {std::unique_lock<std::shared_mutex> lock(commandsMutex);commands[l.Get()]=std::make_shared<Command>();++commandEpoch;}
            auto& s=State(l.Get());s.pso=p;s.target=IsTarget(p);HookCommand(l.Get());
        }
    }return hr;
}
using MakeQueue=HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*,const D3D12_COMMAND_QUEUE_DESC*,REFIID,void**);
HRESULT STDMETHODCALLTYPE CreateQueue(ID3D12Device* d,const D3D12_COMMAND_QUEUE_DESC* desc,REFIID iid,void** out) {
    HRESULT hr=Original<MakeQueue,8>(d)(d,desc,iid,out);
    if(!Bypass() && SUCCEEDED(hr) && desc && desc->Type==D3D12_COMMAND_LIST_TYPE_DIRECT && out && *out) {
        ComPtr<ID3D12CommandQueue> q;if(SUCCEEDED(static_cast<IUnknown*>(*out)->QueryInterface(IID_PPV_ARGS(&q))))Hook(q.Get(),10,reinterpret_cast<void*>(ExecuteLists));
    }return hr;
}

DWORD WINAPI ScaleStartup(void*) {
    const auto scaleStart=GetTickCount64();
    do {TryGameScalePresets();TryGameSharpness();
        if(grounded::viewport_scale::TryInstall()){
            if(grounded::viewport_scale::state==1)Log("Game viewport scale normalized: presets use actual temporal output; legacy borderless resolution multiplier removed");
            else if(grounded::viewport_scale::state==-1)Log("Game viewport scale refused: %s",grounded::viewport_scale::refusal);
        }
        if(grounded::scale_control::state.load()!=0 && grounded::sharpness_control::state.load()!=0 && grounded::viewport_scale::state.load()!=0)break;Sleep(5);}
    while(GetTickCount64()-scaleStart<500);
    return 0;
}
DWORD WINAPI Initialize(void*) {
    HMODULE pinned=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&Initialize),&pinned);
    wchar_t path[32768]{};GetModuleFileNameW(gModule,path,32768);directory=std::filesystem::path(path).parent_path();
    {   // Keep one previous log so long play does not grow the file without bound.
        const auto log=directory/L"GroundedDLSS-runtime.log";WIN32_FILE_ATTRIBUTE_DATA a{};
        if(GetFileAttributesExW(log.c_str(),GetFileExInfoStandard,&a) && (a.nFileSizeHigh || a.nFileSizeLow>4u*1024*1024))
            MoveFileExW(log.c_str(),(directory/L"GroundedDLSS-runtime.previous.log").c_str(),MOVEFILE_REPLACE_EXISTING);
    }
    Log("Grounded DLSS runtime v11.23.2 beta; own NGX ProjectID; direct NVIDIA NGX; standalone loader supported; time-based native autofocus; GPU-aware K/M/L presets; sharpening OFF by default; F8 toggles native/DLSS; no overlay");
    const auto sharpening=grounded::sharpness_control::ReadSettings((directory/L"GroundedDLSS.ini").c_str());
    grounded::sharpness_control::Configure(sharpening);
    grounded::focus_control::Configure((directory/L"GroundedDLSS.ini").c_str());
    grounded::focus_control::TryInstall();
    Log("Native focus timing state=%d; time constant=%.3f seconds; refusal=%s",grounded::focus_control::state.load(),grounded::focus_control::timeConstant,grounded::focus_control::refusal);
    Log("Per-mode sharpening settings: enabled=%u Quality=%.3f Balanced=%.3f Performance=%.3f DLAA=%.3f UltraPerformance=%.3f",unsigned(sharpening.enabled),sharpening.quality,sharpening.balanced,sharpening.performance,sharpening.dlaa,sharpening.ultraPerformance);
    const UINT bridgeRequest=GetPrivateProfileIntW(L"DLSS",L"NativeTemporalBridge",2,(directory/L"GroundedDLSS.ini").c_str());
    if((bridgeRequest==1 || bridgeRequest==2) && grounded::native_temporal::Install(AllowNativePass,nullptr))nativeBridgeMode=bridgeRequest;
#ifdef GROUNDED_NATIVE_BRIDGE_TEST
    if(GetPrivateProfileIntW(L"DLSS",L"NativeBridgeFixture",0,(directory/L"GroundedDLSS.ini").c_str()))nativeBridgeMode=2;
#endif
    Log("Native temporal bridge mode=%u; installed=%u; descriptor tracking=%s",nativeBridgeMode.load(),unsigned(grounded::native_temporal::installed.load()),nativeBridgeMode==2?"off":"on");
    if(bridgeRequest==2 && nativeBridgeMode!=2)Log("Native temporal bridge unavailable; using the descriptor-tracking fallback, which costs more CPU per frame");
    grounded::scale_control::Trace scaleTrace=nullptr;
    if(GetPrivateProfileIntW(L"DLSS",L"TraceScaleWrites",0,(directory/L"GroundedDLSS.ini").c_str()))
        scaleTrace=[](const char* source,double request,unsigned preset,uint32_t flags,uintptr_t caller,DWORD thread){
            const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
            Log("Scale write trace: %s request=%.8g canonical=%u flags=0x%x caller_rva=0x%llx thread=%lu",source,request,preset,flags,
                caller>=base && caller-base<grounded::scale_control::ImageSize?static_cast<unsigned long long>(caller-base):0,thread);
        };
    grounded::scale_control::Configure(GetPrivateProfileIntW(L"DLSS",L"SnapScalePresets",1,(directory/L"GroundedDLSS.ini").c_str())!=0,
        [](const char* source,double request,unsigned preset){Log("DLSS scale preset: %s %.4f%% -> %u%%",source,request,preset);},scaleTrace);
    grounded::viewport_scale::Configure(grounded::scale_control::state.load()!=3 && GetPrivateProfileIntW(L"DLSS",L"NormalizeViewportScale",1,(directory/L"GroundedDLSS.ini").c_str())!=0);
    // Console objects are registered by the executable's static initializers;
    // the scale controls must be installed before settings are applied, without
    // invoking UE from a worker. The D3D12 hooks must precede the game's first
    // temporal pipeline, root signature and upload mapping. Neither waits for
    // the other: the scale controls run on their own short-lived thread.
    HANDLE scaleThread=CreateThread(nullptr,0,ScaleStartup,nullptr,0,nullptr);
    if(!scaleThread)ScaleStartup(nullptr);
    ComPtr<IDXGIFactory4> factory;ComPtr<IDXGIAdapter1> selected;
    if(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))for(UINT i=0;;++i) {
        ComPtr<IDXGIAdapter1> a;if(factory->EnumAdapters1(i,&a)==DXGI_ERROR_NOT_FOUND)break;
        DXGI_ADAPTER_DESC1 desc{};a->GetDesc1(&desc);if(desc.VendorId==0x10de){selected=a;break;}
    }
    ComPtr<ID3D12Device> device;
    bool ok=selected && SUCCEEDED(D3D12CreateDevice(selected.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)));
    if(ok) {
        Internal guard;
        D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;desc.Width=65536;desc.Height=1;
        desc.DepthOrArraySize=1;desc.MipLevels=1;desc.SampleDesc.Count=1;desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ComPtr<ID3D12Resource> upload;
        ok=SUCCEEDED(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&upload))) && HookResource(upload.Get());
        ComPtr<ID3D12CommandAllocator> a;ComPtr<ID3D12GraphicsCommandList> l;ComPtr<ID3D12CommandQueue> q;
        device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&a));
        if(a)device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,a.Get(),nullptr,IID_PPV_ARGS(&l));
        D3D12_COMMAND_QUEUE_DESC qd{};qd.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;device->CreateCommandQueue(&qd,IID_PPV_ARGS(&q));
        ok=ok && l && q && HookCommand(l.Get()) && Hook(q.Get(),10,reinterpret_cast<void*>(ExecuteLists));
        ok=ok && Hook(device.Get(),8,reinterpret_cast<void*>(CreateQueue)) && Hook(device.Get(),11,reinterpret_cast<void*>(CreateCompute)) &&
            Hook(device.Get(),12,reinterpret_cast<void*>(CreateList)) && Hook(device.Get(),16,reinterpret_cast<void*>(MakeRoot));
        if(nativeBridgeMode!=2)ok=ok && Hook(device.Get(),14,reinterpret_cast<void*>(CreateHeap)) &&
            Hook(device.Get(),17,reinterpret_cast<void*>(MakeCBV)) && Hook(device.Get(),18,reinterpret_cast<void*>(MakeSRV)) &&
            Hook(device.Get(),19,reinterpret_cast<void*>(MakeUAV)) && Hook(device.Get(),23,reinterpret_cast<void*>(CopyDescriptors)) &&
            Hook(device.Get(),24,reinterpret_cast<void*>(CopySimple));
        ComPtr<ID3D12Device1> d1;ComPtr<ID3D12Device2> d2;
        if(SUCCEEDED(device.As(&d1))) {
            ComPtr<ID3D12PipelineLibrary> lib;if(SUCCEEDED(d1->CreatePipelineLibrary(nullptr,0,IID_PPV_ARGS(&lib))))ok=HookLibrary(lib.Get())&&ok;
            ok=Hook(d1.Get(),44,reinterpret_cast<void*>(CreateLibrary))&&ok;
        }
        if(SUCCEEDED(device.As(&d2)))ok=Hook(d2.Get(),47,reinterpret_cast<void*>(CreateStream)) && Hook(d2.Get(),44,reinterpret_cast<void*>(CreateLibrary)) && ok;
    }
    if(ok){
        std::filesystem::path driverPath;
        if(auto ngx=FindNGXModule()){wchar_t name[32768]{};if(GetModuleFileNameW(ngx,name,32768))driverPath=std::filesystem::path(name).parent_path();}
        localRuntime=grounded::local_dlss::Select(directory,grounded::local_dlss::CacheRoot(),driverPath);
        Log("Local NVIDIA DLSS discovery: version=%ls; source=%ls; feature directory=%ls; automatic=%u; warning=%ls",localRuntime.version.c_str(),localRuntime.source.c_str(),localRuntime.directory.c_str(),unsigned(localRuntime.automatic),localRuntime.warning.c_str());
    }
    // The main loop below also installs scale controls; never run both at once.
    if(scaleThread){WaitForSingleObject(scaleThread,INFINITE);CloseHandle(scaleThread);}
    ready=ok?1:-1;Log("Hooks ready=%s",ok?"true":"false");
    bool lastF8=false;ULONGLONG lastStatus=0,lastControl=0;UINT64 controlSequence=0;
#ifdef GROUNDED_FRAME_CAPTURE
    UINT64 captureSequence=0;ULONGLONG captureDeadline=0;std::string captureError;
    Log("One-frame texture capture diagnostic enabled; explicit PID-scoped request required; no continuous capture");
#endif
    for(;;) {
        DWORD foregroundPID=0;GetWindowThreadProcessId(GetForegroundWindow(),&foregroundPID);
        const auto key=GetAsyncKeyState(VK_F8);
        bool f8=foregroundPID==GetCurrentProcessId() && (key&0x8001)!=0;
        if(f8 && !lastF8) {
            std::lock_guard<std::mutex> lock(runtimeMutex);
            if(failed){Log("F8: DLSS unavailable after failure; restart required");MessageBeep(MB_ICONERROR);}
            else {enabled=!enabled.load();evaluated=false;historyDirty=true;grounded::sharpness_control::Publish(false,0);Log("F8: %s",enabled?"DLSS requested":"native TAAU");MessageBeep(MB_OK);}
        }
        lastF8=f8;
        if(GetTickCount64()-lastControl>=250) {
            TryGameScalePresets();
            {std::lock_guard<std::mutex> lock(runtimeMutex);TryGameSharpness();}
            // Local automation control: unique increasing sequence and mode 0/1.
            // Read only after startup; the default remains ON on every launch.
            std::ifstream control(directory/L"GroundedDLSS-control.txt");
            UINT64 sequence=0;int mode=-1,profileMode=0,legacyCache=0,treeCache=0;
            if(control>>sequence>>mode && sequence>controlSequence && (mode==0 || mode==1)) {
                control>>profileMode>>legacyCache>>treeCache;
                profiling=profileMode==1;legacyHeapCache=legacyCache==1;treeHeapCache=treeCache==1;
                controlSequence=sequence;
                std::lock_guard<std::mutex> lock(runtimeMutex);
                if(!failed) {enabled=mode!=0;evaluated=false;historyDirty=true;grounded::sharpness_control::Publish(false,0);Log("Local test control %llu: %s",sequence,mode?"DLSS requested":"native TAAU");}
            }
#ifdef GROUNDED_FRAME_CAPTURE
            {
                std::lock_guard<std::mutex> lock(runtimeMutex);Internal guard;
                std::ifstream request(directory/L"GroundedDLSS-frame-capture.txt");UINT64 number=0;DWORD pid=0;
                if(request>>number>>pid && number>captureSequence && pid==GetCurrentProcessId() && !frameCapture->Busy()) {
                    captureSequence=number;captureError.clear();
                    if(frameCapture->Arm(number)){
                        captureDeadline=GetTickCount64()+15000;captureArmed=true;
                        std::ofstream(directory/L"GroundedDLSS-frame-capture-status.txt")<<"pid="<<pid<<"\nsequence="<<number<<"\nphase=armed\n";
                    }
                }
                if(captureArmed && GetTickCount64()>captureDeadline){frameCapture->CancelArmed();captureArmed=false;}
                const int result=frameCapture->Poll(directory/L"GroundedDLSS-captures");
                if(result==1)std::ofstream(directory/L"GroundedDLSS-frame-capture-status.txt")<<"pid="<<GetCurrentProcessId()<<"\nsequence="<<captureSequence<<"\nphase=saved\n";
                if(!frameCapture->Error().empty() && captureError!=frameCapture->Error()){
                    captureError=frameCapture->Error();Log("Frame capture failed: %s",captureError.c_str());
                    std::ofstream(directory/L"GroundedDLSS-frame-capture-status.txt")<<"pid="<<GetCurrentProcessId()<<"\nsequence="<<captureSequence<<"\nphase=failed\nerror="<<captureError<<'\n';
                }
            }
#endif
            lastControl=GetTickCount64();
        }
        if(GetTickCount64()-lastStatus>=5000) {
            std::ofstream status(directory/L"GroundedDLSS-status.txt",std::ios::trunc);
            status<<"ready="<<ready<<"\nenabled="<<enabled<<"\nevaluated="<<evaluated<<"\nfailed="<<failed<<"\nreplacements="<<successes<<"\nnative_fallbacks="<<fallbacks<<"\nmatched_shader="<<candidates<<"\nprofiling="<<profiling<<"\nlegacy_heap_cache="<<legacyHeapCache<<"\nprofile_hot_sample_stride=1024\nprofile_submit_sample_stride=64\nprofile_dlss_sample_stride=1\n";
            status<<"render_preset_requested="<<renderPresetRequested.load()<<"\n";
            status<<"gpu_architecture="<<gpuArchitecture.load()<<"\n";
            status<<"focus_timing_state="<<grounded::focus_control::state.load()
                  <<"\nfocus_updates="<<grounded::focus_control::updates.load()
                  <<"\nfocus_interval_seconds="<<grounded::focus_control::lastInterval.load()
                  <<"\nfocus_coefficient="<<grounded::focus_control::lastCoefficient.load()
                  <<"\nfocus_time_constant_seconds="<<grounded::focus_control::timeConstant<<"\n";
            status<<"sharpening_state="<<grounded::sharpness_control::state.load()
                  <<"\nsharpening_mode="<<grounded::sharpness_control::controller.mode.load()
                  <<"\nsharpening_strength="<<grounded::sharpness_control::controller.strength.load()
                  <<"\nsharpening_changes="<<grounded::sharpness_control::controller.changes.load()<<"\n";
            status<<"status_tick_ms="<<GetTickCount64()<<"\nnative_passes="<<nativePasses.load()<<"\n";
            status<<"native_bridge_mode="<<nativeBridgeMode.load()<<"\nnative_scopes="<<nativeScopes.load()
                  <<"\nnative_compared="<<nativeCompared.load()<<"\nnative_mismatches="<<nativeMismatches.load()
                  <<"\nnative_missing="<<nativeMissing.load()<<"\nnative_rejected="<<nativeRejected.load()
                  <<"\nnative_tagged="<<grounded::native_temporal::tagged.load()<<"\nnative_executed="<<grounded::native_temporal::executed.load()
                  <<"\nnative_refused="<<grounded::native_temporal::refused.load()
                  <<"\nnative_unclassified="<<nativeUnclassified.load()
                  <<"\nnative_stale_packets="<<grounded::native_temporal::stale.load()<<"\n";
            status<<"scale_presets_state="<<grounded::scale_control::state.load()
                  <<"\nviewport_scale_state="<<grounded::viewport_scale::state.load()
                  <<"\nscale_requested="<<grounded::scale_control::requested.load()
                  <<"\nscale_effective="<<grounded::scale_control::effective.load()
                  <<"\nscale_corrections="<<grounded::scale_control::corrections.load()<<"\n";
            status<<"quality_mode="<<qualityMode.load()<<"\nfeature_generations="<<featureGenerations.load()
                  <<"\nactive_width="<<activeWidth.load()<<"\nactive_height="<<activeHeight.load()
                  <<"\noutput_width="<<outputWidth.load()<<"\noutput_height="<<outputHeight.load()<<"\n";
            status<<"tree_heap_cache="<<treeHeapCache<<"\n";
            {std::shared_lock<std::shared_mutex> lock(metadataMutex);UINT64 slots=0,storageCount=0;
                for(const auto& h:heaps)if(h.second.storage){slots+=h.second.count;++storageCount;}
                status<<"tracked_heaps="<<heaps.size()<<"\ndescriptor_slots="<<slots<<"\ndescriptor_views="<<descriptorViews.size()
                      <<"\ndescriptor_metadata_bytes="<<slots*sizeof(std::atomic<UINT64>)+storageCount*sizeof(DescriptorStorage)+descriptorViews.size()*sizeof(Descriptor)<<"\n";}
            LARGE_INTEGER frequency;QueryPerformanceFrequency(&frequency);
            for(UINT i=0;i<profileCounters.size();++i){const auto& p=profileCounters[i];
                status<<"profile_"<<i<<"_calls="<<p.calls<<"\nprofile_"<<i<<"_estimated_ms="<<(p.samples?double(p.ticks.load())*double(p.calls.load())/double(p.samples.load())*1000/frequency.QuadPart:0)<<"\n";}
            {std::lock_guard<std::mutex> lock(runtimeMutex);if(integration){auto p=integration->ReadGpuProfile();
                status<<"gpu_samples="<<p.samples<<"\ngpu_prepare_sum_ms="<<p.preparationMs<<"\ngpu_ngx_sum_ms="<<p.ngxMs<<"\ngpu_finish_sum_ms="<<p.finishMs<<"\n";}}
            lastStatus=GetTickCount64();
        }
        Sleep(10);
    }
}
}
extern "C" __declspec(dllexport) int __cdecl GroundedDLSSWaitReady(UINT ms) {
    auto start=GetTickCount64();while(!ready && GetTickCount64()-start<ms)Sleep(5);return ready;
}
extern "C" __declspec(dllexport) void __cdecl GroundedDLSSSetEnabled(int value) {
    std::lock_guard<std::mutex> lock(runtimeMutex);
    if(!failed && enabled.exchange(value!=0)!=(value!=0)){evaluated=false;historyDirty=true;grounded::sharpness_control::Publish(false,0);}
}
extern "C" __declspec(dllexport) UINT64 __cdecl GroundedDLSSReplacements() {return successes.load();}
extern "C" __declspec(dllexport) UINT __cdecl GroundedDLSSRenderPreset() {return renderPresetRequested.load();}
extern "C" __declspec(dllexport) UINT __cdecl GroundedDLSSGpuArchitecture() {return gpuArchitecture.load();}
#ifdef GROUNDED_NATIVE_BRIDGE_TEST
// Isolated GPU fixture only. These entry points are absent from the game ASI.
extern "C" __declspec(dllexport) int __cdecl GroundedDLSSBindSharpnessFixture(void* volatile* slot,void* baseline) {
    std::lock_guard<std::mutex> lock(runtimeMutex);
    if(!grounded::sharpness_control::controller.Bind(slot,baseline))return 0;
    grounded::sharpness_control::state=1;return 1;
}
thread_local grounded::native_temporal::Frame nativeFixture;
extern "C" __declspec(dllexport) int __cdecl GroundedDLSSBeginNativeFixture(ID3D12Resource* const* textures,UINT x,UINT y,UINT z) {
    if(!textures || grounded::native_temporal::current || nativeBridgeMode!=2)return 0;
    nativeFixture={};nativeFixture.groups={x,y,z};
    for(UINT i=0;i<5;++i){if(!textures[i])return 0;nativeFixture.resources[i]=textures[i];nativeFixture.descriptions[i]=textures[i]->GetDesc();}
    nativeFixture.resourcesValid=true;grounded::native_temporal::current=&nativeFixture;++nativeScopes;return 1;
}
extern "C" __declspec(dllexport) void __cdecl GroundedDLSSEndNativeFixture() {
    grounded::native_temporal::current=nullptr;nativeFixture={};
}
#endif
extern "C" __declspec(dllexport) UINT64 __cdecl GroundedDLSSDescriptorViews() {
    std::shared_lock<std::shared_mutex> lock(metadataMutex);return descriptorViews.size();
}
extern "C" __declspec(dllexport) void __cdecl GroundedDLSSSetProfiling(int value) {profiling=value!=0;}
extern "C" __declspec(dllexport) void __cdecl GroundedDLSSSetTreeHeapCache(int value) {treeHeapCache=value!=0;}
extern "C" __declspec(dllexport) int __cdecl GroundedDLSSGetGpuProfile(grounded::Integration::GpuProfile* output) {
    if(!output)return 0;std::lock_guard<std::mutex> lock(runtimeMutex);
    if(!integration)return 0;*output=integration->ReadGpuProfile();return 1;
}
BOOL APIENTRY DllMain(HMODULE module,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) {gModule=module;DisableThreadLibraryCalls(module);HANDLE thread=CreateThread(nullptr,0,Initialize,nullptr,0,nullptr);if(thread)CloseHandle(thread);}
    else if(reason==DLL_PROCESS_DETACH)terminating.store(true,std::memory_order_release);
    return TRUE;
}
