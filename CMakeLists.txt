cmake_minimum_required(VERSION 3.24)

project(KizuriEngine
    VERSION 0.2.0
    DESCRIPTION "Kizuri Engine - AAA open-world anime-style game engine"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/KizuriCompileOptions.cmake)

option(KIZURI_BUILD_TESTS "Build unit tests" OFF)
option(KIZURI_BUILD_TOOLS "Build tools" ON)

if(KIZURI_WITH_EDITOR)
    message(FATAL_ERROR "KIZURI_WITH_EDITOR belongs to Fase 6.")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

find_package(spdlog CONFIG REQUIRED)

add_subdirectory(engine/core)
add_subdirectory(engine/rhi)
add_subdirectory(engine/renderer)
add_subdirectory(app)

if(KIZURI_BUILD_TOOLS)
    add_subdirectory(tools/kizuri-shader-validator)
endif()