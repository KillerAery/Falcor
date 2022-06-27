import Scene.Raster;
import Utils.Math.MathHelpers;
import Rendering.Lights.LightHelpers;
import Utils.Sampling.TinyUniformSampleGenerator;

static const float PI = 3.1415926f;

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

    BasicMaterialData md = gScene.materials.getBasicMaterialData(vsOut.materialID);
    SamplerState s = gScene.materials.getDefaultTextureSampler(vsOut.materialID);

    float3 V = normalize(gScene.camera.getPosition() - vsOut.posW);
    ShadingData sd = prepareShadingData(vsOut, triangleIndex, V, lod);

    // Create BSDF instance and query its properties.
    let bsdf = gScene.materials.getBSDF(sd, lod);
    let bsdfProperties = bsdf.getProperties(sd);

    float4 finalColor = float4(0, 0, 0, 1);
    float3 normalW = normalize(vsOut.normalW);
    float3 tangentW = normalize(vsOut.tangentW.xyz/vsOut.tangentW.w);
    float3 bitangentW = cross(normalW, tangentW);
    float3x3 tbn = float3x3(tangentW, bitangentW, normalW);
    float3 NinTex = gScene.materials.sampleTexture(
        md.texNormalMap, s, sd.uv, 0.f).rgb * 2.f - float3(1.f, 1.f, 1.f);
    float3 N = mul(NinTex, tbn);

    // Diffuse Direct lighting from analytic light sources
    // const uint2 pixel = vsOut.posH.xy;
    // TinyUniformSampleGenerator sg = TinyUniformSampleGenerator(pixel, gFrameCount);
    
    float3 albedo = gScene.materials.sampleTexture(md.texBaseColor, s, sd.uv, 0.f).rgb;
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
        // finalColor.rgb += bsdf.eval(sd, ls.dir, sg) * ls.Li * shadowFactor;
        
        float NdotL = dot(N, ls.dir);
        finalColor.rgb += NdotL * ls.Li * shadowFactor;
    }

    // finalColor.rgb = albedo;

    // Add the emissive component
    // finalColor.rgb += bsdf.getProperties(sd).emission;

    finalColor.a = sd.opacity;
    psOut.color = finalColor;

    return psOut;
}
