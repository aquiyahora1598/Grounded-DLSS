#pragma once
namespace grounded {
inline constexpr char PrepareShader[] = R"hlsl(
cbuffer PassConstants : register(b0) { float4 Temporal[29]; };
cbuffer ViewConstants : register(b1) { float4 View[145]; };
Texture2D<float> Depth : register(t0);
Texture2D<float4> EncodedVelocity : register(t1);
Texture2D<float4> UpscaledColor : register(t2);
Texture2D<float4> CurrentColor : register(t3);
Texture2D<float4> EngineExposure : register(t4);
RWTexture2D<float> ResolvedDepth : register(u0);
RWTexture2D<float2> ResolvedVelocity : register(u1);
RWTexture2D<float4> Output : register(u2);
RWTexture2D<float4> ResolvedColor : register(u3);
RWTexture2D<float> ResolvedExposure : register(u4);
float4 CubicWeights(float t) {
    float t2=t*t, t3=t2*t;
    return float4(-0.5*t+t2-0.5*t3, 1-2.5*t2+1.5*t3,
                  0.5*t+2*t2-1.5*t3, -0.5*t2+0.5*t3);
}
float4 ResampleColor(float2 position, uint2 active) {
    int2 low=int2(floor(position)), last=int2(active)-1;
    float2 fraction=frac(position);
    float4 wx=CubicWeights(fraction.x), wy=CubicWeights(fraction.y), result=0;
    // Interpolate the same whole viewport without the bilinear contrast loss
    // at half-texel positions. Clamp to the central neighborhood to prevent
    // ringing/negative HDR colors. This is not a sharpening pass.
    [unroll] for(int y=0;y<4;++y) {
        float4 row=0;
        [unroll] for(int x=0;x<4;++x)
            row+=wx[x]*CurrentColor.Load(int3(clamp(low+int2(x-1,y-1),0,last),0));
        result+=wy[y]*row;
    }
    float4 a=CurrentColor.Load(int3(clamp(low,0,last),0));
    float4 b=CurrentColor.Load(int3(clamp(low+int2(1,0),0,last),0));
    float4 c=CurrentColor.Load(int3(clamp(low+int2(0,1),0,last),0));
    float4 d=CurrentColor.Load(int3(clamp(low+int2(1,1),0,last),0));
    return clamp(result,min(min(a,b),min(c,d)),max(max(a,b),max(c,d)));
}
[numthreads(8,8,1)]
void Prepare(uint3 id : SV_DispatchThreadID) {
    uint2 extent = uint2(Temporal[19].xy);
    if (any(id.xy >= extent)) return;
    uint2 active = uint2(Temporal[27].xy);
    uint2 resolvedActive = uint2(Temporal[0].xy); // private preparation constants
    bool resample = any(active != resolvedActive);
    uint2 resolvedPixel = min(id.xy, resolvedActive - 1);
    float2 sourcePosition = (float2(resolvedPixel) + 0.5) * float2(active) / float2(resolvedActive) - 0.5;
    uint2 pixel = resample ? min(uint2(sourcePosition + 0.5),active - 1) : min(id.xy,active - 1);
    float depth = Depth.Load(int3(pixel,0));
    float2 encoded = EncodedVelocity.Load(int3(pixel,0)).xy;
    float2 motion;
    if (encoded.x > 0) {
        motion = encoded * asfloat(0x408041ab) + asfloat(0xc000412b);
    } else {
        // Reproduce the native shader's unjittered current clip coordinate and
        // cb1[122..125] row-vector reprojection. Static geometry needs camera motion.
        float2 position = resample ? sourcePosition : float2(pixel);
        float2 uv = (position + 0.5 - Temporal[28].xy) / Temporal[27].xy;
        float2 clip = uv * float2(2,-2) + float2(-1,1);
        float4 previous = clip.x * View[122] + clip.y * View[123] + depth * View[124] + View[125];
        motion = previous.w > 1e-8 ? clip - previous.xy / previous.w : float2(0,0);
    }
    float2 pixels = motion * float2(-0.5,0.5) * float2(resolvedActive);
    ResolvedDepth[id.xy] = isfinite(depth) ? depth : 0;
    ResolvedVelocity[id.xy] = all(isfinite(pixels)) ? clamp(pixels, -32752, 32752) : float2(0,0);
    if(resample) ResolvedColor[id.xy] = ResampleColor(sourcePosition,active);
    else ResolvedColor[id.xy] = CurrentColor.Load(int3(id.xy,0));
    if(all(id.xy==0)) {
        float exposure=EngineExposure.Load(int3(0,0,0)).x;
        ResolvedExposure[uint2(0,0)] = isfinite(exposure) && exposure>0 ? exposure : 1;
    }
}
[numthreads(8,8,1)]
void Finish(uint3 id : SV_DispatchThreadID) {
    if (any(id.xy >= uint2(Temporal[23].xy))) return;
    // Native TAAU writes alpha zero. Preserve that downstream contract.
    Output[id.xy] = float4(UpscaledColor.Load(int3(id.xy,0)).rgb,0);
}
)hlsl";
}
