add_plugin(VolumeTracer)

target_sources(VolumeTracer PRIVATE
    VolumeTracer.cpp
    VolumeTracer.h
)

target_source_group(VolumeTracer "RenderPasses")
