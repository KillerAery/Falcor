import Scene.Raster;
import Utils.Math.MathHelpers;
import Rendering.Lights.LightHelpers;
import Utils.Sampling.TinyUniformSampleGenerator;

cbuffer PerFrameCB
{
    float gFrameCount;
};

SamplerState gLinearSampler;

Texture2D<float3> gTexAlbedo;
Texture2D<float3> gTexNormal;
Texture2D<float> gTexRoughness;
Texture2D<float> gTexCavity;
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

    // texture coord
    float2 texC = sd.uv;
    // normal
    float3 normalInTexture = normalize(gTexNormal.Sample(gLinearSampler, texC));
    //normalInTexture =  normalInTexture*2.0-1.0;
    float3 wnormal = normalize(vsOut.normalW);
    float3 wtangent = normalize(vsOut.tangentW.xyz/vsOut.tangentW.w);
    float3 wbitangent = cross(wnormal, wtangent);
    float3x3 tbn = float3x3(wtangent, wbitangent, wnormal);
	sd.N = normalize(mul(tbn, normalInTexture));
    sd.faceN = sd.N;

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

    psOut.color = finalColor 
        * float4(gTexAlbedo.Sample(gLinearSampler, texC), 1.0)
        * float(gTexCavity.Sample(gLinearSampler, texC));

#if defined(_VISUALIZE_CASCADES) && defined(_ENABLE_SHADOWS)
    float3 cascadeColor = gVisBuffer.Load(int3(vsOut.posH.xy, 0)).gba;
    psOut.color.rgb *= cascadeColor;
#endif
    return psOut;
}
