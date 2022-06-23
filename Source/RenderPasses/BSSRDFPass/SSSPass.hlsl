import RenderPasses.BSSRDFPass.BSSRDFParams;

cbuffer PerFrameCB
{
    SSSParams gParams;
};

SamplerState gLinearSampler;

Texture2D<float4> gTexDiffuse;
Texture2D<float> gTexDepth;

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
    float4 color = texDiffuse.Sample(gLinearSampler, uv);
    const float4 pos = (uv, texDepth.Sample(gLinearSampler, uv), 1.f);

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
        float depth = texDepth.Sample(gLinearSampler, uv + dt[i]);
        float dist = length(mul(float4(dt[i],depth,1.f)-pos, gParams.InverseScreenAndProj));
        w = DiffusionProfile(dist, d);
        color += texDiffuse.Sample(gLinearSampler, uv + dt[i]) * w;
        sum += w;
    }
    return color / sum;
}

float4 main(float2 texC : TEXCOORD) : SV_TARGET0
{
    return bssrdfFilter3x3(
        gTexDiffuse, 
        gTexDepth, 
        texC, 
        gParams.d, 
        gParams.uScale, 
        gParams.vScale
        );
}