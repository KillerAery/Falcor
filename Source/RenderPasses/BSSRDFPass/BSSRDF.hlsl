/*
cbuffer PerFrameCB : register(b0)
{
    float2 gRenderTargetDim;
    float  gFrameCount;
};
*/
SamplerState gTexDiffuseSampler : register(s0);
SamplerState gTexPosSampler : register(s1);

Texture2D gTexDiffuse;
Texture2D gTexPos;

static const float PI = 3.1415926f;

float DiffusionProfile(float r, float d)
{
    float rd = r/d;
    return (exp(rd)+exp(rd/3))/(8*PI*d*r);
}

float4 bssrdfFilter3x3(
    Texture2D<float4> texDiffuse,
    Texture2D<float4> texDepth, 
    const float2 uv, const float d, const float uscale, const float vscale)
{
    float sum = 1.f;
    const float pos = texPos.SampleLevel(gTexDepthSampler, uv, 0.f);
    float4 color = texDiffuse.SampleLevel(gTexDiffuseSampler, uv, 0.f);

    float w;
    w = DiffusionProfile(distance(texPos.SampleLevel(gTexPosSampler,    uv + float2(-x1u, -x1v), 0.f),pos),d);
    color += texDiffuse.SampleLevel(gTexDiffuseSampler,         uv + float2(-x1u, -x1v), 0.f) * w;
    sum += w;
    w = DiffusionProfile(distance(texPos.SampleLevel(gTexPosSampler,    uv + float2(x1u, -x1v), 0.f),pos),d);
    color += texDiffuse.SampleLevel(gTexDiffuseSampler,         uv + float2(x1u, -x1v), 0.f) * w;
    sum += w;
    w = DiffusionProfile(distance(texPos.SampleLevel(gTexPosSampler,    uv + float2(-x1u, x1v), 0.f),pos),d);
    color += texDiffuse.SampleLevel(gTexDiffuseSampler,         uv + float2(-x1u, x1v), 0.f) * w;
    sum += w;
    w = DiffusionProfile(distance(texPos.SampleLevel(gTexPosSampler,    uv + float2(x1u, x1v), 0.f),pos),d);
    color += texDiffuse.SampleLevel(gTexDiffuseSampler,         uv + float2(x1u, x1v), 0.f) * w;
    sum += w;    
    w = DiffusionProfile(distance(texPos.SampleLevel(gTexPosSampler,    uv + float2(0.f, -x1v), 0.f),pos),d);
    color += texDiffuse.SampleLevel(gTexDiffuseSampler,         uv + float2(0.f, -x1v), 0.f) * w;
    sum += w;
    w = DiffusionProfile(distance(texPos.SampleLevel(gTexPosSampler,    uv + float2(0.f, x1v), 0.f),pos),d);
    color += texDiffuse.SampleLevel(gTexDiffuseSampler,         uv + float2(0.f, x1v), 0.f) * w;
    sum += w;
    w = DiffusionProfile(distance(texPos.SampleLevel(gTexPosSampler,    uv + float2(-x1u, 0.f), 0.f),pos),d);
    color += texDiffuse.SampleLevel(gTexDiffuseSampler,         uv + float2(-x1u, 0.f), 0.f) * w;
    sum += w;
    w = DiffusionProfile(distance(texPos.SampleLevel(gTexPosSampler,    uv + float2(-x1u, 0.f), 0.f),pos),d);
    color += texDiffuse.SampleLevel(gTexDiffuseSampler,         uv + float2(-x1u, 0.f), 0.f) * w;
    sum += w;

    return color / sum;
}

float4 main(float2 texC : TEXCOORD) : SV_TARGET0
{
    return bssrdfFilter3x3(gTexDiffuse, gTexPos, texC, 1.0f , 1.0f, 1.0f);
}