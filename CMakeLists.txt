cmake_minimum_required(VERSION 3.16)
project(TarotGame)

set(CMAKE_CXX_STANDARD 17)

# Output executable to ENGINE folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/ENGINE)

# Tell CMake to use vcpkg
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "")
endif()

# Source files
file(GLOB ENGINE_SRC
    "ENGINE/*.cpp"
)

# Include and link SDL2 + JSON
add_executable(engine ${ENGINE_SRC})

target_link_libraries(engine
    SDL2
    SDL2main
    SDL2_image
)

# Include headers
target_include_directories(engine PRIVATE
    ${CMAKE_SOURCE_DIR}/ENGINE
)
