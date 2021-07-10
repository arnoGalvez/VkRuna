# Copyright (c) 2021 Arno Galvez

add_library(
        rnLib_lib STATIC
        Camera.cpp
 "Event.h")

target_include_directories(
        rnLib_lib
        PRIVATE
        ${PROJECT_SOURCE_DIR}
)

