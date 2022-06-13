
cbuffer PerFrameCB : register(b0)
{
    float4x4 InverseScreenAndProj;
};

SamplerState gTexDiffuseSampler : register(s0);
SamplerState gTexDepthSampler : register(s1);

Texture2D gTexDiffuse;
Texture2D gTexDepth;

static const float PI = 3.1415926f;

float DiffusionProfile(float r, float d)
{
    float rd = r/d;
    return (exp(rd)+exp(rd/3))/(8*PI*d*r);
}

float4 bssrdfFilter3x3(
    Texture2D<float4> texDiffuse,
    Texture2D<float> texDepth, 
    const float2 uv, const float d, const float uscale, const float vscale)
{
    float sum = 1.f;
    float4 color = texDiffuse.SampleLevel(gTexDiffuseSampler, uv, 0.f);
    const float4 pos = (uv, texDepth.SampleLevel(gTexDepthSampler, uv, 0.f), 1.f);

    float x1u = uscale;
    float x1v = vscale;

    const float2 dt[8] = {
        float2(-x1u, -x1v),float2(x1u, -x1v),float2(-x1u, x1v),float2(x1u, x1v),
        float2(0, -x1v),float2(-x1u, 0),float2(0, x1v),float2(x1u, 0),
    };
    float w;
    float4 dist;

    [unroll]
    for(int i = 0 ; i < 8 ; i++)
    {
        float depth = texDepth.SampleLevel(gTexDepthSampler, uv + dt[i], 0.f);
        float dist = length(mul(float4(dt[i],depth,1.f)-pos, InverseScreenAndProj));
        w = DiffusionProfile(dist, d);
        color += texDiffuse.SampleLevel(gTexDiffuseSampler, uv + dt[i], 0.f) * w;
        sum += w;
    }
    return color / sum;
}

float4 main(float2 texC : TEXCOORD) : SV_TARGET0
{
    return bssrdfFilter3x3(gTexDiffuse, gTexPos, texC, 1.0f , 1.0f, 1.0f);
}