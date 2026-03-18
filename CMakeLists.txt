cmake_minimum_required(VERSION 3.20)

project(
    voxeltest
    VERSION 0.1
    LANGUAGES C
)

set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(glfw3 CONFIG REQUIRED)

add_executable(
    voxeltest
    src/main.c
)

target_include_directories(
    voxeltest 
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/external/jsk
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/stb/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(
    voxeltest 
    PRIVATE
    glfw
)

if(APPLE)
    target_link_libraries(
        voxeltest 
        PRIVATE
        "-framework OpenGL"
    )
endif()

target_sources(
    voxeltest 
    PRIVATE
    external/glad/src/glad.c
)

target_compile_definitions(
    voxeltest
    PRIVATE
    GLFW_INCLUDE_NONE
    GL_SILENCE_DEPRECATION
)
