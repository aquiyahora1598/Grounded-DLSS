#include "integration.h"
#include "prepare_shader.h"
#ifdef GROUNDED_FSR_FRAMEGEN
#include "../framegen/fsr_runtime.h"
#endif
#include "gpu_identity.h"
#include "preset_policy.h"
#include "scale_presets.h"
#include "../../external/nvidia/nvsdk_ngx.h"
#include <d3dcompiler.h>
#include <algorithm>
#include <array>
#include <vector>
#include <sstream>
#include <atomic>
#include <cstring>

namespace grounded {
using Microsoft::WRL::ComPtr;
namespace {
#ifdef GROUNDED_INTEGRATION_TEST
std::atomic<bool> nvidiaEvaluationObserved{false};
#endif
// NGX Core export ABI: SDK version precedes feature info (unlike the public
// SDK wrapper named Init_with_ProjectID). Never substitute another title's ID.
using InitProject = NVSDK_NGX_Result(NVSDK_CONV*)(const char*, NVSDK_NGX_EngineType, const char*, const wchar_t*, ID3D12Device*, NVSDK_NGX_Version, const NVSDK_NGX_FeatureCommonInfo*);
constexpr char ProjectId[] = "d91874f0-6893-4ba7-ae47-a9b3384f7530";
using Parameters = NVSDK_NGX_Result(NVSDK_CONV*)(NVSDK_NGX_Parameter**);
using DestroyParameters = NVSDK_NGX_Result(NVSDK_CONV*)(NVSDK_NGX_Parameter*);
using Create = NVSDK_NGX_Result(NVSDK_CONV*)(ID3D12GraphicsCommandList*, NVSDK_NGX_Feature, NVSDK_NGX_Parameter*, NVSDK_NGX_Handle**);
using Evaluate = NVSDK_NGX_Result(NVSDK_CONV*)(ID3D12GraphicsCommandList*, const NVSDK_NGX_Handle*, const NVSDK_NGX_Parameter*, void*);
using ReleaseFeature = NVSDK_NGX_Result(NVSDK_CONV*)(NVSDK_NGX_Handle*);
D3D12_RESOURCE_BARRIER Transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER b{}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition = {resource, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, before, after}; return b;
}
ComPtr<ID3D12Resource> Texture(ID3D12Device* device, UINT w, UINT h, DXGI_FORMAT format) {
    D3D12_HEAP_PROPERTIES heap{}; heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC desc{}; desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = w; desc.Height = h; desc.DepthOrArraySize = 1; desc.MipLevels = 1; desc.Format = format;
    desc.SampleDesc.Count = 1; desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    ComPtr<ID3D12Resource> result;
    device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&result));
    return result;
}
bool Shape(ID3D12Resource* r, UINT w, UINT h, DXGI_FORMAT format, bool uav = false) {
    if (!r) return false;
    const auto d = r->GetDesc();
    return d.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && d.Width == w && d.Height == h &&
        d.Format == format && d.DepthOrArraySize == 1 && d.MipLevels == 1 && d.SampleDesc.Count == 1 &&
        (!uav || (d.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS));
}
}

struct Integration::Impl {
    struct Frame {
        ComPtr<ID3D12Resource> depth, velocity, color, inputColor, exposure, constants;
        ComPtr<ID3D12DescriptorHeap> heap;
        std::array<ComPtr<ID3D12Resource>,5> retained;
        ID3D12CommandList* recording = nullptr;
        UINT64 fence = 0;
        bool featureCreation = false;
        unsigned char* mapped = nullptr;
        ComPtr<ID3D12QueryHeap> timestamps;
        ComPtr<ID3D12Resource> timestampReadback;
        const UINT64* timestampValues=nullptr;
        UINT64 timestampFrequency=0;
        bool timestampPending=false;
    };
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12RootSignature> root;
    ComPtr<ID3D12PipelineState> prepare, finish;
    ComPtr<ID3D12Fence> fence;
    ComPtr<ID3D12CommandQueue> lastQueue;
    ID3D12CommandList* creationList = nullptr;
    UINT64 lastSignal = 0, evaluations = 0;
    UINT width = 0, height = 0, outputWidth = 0, outputHeight = 0, step = 0;
    UINT activeWidth = 0, activeHeight = 0;
    NVSDK_NGX_Parameter* capabilities = nullptr;
    NVSDK_NGX_Parameter* parameters = nullptr;
    NVSDK_NGX_Handle* feature = nullptr;
    Create create = nullptr;
    Evaluate evaluate = nullptr;
    ReleaseFeature release = nullptr;
    DestroyParameters destroy = nullptr;
    std::vector<std::unique_ptr<Frame>> frames;
    std::string error;
    bool resetNext = true, disabled = false;
    bool profiling=false;
    UINT64 profileCandidates=0;
    GpuProfile gpuProfile;
    float jitterSign = 1;
    UINT renderPreset = 0; // 0: automatic policy, otherwise explicit override.
    GpuIdentity gpu;
    struct Settings {
        NVSDK_NGX_PerfQuality_Value mode = NVSDK_NGX_PerfQuality_Value_MaxQuality;
        UINT width = 0, height = 0,renderWidth=0,renderHeight=0;
    } settings;
    UINT64 featureGenerations = 0;

    bool SelectSettings(const Constants& c, Settings& selected) {
        // Follow the actual viewport, not a second INI percentage. Capability
        // queries are separate from parameters used by recorded evaluations.
        void* callback = nullptr;
        if (!capabilities || NVSDK_NGX_FAILED(capabilities->Get(NVSDK_NGX_Parameter_DLSSOptimalSettingsCallback,&callback)) || !callback)
            return Fail("DLSS optimal-settings query unavailable");
        const bool full = c.activeWidth == c.outputWidth && c.activeHeight == c.outputHeight;
        const bool ultra=double(c.activeWidth)/c.outputWidth<0.416667 && double(c.activeHeight)/c.outputHeight<0.416667;
        double best = 1e10;
        for (auto mode : {NVSDK_NGX_PerfQuality_Value_MaxQuality, NVSDK_NGX_PerfQuality_Value_Balanced,
                          NVSDK_NGX_PerfQuality_Value_MaxPerf, NVSDK_NGX_PerfQuality_Value_UltraPerformance,
                          NVSDK_NGX_PerfQuality_Value_DLAA}) {
            if (full != (mode == NVSDK_NGX_PerfQuality_Value_DLAA)) continue;
            if(ultra != (mode==NVSDK_NGX_PerfQuality_Value_UltraPerformance))continue;
            capabilities->Set(NVSDK_NGX_Parameter_Width,c.outputWidth);
            capabilities->Set(NVSDK_NGX_Parameter_Height,c.outputHeight);
            capabilities->Set(NVSDK_NGX_Parameter_PerfQualityValue,int(mode));
            capabilities->Set(NVSDK_NGX_Parameter_RTXValue,0);
            auto result = reinterpret_cast<PFN_NVSDK_NGX_DLSS_GetOptimalSettingsCallback>(callback)(capabilities);
            UINT w=0,h=0;
            if (NVSDK_NGX_FAILED(result) || NVSDK_NGX_FAILED(capabilities->Get(NVSDK_NGX_Parameter_OutWidth,&w)) ||
                NVSDK_NGX_FAILED(capabilities->Get(NVSDK_NGX_Parameter_OutHeight,&h)) || !w || !h) continue;
            UINT minW=w,minH=h,maxW=w,maxH=h;
            capabilities->Get(NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Min_Render_Width,&minW);
            capabilities->Get(NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Min_Render_Height,&minH);
            capabilities->Get(NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Max_Render_Width,&maxW);
            capabilities->Get(NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Max_Render_Height,&maxH);
            UINT renderW=c.activeWidth,renderH=c.activeHeight;
            if(mode==NVSDK_NGX_PerfQuality_Value_UltraPerformance &&
               std::abs(int(c.activeWidth)-int(w))<=1 && std::abs(int(c.activeHeight)-int(h))<=1) {
                // UE rounds its viewport up; Ultra has a fixed NGX input size.
                // Resample the complete viewport in our existing preparation
                // dispatch when these differ by one pixel; never crop an edge.
                renderW=w;renderH=h;
            }
            if(renderW<minW || renderH<minH || renderW>maxW || renderH>maxH || renderW>c.width || renderH>c.height)continue;
            const double distance = std::abs(double(c.activeWidth)-w)/c.outputWidth +
                                    std::abs(double(c.activeHeight)-h)/c.outputHeight;
            if (distance<best) { best=distance;selected={mode,w,h,renderW,renderH}; }
        }
        return best<1e10 || Fail("Render scale outside supported DLSS ranges");
    }

    bool Fail(const char* context, UINT value = 0) {
        std::ostringstream s; s << context << " (0x" << std::hex << value << ')'; error = s.str(); return false;
    }
    bool Idle(const Frame& f) const { return !f.recording && (!f.fence || fence->GetCompletedValue() >= f.fence); }
    bool AllIdle() const { return std::all_of(frames.begin(), frames.end(), [&](const auto& f) { return Idle(*f); }); }
    void HarvestTimestamps() {
        const auto completed=fence?fence->GetCompletedValue():0;
        for(auto& f:frames)if(f->timestampPending && f->fence && completed>=f->fence && completed!=UINT64_MAX) {
            const auto* t=f->timestampValues;
            if(f->timestampFrequency && t && t[1]>=t[0] && t[2]>=t[1] && t[3]>=t[2]) {
                const double ms=1000.0/double(f->timestampFrequency);
                ++gpuProfile.samples;gpuProfile.preparationMs+=double(t[1]-t[0])*ms;
                gpuProfile.ngxMs+=double(t[2]-t[1])*ms;gpuProfile.finishMs+=double(t[3]-t[2])*ms;
            }
            f->timestampPending=false;
        }
    }
    bool MakeTimestamps(Frame& f) {
        if(f.timestamps && f.timestampValues)return true;
        f.timestamps.Reset();f.timestampReadback.Reset();f.timestampValues=nullptr;
        D3D12_QUERY_HEAP_DESC q{};q.Type=D3D12_QUERY_HEAP_TYPE_TIMESTAMP;q.Count=4;
        if(FAILED(device->CreateQueryHeap(&q,IID_PPV_ARGS(&f.timestamps))))return false;
        D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=4*sizeof(UINT64);
        d.Height=1;d.DepthOrArraySize=d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if(FAILED(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&f.timestampReadback))))return false;
        D3D12_RANGE read{0,4*sizeof(UINT64)};void* data=nullptr;
        if(FAILED(f.timestampReadback->Map(0,&read,&data)))return false;
        f.timestampValues=static_cast<const UINT64*>(data);return true;
    }
    bool MakePipeline(const char* entry, ComPtr<ID3D12PipelineState>& output) {
        ComPtr<ID3DBlob> code, diagnostics;
        HRESULT hr = D3DCompile(PrepareShader, sizeof(PrepareShader)-1, "GroundedPrepare", nullptr, nullptr, entry, "cs_5_0",
                               D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_WARNINGS_ARE_ERRORS, 0, &code, &diagnostics);
        if (FAILED(hr)) { error = diagnostics ? static_cast<const char*>(diagnostics->GetBufferPointer()) : "Shader compilation failed"; return false; }
        D3D12_COMPUTE_PIPELINE_STATE_DESC desc{}; desc.pRootSignature = root.Get();
        desc.CS = {code->GetBufferPointer(), code->GetBufferSize()};
        hr = device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&output));
        return SUCCEEDED(hr) || Fail("Create preparation pipeline", UINT(hr));
    }
    std::unique_ptr<Frame> MakeFrame() {
        auto f = std::make_unique<Frame>();
        f->depth = Texture(device.Get(), width, height, DXGI_FORMAT_R32_FLOAT);
        f->velocity = Texture(device.Get(), width, height, DXGI_FORMAT_R32G32_FLOAT);
        f->color = Texture(device.Get(), outputWidth, outputHeight, DXGI_FORMAT_R16G16B16A16_FLOAT);
        f->inputColor = Texture(device.Get(), width, height, DXGI_FORMAT_R16G16B16A16_FLOAT);
        f->exposure = Texture(device.Get(), 1, 1, DXGI_FORMAT_R32_FLOAT);
        D3D12_DESCRIPTOR_HEAP_DESC heap{}; heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heap.NumDescriptors = 10; heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (!f->depth || !f->velocity || !f->color || !f->inputColor || !f->exposure || FAILED(device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&f->heap)))) return {};
        D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC d{}; d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; d.Width = 3072;
        d.Height = 1; d.DepthOrArraySize = 1; d.MipLevels = 1; d.SampleDesc.Count = 1; d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if (FAILED(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&f->constants)))) return {};
        D3D12_RANGE empty{};
        if (FAILED(f->constants->Map(0, &empty, reinterpret_cast<void**>(&f->mapped)))) return {};
        return f;
    }
    void Bind(ID3D12GraphicsCommandList* list, Frame& f, ID3D12PipelineState* pipeline) {
        list->SetPipelineState(pipeline); list->SetComputeRootSignature(root.Get());
        ID3D12DescriptorHeap* heaps[] = {f.heap.Get()}; list->SetDescriptorHeaps(1, heaps);
        auto base = f.heap->GetGPUDescriptorHandleForHeapStart();
        list->SetComputeRootDescriptorTable(0, base); base.ptr += UINT64(step) * 5;
        list->SetComputeRootDescriptorTable(1, base);
        list->SetComputeRootConstantBufferView(2, f.constants->GetGPUVirtualAddress());
        list->SetComputeRootConstantBufferView(3, f.constants->GetGPUVirtualAddress() + 512);
    }
};
Integration::Integration() : impl(std::make_unique<Impl>()) {}
Integration::~Integration() {
    // Production Integration is process-resident. Standalone tests destroy it only
    // after discarding recordings and waiting for the submitted fence.
    if (impl->AllIdle()) {
        if (impl->feature && impl->release) impl->release(impl->feature);
        if (impl->parameters && impl->destroy) impl->destroy(impl->parameters);
        if (impl->capabilities && impl->destroy) impl->destroy(impl->capabilities);
    } else {
        // Retain resources rather than freeing GPU work still in flight on teardown.
        (void)impl.release();
    }
}
bool Integration::Initialize(ID3D12Device* device, HMODULE module, const std::filesystem::path& dataDirectory,
                             const std::filesystem::path& featureDirectory) {
    auto& s = *impl;
    s.device = device;
    s.gpu=IdentifyGpu(device);
    // Read once, before feature creation. Changing an existing NGX feature
    // while its recorded work is in flight would require retiring it first.
    const auto presetFile = dataDirectory.parent_path()/L"GroundedDLSS.ini";
    const UINT requested = GetPrivateProfileIntW(L"DLSS",L"RenderPreset",0,presetFile.c_str());
    if(ValidPresetOverride(requested))s.renderPreset=requested;
    auto init = reinterpret_cast<InitProject>(GetProcAddress(module, "NVSDK_NGX_D3D12_Init_ProjectID"));
    auto getParameters = reinterpret_cast<Parameters>(GetProcAddress(module, "NVSDK_NGX_D3D12_GetCapabilityParameters"));
    auto allocate = reinterpret_cast<Parameters>(GetProcAddress(module, "NVSDK_NGX_D3D12_AllocateParameters"));
    s.create = reinterpret_cast<Create>(GetProcAddress(module, "NVSDK_NGX_D3D12_CreateFeature"));
    s.evaluate = reinterpret_cast<Evaluate>(GetProcAddress(module, "NVSDK_NGX_D3D12_EvaluateFeature"));
    s.release = reinterpret_cast<ReleaseFeature>(GetProcAddress(module, "NVSDK_NGX_D3D12_ReleaseFeature"));
    s.destroy = reinterpret_cast<DestroyParameters>(GetProcAddress(module, "NVSDK_NGX_D3D12_DestroyParameters"));
    if (!init || !getParameters || !allocate || !s.create || !s.evaluate || !s.release || !s.destroy) return s.Fail("Missing NGX exports");
    std::filesystem::create_directories(dataDirectory);
    const auto search = featureDirectory.wstring(); const wchar_t* paths[] = {search.c_str()};
    NVSDK_NGX_FeatureCommonInfo info{}; info.PathListInfo.Path = paths; info.PathListInfo.Length = 1;
    info.LoggingInfo.MinimumLoggingLevel = NVSDK_NGX_LOGGING_LEVEL_OFF;
#ifdef GROUNDED_INTEGRATION_TEST
    info.LoggingInfo.MinimumLoggingLevel = NVSDK_NGX_LOGGING_LEVEL_VERBOSE;
    info.LoggingInfo.LoggingCallback = [](const char* message, NVSDK_NGX_Logging_Level, NVSDK_NGX_Feature component) {
        if(component == NVSDK_NGX_Feature_SuperSampling && strstr(message,"NgxDltss::EvaluateDltss")) nvidiaEvaluationObserved=true;
        fprintf(stderr,"NGX[%d]: %s\n", int(component), message);
    };
#endif
    // Own stable GUID, using CUSTOM for this independent rendering adapter.
    // No assigned numeric NVIDIA application ID is claimed or borrowed.
    auto result = init(ProjectId, NVSDK_NGX_ENGINE_TYPE_CUSTOM, "11.23.2", dataDirectory.c_str(), device, NVSDK_NGX_Version_API, &info);
    if (NVSDK_NGX_FAILED(result)) return s.Fail("NGX initialization", UINT(result));
    result = getParameters(&s.capabilities);
    if (NVSDK_NGX_FAILED(result) || !s.capabilities) return s.Fail("NGX capability parameters", UINT(result));
    int available = 0;
    result = s.capabilities->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &available);
    if (NVSDK_NGX_FAILED(result) || !available) return s.Fail("DLSS unavailable", UINT(result));
    result=allocate(&s.parameters);
    if(NVSDK_NGX_FAILED(result) || !s.parameters)return s.Fail("NGX evaluation parameters",UINT(result));
    s.step = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_DESCRIPTOR_RANGE ranges[2]{};
    ranges[0] = {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 0, 0, 0};
    ranges[1] = {D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 5, 0, 0, 0};
    D3D12_ROOT_PARAMETER params[4]{};
    for (UINT i = 0; i < 2; ++i) { params[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; params[i].DescriptorTable = {1, &ranges[i]}; }
    for (UINT i = 2; i < 4; ++i) { params[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; params[i].Descriptor = {i-2, 0}; }
    D3D12_ROOT_SIGNATURE_DESC desc{}; desc.NumParameters = 4; desc.pParameters = params;
    ComPtr<ID3DBlob> blob, diagnostics;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &diagnostics);
    if (SUCCEEDED(hr)) hr = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&s.root));
    if (FAILED(hr)) return s.Fail("Preparation root signature", UINT(hr));
    if (FAILED(hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&s.fence)))) return s.Fail("GPU fence", UINT(hr));
    return s.MakePipeline("Prepare", s.prepare) && s.MakePipeline("Finish", s.finish);
}

bool Integration::Record(ID3D12GraphicsCommandList* list, const PassInput& input, bool reset) {
    auto& s = *impl;
    s.HarvestTimestamps();
    if (s.disabled || !s.parameters) return s.Fail("DLSS disabled after runtime failure");
    if(s.creationList && s.creationList!=list){s.resetNext=true;return s.Fail("Waiting for first feature submission");}
    const auto& c = input.constants;
    if (!Shape(input.color,c.width,c.height,DXGI_FORMAT_R16G16B16A16_FLOAT) ||
        !Shape(input.depth,c.width,c.height,DXGI_FORMAT_R32G8X24_TYPELESS) ||
        !Shape(input.velocity,c.width,c.height,DXGI_FORMAT_R16G16B16A16_UNORM) ||
        !Shape(input.exposure,1,1,DXGI_FORMAT_R32G32B32A32_FLOAT) ||
        !Shape(input.output,c.outputWidth,c.outputHeight,DXGI_FORMAT_R16G16B16A16_FLOAT,true) ||
        input.color == input.output || input.depthView.Format != DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS ||
        input.velocityView.Format != DXGI_FORMAT_R16G16B16A16_UNORM) return s.Fail("Resource contract mismatch");
    bool different = s.width != c.width || s.height != c.height || s.outputWidth != c.outputWidth || s.outputHeight != c.outputHeight ||
                     s.activeWidth != c.activeWidth || s.activeHeight != c.activeHeight;
    if (different || !s.feature) {
        if (!s.AllIdle()) { s.resetNext = true; return s.Fail("Waiting for GPU before resolution change"); }
        Impl::Settings selected;
        if (!s.SelectSettings(c,selected)) { s.resetNext=true;return false; }
        if (s.feature) { s.release(s.feature); s.feature = nullptr; }
        s.frames.clear(); s.width=c.width; s.height=c.height; s.outputWidth=c.outputWidth; s.outputHeight=c.outputHeight; s.resetNext=true;
        s.activeWidth=c.activeWidth;s.activeHeight=c.activeHeight;s.settings=selected;
    }
    Impl::Frame* frame = nullptr;
    for (auto& f : s.frames) if (s.Idle(*f)) { frame=f.get(); break; }
    if (!frame && s.frames.size() < 8) {
        auto f = s.MakeFrame(); if (f) { frame=f.get(); s.frames.push_back(std::move(f)); }
    }
    if (!frame) { s.resetNext=true; return s.Fail("No retired GPU frame available"); }
    frame->recording=list; frame->fence=0;
    frame->retained = {input.color,input.depth,input.velocity,input.exposure,input.output};
    memcpy(frame->mapped, c.pass.data(), sizeof(c.pass)); memcpy(frame->mapped+512, c.view.data(), sizeof(c.view));
    // Private copy for PrepareShader only; the engine's CBVs stay unchanged.
    auto preparation= reinterpret_cast<float*>(frame->mapped);
    preparation[0]=float(s.settings.renderWidth);preparation[1]=float(s.settings.renderHeight);
    auto cpu=frame->heap->GetCPUDescriptorHandleForHeapStart();
    s.device->CreateShaderResourceView(input.depth,&input.depthView,cpu); cpu.ptr+=s.step;
    s.device->CreateShaderResourceView(input.velocity,&input.velocityView,cpu); cpu.ptr+=s.step;
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{}; srv.Format=DXGI_FORMAT_R16G16B16A16_FLOAT; srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; srv.Texture2D.MipLevels=1;
    s.device->CreateShaderResourceView(frame->color.Get(),&srv,cpu); cpu.ptr+=s.step;
    s.device->CreateShaderResourceView(input.color,&srv,cpu);cpu.ptr+=s.step;
    srv.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;s.device->CreateShaderResourceView(input.exposure,&srv,cpu);cpu.ptr+=s.step;
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav{}; uav.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;
    uav.Format=DXGI_FORMAT_R32_FLOAT; s.device->CreateUnorderedAccessView(frame->depth.Get(),nullptr,&uav,cpu); cpu.ptr+=s.step;
    uav.Format=DXGI_FORMAT_R32G32_FLOAT; s.device->CreateUnorderedAccessView(frame->velocity.Get(),nullptr,&uav,cpu); cpu.ptr+=s.step;
    uav.Format=DXGI_FORMAT_R16G16B16A16_FLOAT; s.device->CreateUnorderedAccessView(input.output,nullptr,&uav,cpu);cpu.ptr+=s.step;
    s.device->CreateUnorderedAccessView(frame->inputColor.Get(),nullptr,&uav,cpu);cpu.ptr+=s.step;
    uav.Format=DXGI_FORMAT_R32_FLOAT;s.device->CreateUnorderedAccessView(frame->exposure.Get(),nullptr,&uav,cpu);

    // Four GPU timestamps on one stable frame in 64. Query readback uses the
    // existing submission fence; profiling adds no CPU or GPU fence waits.
    const bool timed=s.profiling && s.feature && ((s.profileCandidates++&63)==0) && s.MakeTimestamps(*frame);
    frame->timestampPending=false;frame->timestampFrequency=0;
    if(timed)list->EndQuery(frame->timestamps.Get(),D3D12_QUERY_TYPE_TIMESTAMP,0);
    s.Bind(list,*frame,s.prepare.Get()); list->Dispatch((c.width+7)/8,(c.height+7)/8,1);
    D3D12_RESOURCE_BARRIER barriers[] = {
        Transition(frame->depth.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
        Transition(frame->velocity.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
        Transition(frame->inputColor.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
        Transition(frame->exposure.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)};
    list->ResourceBarrier(4,barriers);
    auto* p=s.parameters;
    if (!s.feature) {
        p->Set(NVSDK_NGX_Parameter_Width,s.settings.width); p->Set(NVSDK_NGX_Parameter_Height,s.settings.height);
        p->Set(NVSDK_NGX_Parameter_OutWidth,c.outputWidth); p->Set(NVSDK_NGX_Parameter_OutHeight,c.outputHeight);
        p->Set(NVSDK_NGX_Parameter_PerfQualityValue,int(s.settings.mode));
        const std::pair<const char*,unsigned> hints[]={
            {NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Quality,2},
            {NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Balanced,1},
            {NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Performance,0},
            {NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_UltraPerformance,3},
            {NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_DLAA,5}};
        for(const auto& hint:hints)p->Set(hint.first,RecommendedRenderPreset(hint.second,s.gpu.architecture,s.renderPreset));
        p->Set(NVSDK_NGX_Parameter_DLSS_Feature_Create_Flags,int(NVSDK_NGX_DLSS_Feature_Flags_IsHDR |
               NVSDK_NGX_DLSS_Feature_Flags_MVLowRes | NVSDK_NGX_DLSS_Feature_Flags_DepthInverted));
        p->Set(NVSDK_NGX_Parameter_CreationNodeMask,1u); p->Set(NVSDK_NGX_Parameter_VisibilityNodeMask,1u);
        p->Set(NVSDK_NGX_Parameter_DLSS_Enable_Output_Subrects,0);
        auto result=s.create(list,NVSDK_NGX_Feature_SuperSampling,p,&s.feature);
        frame->featureCreation=s.feature!=nullptr;
        if(s.feature)s.creationList=list;
        if (NVSDK_NGX_FAILED(result) || !s.feature) {
            for (auto& b:barriers) std::swap(b.Transition.StateBefore,b.Transition.StateAfter);
            list->ResourceBarrier(4,barriers); s.disabled=true; return s.Fail("NGX CreateFeature",UINT(result));
        }
        ++s.featureGenerations;
    }
    p->Set(NVSDK_NGX_Parameter_Color,frame->inputColor.Get()); p->Set(NVSDK_NGX_Parameter_Depth,frame->depth.Get());
    p->Set(NVSDK_NGX_Parameter_MotionVectors,frame->velocity.Get()); p->Set(NVSDK_NGX_Parameter_Output,frame->color.Get());
    p->Set(NVSDK_NGX_Parameter_ExposureTexture,frame->exposure.Get());
    p->Set(NVSDK_NGX_Parameter_Jitter_Offset_X,s.jitterSign*c.jitterX*s.settings.renderWidth/c.activeWidth);
    p->Set(NVSDK_NGX_Parameter_Jitter_Offset_Y,s.jitterSign*c.jitterY*s.settings.renderHeight/c.activeHeight);
    p->Set(NVSDK_NGX_Parameter_MV_Scale_X,1.0f); p->Set(NVSDK_NGX_Parameter_MV_Scale_Y,1.0f);
    p->Set(NVSDK_NGX_Parameter_DLSS_Pre_Exposure,1.0f); p->Set(NVSDK_NGX_Parameter_DLSS_Exposure_Scale,1.0f);
    p->Set(NVSDK_NGX_Parameter_Sharpness,0.0f);
    p->Set(NVSDK_NGX_Parameter_FrameTimeDeltaInMsec,16.666667f);
    p->Set(NVSDK_NGX_Parameter_TonemapperType,0u);
    p->Set(NVSDK_NGX_Parameter_DLSS_Indicator_Invert_X_Axis,0);
    p->Set(NVSDK_NGX_Parameter_DLSS_Indicator_Invert_Y_Axis,0);
    for(const char* name:{NVSDK_NGX_Parameter_TransparencyMask,NVSDK_NGX_Parameter_DLSS_Input_Bias_Current_Color_Mask,
        NVSDK_NGX_Parameter_GBuffer_Albedo,NVSDK_NGX_Parameter_GBuffer_Roughness,NVSDK_NGX_Parameter_GBuffer_Metallic,
        NVSDK_NGX_Parameter_GBuffer_Specular,NVSDK_NGX_Parameter_GBuffer_Subsurface,NVSDK_NGX_Parameter_GBuffer_Normals,
        NVSDK_NGX_Parameter_GBuffer_ShadingModelId,NVSDK_NGX_Parameter_GBuffer_MaterialId,NVSDK_NGX_Parameter_GBuffer_SpecularAlbedo,
        NVSDK_NGX_Parameter_GBuffer_IndirectAlbedo,NVSDK_NGX_Parameter_GBuffer_SpecularMvec,NVSDK_NGX_Parameter_GBuffer_DisocclusionMask,
        "GBuffer.Emissive",NVSDK_NGX_Parameter_GBuffer_Atrrib_8,NVSDK_NGX_Parameter_GBuffer_Atrrib_9,
        NVSDK_NGX_Parameter_GBuffer_Atrrib_10,NVSDK_NGX_Parameter_GBuffer_Atrrib_11,NVSDK_NGX_Parameter_GBuffer_Atrrib_12,
        NVSDK_NGX_Parameter_GBuffer_Atrrib_13,NVSDK_NGX_Parameter_GBuffer_Atrrib_14,NVSDK_NGX_Parameter_GBuffer_Atrrib_15,
        NVSDK_NGX_Parameter_MotionVectors3D,NVSDK_NGX_Parameter_IsParticleMask,NVSDK_NGX_Parameter_AnimatedTextureMask,
        NVSDK_NGX_Parameter_DepthHighRes,NVSDK_NGX_Parameter_Position_ViewSpace,NVSDK_NGX_Parameter_RayTracingHitDistance,
        NVSDK_NGX_Parameter_MotionVectorsReflection})p->Set(name,static_cast<ID3D12Resource*>(nullptr));
    p->Set(NVSDK_NGX_Parameter_Reset,int(reset || s.resetNext));
    p->Set(NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Width,s.settings.renderWidth);
    p->Set(NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Height,s.settings.renderHeight);
    for (const char* name : {NVSDK_NGX_Parameter_DLSS_Input_Color_Subrect_Base_X,NVSDK_NGX_Parameter_DLSS_Input_Color_Subrect_Base_Y,
                            NVSDK_NGX_Parameter_DLSS_Input_Depth_Subrect_Base_X,NVSDK_NGX_Parameter_DLSS_Input_Depth_Subrect_Base_Y,
                            NVSDK_NGX_Parameter_DLSS_Input_MV_SubrectBase_X,NVSDK_NGX_Parameter_DLSS_Input_MV_SubrectBase_Y,
                            NVSDK_NGX_Parameter_DLSS_Input_Translucency_SubrectBase_X,NVSDK_NGX_Parameter_DLSS_Input_Translucency_SubrectBase_Y,
                            NVSDK_NGX_Parameter_DLSS_Input_Bias_Current_Color_SubrectBase_X,NVSDK_NGX_Parameter_DLSS_Input_Bias_Current_Color_SubrectBase_Y,
                            NVSDK_NGX_Parameter_DLSS_Output_Subrect_Base_X,NVSDK_NGX_Parameter_DLSS_Output_Subrect_Base_Y}) p->Set(name,0u);
    if(timed)list->EndQuery(frame->timestamps.Get(),D3D12_QUERY_TYPE_TIMESTAMP,1);
    auto result=s.evaluate(list,s.feature,p,nullptr);
    if(timed)list->EndQuery(frame->timestamps.Get(),D3D12_QUERY_TYPE_TIMESTAMP,2);
    for (auto& b:barriers) std::swap(b.Transition.StateBefore,b.Transition.StateAfter);
    list->ResourceBarrier(4,barriers);
    if (NVSDK_NGX_FAILED(result)) { s.disabled=true; s.resetNext=true; return s.Fail("NGX EvaluateFeature",UINT(result)); }
    auto colorBarrier=Transition(frame->color.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    list->ResourceBarrier(1,&colorBarrier);
    s.Bind(list,*frame,s.finish.Get()); list->Dispatch((c.outputWidth+7)/8,(c.outputHeight+7)/8,1);
    std::swap(colorBarrier.Transition.StateBefore,colorBarrier.Transition.StateAfter); list->ResourceBarrier(1,&colorBarrier);
    D3D12_RESOURCE_BARRIER outputBarrier{}; outputBarrier.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV; outputBarrier.UAV.pResource=input.output;
    list->ResourceBarrier(1,&outputBarrier);
    if(timed){list->EndQuery(frame->timestamps.Get(),D3D12_QUERY_TYPE_TIMESTAMP,3);
        list->ResolveQueryData(frame->timestamps.Get(),D3D12_QUERY_TYPE_TIMESTAMP,0,4,frame->timestampReadback.Get(),0);
        frame->timestampPending=true;}
#ifdef GROUNDED_FSR_FRAMEGEN
    framegen::fsr::RecordPrepared(list,c,frame->depth.Get(),frame->velocity.Get(),s.settings.renderWidth,s.settings.renderHeight,reset || s.resetNext);
#endif
    s.resetNext=false; ++s.evaluations; s.error.clear(); return true;
}
void Integration::BeforeSubmit(ID3D12CommandQueue* queue) {
    auto& s=*impl;
    if (s.lastQueue && queue!=s.lastQueue.Get() && s.lastSignal) {
        HRESULT hr=queue->Wait(s.fence.Get(),s.lastSignal);
        if (FAILED(hr)) { s.disabled=true; s.Fail("Cross-queue fence wait",UINT(hr)); }
    }
}
void Integration::Submitted(ID3D12CommandQueue* queue, UINT count, ID3D12CommandList* const* lists) {
    auto& s=*impl; bool pending=false;
    for (auto& f:s.frames) for (UINT i=0;i<count;++i) if (f->recording==lists[i]) pending=true;
    if (!pending) return;
    const auto value=++s.lastSignal;
    HRESULT hr=queue->Signal(s.fence.Get(),value);
    for (auto& f:s.frames) for (UINT i=0;i<count;++i) if (f->recording==lists[i]) { f->fence=value; f->featureCreation=false;
        if(f->timestampPending && FAILED(queue->GetTimestampFrequency(&f->timestampFrequency)))f->timestampPending=false;}
    for(UINT i=0;i<count;++i)if(s.creationList==lists[i])s.creationList=nullptr;
    s.lastQueue=queue;
    if (FAILED(hr)) { s.disabled=true; s.Fail("Queue fence signal",UINT(hr)); }
}
void Integration::Discarded(ID3D12GraphicsCommandList* list) {
    auto& s=*impl;
    for (auto& f:s.frames) if (f->recording==list) {
        if (f->featureCreation) {
            // Creation work was never submitted. A new feature must initialize on GPU.
            if (s.feature) { s.release(s.feature); s.feature=nullptr; }
            s.creationList=nullptr;
            s.resetNext=true; f->featureCreation=false;
        }
        f->recording=nullptr;
        if(!f->fence)f->timestampPending=false;
        if (s.Idle(*f)) for (auto& retained:f->retained) retained.Reset();
    }
}
bool Integration::HasPending(ID3D12GraphicsCommandList* list) const {
    for (const auto& f:impl->frames) if (f->recording==list) return true; return false;
}
const std::string& Integration::Error() const { return impl->error; }
uint64_t Integration::Evaluations() const { return impl->evaluations; }
bool Integration::Disabled() const { return impl->disabled; }
unsigned Integration::RenderPresetRequested() const { return RecommendedRenderPreset(unsigned(impl->settings.mode),impl->gpu.architecture,impl->renderPreset); }
unsigned Integration::RenderPresetFor(unsigned qualityMode) const { return RecommendedRenderPreset(qualityMode,impl->gpu.architecture,impl->renderPreset); }
unsigned Integration::GpuArchitecture() const { return impl->gpu.architecture; }
const std::wstring& Integration::GpuName() const { return impl->gpu.name; }
unsigned Integration::InputWidth() const { return impl->settings.renderWidth; }
unsigned Integration::InputHeight() const { return impl->settings.renderHeight; }
unsigned Integration::QualityMode() const { return unsigned(impl->settings.mode); }
uint64_t Integration::FeatureGenerations() const { return impl->featureGenerations; }
void Integration::SetProfiling(bool value) {impl->profiling=value;}
Integration::GpuProfile Integration::ReadGpuProfile(){impl->HarvestTimestamps();return impl->gpuProfile;}
#ifdef GROUNDED_INTEGRATION_TEST
bool Integration::UltraViewportForTest(unsigned width,unsigned height) {
    auto& s=*impl;void* callback=nullptr;
    if(!s.capabilities || NVSDK_NGX_FAILED(s.capabilities->Get(NVSDK_NGX_Parameter_DLSSOptimalSettingsCallback,&callback)) || !callback)return false;
    s.capabilities->Set(NVSDK_NGX_Parameter_Width,width);s.capabilities->Set(NVSDK_NGX_Parameter_Height,height);
    s.capabilities->Set(NVSDK_NGX_Parameter_PerfQualityValue,int(NVSDK_NGX_PerfQuality_Value_UltraPerformance));
    s.capabilities->Set(NVSDK_NGX_Parameter_RTXValue,0);
    if(NVSDK_NGX_FAILED(reinterpret_cast<PFN_NVSDK_NGX_DLSS_GetOptimalSettingsCallback>(callback)(s.capabilities)))return false;
    UINT w=0,h=0,minW=0,minH=0,maxW=0,maxH=0;
    s.capabilities->Get(NVSDK_NGX_Parameter_OutWidth,&w);s.capabilities->Get(NVSDK_NGX_Parameter_OutHeight,&h);
    minW=maxW=w;minH=maxH=h;
    s.capabilities->Get(NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Min_Render_Width,&minW);
    s.capabilities->Get(NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Min_Render_Height,&minH);
    s.capabilities->Get(NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Max_Render_Width,&maxW);
    s.capabilities->Get(NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Max_Render_Height,&maxH);
    const auto actualW=UINT(std::ceil(width*ScalePresetPercentage(33)/100.0)),actualH=UINT(std::ceil(height*ScalePresetPercentage(33)/100.0));
    printf("Ultra viewport query: output=%ux%u optimal=%ux%u minimum=%ux%u maximum=%ux%u proposed=%ux%u\n",width,height,w,h,minW,minH,maxW,maxH,actualW,actualH);
    return (actualW>=minW && actualW<=maxW && actualH>=minH && actualH<=maxH) ||
        (std::abs(int(actualW)-int(w))<=1 && std::abs(int(actualH)-int(h))<=1);
}
void Integration::JitterSignForTest(float sign) { impl->jitterSign=sign; }
bool Integration::NvidiaEvaluationObservedForTest() const { return nvidiaEvaluationObserved.load(); }
void Integration::ParametersForTest(void* parameters) {
    if(impl->parameters && impl->destroy)impl->destroy(impl->parameters);
    impl->parameters=static_cast<NVSDK_NGX_Parameter*>(parameters);impl->destroy=nullptr;
}
bool Integration::PreparedForTest(ID3D12GraphicsCommandList* list, ID3D12Resource** depth, ID3D12Resource** motion,ID3D12Resource** color) {
    for(auto& f:impl->frames)if(f->recording==list) {*depth=f->depth.Get();*motion=f->velocity.Get();if(color)*color=f->inputColor.Get();return true;}return false;
}
#endif
}
