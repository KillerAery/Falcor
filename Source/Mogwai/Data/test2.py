from falcor import *

def WindowsPath(s):
    return ("C:/Project/Falcor/Media/") + s

def render_graph_SSSRenderer():
    g = RenderGraph('SSSRenderer')
    loadRenderPassLibrary('AccumulatePass.dll')
    loadRenderPassLibrary('BSDFViewer.dll')
    loadRenderPassLibrary('Antialiasing.dll')
    loadRenderPassLibrary('BlitPass.dll')
    loadRenderPassLibrary('CSM.dll')
    loadRenderPassLibrary('DebugPasses.dll')
    loadRenderPassLibrary('PathTracer.dll')
    loadRenderPassLibrary('DepthPass.dll')
    loadRenderPassLibrary('DiffusePass.dll')
    loadRenderPassLibrary('DLSSPass.dll')
    loadRenderPassLibrary('ErrorMeasurePass.dll')
    loadRenderPassLibrary('SimplePostFX.dll')
    loadRenderPassLibrary('FLIPPass.dll')
    loadRenderPassLibrary('ForwardLightingPass.dll')
    loadRenderPassLibrary('GBuffer.dll')
    loadRenderPassLibrary('WhittedRayTracer.dll')
    loadRenderPassLibrary('ImageLoader.dll')
    loadRenderPassLibrary('MegakernelPathTracer.dll')
    loadRenderPassLibrary('MinimalPathTracer.dll')
    loadRenderPassLibrary('ModulateIllumination.dll')
    loadRenderPassLibrary('NRDPass.dll')
    loadRenderPassLibrary('OptixDenoiser.dll')
    loadRenderPassLibrary('PassLibraryTemplate.dll')
    loadRenderPassLibrary('PixelInspectorPass.dll')
    loadRenderPassLibrary('SkyBox.dll')
    loadRenderPassLibrary('RTXDIPass.dll')
    loadRenderPassLibrary('RTXGIPass.dll')
    loadRenderPassLibrary('SceneDebugger.dll')
    loadRenderPassLibrary('SDFEditor.dll')
    loadRenderPassLibrary('SpecularPass.dll')
    loadRenderPassLibrary('SSAO.dll')
    loadRenderPassLibrary('SSSPass.dll')
    loadRenderPassLibrary('SVGFPass.dll')
    loadRenderPassLibrary('TemporalDelayPass.dll')
    loadRenderPassLibrary('TestPasses.dll')
    loadRenderPassLibrary('ToneMapper.dll')
    loadRenderPassLibrary('Utils.dll')

    GBuffer = createPass("GBufferRaster")
    g.addPass(GBuffer, "GBuffer")

    CSM = createPass('CSM', {'mapSize': uint2(4096,4096), 'visibilityBufferSize': uint2(0,0), 'cascadeCount': 4, 'visibilityMapBitsPerChannel': 32, 'kSdsmReadbackLatency': 1, 'blurWidth': 5, 'blurSigma': 2.0})
    g.addPass(CSM, 'CSM')

    SSSPass = createPass('SSSPass', {'uScale': 0.005, 'vScale': 0.005, 'd': float3(0.436000,0.227000,0.131000)})
    g.addPass(SSSPass, 'SSSPass')

    BlitPass = createPass('BlitPass', {'filter': SamplerFilter.Linear})
    g.addPass(BlitPass, 'BlitPass')

    ToneMapper = createPass('ToneMapper', {'outputSize': IOSize.Default, 'useSceneMetadata': True, 'exposureCompensation': 0.0, 'autoExposure': False, 'filmSpeed': 100.0, 'whiteBalance': False, 'whitePoint': 6500.0, 'operator': ToneMapOp.Aces, 'clamp': True, 'whiteMaxLuminance': 1.0, 'whiteScale': 11.199999809265137, 'fNumber': 1.0, 'shutter': 1.0, 'exposureMode': ExposureMode.AperturePriority})
    g.addPass(ToneMapper, 'ToneMapper')

    SkyBox = createPass('SkyBox', {'loadAsSrgb': True, 'filter': SamplerFilter.Linear})
    g.addPass(SkyBox, 'SkyBox')

    DiffusePass = createPass('DiffusePass')
    g.addPass(DiffusePass, 'DiffusePass')

    SpecularPass = createPass('SpecularPass')
    g.addPass(SpecularPass, 'SpecularPass')

    SSAO = createPass('SSAO', {'aoMapSize': uint2(1024,1024), 'kernelSize': 16, 'noiseSize': uint2(16,16), 'radius': 0.10000000149011612, 'distribution': SampleDistribution.CosineHammersley, 'blurWidth': 5, 'blurSigma': 2.0})
    g.addPass(SSAO, 'SSAO')

    RTXGIPass = createPass("RTXGIPass", {'useVBuffer': False})
    g.addPass(RTXGIPass, "RTXGIPass")

    Composite0 = createPass('Composite', {'mode': CompositeMode.Add, 'scaleA': 1.0, 'scaleB': 1.0, 'outputFormat': ResourceFormat.RGBA32Float})
    g.addPass(Composite0, 'Composite0')
    
    Composite1 = createPass('Composite', {'mode': CompositeMode.Add, 'scaleA': 1.0, 'scaleB': 1.0, 'outputFormat': ResourceFormat.RGBA32Float})
    g.addPass(Composite1, 'Composite1')

    # CSM
    g.addEdge('GBuffer.depth', 'CSM.depth')

    # SkyBox
    g.addEdge('GBuffer.depth', 'SkyBox.depth')

    # Diffuse Pass
    g.addEdge('CSM.visibility', 'DiffusePass.visBuffer')
    g.addEdge('GBuffer.depth', 'DiffusePass.depthBuffer')

    # SSS Pass
    g.addEdge('GBuffer.depth', 'SSSPass.depthBuffer')
    g.addEdge('DiffusePass.dst', 'SSSPass.texDiffuse')

    # SpecularPass
    g.addEdge('SSSPass.dst', 'SpecularPass.irradianceMap')
    g.addEdge('GBuffer.depth', 'SpecularPass.depthBuffer')
    g.addEdge('CSM.visibility', 'SpecularPass.visBuffer')

    # SSAO
    g.addEdge('SpecularPass.normals', 'SSAO.normals')
    g.addEdge('SpecularPass.dst', 'SSAO.colorIn')
    g.addEdge('GBuffer.depth', 'SSAO.depth')

    # RTXGI
    g.addEdge("GBuffer.posW", "RTXGIPass.posW")
    g.addEdge("GBuffer.normW", "RTXGIPass.normalW")
    g.addEdge("GBuffer.tangentW", "RTXGIPass.tangentW")
    g.addEdge("GBuffer.faceNormalW", "RTXGIPass.faceNormalW")
    g.addEdge("GBuffer.texC", "RTXGIPass.texC")
    g.addEdge("GBuffer.texGrads", "RTXGIPass.texGrads") # This input is optional
    g.addEdge("GBuffer.mtlData", "RTXGIPass.mtlData")
    g.addEdge("GBuffer.depth", "RTXGIPass.depth") # This input is optional

    # Composite
    g.addEdge('SkyBox.target', 'Composite0.A')
    g.addEdge('SSAO.colorOut', 'Composite0.B')
    g.addEdge('Composite0.out', 'Composite1.A')
    g.addEdge("RTXGIPass.output", "Composite1.B")

    # ToneMapper
    g.addEdge('Composite1.out', 'ToneMapper.src')

    # BlitPass
    g.addEdge('ToneMapper.dst', 'BlitPass.src')
    g.markOutput('BlitPass.dst')
    return g

SSSRenderer = render_graph_SSSRenderer()
try: m.addGraph(SSSRenderer)
except NameError: None
