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
#include "SSSPass.h"

const RenderPass::Info SSSPass::kInfo { "SSSPass", "基于屏幕空间的SSS方法（burley normalize）" };

namespace
{
    const char kTexDiffuse[] = "texDiffuse";
    const char kDepthBuffer[] = "depthBuffer";

    const char kDst[]    = "dst";

    const char kUScale[] = "uScale";
    const char kVScale[] = "vScale";
    const char kD[] = "d";

    const char kSSSPassFile[] = "RenderPasses/SSSPass/SSSPass.hlsl";
}

// Don't remove this. it's required for hot-reload to function properly
extern "C" FALCOR_API_EXPORT const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" FALCOR_API_EXPORT void getPasses(Falcor::RenderPassLibrary & lib)
{
    lib.registerPass(SSSPass::kInfo, SSSPass::create);
}

SSSPass::SharedPtr SSSPass::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    auto pThis = SSSPass::SharedPtr(new SSSPass());
    return pThis;
}

Dictionary SSSPass::getScriptingDictionary()
{
    Dictionary d;
    d[kUScale] = mUScale;
    d[kVScale] = mVScale;
    d[kD] = mD;
    return d;
}

RenderPassReflection SSSPass::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;
    reflector.addInput(kTexDiffuse, "Diffuse Texture");
    reflector.addInput(kDepthBuffer, "Depth Buffer");

    mOutputSize = RenderPassHelpers::calculateIOSize(RenderPassHelpers::IOSize::Fixed, { 1024, 1024 }, compileData.defaultTexDims);
    reflector.addOutput(kDst, "Output texture").format(ResourceFormat::RGBA32Float).texture2D(0, 0);

    return reflector;
}

void SSSPass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (!mpScene) return;

    auto pDst = renderData[kDst]->asTexture();

    // -------------------------------------------------------------------------------------------------------
    // SSS Pass：采用屏幕空间 Pass
    // -------------------------------------------------------------------------------------------------------

    mpSSSPass["gTexDiffuse"] = renderData[kTexDiffuse]->asTexture();
    mpSSSPass["gDepthBuffer"] = renderData[kDepthBuffer]->asTexture();
    mpSSSPass["gLinearSampler"] = mpLinearSampler;

    mpSSSPass->getRootVar()["PerFrameCB"]["gD"] = mD;
    mpSSSPass->getRootVar()["PerFrameCB"]["gInverseScreenAndProj"] = static_cast<float4x4>(mpScene->getCamera()->getInvViewProjMatrix());
    mpSSSPass->getRootVar()["PerFrameCB"]["gUScale"] = mUScale;
    mpSSSPass->getRootVar()["PerFrameCB"]["gVScale"] = mVScale;

    mpFbo->attachColorTarget(pDst, 0);

    // clear view
    const auto& pRtv = mpFbo->getRenderTargetView(0).get();
    if (pRtv->getResource() != nullptr) pRenderContext->clearRtv(pRtv, float4(0));

    mpSSSPass->execute(pRenderContext, mpFbo);
}

void SSSPass::renderUI(Gui::Widgets& widget)
{
    if (auto group = widget.group("SSS", true))
    {
        group.var("uScale", mUScale, 0.f, 1.f, 0.00001f, false, "%.6f");
        group.var("vScale", mVScale, 0.f, 1.f, 0.00001f, false, "%.6f");
        group.var("d:red", mD.r, 0.f, 1.f, 0.00001f, false, "%.6f");
        group.var("d:green", mD.g, 0.f, 1.f, 0.00001f, false, "%.6f");
        group.var("d:blue", mD.b, 0.f, 1.f, 0.00001f, false, "%.6f");
    }
}

void SSSPass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
{
    mpScene = pScene;
    if (!mpScene)return;
    createSampler();
}


void SSSPass::createSSSPass()
{
    Program::DefineList defines;
    mpSSSPass = FullScreenPass::create(kSSSPassFile, defines);

    // stencil test 0x1
    DepthStencilState::Desc desc;
    desc.setStencilEnabled(true);
    desc.setStencilReadMask(0x1);
    desc.setStencilFunc(DepthStencilState::Face::FrontAndBack, DepthStencilState::Func::Equal);
    desc.setDepthWriteMask(false);
    desc.setDepthEnabled(false);
     
    DepthStencilState::SharedPtr pDsc = DepthStencilState::create(desc);
    mpSSSPass->getState()->setDepthStencilState(pDsc);

    // 创建 FrameBuffer
    mpFbo = Fbo::create();
}

void SSSPass::createSampler()
{
    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Point);
    mpLinearSampler = Sampler::create(samplerDesc);
}

SSSPass::SSSPass(): RenderPass(kInfo)
{
    createSSSPass();
}
