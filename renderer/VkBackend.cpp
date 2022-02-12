// Copyright (c) 2021 Arno Galvez

#include "renderer/VkBackend.h"

#include "platform/Heap.h"
#include "platform/Window.h"
#include "renderer/Buffer.h"
#include "renderer/Check.h"
#include "renderer/GPUMailManager.h"
#include "renderer/Image.h"
#include "renderer/RenderProgs.h"
#include "renderer/VkUtil.h"
#include "renderer/uiBackend.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <utility>
#include <vector>

namespace vkRuna
{
namespace render
{
enum renderPassAttachment_t
{
	RPA_color,
	RPA_stencilDepth,
	RPA_COUNT
};

#ifdef RUNA_DEBUG
static const std::array< const char *, 3 > g_instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME,
																	VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
																	VK_EXT_DEBUG_REPORT_EXTENSION_NAME };
#else
static const std::array< const char *, 2 > g_instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME,
																	VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
#endif

static const std::array< const char *, 1 > g_validationLayers = {
	/*"VK_LAYER_LUNARG_standard_validation"*/ "VK_LAYER_KHRONOS_validation"
};

static const std::array< const char *, 1 > g_deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

static VulkanBackend g_vkInstance;

static vulkanContext_t g_vulkanContext;

vulkanContext_t &GetVulkanContext()
{
	return g_vulkanContext;
}

VulkanBackend::VulkanBackend() {}

VulkanBackend &VulkanBackend::GetInstance()
{
	return g_vkInstance;
}

void VulkanBackend::Init()
{
	CreateInstance();

	CreatePresentationSurface();

	PickPhysicalDevice();

	CreateDeviceAndAndQueues();

	CreateSemaphores();

	CreateCommandPool();

	CreateCommandBuffers();

	g_vulkanAllocator.Init();

	g_gpuMail.Init();

	CreateSwapChain();

	CreateRenderTargets();

	CreateRenderPass();

	CreateFramebuffers();

	g_pipelineManager.Init();

	ImGui_ImplVulkan_InitInfo imgui_init_info {};
	imgui_init_info.Instance		= m_instance;
	imgui_init_info.PhysicalDevice	= g_vulkanContext.gpu.device;
	imgui_init_info.Device			= g_vulkanContext.device;
	imgui_init_info.QueueFamily		= g_vulkanContext.graphicsFamilyId;
	imgui_init_info.Queue			= g_vulkanContext.graphicsQueue;
	imgui_init_info.PipelineCache	= g_pipelineManager.GetPipelineCache();
	imgui_init_info.DescriptorPool	= g_pipelineManager.GetDescriptorPool();
	imgui_init_info.Allocator		= nullptr;
	imgui_init_info.MinImageCount	= SWAPCHAIN_BUFFERING_LEVEL;
	imgui_init_info.ImageCount		= SWAPCHAIN_BUFFERING_LEVEL;
	imgui_init_info.CheckVkResultFn = nullptr;

	g_uiBackend.Init( &imgui_init_info, g_vulkanContext.renderPass, m_commandBuffers[ 0 ] );

	vkResetCommandPool( g_vulkanContext.device, m_commandPool, 0 );
}

void VulkanBackend::Shutdown()
{
	g_uiBackend.Shutdown();

	g_pipelineManager.Shutdown();

	DestroyFramebuffers();

	DestroyRenderPass();

	DestroyRenderTargets();

	DestroySwapChain();

	g_gpuMail.Shutdown();

	g_vulkanAllocator.Shutdown();

	DestroyCommandBuffers();

	DestroyCommandPool();

	DestroySemaphores();

	DestroyDevice();

	DestroyPresentationSurface();

	DestroyInstance();
}

void VulkanBackend::ExecuteCommands( int			 preRenderCount,
									 const gpuCmd_t *preRenderCmds,
									 int			 renderCmdCount,
									 const gpuCmd_t *renderCmds )
{
	if ( !StartFrame() )
	{
		return;
	}

	for ( int i = 0; i < preRenderCount; ++i )
	{
		const gpuCmd_t &cmd = preRenderCmds[ i ];

		switch ( cmd.type )
		{
			case CT_COMPUTE:
			{
				if ( cmd.pipeline != nullptr )
				{
					g_pipelineManager.BindComputePipeline( m_commandBuffers[ m_current ], *cmd.pipeline );
				}
				Dispatch( cmd.groupCountDim[ 0 ], cmd.groupCountDim[ 1 ], cmd.groupCountDim[ 2 ] );
			}
			break;

			case CT_BARRIER:
			{
				auto *gpuBarrier = static_cast< gpuBarrier_t * >( cmd.obj );
				InsertBarriers( *gpuBarrier );
			}
			break;

			default: CHECK_PRED( false ) break;
		}
	}

	BeginRenderPass();

	for ( int i = 0; i < renderCmdCount; ++i )
	{
		const gpuCmd_t &cmd = renderCmds[ i ];

		switch ( cmd.type )
		{
			case CT_GRAPHIC:
			{
				if ( ( cmd.pipeline != nullptr ) )
				{
					if ( ( cmd.pipeline->pipeline == VK_NULL_HANDLE ) ||
						 ( g_vulkanContext.boundGraphicsPipelines[ m_current ] != cmd.pipeline->pipeline ) )
					{
						g_pipelineManager.BindGraphicsPipeline( m_commandBuffers[ m_current ], *cmd.pipeline );
						g_vulkanContext.boundGraphicsPipelines[ m_current ] = cmd.pipeline->pipeline;
					}
				}
				Draw( cmd.drawSurf );
			}
			break;

			case CT_BARRIER:
			{
				auto *gpuBarrier = static_cast< gpuBarrier_t * >( cmd.obj );
				InsertBarriers( *gpuBarrier );
			}
			break;

			case CT_UI:
			{
				g_uiBackend.Draw( cmd.obj, m_commandBuffers[ m_current ] );
			}
			break;

			default: CHECK_PRED( false ) break;
		}
	}

	EndFrame();
}

void VulkanBackend::Present()
{
	// Does not seem to be needed
	VK_CHECK( vkWaitForFences( g_vulkanContext.device, 1, &m_commandBufferFences[ m_current ], VK_TRUE, UINT64_MAX ) );

	VK_CHECK( vkResetFences( g_vulkanContext.device, 1, &m_commandBufferFences[ m_current ] ) );

	VkResult		 swapchainResult;
	VkPresentInfoKHR presentInfo {};
	presentInfo.sType			   = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext			   = nullptr;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores	   = &m_renderCompleteSemaphores[ m_current ];
	presentInfo.swapchainCount	   = 1;
	presentInfo.pSwapchains		   = &m_swapchain;
	presentInfo.pImageIndices	   = &m_currentSwapChainImage;
	presentInfo.pResults		   = &swapchainResult;

	vkQueuePresentKHR( g_vulkanContext.presentQueue, &presentInfo );

	switch ( swapchainResult )
	{
		case VK_SUCCESS: break;
		case VK_ERROR_OUT_OF_DATE_KHR:
		case VK_SUBOPTIMAL_KHR:
		{
			OnWindowSizeChanged();
			break;
		}
		default: CHECK_PRED( false );
	}

	m_current = ( m_current + 1 ) % SWAPCHAIN_BUFFERING_LEVEL;
	++m_frameCount;
}

void VulkanBackend::ExecuteComputeCommands( int count, const gpuCmd_t *cmds )
{
	StartComputeFrame();

	for ( int i = 0; i < count; ++i )
	{
		const gpuCmd_t &cmd = cmds[ i ];

		switch ( cmd.type )
		{
			case CT_COMPUTE:
			{
				if ( cmd.pipeline )
				{
					g_pipelineManager.BindComputePipeline( m_computeCommandBuffers[ m_computeCurrent ], *cmd.pipeline );
				}
				Dispatch( m_computeCommandBuffers[ m_computeCurrent ],
						  cmd.groupCountDim[ 0 ],
						  cmd.groupCountDim[ 1 ],
						  cmd.groupCountDim[ 2 ] );
			}
			break;

			default: CHECK_PRED( false ) break;
		}
	}

	EndComputeFrame();
}

bool VulkanBackend::StartFrame()
{
	g_gpuMail.Flush();
	g_vulkanAllocator.EmptyGarbage();

	VkResult acquireResult = vkAcquireNextImageKHR( g_vulkanContext.device,
													m_swapchain,
													UINT64_MAX,
													m_imageAvailableSemaphores[ m_current ],
													VK_NULL_HANDLE,
													&m_currentSwapChainImage );

	switch ( acquireResult )
	{
		case VK_SUCCESS:
		case VK_SUBOPTIMAL_KHR: break;

		case VK_ERROR_OUT_OF_DATE_KHR:
		{
			OnWindowSizeChanged();
			return false;
		}
		default:
		{
			CHECK_PRED( false );
			return false;
		}
	}

	VkCommandBufferBeginInfo cmdBufferBeginInfo {};
	cmdBufferBeginInfo.sType			= VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufferBeginInfo.pNext			= nullptr;
	cmdBufferBeginInfo.flags			= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	cmdBufferBeginInfo.pInheritanceInfo = nullptr;

	VkViewport viewport {};
	viewport.x		  = 0.0f;
	viewport.y		  = 0.0f;
	viewport.width	  = static_cast< float >( m_swapchainExtent.width );
	viewport.height	  = static_cast< float >( m_swapchainExtent.height );
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent	 = m_swapchainExtent;

	VK_CHECK( vkResetCommandPool(
		g_vulkanContext.device,
		m_commandPool,
		0 ) ); // Careful with that call, no command buffer should be in use. Wait this call goes against the fact that
			   // there is an array of command buffers so that multiple buffers can be in flight !

	VK_CHECK( vkBeginCommandBuffer( m_commandBuffers[ m_current ], &cmdBufferBeginInfo ) );

	vkCmdSetViewport( m_commandBuffers[ m_current ], 0, 1, &viewport );

	vkCmdSetScissor( m_commandBuffers[ m_current ], 0, 1, &scissor );

	if ( g_vulkanContext.gpu.features.depthBounds )
	{
		vkCmdSetDepthBounds( m_commandBuffers[ m_current ], 0.0f, 1.0f );
	}

	// pipeline barrier for acquired swap chain image ?

	return true;
}

void VulkanBackend::BeginRenderPass()
{
	std::array< VkClearValue, RPA_COUNT > clearValues {};

	VkClearValue &colorClear	  = clearValues[ RPA_color ];
	colorClear.color.float32[ 0 ] = 0.05f; // 0.4f;
	colorClear.color.float32[ 1 ] = 0.05f; // 0.0275f;
	colorClear.color.float32[ 2 ] = 0.05f; // 0.102f;
	colorClear.color.float32[ 3 ] = 1.0f;

	VkClearValue &depthStencil		  = clearValues[ RPA_stencilDepth ];
	depthStencil.depthStencil.depth	  = 1.0f;
	depthStencil.depthStencil.stencil = 0;

	VkRenderPassBeginInfo renderPassBeginCI {};
	renderPassBeginCI.sType				= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginCI.pNext				= nullptr;
	renderPassBeginCI.renderPass		= g_vulkanContext.renderPass;
	renderPassBeginCI.framebuffer		= m_framebuffers[ m_current ];
	renderPassBeginCI.renderArea.extent = m_swapchainExtent;
	renderPassBeginCI.clearValueCount	= static_cast< uint32_t >( clearValues.size() );
	renderPassBeginCI.pClearValues		= clearValues.data();
	vkCmdBeginRenderPass( m_commandBuffers[ m_current ], &renderPassBeginCI, VK_SUBPASS_CONTENTS_INLINE );
}

void VulkanBackend::Draw( const drawSurf_t &surf )
{
	if ( !surf.vertexBuffer && !surf.indexBuffer )
	{
		return;
	}

	if ( surf.vertexBuffer )
	{
		VkBuffer vertexBuffer = surf.vertexBuffer->GetHandle();
		vkCmdBindVertexBuffers( m_commandBuffers[ m_current ], 0, 1, &vertexBuffer, &surf.vertexBufferOffset );
	}

	if ( surf.indexBuffer )
	{
		VkBuffer indexBuffer = surf.indexBuffer->GetHandle();
		vkCmdBindIndexBuffer( m_commandBuffers[ m_current ],
							  indexBuffer,
							  surf.indexBufferOffset,
							  VK_INDEX_TYPE_UINT16 );

		vkCmdDrawIndexed( m_commandBuffers[ m_current ], surf.indexCount, surf.instanceCount, 0, 0, 0 );
	}
	else
	{
		vkCmdDraw( m_commandBuffers[ m_current ], surf.vertexCount, surf.instanceCount, 0, 0 );
	}
}

void VulkanBackend::Dispatch( uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ )
{
	vkCmdDispatch( m_commandBuffers[ m_current ], groupCountX, groupCountY, groupCountZ );
}

void VulkanBackend::Dispatch( VkCommandBuffer cmdBuffer,
							  uint32_t		  groupCountX,
							  uint32_t		  groupCountY,
							  uint32_t		  groupCountZ )
{
	vkCmdDispatch( cmdBuffer, groupCountX, groupCountY, groupCountZ );
}

void VulkanBackend::EndComputeFrame()
{
	VK_CHECK( vkEndCommandBuffer( m_computeCommandBuffers[ m_computeCurrent ] ) );

	VkSubmitInfo submitInfo {};
	submitInfo.sType				= VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext				= nullptr;
	submitInfo.waitSemaphoreCount	= 0;
	submitInfo.pWaitSemaphores		= nullptr;
	submitInfo.pWaitDstStageMask	= nullptr;
	submitInfo.commandBufferCount	= 1;
	submitInfo.pCommandBuffers		= &m_computeCommandBuffers[ m_computeCurrent ];
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores	= nullptr;

	vkQueueSubmit( g_vulkanContext.graphicsQueue, 1, &submitInfo, m_computeCommandBufferFences[ m_computeCurrent ] );

	VK_CHECK( vkWaitForFences( g_vulkanContext.device,
							   1,
							   &m_computeCommandBufferFences[ m_computeCurrent ],
							   VK_TRUE,
							   UINT64_MAX ) );

	VK_CHECK( vkResetFences( g_vulkanContext.device, 1, &m_computeCommandBufferFences[ m_computeCurrent ] ) );

	m_computeCurrent = ( m_computeCurrent + 1 ) % m_computeCommandBufferFences.size();
	++m_computeFrameCount;
}

void VulkanBackend::InsertBarriers( const gpuBarrier_t &gpuBarrier )
{
	vkCmdPipelineBarrier( m_commandBuffers[ m_current ],
						  gpuBarrier.srcStageMask,
						  gpuBarrier.dstStageMask,
						  gpuBarrier.dependencyFlags,
						  static_cast< uint32_t >( gpuBarrier.globalBarriers.size() ),
						  gpuBarrier.globalBarriers.data(),
						  static_cast< uint32_t >( gpuBarrier.bufferBarriers.size() ),
						  gpuBarrier.bufferBarriers.data(),
						  static_cast< uint32_t >( gpuBarrier.imageBarriers.size() ),
						  gpuBarrier.imageBarriers.data() );
}

void VulkanBackend::EndFrame()
{
	vkCmdEndRenderPass( m_commandBuffers[ m_current ] );

	if ( g_vulkanContext.presentFamilyId != g_vulkanContext.graphicsFamilyId )
	{
		VkImageMemoryBarrier barrier {};
		barrier.sType							= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.pNext							= nullptr;
		barrier.srcAccessMask					= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		barrier.dstAccessMask					= VK_ACCESS_MEMORY_READ_BIT;
		barrier.oldLayout						= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		barrier.newLayout						= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		barrier.srcQueueFamilyIndex				= g_vulkanContext.graphicsFamilyId;
		barrier.dstQueueFamilyIndex				= g_vulkanContext.presentFamilyId;
		barrier.image							= m_swapchainImages[ m_currentSwapChainImage ];
		barrier.subresourceRange.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel	= 0;
		barrier.subresourceRange.levelCount		= 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount		= 1;

		vkCmdPipelineBarrier( m_commandBuffers[ m_current ],
							  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
							  VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
							  0,
							  0,
							  nullptr,
							  0,
							  nullptr,
							  0,
							  nullptr );
	}

	VK_CHECK( vkEndCommandBuffer( m_commandBuffers[ m_current ] ) );

	const VkPipelineStageFlags pipelineStageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSubmitInfo submitInfo {};
	submitInfo.sType				= VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext				= nullptr;
	submitInfo.waitSemaphoreCount	= 1;
	submitInfo.pWaitSemaphores		= &m_imageAvailableSemaphores[ m_current ];
	submitInfo.pWaitDstStageMask	= &pipelineStageFlags;
	submitInfo.commandBufferCount	= 1;
	submitInfo.pCommandBuffers		= &m_commandBuffers[ m_current ];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores	= &m_renderCompleteSemaphores[ m_current ];

	VK_CHECK( vkQueueSubmit( g_vulkanContext.graphicsQueue, 1, &submitInfo, m_commandBufferFences[ m_current ] ) );

	g_vulkanContext.boundGraphicsPipelines[ m_current ] = VK_NULL_HANDLE;
}

void VulkanBackend::StartComputeFrame()
{
	VkCommandBufferBeginInfo cmdBufferBeginInfo {};
	cmdBufferBeginInfo.sType			= VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufferBeginInfo.pNext			= nullptr;
	cmdBufferBeginInfo.flags			= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	cmdBufferBeginInfo.pInheritanceInfo = nullptr;

	VK_CHECK( vkResetCommandPool( g_vulkanContext.device, m_commandPool, 0 ) );

	VK_CHECK( vkBeginCommandBuffer( m_computeCommandBuffers[ m_computeCurrent ], &cmdBufferBeginInfo ) );
}

void VulkanBackend::CreateInstance()
{
	VkApplicationInfo appInfo {};
	appInfo.sType			   = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext			   = nullptr;
	appInfo.pApplicationName   = "";
	appInfo.applicationVersion = VK_MAKE_VERSION( 0, 0, 1 );
	appInfo.pEngineName		   = "VkRuna";
	appInfo.engineVersion	   = VK_MAKE_VERSION( 0, 0, 1 );
	appInfo.apiVersion		   = VK_API_VERSION_1_2;

	VkInstanceCreateInfo vkinstanceCI {};
	vkinstanceCI.sType			  = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	vkinstanceCI.pNext			  = nullptr;
	vkinstanceCI.pApplicationInfo = &appInfo;

#ifdef RUNA_DEBUG
	CHECK_PRED( CheckValidationLayers( g_validationLayers.data(), g_validationLayers.size() ) );
	vkinstanceCI.enabledLayerCount	 = static_cast< uint32_t >( g_validationLayers.size() );
	vkinstanceCI.ppEnabledLayerNames = g_validationLayers.data();
#else
	vkinstanceCI.enabledLayerCount	 = 0;
	vkinstanceCI.ppEnabledLayerNames = nullptr;
#endif

	CHECK_PRED( CheckExtensionsInstanceLevel( g_instanceExtensions.data(), g_instanceExtensions.size() ) );
	vkinstanceCI.enabledExtensionCount	 = static_cast< uint32_t >( g_instanceExtensions.size() );
	vkinstanceCI.ppEnabledExtensionNames = g_instanceExtensions.data();

	VK_CHECK( vkCreateInstance( &vkinstanceCI, nullptr, &m_instance ) );
}

void VulkanBackend::DestroyInstance()
{
	vkDestroyInstance( m_instance, nullptr );
	m_instance = VK_NULL_HANDLE;
}

void VulkanBackend::CreatePresentationSurface()
{
	auto &win = Window::GetInstance();

	VkWin32SurfaceCreateInfoKHR surfaceCreateInfo {};
	surfaceCreateInfo.sType		= VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surfaceCreateInfo.pNext		= nullptr;
	surfaceCreateInfo.flags		= 0;
	surfaceCreateInfo.hinstance = win.GetProps().hInstance;
	surfaceCreateInfo.hwnd		= win.GetHWND();

	VK_CHECK( vkCreateWin32SurfaceKHR( m_instance, &surfaceCreateInfo, nullptr, &m_presentationSurface ) );
}

void VulkanBackend::DestroyPresentationSurface()
{
	vkDestroySurfaceKHR( m_instance, m_presentationSurface, nullptr );
	m_presentationSurface = VK_NULL_HANDLE;
}

static bool CheckExtensions( size_t										 numExtensions,
							 const char *const *						 extensions,
							 const std::vector< VkExtensionProperties > &available )
{
	for ( size_t i = 0; i < numExtensions; ++i )
	{
		bool found = false;
		for ( auto &other : available )
		{
			if ( std::strcmp( extensions[ i ], other.extensionName ) == 0 )
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

void VulkanBackend::PickPhysicalDevice()
{
	uint32_t numPhysicalDevices = 0;
	VK_CHECK_PRED( vkEnumeratePhysicalDevices( m_instance, &numPhysicalDevices, nullptr ), numPhysicalDevices != 0 );

	std::vector< VkPhysicalDevice > physicalDevices( numPhysicalDevices );
	VK_CHECK( vkEnumeratePhysicalDevices( m_instance, &numPhysicalDevices, physicalDevices.data() ) );

	std::vector< GPUInfo_t > gpus( numPhysicalDevices );

	for ( size_t i = 0; i < numPhysicalDevices; ++i )
	{
		auto &gpu = gpus[ i ];

		gpu.device = physicalDevices[ i ];

		vkGetPhysicalDeviceProperties( gpu.device, &gpu.properties );
		/*	gpu.properties3.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES;
			gpu.properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;*/
		gpu.properties2.pNext = reinterpret_cast< void * >( &gpu.properties3 );
		vkGetPhysicalDeviceProperties2( gpu.device, &gpu.properties2 );
		vkGetPhysicalDeviceMemoryProperties( gpu.device, &gpu.memProps );
		vkGetPhysicalDeviceFeatures( gpu.device, &gpu.features );
		{
			uint32_t numExtensions = 0;
			VK_CHECK_PRED( vkEnumerateDeviceExtensionProperties( gpu.device, nullptr, &numExtensions, nullptr ),
						   numExtensions > 0 );

			gpu.extensionsProps.resize( numExtensions );
			VK_CHECK( vkEnumerateDeviceExtensionProperties( gpu.device,
															nullptr,
															&numExtensions,
															gpu.extensionsProps.data() ) );
		}

		VK_CHECK( vkGetPhysicalDeviceSurfaceCapabilitiesKHR( gpu.device, m_presentationSurface, &gpu.surfaceCaps ) );
		{
			uint32_t numFormats = 0;
			VK_CHECK_PRED(
				vkGetPhysicalDeviceSurfaceFormatsKHR( gpu.device, m_presentationSurface, &numFormats, nullptr ),
				numFormats > 0 );

			gpu.surfaceFormats.resize( numFormats );
			VK_CHECK( vkGetPhysicalDeviceSurfaceFormatsKHR( gpu.device,
															m_presentationSurface,
															&numFormats,
															gpu.surfaceFormats.data() ) );
		}
		{
			uint32_t numModes = 0;
			VK_CHECK_PRED(
				vkGetPhysicalDeviceSurfacePresentModesKHR( gpu.device, m_presentationSurface, &numModes, nullptr ),
				numModes != 0 );

			gpu.presentModes.resize( numModes );
			VK_CHECK( vkGetPhysicalDeviceSurfacePresentModesKHR( gpu.device,
																 m_presentationSurface,
																 &numModes,
																 gpu.presentModes.data() ) );
		}

		{
			uint32_t numQueueFams = 0;
			vkGetPhysicalDeviceQueueFamilyProperties( gpu.device, &numQueueFams, nullptr );
			CHECK_PRED( numQueueFams > 0 );

			gpu.queueFamiliesProps.resize( numQueueFams );
			vkGetPhysicalDeviceQueueFamilyProperties( gpu.device, &numQueueFams, gpu.queueFamiliesProps.data() );
		}
	}

	bool deviceFound = false;

	for ( auto &gpu : gpus )
	{
		if ( !CheckExtensions( g_deviceExtensions.size(), g_deviceExtensions.data(), gpu.extensionsProps ) )
		{
			continue;
		}

		if ( gpu.surfaceFormats.size() == 0 )
		{
			continue;
		}
		if ( gpu.presentModes.size() == 0 )
		{
			continue;
		}

		int graphicsQueueId = -1;
		int presentQueueId	= -1;

		for ( int i = 0; i < gpu.queueFamiliesProps.size(); ++i )
		{
			auto &props = gpu.queueFamiliesProps[ i ];

			if ( props.queueCount == 0 )
			{
				continue;
			}

			VkQueueFlags flags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
			if ( ( props.queueFlags & flags ) == flags )
			{
				graphicsQueueId = i;
			}

			VkBool32 supportPresent = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR( gpu.device, i, m_presentationSurface, &supportPresent );
			if ( supportPresent )
			{
				presentQueueId = i;
			}

			if ( graphicsQueueId > -1 && presentQueueId > -1 )
			{
				break;
			}
		}

		if ( graphicsQueueId > -1 && presentQueueId > -1 )
		{
			g_vulkanContext.graphicsFamilyId = graphicsQueueId;
			g_vulkanContext.presentFamilyId	 = presentQueueId;
			g_vulkanContext.gpu				 = gpu;

			deviceFound = true;

			if ( gpu.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU )
			{
				return;
			}
		}
	}

	CHECK_PRED( deviceFound );
}

void VulkanBackend::CreateDeviceAndAndQueues()
{
	std::vector< int > queuesId;
	queuesId.emplace_back( g_vulkanContext.graphicsFamilyId );
	queuesId.emplace_back( g_vulkanContext.presentFamilyId );
	auto last = std::unique( queuesId.begin(), queuesId.end() );
	queuesId.erase( last, queuesId.end() );

	std::vector< VkDeviceQueueCreateInfo > queuesCI( queuesId.size() );

	const float priority = 1.0f;
	for ( size_t i = 0; i < queuesId.size(); ++i )
	{
		auto &queueCI = queuesCI[ i ];

		queueCI.sType			 = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCI.pNext			 = nullptr;
		queueCI.flags			 = 0;
		queueCI.queueFamilyIndex = queuesId[ i ];
		queueCI.queueCount		 = 1;
		queueCI.pQueuePriorities = &priority;
	}

	VkPhysicalDeviceFeatures deviceFeatures = {};
	deviceFeatures.imageCubeArray			= VK_TRUE;
	deviceFeatures.depthClamp				= VK_TRUE;
	deviceFeatures.depthBiasClamp			= VK_TRUE;
	deviceFeatures.depthBounds				= g_vulkanContext.gpu.features.depthBounds;
	deviceFeatures.fillModeNonSolid			= VK_TRUE;

	VkDeviceCreateInfo deviceCI {};
	deviceCI.sType				  = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCI.pNext				  = nullptr;
	deviceCI.flags				  = 0;
	deviceCI.queueCreateInfoCount = static_cast< uint32_t >( queuesCI.size() );
	deviceCI.pQueueCreateInfos	  = queuesCI.data();
#ifdef RUNA_DEBUG
	deviceCI.enabledLayerCount	 = static_cast< uint32_t >( g_validationLayers.size() );
	deviceCI.ppEnabledLayerNames = g_validationLayers.data();
#else
	deviceCI.enabledLayerCount		 = 0;
	deviceCI.ppEnabledLayerNames	 = nullptr;
#endif
	deviceCI.enabledExtensionCount	 = static_cast< uint32_t >( g_deviceExtensions.size() );
	deviceCI.ppEnabledExtensionNames = g_deviceExtensions.data();
	deviceCI.pEnabledFeatures		 = &deviceFeatures; // will certainly change, according to vulkan layers output.

	VK_CHECK( vkCreateDevice( g_vulkanContext.gpu.device, &deviceCI, nullptr, &g_vulkanContext.device ) );

	vkGetDeviceQueue( g_vulkanContext.device, g_vulkanContext.graphicsFamilyId, 0, &g_vulkanContext.graphicsQueue );
	vkGetDeviceQueue( g_vulkanContext.device, g_vulkanContext.presentFamilyId, 0, &g_vulkanContext.presentQueue );
}

void VulkanBackend::DestroyDevice()
{
	vkDestroyDevice( g_vulkanContext.device, nullptr );
	g_vulkanContext.device = VK_NULL_HANDLE;
}

void VulkanBackend::CreateSemaphores()
{
	VkSemaphoreCreateInfo semCI {};
	semCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semCI.pNext = nullptr;
	semCI.flags = 0;

	for ( int i = 0; i < SWAPCHAIN_BUFFERING_LEVEL; ++i )
	{
		VK_CHECK( vkCreateSemaphore( g_vulkanContext.device, &semCI, nullptr, &m_imageAvailableSemaphores[ i ] ) );
		VK_CHECK( vkCreateSemaphore( g_vulkanContext.device, &semCI, nullptr, &m_renderCompleteSemaphores[ i ] ) );
	}
}

void VulkanBackend::DestroySemaphores()
{
	for ( int i = 0; i < SWAPCHAIN_BUFFERING_LEVEL; ++i )
	{
		vkDestroySemaphore( g_vulkanContext.device, m_imageAvailableSemaphores[ i ], nullptr );
		vkDestroySemaphore( g_vulkanContext.device, m_renderCompleteSemaphores[ i ], nullptr );
		m_imageAvailableSemaphores[ i ] = VK_NULL_HANDLE;
		m_renderCompleteSemaphores[ i ] = VK_NULL_HANDLE;
	}
}

void VulkanBackend::CreateCommandPool()
{
	VkCommandPoolCreateInfo commandPoolCI {};
	commandPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCI.pNext = nullptr;
	commandPoolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	commandPoolCI.queueFamilyIndex = g_vulkanContext.graphicsFamilyId;

	VK_CHECK( vkCreateCommandPool( g_vulkanContext.device, &commandPoolCI, nullptr, &m_commandPool ) );
}

void VulkanBackend::DestroyCommandPool()
{
	vkDestroyCommandPool( g_vulkanContext.device, m_commandPool, nullptr );
	m_commandPool = VK_NULL_HANDLE;
}

void VulkanBackend::CreateCommandBuffers()
{
	VkCommandBufferAllocateInfo cmdBufferAllocInfo;
	cmdBufferAllocInfo.sType			  = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufferAllocInfo.pNext			  = nullptr;
	cmdBufferAllocInfo.commandPool		  = m_commandPool;
	cmdBufferAllocInfo.level			  = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBufferAllocInfo.commandBufferCount = static_cast< uint32_t >( m_commandBuffers.size() );

	VK_CHECK( vkAllocateCommandBuffers( g_vulkanContext.device, &cmdBufferAllocInfo, m_commandBuffers.data() ) );

	cmdBufferAllocInfo.commandBufferCount = static_cast< uint32_t >( m_computeCommandBuffers.size() );

	VK_CHECK( vkAllocateCommandBuffers( g_vulkanContext.device, &cmdBufferAllocInfo, m_computeCommandBuffers.data() ) );

	VkFenceCreateInfo fenceCI {};
	fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCI.pNext = nullptr;
	fenceCI.flags = 0; // VK_FENCE_CREATE_SIGNALED_BIT;

	for ( auto &fence : m_commandBufferFences )
	{
		VK_CHECK( vkCreateFence( g_vulkanContext.device, &fenceCI, nullptr, &fence ) );
	}

	for ( auto &fence : m_computeCommandBufferFences )
	{
		VK_CHECK( vkCreateFence( g_vulkanContext.device, &fenceCI, nullptr, &fence ) );
	}
}

void VulkanBackend::DestroyCommandBuffers()
{
	for ( auto &fence : m_commandBufferFences )
	{
		vkDestroyFence( g_vulkanContext.device, fence, nullptr );
		fence = VK_NULL_HANDLE;
	}
	for ( auto &fence : m_computeCommandBufferFences )
	{
		vkDestroyFence( g_vulkanContext.device, fence, nullptr );
		fence = VK_NULL_HANDLE;
	}

	vkFreeCommandBuffers( g_vulkanContext.device,
						  m_commandPool,
						  static_cast< uint32_t >( m_commandBuffers.size() ),
						  m_commandBuffers.data() );
	vkFreeCommandBuffers( g_vulkanContext.device,
						  m_commandPool,
						  static_cast< uint32_t >( m_commandBuffers.size() ),
						  m_computeCommandBuffers.data() );

	std::memset( m_commandBuffers.data(), 0, m_commandBuffers.size() * sizeof( VkCommandBuffer ) );
	std::memset( m_computeCommandBuffers.data(), 0, m_computeCommandBuffers.size() * sizeof( VkCommandBuffer ) );
}

void VulkanBackend::CreateSwapChain()
{
	if ( g_vulkanContext.device != VK_NULL_HANDLE )
	{
		vkDeviceWaitIdle( g_vulkanContext.device );
	}

	auto &gpu = g_vulkanContext.gpu;

	VK_CHECK( vkGetPhysicalDeviceSurfaceCapabilitiesKHR( gpu.device, m_presentationSurface, &gpu.surfaceCaps ) );

	VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat( gpu.surfaceFormats );
	VkPresentModeKHR   presentMode	 = ChoosePresentMode( gpu.presentModes );
	VkExtent2D		   surfaceExtent = ChooseSurfaceExtent( gpu.surfaceCaps );

	const VkImageUsageFlags usageFlags =
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; // | VK_IMAGE_USAGE_TRANSFER_SRC_BIT ???
	CHECK_PRED( ( gpu.surfaceCaps.supportedUsageFlags & usageFlags ) == usageFlags );

	auto surfaceTransform = gpu.surfaceCaps.currentTransform;
	if ( gpu.surfaceCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR )
	{
		surfaceTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}

	VkSwapchainCreateInfoKHR swapchainCI {};
	swapchainCI.sType			 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCI.pNext			 = nullptr;
	swapchainCI.flags			 = 0;
	swapchainCI.surface			 = m_presentationSurface;
	swapchainCI.minImageCount	 = SWAPCHAIN_BUFFERING_LEVEL;
	swapchainCI.imageFormat		 = surfaceFormat.format;
	swapchainCI.imageColorSpace	 = surfaceFormat.colorSpace;
	swapchainCI.imageExtent		 = surfaceExtent;
	swapchainCI.imageArrayLayers = 1;
	swapchainCI.imageUsage		 = usageFlags;
	if ( g_vulkanContext.graphicsFamilyId != g_vulkanContext.presentFamilyId )
	{
		std::array< uint32_t, 2 > indices = { g_vulkanContext.graphicsFamilyId, g_vulkanContext.presentFamilyId };

		swapchainCI.imageSharingMode	  = VK_SHARING_MODE_CONCURRENT;
		swapchainCI.queueFamilyIndexCount = static_cast< uint32_t >( indices.size() );
		swapchainCI.pQueueFamilyIndices	  = indices.data();
	}
	else
	{
		swapchainCI.imageSharingMode	  = VK_SHARING_MODE_EXCLUSIVE;
		swapchainCI.queueFamilyIndexCount = 0;
		swapchainCI.pQueueFamilyIndices	  = nullptr;
	}
	swapchainCI.preTransform   = surfaceTransform;
	swapchainCI.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // It is assumed that this mode exists !
	swapchainCI.presentMode	   = presentMode;
	swapchainCI.clipped		   = VK_TRUE;
	swapchainCI.oldSwapchain   = m_swapchain;

	VK_CHECK( vkCreateSwapchainKHR( g_vulkanContext.device, &swapchainCI, nullptr, &m_swapchain ) );

	m_swapchainFormat = swapchainCI.imageFormat;
	m_swapchainExtent = swapchainCI.imageExtent;
	m_presentMode	  = swapchainCI.presentMode;

	if ( swapchainCI.oldSwapchain != VK_NULL_HANDLE )
	{
		vkDestroySwapchainKHR( g_vulkanContext.device, swapchainCI.oldSwapchain, nullptr );
	}

	uint32_t numImages = 0;
	VK_CHECK_PRED( vkGetSwapchainImagesKHR( g_vulkanContext.device, m_swapchain, &numImages, nullptr ), numImages > 0 );

	VK_CHECK( vkGetSwapchainImagesKHR( g_vulkanContext.device, m_swapchain, &numImages, m_swapchainImages.data() ) );

	for ( size_t i = 0; i < SWAPCHAIN_BUFFERING_LEVEL; ++i )
	{
		VkImageViewCreateInfo imageViewCI = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
											  nullptr,
											  0,
											  m_swapchainImages[ i ],
											  VK_IMAGE_VIEW_TYPE_2D,
											  m_swapchainFormat,
											  { VK_COMPONENT_SWIZZLE_IDENTITY,
												VK_COMPONENT_SWIZZLE_IDENTITY,
												VK_COMPONENT_SWIZZLE_IDENTITY,
												VK_COMPONENT_SWIZZLE_IDENTITY },
											  { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };

		VK_CHECK( vkCreateImageView( g_vulkanContext.device, &imageViewCI, nullptr, &m_swapchainImagesViews[ i ] ) );
	}
}

void VulkanBackend::DestroySwapChain()
{
	for ( size_t i = 0; i < m_swapchainImagesViews.size(); ++i )
	{
		vkDestroyImageView( g_vulkanContext.device, m_swapchainImagesViews[ i ], nullptr );
	}
	std::memset( m_swapchainImagesViews.data(), 0, m_swapchainImagesViews.size() * sizeof( VkImageView ) );
	std::memset( m_swapchainImages.data(), 0, m_swapchainImages.size() * sizeof( VkImage ) );

	vkDestroySwapchainKHR( g_vulkanContext.device, m_swapchain, nullptr );
	m_swapchain = VK_NULL_HANDLE;
}

VkFormat ChooseFormat( const VkFormat *formats, size_t count, VkImageTiling tiling, VkFormatFeatureFlagBits features )
{
	for ( size_t i = 0; i < count; ++i )
	{
		auto &format = formats[ i ];

		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties( g_vulkanContext.gpu.device, format, &props );

		if ( tiling == VK_IMAGE_TILING_LINEAR )
		{
			if ( ( props.linearTilingFeatures & features ) == features )
			{
				return format;
			}
		}
		else if ( tiling == VK_IMAGE_TILING_OPTIMAL )
		{
			if ( ( props.optimalTilingFeatures & features ) == features )
			{
				return format;
			}
		}
	}

	CHECK_PRED( false );

	return VK_FORMAT_UNDEFINED;
}

void VulkanBackend::CreateRenderTargets()
{
	const std::array< VkFormat, 2 > depthFormats = { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };

	VkFormat depthFormat = ChooseFormat( depthFormats.data(),
										 depthFormats.size(),
										 VK_IMAGE_TILING_OPTIMAL,
										 VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT );

	imageOpts_t imageOpts;
	imageOpts.type		 = TT_DEPTH;
	imageOpts.format	 = depthFormat;
	imageOpts.width		 = m_swapchainExtent.width;
	imageOpts.height	 = m_swapchainExtent.height;
	imageOpts.usageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	samplerOpts_t samplerOpts;
	samplerOpts.filter		= VK_FILTER_LINEAR;
	samplerOpts.addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;

	m_depthImage = new Image();
	m_depthImage->AllocImage( imageOpts, samplerOpts );
}

void VulkanBackend::DestroyRenderTargets()
{
	delete m_depthImage;
	m_depthImage = nullptr;
}

void VulkanBackend::CreateRenderPass()
{
	std::array< VkAttachmentDescription, RPA_COUNT > attachments;

	auto &colorAttachment		   = attachments[ RPA_color ];
	colorAttachment.flags		   = 0;
	colorAttachment.format		   = m_swapchainFormat;
	colorAttachment.samples		   = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp		   = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp		   = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout	   = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	auto &depthAttachment		   = attachments[ RPA_stencilDepth ];
	depthAttachment.flags		   = 0;
	depthAttachment.format		   = m_depthImage->GetFormat();
	depthAttachment.samples		   = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp		   = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp		   = VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout	   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorRef {};
	colorRef.attachment = 0;
	colorRef.layout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthRef {};
	depthRef.attachment = 1;
	depthRef.layout		= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass {};
	subpass.flags					= 0;
	subpass.pipelineBindPoint		= VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.inputAttachmentCount	= 0;
	subpass.pInputAttachments		= nullptr;
	subpass.colorAttachmentCount	= 1;
	subpass.pColorAttachments		= &colorRef;
	subpass.pResolveAttachments		= nullptr;
	subpass.pDepthStencilAttachment = &depthRef;
	subpass.preserveAttachmentCount = 0;
	subpass.pPreserveAttachments	= nullptr;

	VkSubpassDependency dependencyPresentToDraw {};
	dependencyPresentToDraw.srcSubpass		= VK_SUBPASS_EXTERNAL;
	dependencyPresentToDraw.dstSubpass		= 0;
	dependencyPresentToDraw.srcStageMask	= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencyPresentToDraw.dstStageMask	= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencyPresentToDraw.srcAccessMask	= VK_ACCESS_MEMORY_READ_BIT;
	dependencyPresentToDraw.dstAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencyPresentToDraw.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkSubpassDependency dependencyDrawToPresent = dependencyPresentToDraw;
	std::swap( dependencyDrawToPresent.srcSubpass, dependencyDrawToPresent.dstSubpass );
	std::swap( dependencyDrawToPresent.srcStageMask, dependencyDrawToPresent.dstStageMask );
	std::swap( dependencyDrawToPresent.srcAccessMask, dependencyDrawToPresent.dstAccessMask );

	std::array< VkSubpassDependency, 2 > dependencies = { dependencyPresentToDraw, dependencyDrawToPresent };

	VkRenderPassCreateInfo renderPass {};
	renderPass.sType		   = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPass.pNext		   = nullptr;
	renderPass.flags		   = 0;
	renderPass.attachmentCount = static_cast< uint32_t >( attachments.size() );
	renderPass.pAttachments	   = attachments.data();
	renderPass.subpassCount	   = 1;
	renderPass.pSubpasses	   = &subpass;
	renderPass.dependencyCount = static_cast< uint32_t >( dependencies.size() );
	renderPass.pDependencies   = dependencies.data();

	VK_CHECK( vkCreateRenderPass( g_vulkanContext.device, &renderPass, nullptr, &g_vulkanContext.renderPass ) );
}

void VulkanBackend::DestroyRenderPass()
{
	vkDestroyRenderPass( g_vulkanContext.device, g_vulkanContext.renderPass, nullptr );
	g_vulkanContext.renderPass = VK_NULL_HANDLE;
}

void VulkanBackend::CreateFramebuffers()
{
	std::array< VkImageView, 2 > attachments {};

	CHECK_PRED( m_depthImage != nullptr );

	attachments[ RPA_stencilDepth ] = m_depthImage->GetView();

	VkFramebufferCreateInfo framebufferCI {};
	framebufferCI.sType			  = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferCI.pNext			  = nullptr;
	framebufferCI.flags			  = 0;
	framebufferCI.renderPass	  = g_vulkanContext.renderPass;
	framebufferCI.width			  = m_swapchainExtent.width;
	framebufferCI.height		  = m_swapchainExtent.height;
	framebufferCI.layers		  = 1;
	framebufferCI.attachmentCount = static_cast< uint32_t >( attachments.size() );
	framebufferCI.pAttachments	  = attachments.data();

	for ( int i = 0; i < SWAPCHAIN_BUFFERING_LEVEL; ++i )
	{
		attachments[ RPA_color ] = m_swapchainImagesViews[ i ];
		VK_CHECK( vkCreateFramebuffer( g_vulkanContext.device, &framebufferCI, nullptr, &m_framebuffers[ i ] ) );
	}
}

void VulkanBackend::DestroyFramebuffers()
{
	for ( size_t i = 0; i < m_framebuffers.size(); ++i )
	{
		vkDestroyFramebuffer( g_vulkanContext.device, m_framebuffers[ i ], nullptr );
		m_framebuffers[ i ] = VK_NULL_HANDLE;
	}
}

//  Unhandled case: minimized window
void VulkanBackend::OnWindowSizeChanged()
{
	DestroySwapChain();
	CreateSwapChain();

	DestroyRenderTargets();
	CreateRenderTargets();

	DestroyRenderPass();
	CreateRenderPass();

	DestroyFramebuffers();
	CreateFramebuffers();
}

} // namespace render
} // namespace vkRuna
