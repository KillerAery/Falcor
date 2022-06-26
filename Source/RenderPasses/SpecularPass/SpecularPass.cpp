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

const RenderPass::Info SpecularPass::kInfo { "SpecularPass", "Specular Lighting" };

namespace
{
    const char kVisBuffer[] = "visBuffer";
    const char kDepthBuffer[] = "depthBuffer";

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
    reflector.addInput(kDepthBuffer, "Depth Buffer");
    reflector.addInput(kVisBuffer, "Visibility buffer used for shadowing. Range is [0,1] where 0 means the pixel is fully-shadowed and 1 means the pixel is not shadowed at all").flags(RenderPassReflection::Field::Flags::Optional);

    mOutputSize = RenderPassHelpers::calculateIOSize(RenderPassHelpers::IOSize::Fixed, { 1024, 1024 }, compileData.defaultTexDims);
    reflector.addOutput(kDst, "output texture").format(ResourceFormat::RGBA32Float).texture2D(0, 0);

    return reflector;
}

void SpecularPass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (!mpScene) return;

    auto pDst = renderData[kDst]->asTexture();
    
}

void SpecularPass::renderUI(Gui::Widgets& widget)
{
    if (auto group = widget.group("SSS", true))
    {

    }
}

void SpecularPass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
{
    mpScene = pScene;
    if (!mpScene)return;
    createSampler();
}

void SpecularPass::createSpecularPass()
{
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
