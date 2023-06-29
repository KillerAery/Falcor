add_plugin(SSS_Shading)

target_sources(SSS_Shading PRIVATE
    SSS_Shading.cpp
    SSS_Shading.h
)

target_source_group(SSS_Shading "RenderPasses")
