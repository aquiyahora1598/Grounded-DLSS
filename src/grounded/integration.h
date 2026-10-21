#pragma once
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <filesystem>
#include <memory>
#include <string>
#include "contract.h"

namespace grounded {
struct PassInput {
    Constants constants;
    ID3D12Resource* color = nullptr;
    ID3D12Resource* depth = nullptr;
    ID3D12Resource* velocity = nullptr;
    ID3D12Resource* exposure = nullptr;
    ID3D12Resource* output = nullptr;
    D3D12_SHADER_RESOURCE_VIEW_DESC depthView{};
    D3D12_SHADER_RESOURCE_VIEW_DESC velocityView{};
};
// Caller serializes Record/Submit/Discard and restores the host command state.
class Integration {
public:
    Integration();
    ~Integration();
    bool Initialize(ID3D12Device* device, HMODULE ngxModule, const std::filesystem::path& dataDirectory,
                    const std::filesystem::path& featureDirectory);
    bool Record(ID3D12GraphicsCommandList* list, const PassInput& input, bool reset);
    void BeforeSubmit(ID3D12CommandQueue* queue);
    void Submitted(ID3D12CommandQueue* queue, UINT count, ID3D12CommandList* const* lists);
    void Discarded(ID3D12GraphicsCommandList* list);
    bool HasPending(ID3D12GraphicsCommandList* list) const;
    struct GpuProfile {uint64_t samples=0;double preparationMs=0,ngxMs=0,finishMs=0;};
    void SetProfiling(bool enabled);
    GpuProfile ReadGpuProfile(); // Only completed fence values; never waits.
    const std::string& Error() const;
    uint64_t Evaluations() const;
    bool Disabled() const;
    unsigned RenderPresetRequested() const;
    unsigned RenderPresetFor(unsigned qualityMode) const;
    unsigned GpuArchitecture() const;
    const std::wstring& GpuName() const;
    unsigned InputWidth() const;
    unsigned InputHeight() const;
    unsigned QualityMode() const;
    uint64_t FeatureGenerations() const;
#ifdef GROUNDED_INTEGRATION_TEST
    bool PreparedForTest(ID3D12GraphicsCommandList* list, ID3D12Resource** depth, ID3D12Resource** motion,ID3D12Resource** color=nullptr);
    void JitterSignForTest(float sign);
    void ParametersForTest(void* parameters);
    bool NvidiaEvaluationObservedForTest() const;
    bool UltraViewportForTest(unsigned width,unsigned height);
#endif
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
