import Scene.Raster;
import Utils.Math.MathHelpers;
import Rendering.Lights.LightHelpers;
import Utils.Sampling.TinyUniformSampleGenerator;

static const float PI = 3.1415926f;

cbuffer PerFrameCB
{
    float gFrameCount;
    float gKDiffuse;
    float gKSpecular;
};

SamplerState gLinearSampler;
Texture2D<float4> gVisBuffer;
Texture2D<float4> gIrradianceMap;

VSOut vsMain(VSIn vIn)
{
    VSOut vsOut;
    vsOut = defaultVS(vIn);
    return vsOut;
}

struct PsOut
{
    float4 color : SV_TARGET0;
    float4 normal : SV_TARGET1;
};

PsOut psMain(VSOut vsOut, uint triangleIndex : SV_PrimitiveID)
{
    PsOut psOut;

    let lod = ImplicitLodTextureSampler();
    if (alphaTest(vsOut, triangleIndex, lod)) discard;

    float3 viewDir = normalize(gScene.camera.getPosition() - vsOut.posW);
    ShadingData sd = prepareShadingData(vsOut, triangleIndex, viewDir, lod);
    BasicMaterialData md = gScene.materials.getBasicMaterialData(vsOut.materialID);
    SamplerState s = gScene.materials.getDefaultTextureSampler(vsOut.materialID);

    // Create BSDF instance and query its properties.
    let bsdf = gScene.materials.getBSDF(sd, lod);
    let bsdfProperties = bsdf.getProperties(sd);

    float4 finalColor = float4(0, 0, 0, 1);

    // Specular lighting from analytic light sources
    const uint2 pixel = vsOut.posH.xy;
    TinyUniformSampleGenerator sg = TinyUniformSampleGenerator(pixel, gFrameCount);
    for (int i = 0; i < gScene.getLightCount(); i++)
    {
        float shadowFactor = 1;
        if (i == 0)
        {
            shadowFactor = gVisBuffer.Load(int3(vsOut.posH.xy, 0)).r;
            shadowFactor *= sd.opacity;
        }

        AnalyticLightSample ls;
        evalLightApproximate(sd.posW, gScene.getLight(i), ls);
        finalColor.rgb += bsdf.eval(sd, ls.dir, sg) * ls.Li * shadowFactor * gKSpecular;
    }

    // Diffuse Lighting
    float4 albedo = gScene.materials.sampleTexture(md.texBaseColor, s, sd.uv, 0.f);
    float4 coord = mul(float4(vsOut.posW, 1.f), gScene.camera.getViewProj());
    coord.xyz/=coord.w;
    coord.x += 1.f;
    coord.y = 1.f - coord.y;
    coord.xy /= 2.f;
    float3 irradiance = gIrradianceMap.Sample(gLinearSampler, coord.xy).rgb;
    finalColor.rgb +=  irradiance * albedo.rgb * gKDiffuse;

    // Cavity
    float occ = 1.0;
    if (md.texOcclusion.packedData)
    {
        occ = gScene.materials.sampleTexture(md.texOcclusion, s, sd.uv, 0.f).r;
    }

    psOut.normal = float4(sd.N * 0.5f + 0.5f, 1.0f);
    psOut.color = finalColor * occ;

    return psOut;
}
