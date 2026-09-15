if(NOT TARGET KizuriCore)
    add_library(KizuriCore STATIC
        src/Memory/Allocator.cpp
        src/Memory/LinearAllocator.cpp
        src/Memory/StackAllocator.cpp
        src/Memory/PoolAllocator.cpp
        src/Memory/FreeListAllocator.cpp
        src/Memory/MemoryTracker.cpp
        src/Jobs/JobSystem.cpp
        src/Jobs/JobCounter.cpp
        src/Platform/Window.cpp
        src/Platform/Input.cpp
        src/Platform/Timer.cpp
        src/Diagnostics/Log.cpp
        src/Diagnostics/Profiler.cpp
    )
    target_include_directories(KizuriCore PUBLIC include)
    target_link_libraries(KizuriCore PUBLIC spdlog::spdlog)
    target_compile_features(KizuriCore PUBLIC cxx_std_20)
    kizuri_apply_compile_options(KizuriCore)
endif()

if(KIZURI_BUILD_TESTS AND NOT TARGET KizuriCoreTests)
    find_package(Catch2 3 CONFIG REQUIRED)
    add_executable(KizuriCoreTests
        tests/TestAllocators.cpp
        tests/TestJobSystem.cpp
        tests/TestMemoryTracker.cpp
        tests/TestProfiler.cpp
    )
    target_link_libraries(KizuriCoreTests PRIVATE KizuriCore Catch2::Catch2WithMain)
    target_compile_features(KizuriCoreTests PRIVATE cxx_std_20)
    kizuri_apply_compile_options(KizuriCoreTests)
    include(CTest)
    add_test(NAME KizuriCoreTests COMMAND KizuriCoreTests)
endif()