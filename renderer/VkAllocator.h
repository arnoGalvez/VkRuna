// Copyright (c) 2021 Arno Galvez

#pragma once

#include "external/vulkan/vulkan.hpp"
#include "platform/defines.h"

#include <array>
#include <cstdint>
#include <vector>

namespace vkRuna
{
namespace render
{
enum vulkanMemoryUsage_t
{
	VULKAN_MEMORY_USAGE_UNKNOWN,
	VULKAN_MEMORY_USAGE_GPU_ONLY,
	VULKAN_MEMORY_USAGE_CPU_ONLY,
	VULKAN_MEMORY_USAGE_CPU_TO_GPU,
	VULKAN_MEMORY_USAGE_GPU_TO_CPU,
};

enum vulkanAllocationType_t
{
	VULKAN_ALLOCATION_TYPE_FREE,
	VULKAN_ALLOCATION_TYPE_BUFFER,
	VULKAN_ALLOCATION_TYPE_IMAGE_LINEAR,
	VULKAN_ALLOCATION_TYPE_IMAGE_OPTIMAL,
};

void UsageToMemPropsFlags( const vulkanMemoryUsage_t usage,
						   VkMemoryPropertyFlags &	 required,
						   VkMemoryPropertyFlags &	 preferred );

int FindMemoryType( const VkPhysicalDeviceMemoryProperties *pMemoryProperties,
					uint32_t								memoryTypeBitsRequirement,
					VkMemoryPropertyFlags					requiredProperties );

class VulkanBlock;
struct vulkanAllocation_t
{
	VulkanBlock *  block		= nullptr;
	uint32_t	   id			= 0;
	VkDeviceMemory deviceMemory = VK_NULL_HANDLE;
	VkDeviceSize   offset		= 0;
	VkDeviceSize   size			= 0;
	void *		   data			= nullptr;
};

class VulkanBlock
{
	NO_COPY_NO_ASSIGN( VulkanBlock )

   public:
	VulkanBlock( uint32_t memoryTypeIndex, VkDeviceSize size, vulkanMemoryUsage_t usage );
	~VulkanBlock();

	void Init();
	void Shutdown();

	bool Alloc( VkDeviceSize		   size,
				VkDeviceSize		   alignment,
				VkDeviceSize		   bufferImageGranularity,
				vulkanAllocationType_t type,
				vulkanAllocation_t &   allocation );
	void Free( vulkanAllocation_t &allocation );

	bool IsHostVisible() { return m_usage != VULKAN_MEMORY_USAGE_GPU_ONLY; }

	void Print() const;

   public:
	uint32_t	 GetMemoryTypeIndex() { return m_memTypeIndex; }
	VkDeviceSize GetSize() { return m_size; }
	VkDeviceSize GetAllocatedSize() { return m_allocated; }

   private:
	struct chunk_t
	{
		uint32_t			   id;
		vulkanAllocationType_t type;
		VkDeviceSize		   size;
		VkDeviceSize		   offset;
	};

	VkDeviceMemory m_deviceMemory = VK_NULL_HANDLE;
	VkDeviceSize   m_size		  = 0;
	VkDeviceSize   m_allocated	  = 0;

	uint32_t			m_memTypeIndex = UINT32_MAX;
	vulkanMemoryUsage_t m_usage		   = VULKAN_MEMORY_USAGE_UNKNOWN;

	std::vector< chunk_t > m_chunks;
	uint32_t			   m_nextChunkId = 0;

	void *m_data = nullptr;
};

class VulkanAllocator
{
	NO_COPY_NO_ASSIGN( VulkanAllocator )

   public:
	VulkanAllocator();
	~VulkanAllocator();

	void Init();
	void Shutdown();

	vulkanAllocation_t Alloc( vulkanAllocationType_t type,
							  vulkanMemoryUsage_t	 usage,
							  VkMemoryRequirements	 requirements );
	void			   Free( vulkanAllocation_t &allocation );
	void			   EmptyGarbage();

	void Print() const;

   private:
	VkDeviceSize m_deviceLocalMemoryBytes = 0;
	VkDeviceSize m_hostVisibleMemoryBytes = 0;

	std::array< std::vector< VulkanBlock * >, VK_MAX_MEMORY_TYPES > m_blockChains;

	VkDeviceSize m_bufferImageGranularity = 0;

	std::vector< vulkanAllocation_t > m_garbage;
};

extern VulkanAllocator g_vulkanAllocator;

} // namespace render
} // namespace vkRuna