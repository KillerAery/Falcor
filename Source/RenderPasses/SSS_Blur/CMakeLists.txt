add_plugin(SSS_Blur)

target_sources(SSS_Blur PRIVATE
    SSS_Blur.cpp
    SSS_Blur.h
)

target_source_group(SSS_Blur "RenderPasses")
