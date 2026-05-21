cmake_minimum_required(VERSION 3.26.0)
project(black_hole_sim VERSION 1.0.0 LANGUAGES C CXX CUDA)
cmake_policy(SET CMP0072 NEW)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(CUDAToolkit REQUIRED)

add_executable(black_hole_sim
    src/config.hpp
    src/main.cpp
    dependencies/glad/glad.c
    src/stb_image_impl.cpp
    src/LightRay.cpp
    src/Schwarzchild.cpp
    src/myRender.cpp
    dependencies/stb_image.h
)

set_source_files_properties(src/myRender.cpp PROPERTIES LANGUAGE CUDA)
set_source_files_properties(src/LightRay.cpp PROPERTIES LANGUAGE CUDA)
set_source_files_properties(src/Schwarzchild.cpp PROPERTIES LANGUAGE CUDA)

target_include_directories(black_hole_sim PRIVATE 
    dependencies
    src
    src/background
    ${CUDAToolkit_INCLUDE_DIRS}
)

target_compile_definitions(black_hole_sim PRIVATE GLFW_INCLUDE_NONE)
target_compile_definitions(black_hole_sim PRIVATE SHADER_DIR=\"${CMAKE_SOURCE_DIR}/src/shaders\")
target_compile_definitions(black_hole_sim PRIVATE ASSET_DIR=\"${CMAKE_SOURCE_DIR}/src/assets\")

set_target_properties(black_hole_sim PROPERTIES CUDA_SEPARABLE_COMPILATION ON)

target_link_libraries(black_hole_sim glfw OpenGL::GL CUDA::cudart)