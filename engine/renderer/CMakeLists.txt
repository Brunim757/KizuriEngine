set(KIZURI_RENDERER_SOURCES
    src/Camera.cpp
    src/ClusterGrid.cpp
    src/Meshes.cpp
    src/ForwardPlusRenderer.cpp
)

add_library(KizuriRenderer STATIC ${KIZURI_RENDERER_SOURCES})
target_include_directories(KizuriRenderer PUBLIC include)
find_package(directxmath CONFIG REQUIRED)
find_package(efsw CONFIG REQUIRED)
target_link_libraries(KizuriRenderer PUBLIC KizuriRHI KizuriCore directxmath::directxmath)
target_link_libraries(KizuriRenderer PRIVATE efsw::efsw)
target_compile_features(KizuriRenderer PUBLIC cxx_std_20)
kizuri_apply_compile_options(KizuriRenderer)

if(WIN32)
    target_compile_definitions(KizuriRenderer PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if(KIZURI_BUILD_TESTS)
    find_package(Catch2 3 CONFIG REQUIRED)
    add_executable(KizuriRendererTests
        tests/TestClusterGrid.cpp
        tests/TestCamera.cpp
    )
    target_link_libraries(KizuriRendererTests PRIVATE KizuriRenderer Catch2::Catch2WithMain)
    target_compile_features(KizuriRendererTests PRIVATE cxx_std_20)
    kizuri_apply_compile_options(KizuriRendererTests)
    add_test(NAME KizuriRendererTests COMMAND KizuriRendererTests)
endif()