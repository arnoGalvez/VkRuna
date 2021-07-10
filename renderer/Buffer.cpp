// Copyright (c) 2021 Arno Galvez

#include "Buffer.h"

#include "platform/Sys.h"
#include "renderer/Check.h"
#include "renderer/GPUMailManager.h"
#include "renderer/VkBackend.h"

#include <cstring>

namespace vkRuna
{
namespace render
{
template< typename T, typename U >
int HasFlag( T x, U flag )
{
	return ( x & flag ) != 0;
}

// A better way to handle this would be welcomed!
VkAccessFlags BufferFlagsToAccessFlags( VkBufferUsageFlags usageFlags )
{
	VkAccessFlags accessFlags = 0;

	// https://www.khronos.org/registry/vulkan/specs/1.1/html/vkspec.html#VkAccessFlagBits
	accessFlags |= HasFlag( usageFlags, VK_BUFFER_USAGE_TRANSFER_SRC_BIT ) * VK_ACCESS_TRANSFER_READ_BIT;

	accessFlags |= HasFlag( usageFlags, VK_BUFFER_USAGE_TRANSFER_DST_BIT ) * VK_ACCESS_TRANSFER_WRITE_BIT;

	accessFlags |= HasFlag( usageFlags, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT ) *
				   ( VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT );

	accessFlags |= HasFlag( usageFlags, VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT ) *
				   ( VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT );

	accessFlags |= HasFlag( usageFlags, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT ) * VK_ACCESS_UNIFORM_READ_BIT;

	accessFlags |= HasFlag( usageFlags, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT ) *
				   ( VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT );

	accessFlags |= HasFlag( usageFlags, VK_BUFFER_USAGE_INDEX_BUFFER_BIT ) * VK_ACCESS_INDEX_READ_BIT;

	accessFlags |= HasFlag( usageFlags, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT ) * VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;

	return accessFlags;
}

Buffer::~Buffer()
{
	Free();
}

void Buffer::Alloc( VkBufferUsageFlags usage,
					bufferProps_t	   memProp,
					VkDeviceSize	   size,
					const void *	   data /*= nullptr */ )
{
	Free();

	auto &device = GetVulkanContext().device;

	if ( memProp == BP_STATIC )
	{
		usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	}

	VkBufferCreateInfo bufferCI {};
	bufferCI.sType				   = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCI.pNext				   = nullptr;
	bufferCI.flags				   = 0;
	bufferCI.size				   = size;
	bufferCI.usage				   = usage;
	bufferCI.sharingMode		   = VK_SHARING_MODE_EXCLUSIVE;
	bufferCI.queueFamilyIndexCount = 0;
	bufferCI.pQueueFamilyIndices   = nullptr;

	VK_CHECK( vkCreateBuffer( device, &bufferCI, nullptr, &m_handle ) );

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements( device, m_handle, &memRequirements );

	vulkanMemoryUsage_t memUsage = memProp == BP_STATIC ? VULKAN_MEMORY_USAGE_GPU_ONLY : VULKAN_MEMORY_USAGE_CPU_TO_GPU;
	m_alloc						 = g_vulkanAllocator.Alloc( VULKAN_ALLOCATION_TYPE_BUFFER, memUsage, memRequirements );

	VK_CHECK( vkBindBufferMemory( device, m_handle, m_alloc.deviceMemory, m_alloc.offset ) );

	// m_size  = size;
	m_usage = usage;
	m_prop	= memProp;

	if ( data != nullptr )
	{
		if ( memProp == BP_STATIC )
		{
			UploadStaticData( size, data );
		}
		else
		{
			Update( size, data );
		}
	}
}

void Buffer::Free()
{
	auto &device = GetVulkanContext().device;

	g_vulkanAllocator.Free( m_alloc );

	if ( m_handle != VK_NULL_HANDLE )
	{
		// Log( "Destroying buffer %p (alloc size: %lu)\n", m_handle, size );
		vkDestroyBuffer( device, m_handle, nullptr );
		m_handle = VK_NULL_HANDLE;
	}

	// m_size = 0;
}

void Buffer::Update( VkDeviceSize size, const void *data, VkDeviceSize writeOffset /*= 0 */ )
{
	CHECK_PRED( m_prop == BP_DYNAMIC );
	CHECK_PRED( ( size + writeOffset ) <= GetAllocSize() );
	std::memcpy( reinterpret_cast< byte * >( m_alloc.data ) + writeOffset, data, size );
}

void Buffer::Fill( uint32_t data )
{
	VkCommandBuffer mailCmdBuffer = g_gpuMail.GetCmdBuffer();
	// Log( "Buffer::Fill m_handle %p, cmd buffer %p\n", m_handle, mailCmdBuffer );

	VkBufferMemoryBarrier barrier {};
	barrier.sType				= VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.pNext				= nullptr;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer				= m_handle;
	barrier.offset				= 0;
	barrier.size				= VK_WHOLE_SIZE;

	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	vkCmdPipelineBarrier( mailCmdBuffer,
						  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
						  VK_PIPELINE_STAGE_TRANSFER_BIT,
						  0,
						  0,
						  nullptr,
						  1,
						  &barrier,
						  0,
						  nullptr );

	vkCmdFillBuffer( mailCmdBuffer, m_handle, 0, VK_WHOLE_SIZE, data );

	barrier.srcAccessMask = barrier.dstAccessMask;
	barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	vkCmdPipelineBarrier( mailCmdBuffer,
						  VK_PIPELINE_STAGE_TRANSFER_BIT,
						  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // TODO: improve based on m_usage
						  0,
						  0,
						  nullptr,
						  1,
						  &barrier,
						  0,
						  nullptr );
}

void Buffer::UploadStaticData( VkDeviceSize size, const void *data )
{
	VkBuffer		mailBuffer;
	VkDeviceSize	mailOffset;
	VkCommandBuffer mailCmdBuffer;
	g_gpuMail.Submit( size, 1, data, mailBuffer, mailOffset, mailCmdBuffer );

	VkBufferMemoryBarrier barrier {};
	barrier.sType				= VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.pNext				= nullptr;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer				= m_handle;
	barrier.offset				= 0;
	barrier.size				= size;

	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	vkCmdPipelineBarrier( mailCmdBuffer,
						  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
						  VK_PIPELINE_STAGE_TRANSFER_BIT,
						  0,
						  0,
						  nullptr,
						  1,
						  &barrier,
						  0,
						  nullptr );

	VkBufferCopy region {};
	region.srcOffset = mailOffset;
	region.dstOffset = 0;
	region.size		 = size;
	vkCmdCopyBuffer( mailCmdBuffer, mailBuffer, m_handle, 1, &region );

	barrier.srcAccessMask = barrier.dstAccessMask;
	barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	vkCmdPipelineBarrier( mailCmdBuffer,
						  VK_PIPELINE_STAGE_TRANSFER_BIT,
						  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // TODO: improve based on m_usage
						  0,
						  0,
						  nullptr,
						  1,
						  &barrier,
						  0,
						  nullptr );
}

} // namespace render

} // namespace vkRuna
