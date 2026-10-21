#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace grounded {
constexpr uint64_t ShaderHash = 0x901e041a7cadc9dbull;
constexpr uint64_t NativeShaderHash = 0xd2e4d8c23c362ed1ull;
enum class PassVariant { Unknown, Upscaling, Native100 };
inline PassVariant ClassifyShader(uint64_t hash, size_t bytes) {
    if (hash == ShaderHash && bytes == 10292) return PassVariant::Upscaling;
    if (hash == NativeShaderHash && bytes == 10864) return PassVariant::Native100;
    return PassVariant::Unknown;
}
struct Constants {
    std::array<float, 29 * 4> pass{};
    std::array<float, 145 * 4> view{};
    unsigned width = 0, height = 0, activeWidth = 0, activeHeight = 0;
    unsigned outputWidth = 0, outputHeight = 0;
    float jitterX = 0, jitterY = 0;
};
inline bool DecodeConstants(const void* pass, size_t passBytes, const void* view, size_t viewBytes, Constants& out) {
    if (!pass || !view || passBytes < sizeof(out.pass) || viewBytes < sizeof(out.view)) return false;
    memcpy(out.pass.data(), pass, sizeof(out.pass));
    memcpy(out.view.data(), view, sizeof(out.view));
    const auto& p = out.pass;
    for (unsigned reg : {19u, 23u, 27u}) {
        for (unsigned axis = 0; axis < 2; ++axis) {
            float extent = p[reg * 4 + axis], reciprocal = p[reg * 4 + axis + 2];
            if (!std::isfinite(extent) || !std::isfinite(reciprocal) || extent < 32 || extent > 8192 ||
                extent != std::floor(extent) || std::abs(extent * reciprocal - 1) > 1e-5f) return false;
        }
    }
    // This verified variant uses an origin of zero and a full output rectangle.
    if (p[26 * 4] != 0 || p[26 * 4 + 1] != 0 || p[24 * 4] != 0 || p[24 * 4 + 1] != 0 ||
        p[24 * 4 + 2] != p[23 * 4] || p[24 * 4 + 3] != p[23 * 4 + 1]) return false;
    out.width = unsigned(p[19 * 4]); out.height = unsigned(p[19 * 4 + 1]);
    out.activeWidth = unsigned(p[27 * 4]); out.activeHeight = unsigned(p[27 * 4 + 1]);
    out.outputWidth = unsigned(p[23 * 4]); out.outputHeight = unsigned(p[23 * 4 + 1]);
    if (out.activeWidth > out.width || out.activeHeight > out.height ||
        out.activeWidth > out.outputWidth || out.activeHeight > out.outputHeight) return false;
    out.jitterX = p[28 * 4]; out.jitterY = p[28 * 4 + 1];
    if (!std::isfinite(out.jitterX) || !std::isfinite(out.jitterY) ||
        std::abs(out.jitterX) > 0.5f || std::abs(out.jitterY) > 0.5f) return false;
    for (unsigned i = 122 * 4; i < 126 * 4; ++i) if (!std::isfinite(out.view[i])) return false;
    // The captured normalization is exactly one. Other engine variants stay native
    // until their pre-exposure contract is verified rather than guessed.
    return out.view[135 * 4 + 2] == 1.0f;
}
inline float VelocityLiteral(uint32_t bits) { float result; memcpy(&result, &bits, 4); return result; }
inline std::array<float, 2> DecodeVelocity(float x, float y, unsigned width, unsigned height) {
    const float scale = VelocityLiteral(0x408041ab), bias = VelocityLiteral(0xc000412b);
    return {(x * scale + bias) * (-0.5f * width), (y * scale + bias) * (0.5f * height)};
}
}
