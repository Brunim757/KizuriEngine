set(KIZURI_RHI_SOURCES
    src/D3D11Device.cpp
    src/D3D11Resources.cpp
    src/D3D11CommandList.cpp
    src/D3D11SwapChain.cpp
    src/RhiFactory.cpp
)

add_library(KizuriRHI STATIC ${KIZURI_RHI_SOURCES})
target_include_directories(KizuriRHI PUBLIC include)
target_link_libraries(KizuriRHI PRIVATE KizuriCore)
target_compile_features(KizuriRHI PUBLIC cxx_std_20)
kizuri_apply_compile_options(KizuriRHI)

if(WIN32)
    target_compile_definitions(KizuriRHI PRIVATE UNICODE _UNICODE)
    target_link_libraries(KizuriRHI PRIVATE d3d11 dxgi d3dcompiler dxguid)
endif()