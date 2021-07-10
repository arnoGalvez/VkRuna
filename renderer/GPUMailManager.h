// Copyright (c) 2021 Arno Galvez

#pragma once

#include "external/vulkan/vulkan.hpp"
#include "platform/defines.h"
#include "renderer/RenderConfig.h"

#include <array>

namespace vkRuna
{
namespace render
{
class GPUMailManager
{
	NO_COPY_NO_ASSIGN( GPUMailManager )

	static_assert( sizeof( size_t ) >= sizeof( VkDeviceSize ),
				   "casting VkDeviceSize to size_t: possible loss of data in GPUMailManger::Submit" );

   public:
	GPUMailManager();

	void Init();
	void Shutdown();

	void			Submit( VkDeviceSize	 size,
							VkDeviceSize	 alignment,
							const void *	 data,
							VkBuffer &		 buffer,
							VkDeviceSize &	 offset,
							VkCommandBuffer &cmdBuffer );
	VkCommandBuffer GetCmdBuffer();

	void Flush();
	void WaitAll();

   private:
	void Wait( uint32_t gpuMaiId );

   private:
	struct GPUMail_t
	{
		VkBuffer		buffer		 = VK_NULL_HANDLE;
		VkCommandBuffer cmdBuffer	 = VK_NULL_HANDLE;
		VkFence			fence		 = VK_NULL_HANDLE;
		byte *			data		 = nullptr;
		VkDeviceSize	occupiedSize = 0;
		bool			submitted	 = false;
	};

	VkCommandPool  m_commandPool = VK_NULL_HANDLE;
	VkDeviceMemory m_memory		 = VK_NULL_HANDLE;

	VkDeviceSize m_bufferSize = 0;

	byte *m_mappedData = nullptr;

	std::array< GPUMail_t, GPU_MAIL_BUFFERING_LEVEL > m_mails;

	int m_currentMail = 0;
};

extern GPUMailManager g_gpuMail;

} // namespace render
} // namespace vkRuna
