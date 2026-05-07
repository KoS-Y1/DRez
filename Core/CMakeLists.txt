add_library(DX
        DX/DxApp.cpp
        DX/DxApp.h
        DX/DXGraphicsPipeline.cpp
        DX/DXGraphicsPipeline.h
        DX/DXPipelineConfig.cpp
        DX/DXPipelineConfig.h
        DX/VertexFormats.cpp
        DX/VertexFormats.h
        DX/DXBuffer.cpp
        DX/DXBuffer.h
        DX/DXTexture.cpp
        DX/DXTexture.h
        DX/DXUtils.cpp
        DX/DXUtils.h
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
        Rendering
)

add_library(ShaderCompiler
        ShaderCompiler/ShaderCompiler.cpp
        ShaderCompiler/ShaderCompiler.h
)
target_include_directories(ShaderCompiler PUBLIC ShaderCompiler)
target_link_libraries(ShaderCompiler PUBLIC
        slang::slang
        d3d12
        Utils
        Files
        Debug
)

add_library(Rendering
        Rendering/Renderer.cpp
        Rendering/Renderer.h
)
target_include_directories(Rendering PUBLIC Rendering)
target_link_libraries(Rendering PUBLIC
        Microsoft::DirectXMath
        DX
        Resources
)

add_library(Resources
        Resources/ResourceManager.cpp
        Resources/ResourceManager.h
        Resources/Mesh.cpp
        Resources/Mesh.h
)
target_include_directories(Resources PUBLIC Resources)
target_link_libraries(Resources PUBLIC
        fastgltf::fastgltf
        Debug
        ThreadPool
        Utils
        Files
        DX
        Shader
)