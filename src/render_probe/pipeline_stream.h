#pragma once
#include <d3d12.h>
#include <cstring>

// Match the SDK's enum/payload pairs, including padding before pointer payloads.
#pragma warning(push)
#pragma warning(disable:4324) // Padding is part of the documented stream ABI.
template<class T> struct alignas(void*) ProbeStreamPart {
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type;
    T value;
};

inline bool ProbeShaderBytecode(const D3D12_PIPELINE_STATE_STREAM_DESC* desc, D3D12_SHADER_BYTECODE& shader,
                               D3D12_PIPELINE_STATE_SUBOBJECT_TYPE wanted) {
    shader = {};
    if (!desc || !desc->pPipelineStateSubobjectStream || desc->SizeInBytes > 1024 * 1024) return false;
    auto bytes = static_cast<const unsigned char*>(desc->pPipelineStateSubobjectStream);
    size_t offset = 0;
    while (offset < desc->SizeInBytes) {
        if (desc->SizeInBytes - offset < sizeof(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE)) return false;
        D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type{};
        memcpy(&type, bytes + offset, sizeof(type));
        size_t size = 0;
        switch (type) {
            case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS: case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS: {
                ProbeStreamPart<D3D12_SHADER_BYTECODE> part{};
                if (desc->SizeInBytes - offset < sizeof(part)) return false;
                memcpy(&part, bytes + offset, sizeof(part)); if(type==wanted)shader = part.value;
                size = sizeof(part); break;
            }
            case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS:
            case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS: case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS:
            case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS: case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS:
            case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS: size = sizeof(ProbeStreamPart<D3D12_SHADER_BYTECODE>); break;
            case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE: size = sizeof(ProbeStreamPart<ID3D12RootSignature*>); break;
            case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT: size = sizeof(ProbeStreamPart<D3D12_STREAM_OUTPUT_DESC>); break;
            case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND: size = sizeof(ProbeStreamPart<D3D12_BLEND_DESC>); break;
            case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK: case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK:
                size = sizeof(ProbeStreamPart<UINT>); break;
            case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER: size = sizeof(ProbeStreamPart<D3D12_RASTERIZER_DESC>); break;
            case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER1: size = sizeof(ProbeStreamPart<D3D12_RASTERIZER_DESC1>); break;
            case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER2: size = sizeof(ProbeStreamPart<D3D12_RASTERIZER_DESC2>); break;
            case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL: size = sizeof(ProbeStreamPart<D3D12_DEPTH_STENCIL_DESC>); break;
            case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1: size = sizeof(ProbeStreamPart<D3D12_DEPTH_STENCIL_DESC1>); break;
            case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL2: size = sizeof(ProbeStreamPart<D3D12_DEPTH_STENCIL_DESC2>); break;
            case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT: size = sizeof(ProbeStreamPart<D3D12_INPUT_LAYOUT_DESC>); break;
            case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE: size = sizeof(ProbeStreamPart<D3D12_INDEX_BUFFER_STRIP_CUT_VALUE>); break;
            case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY: size = sizeof(ProbeStreamPart<D3D12_PRIMITIVE_TOPOLOGY_TYPE>); break;
            case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS: size = sizeof(ProbeStreamPart<D3D12_RT_FORMAT_ARRAY>); break;
            case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT: size = sizeof(ProbeStreamPart<DXGI_FORMAT>); break;
            case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC: size = sizeof(ProbeStreamPart<DXGI_SAMPLE_DESC>); break;
            case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO: size = sizeof(ProbeStreamPart<D3D12_CACHED_PIPELINE_STATE>); break;
            case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS: size = sizeof(ProbeStreamPart<D3D12_PIPELINE_STATE_FLAGS>); break;
            case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING: size = sizeof(ProbeStreamPart<D3D12_VIEW_INSTANCING_DESC>); break;
            default: return false; // A new SDK type must be described explicitly before parsing it.
        }
        if (size > desc->SizeInBytes - offset) return false;
        offset += size;
    }
    return shader.pShaderBytecode && shader.BytecodeLength;
}
inline bool ProbeComputeBytecode(const D3D12_PIPELINE_STATE_STREAM_DESC* desc,D3D12_SHADER_BYTECODE& shader){
    return ProbeShaderBytecode(desc,shader,D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS);
}
#pragma warning(pop)
