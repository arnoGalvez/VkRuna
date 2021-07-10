# Copyright (c) 2021 Arno Galvez

add_library( render_lib STATIC
    Backend.cpp
	Buffer.cpp
	GPUMailManager.cpp
	Image.cpp
    RenderProgs.cpp
    RenderSystem.cpp
    Shader.cpp
	ShaderLexer.cpp
    State.cpp
	uiBackend.cpp
    VFX.cpp
	VkAllocator.cpp
    VkBackend.cpp
    VKRenderSystem.cpp
    VkUtil.cpp
)

target_compile_definitions( render_lib PRIVATE ${VULKAN_PLATFORM} RUNA_SHADER_COMPILER_PATH="bin/glslc.exe" )

target_include_directories(
    render_lib
    PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${Vulkan_INCLUDE_DIRS}
        ${PROJECT_SOURCE_DIR}/external
)

target_link_libraries(render_lib ${Vulkan_LIBRARY})
