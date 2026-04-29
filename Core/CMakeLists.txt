add_library(DX
        DX/DxApp.cpp
        DX/DxApp.h
        DX/DXGraphicsPipeline.cpp
        DX/DXGraphicsPipeline.h
        DX/DXPipelineConfig.cpp
        DX/DXPipelineConfig.h
        DX/VertexFormats.cpp
        DX/VertexFormats.h
)
target_include_directories(DX PUBLIC DX)
target_link_libraries(DX PUBLIC
        Microsoft::DirectX-Headers
        d3d12
        dxgi
        dxguid
        Microsoft::DirectXMath
        Debug
        Files
        ShaderCompiler
)

add_library(Window
        Window/Window.cpp
        Window/Window.h
)
target_include_directories(Window PUBLIC Window)
target_link_libraries(Window PUBLIC
        SDL3::SDL3
        Debug
        DX
)

add_library(ShaderCompiler
        ShaderCompiler/ShaderCompiler.cpp
        ShaderCompiler/ShaderCompiler.h
)
target_include_directories(ShaderCompiler PUBLIC ShaderCompiler)
target_link_libraries(ShaderCompiler PUBLIC
        slang::slang
        Utils
        Files
        Debug
)