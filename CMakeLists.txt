cmake_minimum_required(VERSION 3.16)
project(TarotGame)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/ENGINE)

# Collect source files
file(GLOB ENGINE_SRC
    ${CMAKE_SOURCE_DIR}/ENGINE/*.cpp
    ${CMAKE_SOURCE_DIR}/ENGINE/*.hpp
)

add_executable(engine ${ENGINE_SRC})

target_include_directories(engine PRIVATE ${CMAKE_SOURCE_DIR}/ENGINE)

# VCPKG dependencies
find_package(nlohmann_json CONFIG REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
find_package(SDL2_mixer CONFIG REQUIRED)  # <- ADD THIS LINE

# Link all libraries
target_link_libraries(engine
    PRIVATE
        nlohmann_json::nlohmann_json
        SDL2::SDL2 SDL2::SDL2main
        SDL2_image::SDL2_image
        SDL2_mixer::SDL2_mixer  # <- ADD THIS LINE
)
