cmake_minimum_required(VERSION 3.26)
project(DRez)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    add_compile_definitions(NOMINMAX)
endif()

find_package(directx-headers CONFIG REQUIRED)
find_package(directx-dxc CONFIG REQUIRED)
find_package(directxmath CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(sdl3 CONFIG REQUIRED)
find_package(simdjson CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(slang CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(fastgltf CONFIG REQUIRED)

add_subdirectory(Core)
add_subdirectory(Utils)
add_subdirectory(Dependencies)
add_subdirectory(Assets/Shaders/ShaderIO)

add_executable(DRez
        main.cpp
)

target_link_libraries(DRez PUBLIC
        Window
        Resources
)

# dxil.dll signs DXIL produced by Slang at runtime; D3D12 rejects unsigned blobs.
add_custom_command(TARGET DRez POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:Microsoft::DXIL>
        $<TARGET_FILE_DIR:DRez>
)