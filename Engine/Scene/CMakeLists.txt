add_library(KizuriScene STATIC src/Scene.cpp src/Selection.cpp src/Log.cpp src/EditQueue.cpp src/Picking.cpp)
target_include_directories(KizuriScene PUBLIC include)
target_link_libraries(KizuriScene PUBLIC KizuriCore)
