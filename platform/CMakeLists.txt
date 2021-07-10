# Copyright (c) 2021 Arno Galvez

add_library(
        platform_lib STATIC
        Window.cpp
        Console.cpp
        Serializable.cpp
        Sys.cpp
        Heap.cpp
)

target_include_directories(
        platform_lib
        PRIVATE
        ${PROJECT_SOURCE_DIR}
)
