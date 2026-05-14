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
        DX/DXShaderResourceView.cpp
        DX/DXShaderResourceView.h
        DX/DXUnorderedAccessView.cpp
        DX/DXUnorderedAccessView.h
        DX/DXUtils.cpp
        DX/DXUtils.h
        DX/DXComputePipeline.cpp
        DX/DXComputePipeline.h
        DX/DXDebug.cpp
        DX/DXDebug.h
        DX/DXTimestamps.cpp
        DX/DXTimestamps.h
)
target_include_directories(DX PUBLIC DX)
target_link_libraries(DX PUBLIC
        Microsoft::DirectX-Headers
        d3d12
        dxgi
        dxguid
        imgui::imgui
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
        imgui::imgui
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
        Rendering/Camera.cpp
        Rendering/Camera.h
        Rendering/Pass.cpp
        Rendering/Pass.h
        Rendering/ShadowPass.cpp
        Rendering/ShadowPass.h
        Rendering/GbufferPass.cpp
        Rendering/GbufferPass.h
        Rendering/DeferredPass.cpp
        Rendering/DeferredPass.h
        Rendering/SkyboxPass.cpp
        Rendering/SkyboxPass.h
        Rendering/BlitPass.cpp
        Rendering/BlitPass.h
)
target_include_directories(Rendering PUBLIC Rendering)
target_link_libraries(Rendering PUBLIC
        Microsoft::DirectXMath
        DX
        Resources
        Files
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