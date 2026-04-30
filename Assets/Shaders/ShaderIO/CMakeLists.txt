add_library(Shader INTERFACE
        shader_io.h
        resources_io.slang
)
target_include_directories(Shader INTERFACE ./)
