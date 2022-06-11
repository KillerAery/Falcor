import Scene.Raster;
import Utils.Math.MathHelpers;
import Rendering.Lights.LightHelpers;
import Rendering.Lights.EnvMapLighting;
import Utils.Sampling.TinyUniformSampleGenerator;

cbuffer PerFrameCB
{
    float2 gRenderTargetDim;
    float  gFrameCount;
};

EnvMapLighting gEnvMapLighting;
SamplerState gSampler;
Texture2D visibilityBuffer;
static VSOut vsData;

VSOut vsMain(VSIn vIn)
{
    VSOut vsOut;
    vsOut = defaultVS(vIn);
#ifdef _OUTPUT_MOTION_VECTORS
    vsOut.prevPosH.xy += vsOut.prevPosH.w * 2 * float2(gScene.camera.data.jitterX, gScene.camera.data.jitterY);
#endif
    return vsOut;
}

struct PsOut
{
    float4 color : SV_TARGET0;
    float4 normal : SV_TARGET1;
#ifdef _OUTPUT_MOTION_VECTORS
    float2 motion : SV_TARGET2;
#endif
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
            shadowFactor = visibilityBuffer.Load(int3(vsOut.posH.xy, 0)).r;
            shadowFactor *= sd.opacity;
        }

        AnalyticLightSample ls;
        evalLightApproximate(sd.posW, gScene.getLight(i), ls);
        finalColor.rgb += bsdf.eval(sd, ls.dir, sg) * ls.Li * shadowFactor;
    }

    // Add the emissive component
    finalColor.rgb += bsdf.getProperties(sd).emission;
    finalColor.a = sd.opacity;

#ifdef _USE_ENV_MAP
    // Add lighting from environment map
    finalColor.rgb += gEnvMapLighting.evalDiffuse(sd, bsdfProperties);
    finalColor.rgb += gEnvMapLighting.evalSpecular(sd, bsdfProperties, reflect(-sd.V, sd.N));
#endif

    psOut.color = finalColor;
    psOut.normal = float4(sd.N * 0.5f + 0.5f, 1.0f);

#ifdef _OUTPUT_MOTION_VECTORS
    // Using vsOut.posH.xy as pixel coordinate since it has the SV_Position semantic.
    psOut.motion = calcMotionVector(vsOut.posH.xy, vsOut.prevPosH, gRenderTargetDim);
#endif

#if defined(_VISUALIZE_CASCADES) && defined(_ENABLE_SHADOWS)
    float3 cascadeColor = visibilityBuffer.Load(int3(vsOut.posH.xy, 0)).gba;
    psOut.color.rgb *= cascadeColor;
#endif
    return psOut;
}
