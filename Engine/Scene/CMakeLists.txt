add_library(KizuriScene STATIC src/Scene.cpp src/Selection.cpp src/Log.cpp src/EditQueue.cpp src/Picking.cpp src/Reflection.cpp src/SceneSerializer.cpp)
target_include_directories(KizuriScene PUBLIC include)
target_link_libraries(KizuriScene PUBLIC KizuriCore)
