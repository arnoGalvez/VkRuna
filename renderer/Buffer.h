// Copyright (c) 2021 Arno Galvez

#pragma once

#include "VkAllocator.h"
#include "external/vulkan/vulkan.hpp"
#include "platform/defines.h"

namespace vkRuna
{
namespace render
{
enum bufferProps_t
{
	BP_STATIC, // Will not be updated after initialisation
	BP_DYNAMIC // Will be updated
};

VkAccessFlags BufferFlagsToAccessFlags( VkBufferUsageFlags usageFlags );

class Buffer
{
	NO_COPY_NO_ASSIGN( Buffer );

   public:
	Buffer() = default;
	~Buffer();

	void Alloc( VkBufferUsageFlags usage, bufferProps_t memProp, VkDeviceSize size, const void *data = nullptr );
	void Free();

	void Update( VkDeviceSize size, const void *data, VkDeviceSize writeOffset = 0 );
	void Fill( uint32_t data );

	VkBuffer	 GetHandle() const { return m_handle; };
	VkDeviceSize GetAllocSize() const { return m_alloc.size; }
	void *		 GetPointer() { return m_alloc.data; }

   private:
	void UploadStaticData( VkDeviceSize size, const void *data );

   private:
	// VkDeviceSize       m_size  = 0;
	VkBufferUsageFlags m_usage = 0;
	bufferProps_t	   m_prop  = BP_STATIC;

	VkBuffer		   m_handle = VK_NULL_HANDLE;
	vulkanAllocation_t m_alloc;
};

} // namespace render
} // namespace vkRuna
