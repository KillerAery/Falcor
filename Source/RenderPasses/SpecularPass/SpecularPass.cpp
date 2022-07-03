/***************************************************************************
 # Copyright (c) 2015-21, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#include "SpecularPass.h"

const RenderPass::Info SpecularPass::kInfo { "SpecularPass", "输出 Lighting 计算后的着色效果" };

namespace
{
    const char kIrradianceMap[] = "irradianceMap";
    const char kVisBuffer[] = "visBuffer";
    const char kDepthBuffer[] = "depthBuffer";
    const char kKDiffuse[] = "kDiffuse";
    const char kKSpecular[] = "kSpecular";
    const char kNormals[] = "normals";

    const char kDst[]    = "dst";

    const char kSpecularPassFile[] = "RenderPasses/SpecularPass/SpecularPass.hlsl";

}

// Don't remove this. it's required for hot-reload to function properly
extern "C" FALCOR_API_EXPORT const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" FALCOR_API_EXPORT void getPasses(Falcor::RenderPassLibrary & lib)
{
    lib.registerPass(SpecularPass::kInfo, SpecularPass::create);
}

SpecularPass::SharedPtr SpecularPass::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    auto pThis = SpecularPass::SharedPtr(new SpecularPass());
    return pThis;
}

Dictionary SpecularPass::getScriptingDictionary()
{
    Dictionary d;
    return d;
}

RenderPassReflection SpecularPass::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;
    reflector.addInput(kIrradianceMap, "Irradiance Map");
    reflector.addInput(kDepthBuffer, "Depth Buffer");
    reflector.addInput(kVisBuffer, "Visibility buffer used for shadowing. Range is [0,1] where 0 means the pixel is fully-shadowed and 1 means the pixel is not shadowed at all").flags(RenderPassReflection::Field::Flags::Optional);
    reflector.addOutput(kNormals, "World-space shading normal, [0,1] range. Don't forget to transform it to [-1, 1] range").texture2D(0, 0, 1);

    mOutputSize = RenderPassHelpers::calculateIOSize(RenderPassHelpers::IOSize::Default, { 1024, 1024 }, compileData.defaultTexDims);
    reflector.addOutput(kDst, "output texture").format(ResourceFormat::RGBA32Float).texture2D(0, 0);

    return reflector;
}

void SpecularPass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (!mpScene) return;

    auto pDst = renderData[kDst]->asTexture();

    // -------------------------------------------------------------------------------------------------------
    // Specular Pass：采用几何 Pass
    // ------------------------------------------------------------------------------------------------------

    // Update env map lighting
    const auto& pEnvMap = mpScene->getEnvMap();
    if (pEnvMap && (!mpEnvMapLighting || mpEnvMapLighting->getEnvMap() != pEnvMap))
    {
        mpEnvMapLighting = EnvMapLighting::create(pRenderContext, pEnvMap);
        mpEnvMapLighting->setShaderData(mpSpecularPassVars["gEnvMapLighting"]);
        mpSpecularPassState->getProgram()->addDefine("_USE_ENV_MAP");
    }
    else if (!pEnvMap)
    {
        mpEnvMapLighting = nullptr;
        mpSpecularPassState->getProgram()->removeDefine("_USE_ENV_MAP");
    }

    mpSpecularPassVars->setSampler("gLinearSampler", mpLinearSampler);

    // Visibility Buffer
    mpVisBuffer = renderData[kVisBuffer]->asTexture();
    mpSpecularPassVars->setTexture("gVisBuffer", mpVisBuffer);

    // Irradiance Map
    mpIrradianceMap = renderData[kIrradianceMap]->asTexture();
    mpSpecularPassVars->setTexture("gIrradianceMap", mpIrradianceMap);

    // Depth Buffer
    const auto& pDepthBuffer = renderData[kDepthBuffer]->asTexture();
    mpFbo->attachDepthStencilTarget(pDepthBuffer);

    // 输出 Specular texture
    mpFbo->attachColorTarget(pDst, 0);
    // 输出 World Noraml Map
    mpFbo->attachColorTarget(renderData[kNormals]->asTexture(), 1);

    // clear view
    const auto& pRtv = mpFbo->getRenderTargetView(0).get();
    if (pRtv->getResource() != nullptr) pRenderContext->clearRtv(pRtv, float4(0));

    mpSpecularPassVars["PerFrameCB"]["gFrameCount"] = mFrameCount++;
    mpSpecularPassVars["PerFrameCB"]["gKDiffuse"] = mKDiffuse;
    mpSpecularPassVars["PerFrameCB"]["gKSpecular"] = mKSpecular;

    mpSpecularPassState->setFbo(mpFbo);
    mpScene->rasterize(pRenderContext, mpSpecularPassState.get(), mpSpecularPassVars.get());
}

void SpecularPass::renderUI(Gui::Widgets& widget)
{
    if (auto group = widget.group("SSS: Diffuse & Specular", true))
    {
        group.var("kDiffuse", mKDiffuse, 0.f, 1.f, 0.00001f, false, "%.6f");
        group.var("kSpecular", mKSpecular, 0.f, 1.f, 0.00001f, false, "%.6f");
    }
}

void SpecularPass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
{
    mpScene = pScene;
    if (!mpScene)return;
    mpSpecularPassState->getProgram()->addDefines(mpScene->getSceneDefines());
    mpSpecularPassState->getProgram()->setTypeConformances(mpScene->getTypeConformances());
    mpSpecularPassVars = GraphicsVars::create(mpSpecularPassState->getProgram()->getReflector());
    createSampler();
}

void SpecularPass::createSpecularPass()
{
    DepthStencilState::Desc desc;
    desc.setStencilEnabled(false);
    desc.setDepthEnabled(true);
    desc.setDepthWriteMask(false);
    desc.setDepthFunc(DepthStencilState::Func::LessEqual);
    DepthStencilState::SharedPtr pDsc = DepthStencilState::create(desc);

    // 创建 shader
    GraphicsProgram::SharedPtr pProgram = GraphicsProgram::createFromFile(
        kSpecularPassFile, "vsMain", "psMain");
    mpSpecularPassState = GraphicsState::create();
    mpSpecularPassState->setProgram(pProgram);
    mpSpecularPassState->setDepthStencilState(pDsc);

    // 创建 FrameBuffer
    mpFbo = Fbo::create();
}

void SpecularPass::createSampler()
{
    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point);
    mpPointSampler = Sampler::create(samplerDesc);
    samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Point);
    mpLinearSampler = Sampler::create(samplerDesc);
}

SpecularPass::SpecularPass(): RenderPass(kInfo)
{
    createSpecularPass();
}
