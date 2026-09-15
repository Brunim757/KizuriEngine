cmake_minimum_required(VERSION 3.24)

project(KizuriEngine
    VERSION 0.1.0
    DESCRIPTION "Kizuri Engine - AAA open-world anime-style game engine"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/KizuriCompileOptions.cmake)

option(KIZURI_BUILD_TESTS "Build unit tests" OFF)
option(KIZURI_WITH_EDITOR "Link editor tooling (requires tools phase)" OFF)

if(KIZURI_WITH_EDITOR)
    message(FATAL_ERROR "KIZURI_WITH_EDITOR belongs to Fase 6. Roadmap is phase-locked.")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_subdirectory(engine/core)
add_subdirectory(app)