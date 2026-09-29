add_library(KizuriCore STATIC src/Core.cpp src/Allocator.cpp src/JobSystem.cpp src/Origin.cpp src/Window.cpp src/Input.cpp)
target_include_directories(KizuriCore PUBLIC include)
target_link_libraries(KizuriCore PUBLIC DirectXMath enkiTS libzstd_static flatbuffers)
