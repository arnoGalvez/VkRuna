// Copyright (c) 2021 Arno Galvez

#pragma once

#include "external/vulkan/vulkan.hpp"
#include "renderer/Backend.h"
#include "renderer/RenderConfig.h"
#include "renderer/VkRenderCommon.h"

#include <array>

namespace vkRuna
{
namespace render
{
class Image;

struct GPUInfo_t
{
	VkPhysicalDevice device = VK_NULL_HANDLE;

	VkPhysicalDeviceProperties			 props;
	VkPhysicalDeviceMemoryProperties	 memProps;
	VkPhysicalDeviceFeatures			 features;
	std::vector< VkExtensionProperties > extensionsProps;

	VkSurfaceCapabilitiesKHR		  surfaceCaps;
	std::vector< VkSurfaceFormatKHR > surfaceFormats;
	std::vector< VkPresentModeKHR >	  presentModes;

	std::vector< VkQueueFamilyProperties > queueFamiliesProps;
};

struct vulkanContext_t
{
	GPUInfo_t gpu;

	VkDevice device			  = VK_NULL_HANDLE;
	uint32_t graphicsFamilyId = ~0u;
	uint32_t presentFamilyId  = ~0u;
	VkQueue	 graphicsQueue	  = VK_NULL_HANDLE;
	VkQueue	 presentQueue	  = VK_NULL_HANDLE;

	VkRenderPass renderPass = VK_NULL_HANDLE;

	std::array< VkPipeline, SWAPCHAIN_BUFFERING_LEVEL > boundGraphicsPipelines {};
};

vulkanContext_t &GetVulkanContext();

class VulkanBackend : public Backend
{
   public:
	VulkanBackend();

	static VulkanBackend &GetInstance();

	void Init() final;
	void Shutdown() final;

	void ExecuteCommands( int			  preRenderCount,
						  const gpuCmd_t *preRenderCmds,
						  int			  renderCmdCount,
						  const gpuCmd_t *renderCmds ) final;
	void Present() final;

	void ExecuteComputeCommands( int count, const gpuCmd_t *cmds );

   private:
	bool StartFrame();
	void BeginRenderPass();
	void Draw( const drawSurf_t &surf );
	void Dispatch( uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ );
	void InsertBarriers( const gpuBarrier_t &gpuBarrier );
	void EndFrame();
	void StartComputeFrame();
	void Dispatch( VkCommandBuffer cmdBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ );
	void EndComputeFrame();

   private:
	void CreateInstance();
	void DestroyInstance();

	void CreatePresentationSurface();
	void DestroyPresentationSurface();

	void PickPhysicalDevice();
	void CreateDeviceAndAndQueues();
	void DestroyDevice();

	void CreateSemaphores();
	void DestroySemaphores();

	void CreateCommandPool();
	void DestroyCommandPool();

	void CreateCommandBuffers();
	void DestroyCommandBuffers();

	void CreateSwapChain();
	void DestroySwapChain();

	void CreateRenderTargets();
	void DestroyRenderTargets();

	void CreateRenderPass();
	void DestroyRenderPass();

	void CreateFramebuffers();
	void DestroyFramebuffers();

   public:
	void OnWindowSizeChanged() final;

   private:
	VkInstance	 m_instance			   = VK_NULL_HANDLE;
	VkSurfaceKHR m_presentationSurface = VK_NULL_HANDLE;

	uint32_t m_current				 = 0;
	uint64_t m_frameCount			 = 0;
	uint32_t m_currentSwapChainImage = UINT32_MAX;
	uint32_t m_computeCurrent		 = 0;
	uint64_t m_computeFrameCount	 = 0;

	VkSwapchainKHR										 m_swapchain = VK_NULL_HANDLE;
	std::array< VkImage, SWAPCHAIN_BUFFERING_LEVEL >	 m_swapchainImages {};
	std::array< VkImageView, SWAPCHAIN_BUFFERING_LEVEL > m_swapchainImagesViews {};
	VkFormat											 m_swapchainFormat = VK_FORMAT_UNDEFINED;
	VkExtent2D											 m_swapchainExtent = { 0, 0 };
	VkPresentModeKHR									 m_presentMode	   = VK_PRESENT_MODE_FIFO_KHR;

	std::array< VkCommandBuffer, SWAPCHAIN_BUFFERING_LEVEL > m_commandBuffers {};
	std::array< VkFence, SWAPCHAIN_BUFFERING_LEVEL >		 m_commandBufferFences {};
	std::array< VkSemaphore, SWAPCHAIN_BUFFERING_LEVEL >	 m_imageAvailableSemaphores {};
	std::array< VkSemaphore, SWAPCHAIN_BUFFERING_LEVEL >	 m_renderCompleteSemaphores {};

	std::array< VkFramebuffer, SWAPCHAIN_BUFFERING_LEVEL > m_framebuffers {};

	std::array< VkCommandBuffer, COMPUTE_CHAIN_BUFFERING_LEVEL > m_computeCommandBuffers {};
	std::array< VkFence, COMPUTE_CHAIN_BUFFERING_LEVEL >		 m_computeCommandBufferFences {};

	VkCommandPool m_commandPool = VK_NULL_HANDLE;

	Image *m_depthImage = nullptr;
};

} // namespace render
} // namespace vkRuna
