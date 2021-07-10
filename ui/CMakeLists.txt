# Copyright (c) 2021 Arno Galvez

add_library(
    ui_lib STATIC
    Controller.cpp
    Ui.cpp
    Vfxui.cpp
)

target_compile_definitions( ui_lib PRIVATE ${VULKAN_PLATFORM} )

target_include_directories(
    ui_lib
    PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/external
)
