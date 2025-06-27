cmake_minimum_required(VERSION 3.16)
project(TarotGame)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/ENGINE)

# Collect all source and header files from ENGINE directory
file(GLOB ENGINE_SRC
    ${CMAKE_SOURCE_DIR}/ENGINE/*.cpp
    ${CMAKE_SOURCE_DIR}/ENGINE/*.hpp
)

# Create the executable target
add_executable(engine ${ENGINE_SRC})

# Include local headers
target_include_directories(engine PRIVATE ${CMAKE_SOURCE_DIR}/ENGINE)

# VCPKG-managed dependencies (those that provide CMake config files)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
find_package(SDL2_mixer CONFIG REQUIRED)
find_package(SDL2_ttf CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(sdl2-gfx CONFIG REQUIRED)



# SDL2_gfx does NOT provide a config file, so we locate it manually
set(SDL2_GFX_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/include/SDL2")
set(SDL2_GFX_LIBRARY "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/lib/SDL2_gfx.lib")

if(NOT EXISTS "${SDL2_GFX_INCLUDE_DIR}/SDL2_gfxPrimitives.h" OR NOT EXISTS "${SDL2_GFX_LIBRARY}")
    message(FATAL_ERROR "SDL2_gfx not found. Make sure it is installed via vcpkg.")
endif()

# Add the SDL2_gfx include path
target_include_directories(engine PRIVATE ${SDL2_GFX_INCLUDE_DIR})

# Link all libraries
target_link_libraries(engine
    PRIVATE
        nlohmann_json::nlohmann_json
        SDL2::SDL2 SDL2::SDL2main
        SDL2_image::SDL2_image
        SDL2_mixer::SDL2_mixer
        SDL2_ttf::SDL2_ttf
        glad::glad
        OpenGL::GL
        ${SDL2_GFX_LIBRARY}
)
