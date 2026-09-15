add_executable(KizuriShaderValidator main.cpp)
target_compile_features(KizuriShaderValidator PRIVATE cxx_std_20)
kizuri_apply_compile_options(KizuriShaderValidator)

if(WIN32)
    target_compile_definitions(KizuriShaderValidator PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(KizuriShaderValidator PRIVATE d3dcompiler)
endif()