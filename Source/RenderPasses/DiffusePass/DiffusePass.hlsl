import Scene.Raster;
import Utils.Math.MathHelpers;
import Rendering.Lights.LightHelpers;
import Utils.Sampling.TinyUniformSampleGenerator;

cbuffer PerFrameCB
{
    float gFrameCount;
};

SamplerState gLinearSampler;
Texture2D<float4> gVisBuffer;

VSOut vsMain(VSIn vIn)
{
    VSOut vsOut;
    vsOut = defaultVS(vIn);
    return vsOut;
}

struct PsOut
{
    float4 color : SV_TARGET0;
};

PsOut psMain(VSOut vsOut, uint triangleIndex : SV_PrimitiveID)
{
    PsOut psOut;

    let lod = ImplicitLodTextureSampler();
    if (alphaTest(vsOut, triangleIndex, lod)) discard;

    float3 viewDir = normalize(gScene.camera.getPosition() - vsOut.posW);
    ShadingData sd = prepareShadingData(vsOut, triangleIndex, viewDir, lod);

    // Create BSDF instance and query its properties.
    let bsdf = gScene.materials.getBSDF(sd, lod);
    let bsdfProperties = bsdf.getProperties(sd);

    float4 finalColor = float4(0, 0, 0, 1);

    // Direct lighting from analytic light sources
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
        finalColor.rgb += bsdf.eval(sd, ls.dir, sg) * ls.Li * shadowFactor;
    }

    // Add the emissive component
    finalColor.rgb += bsdf.getProperties(sd).emission;
    finalColor.a = sd.opacity;

    // Cavity
    float occ = 1.0;
    BasicMaterialData md = gScene.materials.getBasicMaterialData(vsOut.materialID);
    if (md.texOcclusion.packedData)
    {
        SamplerState s = gScene.materials.getDefaultTextureSampler(vsOut.materialID);
        occ = gScene.materials.sampleTexture(md.texOcclusion, s, sd.uv, 0.f).r;
    }

    psOut.color = finalColor * occ;

    return psOut;
}
