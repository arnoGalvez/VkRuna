// Copyright (c) 2021 Arno Galvez

#include "GPUMailManager.h"

#include "platform/Sys.h"
#include "renderer/Check.h"
#include "renderer/VkAllocator.h"
#include "renderer/VkBackend.h"
#include "rnLib/Math.h"

#include <cstring>

namespace vkRuna
{
namespace render
{
GPUMailManager g_gpuMail;

GPUMailManager::GPUMailManager() {}

void GPUMailManager::Init()
{
	const VkDeviceSize bufferSize = 2 * 4 * 1920 * 1080;

	VkBufferCreateInfo bufferCI {};
	bufferCI.sType				   = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCI.pNext				   = nullptr;
	bufferCI.flags				   = 0;
	bufferCI.usage				   = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferCI.sharingMode		   = VK_SHARING_MODE_EXCLUSIVE;
	bufferCI.queueFamilyIndexCount = 0;
	bufferCI.pQueueFamilyIndices   = nullptr;
	bufferCI.size				   = bufferSize;

	auto &vkContext = GetVulkanContext();
	for ( auto &mail : m_mails )
	{
		VK_CHECK( vkCreateBuffer( vkContext.device, &bufferCI, nullptr, &mail.buffer ) );
		mail.occupiedSize = 0;
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements( vkContext.device, m_mails[ 0 ].buffer, &memRequirements );

	VkMemoryPropertyFlags requiredProps, preferredProps;
	UsageToMemPropsFlags( VULKAN_MEMORY_USAGE_CPU_TO_GPU, requiredProps, preferredProps );
	int memTypeIndex = FindMemoryType( &vkContext.gpu.memProps, memRequirements.memoryTypeBits, preferredProps );
	if ( memTypeIndex == -1 )
	{
		memTypeIndex = FindMemoryType( &vkContext.gpu.memProps, memRequirements.memoryTypeBits, requiredProps );
	}
	CHECK_PRED( memTypeIndex != -1 );

	m_bufferSize = Align( memRequirements.size, memRequirements.alignment );

	VkMemoryAllocateInfo allocateInfo {};
	allocateInfo.sType			 = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.pNext			 = nullptr;
	allocateInfo.memoryTypeIndex = memTypeIndex;
	allocateInfo.allocationSize	 = GPU_MAIL_BUFFERING_LEVEL * m_bufferSize;
	VK_CHECK( vkAllocateMemory( vkContext.device, &allocateInfo, nullptr, &m_memory ) );

	for ( size_t i = 0; i < m_mails.size(); ++i )
	{
		VK_CHECK( vkBindBufferMemory( vkContext.device, m_mails[ i ].buffer, m_memory, i * m_bufferSize ) );
	}

	VK_CHECK( vkMapMemory( vkContext.device,
						   m_memory,
						   0,
						   GPU_MAIL_BUFFERING_LEVEL * m_bufferSize,
						   0,
						   reinterpret_cast< void ** >( &m_mappedData ) ) );

	VkCommandPoolCreateInfo commandPoolCI {};
	commandPoolCI.sType			   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCI.pNext			   = nullptr;
	commandPoolCI.flags			   = 0;
	commandPoolCI.queueFamilyIndex = vkContext.graphicsFamilyId;
	VK_CHECK( vkCreateCommandPool( vkContext.device, &commandPoolCI, nullptr, &m_commandPool ) );

	VkCommandBufferAllocateInfo cmdBufferAllocateInfo {};
	cmdBufferAllocateInfo.sType				 = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufferAllocateInfo.pNext				 = nullptr;
	cmdBufferAllocateInfo.commandPool		 = m_commandPool;
	cmdBufferAllocateInfo.level				 = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBufferAllocateInfo.commandBufferCount = 1;

	VkFenceCreateInfo fenceCI {};
	fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCI.pNext = nullptr;
	fenceCI.flags = 0;

	VkCommandBufferBeginInfo cmdBufferBeginInfo {};
	cmdBufferBeginInfo.sType			= VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufferBeginInfo.pNext			= nullptr;
	cmdBufferBeginInfo.flags			= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	cmdBufferBeginInfo.pInheritanceInfo = nullptr;

	for ( size_t i = 0; i < m_mails.size(); ++i )
	{
		m_mails[ i ].data = m_mappedData + i * m_bufferSize;
		VK_CHECK( vkAllocateCommandBuffers( vkContext.device, &cmdBufferAllocateInfo, &m_mails[ i ].cmdBuffer ) );
		VK_CHECK( vkCreateFence( vkContext.device, &fenceCI, nullptr, &m_mails[ i ].fence ) );
		VK_CHECK( vkBeginCommandBuffer( m_mails[ i ].cmdBuffer, &cmdBufferBeginInfo ) );
	}
}

void GPUMailManager::Shutdown()
{
	auto &device = GetVulkanContext().device;

	for ( auto &mail : m_mails )
	{
		vkFreeCommandBuffers( device, m_commandPool, 1, &mail.cmdBuffer );
		vkDestroyFence( device, mail.fence, nullptr );
		vkDestroyBuffer( device, mail.buffer, nullptr );
	}

	if ( m_commandPool != VK_NULL_HANDLE )
	{
		vkDestroyCommandPool( device, m_commandPool, nullptr );
		m_commandPool = VK_NULL_HANDLE;
	}

	if ( m_memory != VK_NULL_HANDLE )
	{
		vkUnmapMemory( device, m_memory );
		vkFreeMemory( device, m_memory, nullptr );
		m_memory = VK_NULL_HANDLE;
	}

	std::memset( this, 0, sizeof( *this ) ); // risky ?
}

void GPUMailManager::Submit( VkDeviceSize	  size,
							 VkDeviceSize	  alignment,
							 const void *	  data,
							 VkBuffer &		  buffer,
							 VkDeviceSize &	  offset,
							 VkCommandBuffer &cmdBuffer )
{
	CHECK_PRED( size <= m_bufferSize );

	GPUMail_t *gpuMail = &m_mails[ m_currentMail ];

	gpuMail->occupiedSize = Align( gpuMail->occupiedSize, alignment );

	if ( gpuMail->occupiedSize + size > m_bufferSize )
	{
		Flush();
		gpuMail = &m_mails[ m_currentMail ];
		// CHECK_PRED( gpuMail->occupiedSize == 0 ); // occupiedSize could be > 0, if mail is in flight
	}

	if ( gpuMail->submitted == true )
	{
		Wait( m_currentMail );
	}

	std::memcpy( gpuMail->data + gpuMail->occupiedSize, data, size );

	buffer	  = gpuMail->buffer;
	offset	  = gpuMail->occupiedSize;
	cmdBuffer = gpuMail->cmdBuffer;

	gpuMail->occupiedSize += size;
}

VkCommandBuffer GPUMailManager::GetCmdBuffer()
{
	Wait( m_currentMail );
	return m_mails[ m_currentMail ].cmdBuffer;
}

void GPUMailManager::Flush()
{
	auto &gpuMail = m_mails[ m_currentMail ];
	if ( gpuMail.submitted == true /*|| gpuMail.occupiedSize == 0*/ )
	{
		return;
	}

	auto &vkContext = GetVulkanContext();

	VkMappedMemoryRange memRange {};
	memRange.sType	= VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	memRange.pNext	= nullptr;
	memRange.memory = m_memory;
	memRange.offset = 0;
	memRange.size	= VK_WHOLE_SIZE;

	VK_CHECK( vkEndCommandBuffer( gpuMail.cmdBuffer ) );

	VK_CHECK( vkFlushMappedMemoryRanges( vkContext.device, 1, &memRange ) );

	VkSubmitInfo submitInfo {};
	submitInfo.sType				= VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext				= nullptr;
	submitInfo.commandBufferCount	= 1;
	submitInfo.pCommandBuffers		= &gpuMail.cmdBuffer;
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.waitSemaphoreCount	= 0;

	// Log( "Submitting cmd buffer %p\n", gpuMail.cmdBuffer );
	VK_CHECK( vkQueueSubmit( vkContext.graphicsQueue, 1, &submitInfo, gpuMail.fence ) );

	gpuMail.submitted = true;

	for ( size_t i = 0; i < m_mails.size(); ++i )
	{
		m_currentMail = ( m_currentMail + 1 ) % m_mails.size();
		if ( m_mails[ m_currentMail ].submitted == false )
		{
			break;
		}
	}
}

void GPUMailManager::Wait( uint32_t gpuMaiId )
{
	auto &gpuMail = m_mails[ gpuMaiId ];
	if ( gpuMail.submitted == false )
	{
		return;
	}

	auto &device = GetVulkanContext().device;

	VK_CHECK( vkWaitForFences( device, 1, &gpuMail.fence, VK_TRUE, UINT64_MAX ) );

	VK_CHECK( vkResetFences( device, 1, &gpuMail.fence ) );

	VK_CHECK( vkResetCommandPool( device, m_commandPool, 0 ) );

	gpuMail.submitted	 = false;
	gpuMail.occupiedSize = 0;

	VkCommandBufferBeginInfo cmdBufferBeginInfo {};
	cmdBufferBeginInfo.sType			= VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufferBeginInfo.pNext			= nullptr;
	cmdBufferBeginInfo.flags			= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	cmdBufferBeginInfo.pInheritanceInfo = nullptr;
	VK_CHECK( vkBeginCommandBuffer( gpuMail.cmdBuffer, &cmdBufferBeginInfo ) );
	// Log( "Began cmd buffer %p\n", gpuMail.cmdBuffer );
}

void GPUMailManager::WaitAll()
{
	for ( uint32_t i = 0; i < m_mails.size(); ++i )
	{
		Wait( i );
	}
}

} // namespace render
} // namespace vkRuna
