# Copyright (c) 2021 Arno Galvez

add_library(
        game_lib STATIC
        CameraManager.cpp
        GameLocal.cpp
)

target_include_directories(
        game_lib
        PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/external
)
