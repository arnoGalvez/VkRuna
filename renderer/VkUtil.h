// Copyright (c) 2021 Arno Galvez

#pragma once

#include "external/vulkan/vulkan.hpp"

#include <vector>

namespace vkRuna
{
namespace render
{
bool CheckExtensionsInstanceLevel( const char *const *extensions, size_t count );
bool CheckValidationLayers( const char *const *validationLayers, size_t count );

VkSurfaceFormatKHR ChooseSurfaceFormat( const std::vector< VkSurfaceFormatKHR > &surfaceFormats );
VkPresentModeKHR   ChoosePresentMode( const std::vector< VkPresentModeKHR > &availablePresentModes );
VkExtent2D		   ChooseSurfaceExtent( const VkSurfaceCapabilitiesKHR &surfaceCaps );

bool GetSwapChainImages( const VkDevice &		 device,
						 const VkSwapchainKHR &	 swapChain,
						 std::vector< VkImage > &swapChainImages );

void GetSwapChainImagesCount( const VkDevice &device, const VkSwapchainKHR &swapChain, uint32_t &count );

} // namespace render
} // namespace vkRuna