// Copyright (c) 2021 Arno Galvez

#include "renderer/VkUtil.h"

#include "platform/Sys.h"
#include "platform/Window.h"
#include "platform/defines.h"
#include "renderer/Check.h"

#include <algorithm>
#include <iostream>

namespace vkRuna
{
namespace render
{
const char *ErrorToString( VkResult result )
{
	switch ( result )
	{
		SWITCH_CASE_STRING( VK_SUCCESS );
		SWITCH_CASE_STRING( VK_NOT_READY );
		SWITCH_CASE_STRING( VK_TIMEOUT );
		SWITCH_CASE_STRING( VK_EVENT_SET );
		SWITCH_CASE_STRING( VK_EVENT_RESET );
		SWITCH_CASE_STRING( VK_INCOMPLETE );
		SWITCH_CASE_STRING( VK_ERROR_OUT_OF_HOST_MEMORY );
		SWITCH_CASE_STRING( VK_ERROR_OUT_OF_DEVICE_MEMORY );
		SWITCH_CASE_STRING( VK_ERROR_INITIALIZATION_FAILED );
		SWITCH_CASE_STRING( VK_ERROR_DEVICE_LOST );
		SWITCH_CASE_STRING( VK_ERROR_MEMORY_MAP_FAILED );
		SWITCH_CASE_STRING( VK_ERROR_LAYER_NOT_PRESENT );
		SWITCH_CASE_STRING( VK_ERROR_EXTENSION_NOT_PRESENT );
		SWITCH_CASE_STRING( VK_ERROR_FEATURE_NOT_PRESENT );
		SWITCH_CASE_STRING( VK_ERROR_INCOMPATIBLE_DRIVER );
		SWITCH_CASE_STRING( VK_ERROR_TOO_MANY_OBJECTS );
		SWITCH_CASE_STRING( VK_ERROR_FORMAT_NOT_SUPPORTED );
		SWITCH_CASE_STRING( VK_ERROR_SURFACE_LOST_KHR );
		SWITCH_CASE_STRING( VK_ERROR_NATIVE_WINDOW_IN_USE_KHR );
		SWITCH_CASE_STRING( VK_SUBOPTIMAL_KHR );
		SWITCH_CASE_STRING( VK_ERROR_OUT_OF_DATE_KHR );
		SWITCH_CASE_STRING( VK_ERROR_INCOMPATIBLE_DISPLAY_KHR );
		SWITCH_CASE_STRING( VK_ERROR_VALIDATION_FAILED_EXT );
		SWITCH_CASE_STRING( VK_ERROR_INVALID_SHADER_NV );
		default: return "UNKNOWN VK ERROR";
	};
}

bool CheckExtensionsInstanceLevel( const char *const *extensions, size_t count )
{
	if ( count == 0 )
	{
		return true;
	}

	// https://www.khronos.org/registry/vulkan/specs/1.1/html/vkspec.html#vkEnumerateInstanceExtensionProperties
	uint32_t numExtensions = 0;
	VK_CHECK_PRED( vkEnumerateInstanceExtensionProperties( nullptr, &numExtensions, nullptr ), numExtensions != 0 );

	std::vector< VkExtensionProperties > availableExtensions( numExtensions );

	VK_CHECK_PRED( vkEnumerateInstanceExtensionProperties( nullptr, &numExtensions, availableExtensions.data() ),
				   numExtensions != 0 );

	for ( size_t i = 0; i < count; ++i )
	{
		bool found = false;
		for ( auto &availExtension : availableExtensions )
		{
			if ( std::strcmp( extensions[ i ], availExtension.extensionName ) == 0 )
			{
				found = true;
				break;
			}
		}
		if ( !found )
		{
			return false;
		}
	}

	return true;
}

bool CheckValidationLayers( const char *const *validationLayers, size_t count )
{
	if ( count == 0 )
	{
		return true;
	}

	uint32_t numLayers;
	VK_CHECK_PRED( vkEnumerateInstanceLayerProperties( &numLayers, nullptr ), numLayers != 0 );

	std::vector< VkLayerProperties > availableLayers( numLayers );
	VK_CHECK( vkEnumerateInstanceLayerProperties( &numLayers, availableLayers.data() ) );

	for ( size_t i = 0; i < count; ++i )
	{
		bool layerFound = false;
		for ( auto &layerProperty : availableLayers )
		{
			if ( strcmp( validationLayers[ i ], layerProperty.layerName ) == 0 )
			{
				layerFound = true;
				break;
			}
		}

		if ( !layerFound )
		{
			return false;
		}
	}

	return true;
}

VkSurfaceFormatKHR ChooseSurfaceFormat( const std::vector< VkSurfaceFormatKHR > &surfaceFormats )
{
	CHECK_PRED( surfaceFormats.size() > 0 );

	const VkSurfaceFormatKHR desired = { VK_FORMAT_R8G8B8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR };

	if ( surfaceFormats.size() == 1 && surfaceFormats[ 0 ].format == VK_FORMAT_UNDEFINED )
	{
		VkSurfaceFormatKHR surfaceFormat {};
		surfaceFormat.format	 = VK_FORMAT_R8G8B8A8_UNORM;
		surfaceFormat.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
		return surfaceFormat;
	}

	for ( const VkSurfaceFormatKHR &iter : surfaceFormats )
	{
		if ( iter.format == desired.format && iter.colorSpace == desired.colorSpace )
		{
			return iter;
		}
	}

	return surfaceFormats[ 0 ];
}

VkPresentModeKHR ChoosePresentMode( const std::vector< VkPresentModeKHR > &availablePresentModes )
{
#if 1
	const VkPresentModeKHR desired = VK_PRESENT_MODE_MAILBOX_KHR;

	for ( const VkPresentModeKHR &iter : availablePresentModes )
	{
		if ( iter == desired )
		{
			return desired;
		}
	}
#endif
	// V sync
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D ChooseSurfaceExtent( const VkSurfaceCapabilitiesKHR &caps )
{
	auto extent = caps.currentExtent;

	if ( extent.width == -1 || extent.height == -1 )
	{
		const auto &winProps = Window::GetInstance().GetProps();

		extent.width  = winProps.width;
		extent.height = winProps.height;

		extent.width  = std::min( std::max( extent.width, caps.minImageExtent.width ), caps.maxImageExtent.width );
		extent.height = std::min( std::max( extent.height, caps.minImageExtent.height ), caps.maxImageExtent.height );
	}

	return extent;
}

bool GetSwapChainImages( const VkDevice &		 device,
						 const VkSwapchainKHR &	 swapChain,
						 std::vector< VkImage > &swapChainImages )
{
	uint32_t imagesCount;
	VK_CHECK_PRED( vkGetSwapchainImagesKHR( device, swapChain, &imagesCount, nullptr ), imagesCount == 0 );
	swapChainImages.resize( imagesCount );
	VK_CHECK( vkGetSwapchainImagesKHR( device, swapChain, &imagesCount, swapChainImages.data() ) );

	return true;
}

void GetSwapChainImagesCount( const VkDevice &device, const VkSwapchainKHR &swapChain, uint32_t &count )
{
	VK_CHECK( vkGetSwapchainImagesKHR( device, swapChain, &count, nullptr ) );
}

bool CreateCommandPool( const VkDevice &				device,
						uint32_t						queueFamily,
						const VkCommandPoolCreateFlags &flags,
						VkCommandPool *					commandPool )
{
	VkCommandPoolCreateInfo ci;
	ci.sType			= VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	ci.pNext			= nullptr;
	ci.flags			= flags;
	ci.queueFamilyIndex = queueFamily;

	VK_CHECK( vkCreateCommandPool( device, &ci, nullptr, commandPool ) );

	return true;
}

} // namespace render
} // namespace vkRuna
