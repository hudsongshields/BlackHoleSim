cmake_minimum_required(VERSION 3.26.0)
project(black_hole_sim  VERSION 1.0.0)
cmake_policy(SET CMP0072 NEW)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

add_executable(black_hole_sim
    src/config.h
    src/main.cpp
    dependencies/glad/glad.c
    src/LightRay.cpp
    src/Schwarzchild.cpp
)

target_include_directories(black_hole_sim PRIVATE 
    dependencies
    src
)

target_compile_definitions(black_hole_sim PRIVATE GLFW_INCLUDE_NONE)

target_link_libraries(black_hole_sim glfw OpenGL::GL)