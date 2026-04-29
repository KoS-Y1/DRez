cmake_minimum_required(VERSION 3.26)
project(DRez)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(directx-headers CONFIG REQUIRED)
find_package(directxmath CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(sdl3 CONFIG REQUIRED)
find_package(simdjson CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(slang CONFIG REQUIRED)

add_subdirectory(Core)
add_subdirectory(Utils)

add_executable(DRez
        main.cpp
)

target_link_libraries(DRez PUBLIC
        Window
)