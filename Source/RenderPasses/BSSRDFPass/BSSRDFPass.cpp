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
#include "BSSRDFPass.h"

const RenderPass::Info BSSRDFPass::kInfo { "BSSRDFPass", "包含 3 个 pass：Diffuse Pass->SSS Pass->Specular Pass" };

namespace
{
    const char kTexAlbedo[] = "texAlbedo";
    const char kTexNormal[] = "texNormal";
    const char kTexRoughness[] = "texRoughness";
    const char kTexCavity[] = "texCavity";
    const char kVisBuffer[] = "visBuffer";

    const char kTexDiffuse[] = "texDiffuse";
    const char kTexDepth[] = "texDepth";
    const char kD[] = "d";

    const char kDst[]    = "dst";

    const char kUScale[] = "uScale";
    const char kVScale[] = "vScale";


    const char kDiffusePassFile[] = "RenderPasses/BSSRDFPass/DiffusePass.hlsl";
    const char kSSSPassFile[] = "RenderPasses/BSSRDFPass/SSSPass.hlsl";
    const char kSpecularPassFile[] = "RenderPasses/BSSRDFPass/SpecularPass.hlsl";

}

// Don't remove this. it's required for hot-reload to function properly
extern "C" FALCOR_API_EXPORT const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" FALCOR_API_EXPORT void getPasses(Falcor::RenderPassLibrary& lib)
{
    lib.registerPass(BSSRDFPass::kInfo, BSSRDFPass::create);
}

BSSRDFPass::SharedPtr BSSRDFPass::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    return BSSRDFPass::SharedPtr(new BSSRDFPass());
}

Dictionary BSSRDFPass::getScriptingDictionary()
{
    Dictionary d;
    d[kUScale] = mUScale;
    d[kVScale] = mVScale;
    d[kD] = mD;
    // d[kTexAlbedo] = mpTexAlbedo;
    // d[kTexNormal] = mpTexNormal;
    // d[kTexRoughness] = mpTexRoughness;
    // d[kTexCavity] = mpTexCavity;
    return d;
}

RenderPassReflection BSSRDFPass::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;
    reflector.addInput(kTexAlbedo,"");
    reflector.addInput(kTexCavity,"");
    reflector.addInput(kTexNormal,"");
    reflector.addInput(kTexRoughness,"");
    reflector.addInput(kVisBuffer, "Visibility buffer used for shadowing. Range is [0,1] where 0 means the pixel is fully-shadowed and 1 means the pixel is not shadowed at all").flags(RenderPassReflection::Field::Flags::Optional);

    mOutputSize = RenderPassHelpers::calculateIOSize(RenderPassHelpers::IOSize::Fixed, { 1024, 1024 }, compileData.defaultTexDims);
    reflector.addOutput(kDst, "output texture").texture2D(mOutputSize.x, mOutputSize.y);

    return reflector;
}

void BSSRDFPass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (!mpScene) return;
    // -------------------------------------------------------------------------------------------------------
    // Diffuse Pass：采用几何 Pass
    // ------------------------------------------------------------------------------------------------------    mpDiffusePassVars->setSampler("gLinearSampler", mpLinearSampler);

    mpTexAlbedo = renderData[kTexAlbedo]->asTexture();
    mpDiffusePassVars->setTexture("gTexAlbedo", mpTexAlbedo);
    mpTexNormal = renderData[kTexNormal]->asTexture();
    mpDiffusePassVars->setTexture("gTexNormal", mpTexNormal);
    mpTexRoughness = renderData[kTexRoughness]->asTexture();
    mpDiffusePassVars->setTexture("gTexRoughness", mpTexRoughness);
    mpTexCavity = renderData[kTexCavity]->asTexture();
    mpDiffusePassVars->setTexture("gTexCavity", mpTexCavity);
    mpVisBuffer = renderData[kVisBuffer]->asTexture();
    mpDiffusePassVars->setTexture("gVisBuffer", mpVisBuffer);

    mpDiffusePassVars["PerFrameCB"]["gFrameCount"] = mFrameCount++;
    FALCOR_ASSERT(mpDiffusePassState && mpDiffusePassVars);

    // 输出 diffuse texture
    Texture::SharedPtr pTexDiffuse = Texture::create2D(
        mOutputSize.x, mOutputSize.y,
        ResourceFormat::RGBA32Float, 1, 1, nullptr, Resource::BindFlags::RenderTarget | Resource::BindFlags::ShaderResource);
    mpDiffuseFbo->attachColorTarget(pTexDiffuse,0);
    // 输出 depth texture
    Texture::SharedPtr pTexDepth = Texture::create2D(
        mOutputSize.x, mOutputSize.y,
        ResourceFormat::D32Float, 1, 1, nullptr, Resource::BindFlags::ShaderResource | Resource::BindFlags::DepthStencil);
    mpDiffuseFbo->attachDepthStencilTarget(pTexDepth);

    mpDiffusePassState->setFbo(mpDiffuseFbo);
    mpScene->rasterize(pRenderContext, mpDiffusePassState.get(), mpDiffusePassVars.get());


    // -------------------------------------------------------------------------------------------------------
    // SSS Pass：采用屏幕空间 Pass
    // -------------------------------------------------------------------------------------------------------
    auto pDst = renderData[kDst]->asTexture();

    FALCOR_ASSERT(pTexDiffuse && pTexDepth && pDst);
    // Issue warning if image will be resampled. The render pass supports this but image quality may suffer.
    if (pTexDiffuse->getWidth() != pDst->getWidth() || pTexDiffuse->getHeight() != pDst->getHeight() ||
        pTexDepth->getWidth() != pDst->getWidth() || pTexDepth->getHeight() != pDst->getHeight()
        )
    {
        logWarning("BSSRDF pass I/O has different dimensions. The image will be resampled.");
    }

    mpSSSPass["gTexDiffuse"] = pTexDiffuse;
    mpSSSPass["gTexDepth"] = pTexDepth;
    mpSSSPass["gTexDiffuseSampler"] = mpLinearSampler;
    mpSSSPass["gTexDepthSampler"] = mpPointSampler;

    SSSParams params;
    params.InverseScreenAndProj = static_cast<float4x4>(
            mpScene->getCamera()->getInvViewProjMatrix()
        );
    params.uScale = mUScale;
    params.vScale = mVScale;
    params.d = mD;
    mpSSSPass->getRootVar()["PerFrameCB"]["gParams"].setBlob(&params, sizeof(params));

    mpSSSFbo->attachColorTarget(pDst, 0);
    mpSSSPass->execute(pRenderContext, mpSSSFbo);
}

void BSSRDFPass::renderUI(Gui::Widgets& widget)
{
    if (auto bssrdfGroup = widget.group("BSSRDF", true))
    {
        bssrdfGroup.var("uScale", mUScale, 0.01f, 10.f, 0.1f, false, "%.1f");
        bssrdfGroup.var("vScale", mVScale, 0.01f, 10.f, 0.1f, false, "%.1f");
        bssrdfGroup.var("d", mD, 0.1f, 100.0f, 0.1f, false, "%.1f");
    }
}

void BSSRDFPass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
{
    mpScene = pScene;
    if (!mpScene)return;
    mpDiffusePassState->getProgram()->addDefines(mpScene->getSceneDefines());
    mpDiffusePassState->getProgram()->setTypeConformances(mpScene->getTypeConformances());
    mpDiffusePassVars = GraphicsVars::create(mpDiffusePassState->getProgram()->getReflector());
    createSampler();
}

void BSSRDFPass::createDiffusePass()
{
    // 写入 stencil 0x1：表示皮肤部分
    DepthStencilState::Desc desc;
    desc.setStencilWriteMask(0x1);
    desc.setDepthWriteMask(true).setDepthFunc(DepthStencilState::Func::LessEqual);
    DepthStencilState::SharedPtr pDsc = DepthStencilState::create(desc);

    // 创建 shader
    GraphicsProgram::SharedPtr pProgram = GraphicsProgram::createFromFile(
        kDiffusePassFile, "vsMain", "psMain");
    mpDiffusePassState = GraphicsState::create();
    mpDiffusePassState->setProgram(pProgram);
    mpDiffusePassState->setDepthStencilState(pDsc);

    // 创建 FrameBuffer
    mpDiffuseFbo = Fbo::create();
}

void BSSRDFPass::createSSSPass()
{
    Program::DefineList defines;
    mpSSSPass = FullScreenPass::create(kSSSPassFile, defines);

    // stencil test 0x1
    DepthStencilState::Desc desc;
    desc.setStencilEnabled(true);
    desc.setStencilReadMask(0x1);
    desc.setDepthWriteMask(false);
    desc.setDepthEnabled(false);
    DepthStencilState::SharedPtr pDsc = DepthStencilState::create(desc);
    mpSSSPass->getState()->setDepthStencilState(pDsc);

    // 创建 FrameBuffer
    mpSSSFbo = Fbo::create();
}

void BSSRDFPass::createSpecularPass()
{
}

void BSSRDFPass::createSampler()
{
    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point);
    mpPointSampler = Sampler::create(samplerDesc);
    samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Point);
    mpLinearSampler = Sampler::create(samplerDesc);
}

BSSRDFPass::BSSRDFPass(): RenderPass(kInfo)
{
    createDiffusePass();
    createSSSPass();
    createSpecularPass();
}
