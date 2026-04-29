add_library(Utils INTERFACE
        Singleton.h
)
target_include_directories(Utils INTERFACE .)

add_library(Debug INTERFACE
        Debug/Debug.h
)
target_include_directories(Debug INTERFACE Debug)
target_link_libraries(Debug INTERFACE
        spdlog::spdlog
)

add_library(ThreadPool
        ThreadPool/ThreadPool.cpp
        ThreadPool/ThreadPool.h
)
target_include_directories(ThreadPool PUBLIC ThreadPool)
target_link_libraries(ThreadPool PUBLIC
        Debug
        Utils
)

add_library(Files
        Files/FileSystem.cpp
        Files/FileSystem.h
        Files/JsonFile.h
)
target_include_directories(Files PUBLIC Files)
target_link_libraries(Files PUBLIC
        simdjson::simdjson
        Debug
)