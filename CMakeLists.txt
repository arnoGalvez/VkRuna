# Copyright (c) 2021 Arno Galvez

cmake_minimum_required(VERSION 3.14.1 FATAL_ERROR)
project( vkRuna VERSION 0.1
               DESCRIPTION "vkRuna renderer"
               LANGUAGES CXX )

set(CMAKE_CXX_STANDARD 17)
set_property( GLOBAL PROPERTY USE_FOLDERS ON )

message(STATUS "CMake version: ${CMAKE_VERSION}\n")

# App
set( APP_NAME vkRuna CACHE STRING "Application name" )

# Platform
if ( WIN32 )
	set( VULKAN_PLATFORM "VK_USE_PLATFORM_WIN32_KHR" )
	set( OS WIN32 )
	add_definitions(-DNOMINMAX -DWIN32_LEAN_AND_MEAN -D_CRT_SECURE_NO_WARNINGS)
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${APP_NAME})
    set(FLAGS_DEBUG /Z7 /Od /DEBUG)
    set(FLAGS_RELEASE /O2)
else()
	message(FATAL_ERROR "### vkRuna can only be built on Windows operating system ! ###")
endif()

find_library(Vulkan_LIBRARY NAMES vulkan-1 vulkan PATHS ${CMAKE_SOURCE_DIR}/external/libs)
IF (Vulkan_LIBRARY)
    set(Vulkan_FOUND ON)
ENDIF()

IF (NOT Vulkan_FOUND)
    message(FATAL_ERROR "Could not find Vulkan library")
ENDIF()

# Build
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions ( RUNA_DEBUG )
endif()

add_definitions( "-DTRACY_ENABLE" "-DGLM_FORCE_DEPTH_ZERO_TO_ONE" "-DGLM_FORCE_LEFT_HANDED" )
add_compile_options(
    "$<$<CONFIG:Debug>:${FLAGS_DEBUG}>"
    "$<$<CONFIG:Release>:${FLAGS_RELEASE}>"
)

# Subdirectories
add_subdirectory( platform )
add_subdirectory( app )
add_subdirectory( rnLib )
add_subdirectory( renderer )
add_subdirectory( game )
add_subdirectory( ui )
add_subdirectory( external )
