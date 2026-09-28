add_library(quest_probe SHARED src/quest_probe.cpp src/quest_xr_runtime.cpp)
target_include_directories(quest_probe PRIVATE include)
target_link_libraries(quest_probe SDL2::SDL2 openxr_loader GLESv3 EGL android log dl)
set_target_properties(quest_probe PROPERTIES OUTPUT_NAME "main")
