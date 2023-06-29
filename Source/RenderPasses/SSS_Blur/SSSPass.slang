import RenderPasses.SSSPass.PoissonDiskSampleGenerator;

cbuffer PerFrameCB
{
    float4x4 gInverseScreenAndProj;
    float3 gD;
    float gUScale;
    float gVScale;
};

SamplerState gLinearSampler;
Texture2D<float4> gTexDiffuse;
Texture2D<float> gDepthBuffer;

float DiffusionProfile(float r, float d)
{
    float rd = r/d;
    return (exp(-rd)+exp(-rd/3.f))/(8*PI*d*r);
}

float4 bssrdfFilter(
    Texture2D<float4> texDiffuse,
    Texture2D<float> depthBuffer, 
    const float2 uv, const float3 d, const float x1u, const float x1v)
{   
    static PossionDiskSampleGenerator sg;

    float4 totalColor = texDiffuse.Sample(gLinearSampler, uv);
    float4 sum = float4(1.f, 1.f, 1.f, 1.f);

    float depth = depthBuffer.Sample(gLinearSampler, uv);
    float4 pos = mul(float4(uv,depth,1.f),gInverseScreenAndProj);
    for(int i = 0; i < NUM_SAMPLES; i++)
    {
        float2 dt = sg.getSample(i) * float2(x1u, x1v);
        float sampleDepth = depthBuffer.Sample(gLinearSampler, uv + dt);
        float4 samplePos = mul(float4(uv + dt, sampleDepth, 1.f),gInverseScreenAndProj);
        float dist = length(samplePos - pos);
        float4 w = float4(
            DiffusionProfile(dist, d.x),
            DiffusionProfile(dist, d.y),
            DiffusionProfile(dist, d.z),
            1.f);
        totalColor += texDiffuse.Sample(gLinearSampler, uv + dt) * w;
        sum += w;
    }

    return totalColor / sum;
}

float4 main(float2 texC : TEXCOORD) : SV_TARGET0
{
    return bssrdfFilter(
        gTexDiffuse, 
        gDepthBuffer, 
        texC, 
        gD, 
        gUScale, 
        gVScale
        );
}