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
#include "DiffusePass.h"

const RenderPass::Info DiffusePass::kInfo { "DiffusePass", "输出 Irridiance Map" };

namespace
{
    const char kVisBuffer[] = "visBuffer";
    const char kDepthBuffer[] = "depthBuffer";

    const char kDst[]    = "dst";

    const char kDiffusePassFile[] = "RenderPasses/DiffusePass/DiffusePass.hlsl";
}

// Don't remove this. it's required for hot-reload to function properly
extern "C" FALCOR_API_EXPORT const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" FALCOR_API_EXPORT void getPasses(Falcor::RenderPassLibrary & lib)
{
    lib.registerPass(DiffusePass::kInfo, DiffusePass::create);
}

DiffusePass::SharedPtr DiffusePass::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    auto pThis = DiffusePass::SharedPtr(new DiffusePass());
    return pThis;
}

Dictionary DiffusePass::getScriptingDictionary()
{
    Dictionary d;
    return d;
}

RenderPassReflection DiffusePass::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;
    reflector.addInput(kDepthBuffer, "Depth Buffer");
    reflector.addInput(kVisBuffer, "Visibility buffer used for shadowing. Range is [0,1] where 0 means the pixel is fully-shadowed and 1 means the pixel is not shadowed at all").flags(RenderPassReflection::Field::Flags::Optional);

    mOutputSize = RenderPassHelpers::calculateIOSize(RenderPassHelpers::IOSize::Fixed, { 1024, 1024 }, compileData.defaultTexDims);
    reflector.addOutput(kDst, "output texture").format(ResourceFormat::RGBA32Float).texture2D(0, 0);

    return reflector;
}

void DiffusePass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (!mpScene) return;

    auto pDst = renderData[kDst]->asTexture();

    // -------------------------------------------------------------------------------------------------------
    // Diffuse Pass：采用几何 Pass
    // ------------------------------------------------------------------------------------------------------

    mpDiffusePassVars->setSampler("gLinearSampler", mpLinearSampler);

    mpVisBuffer = renderData[kVisBuffer]->asTexture();
    mpDiffusePassVars->setTexture("gVisBuffer", mpVisBuffer);
    mpDiffusePassVars["PerFrameCB"]["gFrameCount"] = mFrameCount++;

    // depth buffer
    const auto& pDepthBuffer = renderData[kDepthBuffer]->asTexture();
    mpFbo->attachDepthStencilTarget(pDepthBuffer);

    // 输出 diffuse texture
    mpFbo->attachColorTarget(pDst,0);
    
    // clear view
    const auto& pRtv = mpFbo->getRenderTargetView(0).get();
    if (pRtv->getResource() != nullptr) pRenderContext->clearRtv(pRtv, float4(0));

    mpDiffusePassState->setFbo(mpFbo);
    mpScene->rasterize(pRenderContext, mpDiffusePassState.get(), mpDiffusePassVars.get());
}

void DiffusePass::renderUI(Gui::Widgets& widget)
{
}

void DiffusePass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
{
    mpScene = pScene;
    if (!mpScene)return;
    mpDiffusePassState->getProgram()->addDefines(mpScene->getSceneDefines());
    mpDiffusePassState->getProgram()->setTypeConformances(mpScene->getTypeConformances());
    mpDiffusePassVars = GraphicsVars::create(mpDiffusePassState->getProgram()->getReflector());
    createSampler();
}

void DiffusePass::createDiffusePass()
{
    // 写入 stencil 0x1：表示皮肤部分
    DepthStencilState::Desc desc;
    desc.setStencilEnabled(false);
    desc.setStencilWriteMask(0x1);
    desc.setDepthEnabled(true);
    desc.setDepthWriteMask(false);
    desc.setDepthFunc(DepthStencilState::Func::LessEqual);
    DepthStencilState::SharedPtr pDsc = DepthStencilState::create(desc);

    // 创建 shader
    GraphicsProgram::SharedPtr pProgram = GraphicsProgram::createFromFile(
        kDiffusePassFile, "vsMain", "psMain");
    mpDiffusePassState = GraphicsState::create();
    mpDiffusePassState->setProgram(pProgram);
    mpDiffusePassState->setDepthStencilState(pDsc);

    // 创建 FrameBuffer
    mpFbo = Fbo::create();
}

void DiffusePass::createSampler()
{
    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point);
    mpPointSampler = Sampler::create(samplerDesc);
    samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Point);
    mpLinearSampler = Sampler::create(samplerDesc);
}

DiffusePass::DiffusePass(): RenderPass(kInfo)
{
    createDiffusePass();
}
