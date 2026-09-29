add_library(KizuriRender STATIC src/Render.cpp src/RHI.cpp src/RHI_Null.cpp src/RHI_D3D11.cpp)
target_include_directories(KizuriRender PUBLIC include)
target_link_libraries(KizuriRender PUBLIC kizuri_imgui kizuri_imguizmo kizuri_cgltf kizuri_stb d3d11 dxgi)
