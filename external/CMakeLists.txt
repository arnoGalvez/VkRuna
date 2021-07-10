# Copyright (c) 2021 Arno Galvez

add_library(
        external_lib STATIC
        imgui/imgui.cpp
	imgui/imgui_demo.cpp
	imgui/imgui_draw.cpp
	imgui/imgui_impl_vulkan.cpp
	imgui/imgui_impl_win32.cpp
        imgui/imgui_tables.cpp
	imgui/imgui_widgets.cpp
        imgui/ImGuiFileDialog/ImGuiFileDialog.cpp
)

set(Vulkan_INCLUDE_DIRS external/vulkan)

target_include_directories(
        external_lib
        PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
	${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(external_lib
        ${Vulkan_LIBRARY}
)
