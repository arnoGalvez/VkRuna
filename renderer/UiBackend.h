// Copyright (c) 2021 Arno Galvez

#pragma once

#include "external/imgui/imgui_impl_vulkan.h"
#include "external/vulkan/vulkan.hpp"

namespace vkRuna
{
class UiBackend
{
   public:
	void Init( ImGui_ImplVulkan_InitInfo *info, VkRenderPass renderPass, VkCommandBuffer cmdBuffer );
	void Shutdown();

	void  BeginFrame();
	void  EndFrame();
	void *GetDrawData();
	void  Draw( void *drawData, VkCommandBuffer cmdBuffer );
};

extern UiBackend g_uiBackend;

} // namespace vkRuna