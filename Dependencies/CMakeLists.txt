add_library(Stb stb_image.c)

target_include_directories(Stb PRIVATE ${Stb_INCLUDE_DIR})

set_target_properties(Stb PROPERTIES FOLDER "Dependencies")