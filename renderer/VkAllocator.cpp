// Copyright (c) 2021 Arno Galvez

#include "VkAllocator.h"

#include "platform/Heap.h"
#include "renderer/Check.h"
#include "renderer/VkBackend.h"
#include "rnLib/Math.h"

#include <algorithm>
#include <cstdio>

namespace vkRuna
{
namespace render
{
static const VkDeviceSize g_deviceLocalBlocksCount = 256;
static const VkDeviceSize g_hostVisibleBlocksCount = 512;

static VulkanAllocator g_allocator;

void UsageToMemPropsFlags( const vulkanMemoryUsage_t usage,
						   VkMemoryPropertyFlags &	 required,
						   VkMemoryPropertyFlags &	 preferred )
{
	switch ( usage )
	{
		case VULKAN_MEMORY_USAGE_GPU_ONLY:
			required  = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			preferred = required;
			break;
		case VULKAN_MEMORY_USAGE_CPU_ONLY:
			required  = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			preferred = required;
			break;
		case VULKAN_MEMORY_USAGE_CPU_TO_GPU:
			required  = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			preferred = required | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			break;
		case VULKAN_MEMORY_USAGE_GPU_TO_CPU:
			required  = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
			preferred = required | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
			break;
		default:
			CHECK_PRED( false );
			required  = 0;
			preferred = 0;
			break;
	}
}

int32_t FindMemoryType( const VkPhysicalDeviceMemoryProperties *pMemoryProperties,
						uint32_t								memoryTypeBitsRequirement,
						VkMemoryPropertyFlags					requiredProperties )
{
	const uint32_t memoryCount = pMemoryProperties->memoryTypeCount;
	for ( uint32_t memoryTypeIndex = 0; memoryTypeIndex < memoryCount; ++memoryTypeIndex )
	{
		const uint32_t memoryTypeBits		= ( 1 << memoryTypeIndex );
		const bool	   isRequiredMemoryType = memoryTypeBitsRequirement & memoryTypeBits;

		const VkMemoryPropertyFlags properties = pMemoryProperties->memoryTypes[ memoryTypeIndex ].propertyFlags;
		const bool					hasRequiredProperties = ( properties & requiredProperties ) == requiredProperties;

		if ( isRequiredMemoryType && hasRequiredProperties )
			return static_cast< int32_t >( memoryTypeIndex );
	}

	// failed to find memory type
	return -1;
}

VulkanBlock::VulkanBlock( uint32_t memoryTypeIndex, VkDeviceSize size, vulkanMemoryUsage_t usage )
	: m_memTypeIndex( memoryTypeIndex )
	, m_size( size )
	, m_usage( usage )
{
}

VulkanBlock::~VulkanBlock()
{
	Shutdown();
}

VulkanAllocator g_vulkanAllocator;

void VulkanBlock::Init()
{
	auto &vkContext = GetVulkanContext();

	VkMemoryAllocateInfo allocateInfo {};
	allocateInfo.sType			 = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.pNext			 = nullptr;
	allocateInfo.allocationSize	 = m_size;
	allocateInfo.memoryTypeIndex = m_memTypeIndex;

	// cpu -> gpu
	// https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html#VkPhysicalDeviceMaintenance3Properties
	VK_CHECK( vkAllocateMemory( vkContext.device, &allocateInfo, nullptr, &m_deviceMemory ) );

	if ( IsHostVisible() )
	{
		VK_CHECK( vkMapMemory( vkContext.device, m_deviceMemory, 0, m_size, 0, &m_data ) );
	}

	chunk_t chunk;
	chunk.id	 = m_nextChunkId++;
	chunk.size	 = m_size;
	chunk.offset = 0;
	chunk.type	 = VULKAN_ALLOCATION_TYPE_FREE;

	m_chunks.clear();
	m_chunks.emplace_back( chunk );
}

void VulkanBlock::Shutdown()
{
	auto &vkContext = GetVulkanContext();

	if ( IsHostVisible() )
	{
		vkUnmapMemory( vkContext.device, m_deviceMemory );
	}

	vkFreeMemory( vkContext.device, m_deviceMemory, nullptr );

	m_chunks.clear();

	m_deviceMemory = VK_NULL_HANDLE;
	m_allocated	   = 0;
	m_data		   = nullptr;
	m_nextChunkId  = 0;
}

bool CanAllocationTypesAliase( vulkanAllocationType_t type_1, vulkanAllocationType_t type_2 )
{
	// https://www.khronos.org/registry/vulkan/specs/1.1/html/vkspec.html#glossary-linear-resource
	switch ( type_1 )
	{
		case VULKAN_ALLOCATION_TYPE_FREE: return false;
		case VULKAN_ALLOCATION_TYPE_BUFFER: return type_2 == VULKAN_ALLOCATION_TYPE_IMAGE_OPTIMAL;
		case VULKAN_ALLOCATION_TYPE_IMAGE_LINEAR: return type_2 == VULKAN_ALLOCATION_TYPE_IMAGE_OPTIMAL;
		case VULKAN_ALLOCATION_TYPE_IMAGE_OPTIMAL:
			return type_2 == VULKAN_ALLOCATION_TYPE_BUFFER || type_2 == VULKAN_ALLOCATION_TYPE_IMAGE_LINEAR;
		default: CHECK_PRED( false );
	}

	return true;
}

// https://www.khronos.org/registry/vulkan/specs/1.1/html/vkspec.html#VkBindImagePlaneMemoryInfo
// A few lines after the VkBindImagePlaneMemoryInfo is described the following function,
// useful to test whether or not two linear and non-linear resources alias.

bool AreResourcesOnSamePage( VkDeviceSize resourceA_end, VkDeviceSize resourceB_offset, VkDeviceSize bufferGranularity )
{
	// bufferGranularity must be a power of two
	CHECK_PRED( bufferGranularity && ( !( bufferGranularity & ( bufferGranularity - 1 ) ) ) )

	VkDeviceSize resourceA_endPage	 = resourceA_end & ~( bufferGranularity - 1 );
	VkDeviceSize resourceB_startPage = resourceB_offset & ~( bufferGranularity - 1 );

	return resourceA_endPage < resourceB_startPage;
}

bool VulkanBlock::Alloc( VkDeviceSize			size,
						 VkDeviceSize			alignment,
						 VkDeviceSize			bufferImageGranularity,
						 vulkanAllocationType_t type,
						 vulkanAllocation_t &	allocation )
{
	VkDeviceSize freeSize = m_size - m_allocated;

	if ( freeSize < size )
	{
		return false;
	}

	VkDeviceSize allocOffset = 0;
	// VkDeviceSize allocPadding = 0;

	int fittingChunkIndex = -1;
	for ( size_t i = 0; i < m_chunks.size(); ++i )
	{
		auto &chunk = m_chunks[ i ];

		if ( chunk.type != VULKAN_ALLOCATION_TYPE_FREE )
		{
			continue;
		}
		if ( chunk.size < size )
		{
			continue;
		}

		allocOffset = Align( chunk.offset, alignment );

		if ( i > 0 )
		{
			auto &previousChunk = m_chunks[ i - 1 ];
			if ( CanAllocationTypesAliase( previousChunk.type, type ) )
			{
				if ( AreResourcesOnSamePage( previousChunk.offset + previousChunk.size - 1,
											 allocOffset,
											 bufferImageGranularity ) )
				{
					allocOffset = Align( allocOffset, bufferImageGranularity );
				}
			}
		}

		if ( allocOffset + size > chunk.offset + chunk.size )
		{
			continue;
		}

		if ( i < m_chunks.size() - 1 )
		{
			auto &nextChunk = m_chunks[ i + 1 ];
			if ( CanAllocationTypesAliase( type, nextChunk.type ) )
			{
				if ( AreResourcesOnSamePage( allocOffset + size - 1, nextChunk.offset, bufferImageGranularity ) )
				{
					continue;
				}
			}
		}

		fittingChunkIndex = static_cast< int >( i );
		break;
	}

	if ( fittingChunkIndex == -1 )
	{
		return false;
	}

	chunk_t *fittingChunk = &m_chunks[ fittingChunkIndex ];

	VkDeviceSize paddedAllocSize = size + ( allocOffset - fittingChunk->offset );

	if ( fittingChunk->offset + fittingChunk->size > allocOffset + size )
	{
		chunk_t leftover;
		leftover.type	= VULKAN_ALLOCATION_TYPE_FREE;
		leftover.id		= m_nextChunkId++;
		leftover.offset = allocOffset + size;
		leftover.size	= fittingChunk->size - paddedAllocSize;

		m_chunks.insert( m_chunks.begin() + fittingChunkIndex + 1, leftover );
	}

	fittingChunk = &m_chunks[ fittingChunkIndex ];

	fittingChunk->type = type;
	fittingChunk->size = paddedAllocSize;

	m_allocated += paddedAllocSize;

	allocation.block		= this;
	allocation.id			= fittingChunk->id;
	allocation.deviceMemory = m_deviceMemory;
	allocation.offset		= allocOffset;
	allocation.size			= size;
	if ( IsHostVisible() )
	{
		allocation.data = static_cast< byte * >( m_data ) + allocOffset;
	}
	else
	{
		allocation.data = nullptr;
	}

	return true;
}

void VulkanBlock::Free( vulkanAllocation_t &allocation )
{
	int chunkIndex = -1;
	for ( int i = 0; i < m_chunks.size(); ++i )
	{
		auto &chunk = m_chunks[ i ];
		if ( chunk.id == allocation.id && chunk.type != VULKAN_ALLOCATION_TYPE_FREE )
		{
			chunkIndex = i;
			break;
		}
	}

	CHECK_PRED( chunkIndex != -1 );

	m_chunks[ chunkIndex ].type = VULKAN_ALLOCATION_TYPE_FREE;
	m_allocated -= m_chunks[ chunkIndex ].size;

	// merge contiguous free chunks
	if ( chunkIndex > 0 )
	{
		auto &previousChunk = m_chunks[ chunkIndex - 1 ];
		if ( previousChunk.type == VULKAN_ALLOCATION_TYPE_FREE )
		{
			previousChunk.size += m_chunks[ chunkIndex ].size;
			m_chunks.erase( m_chunks.begin() + chunkIndex );
			--chunkIndex;
		}
	}

	if ( chunkIndex < m_chunks.size() - 1 )
	{
		auto &nextChunk = m_chunks[ chunkIndex + 1 ];
		if ( nextChunk.type == VULKAN_ALLOCATION_TYPE_FREE )
		{
			m_chunks[ chunkIndex ].size += nextChunk.size;
			m_chunks.erase( m_chunks.begin() + chunkIndex + 1 );
		}
	}

	allocation.block = nullptr;
	// allocation.id           = UINT32_MAX;
	allocation.deviceMemory = VK_NULL_HANDLE;
	allocation.offset		= 0;
	allocation.size			= 0;
	allocation.data			= nullptr;
}

const char *ToStringMemUsage( vulkanMemoryUsage_t usage )
{
	switch ( usage )
	{
		case VULKAN_MEMORY_USAGE_UNKNOWN: return "Unknown usage";
		case VULKAN_MEMORY_USAGE_GPU_ONLY: return "GPU only";
		case VULKAN_MEMORY_USAGE_CPU_ONLY: return "CPU only";
		case VULKAN_MEMORY_USAGE_CPU_TO_GPU: return "CPU to GPU";
		case VULKAN_MEMORY_USAGE_GPU_TO_CPU: return "GPU to CPU";
		default: return "???";
	}
}

const char *ToStringAllocType( vulkanAllocationType_t type )
{
	switch ( type )
	{
		case VULKAN_ALLOCATION_TYPE_FREE: return "Free";
		case VULKAN_ALLOCATION_TYPE_BUFFER: return "Buffer";
		case VULKAN_ALLOCATION_TYPE_IMAGE_LINEAR: return "Image Linear";
		case VULKAN_ALLOCATION_TYPE_IMAGE_OPTIMAL: return "Image Optimal";
		default: return "???";
	}
}

void VulkanBlock::Print() const
{
	std::printf(
		"Size: %llu  -  Allocated: %llu\n"
		"Usage: %s\n"
		"Memory type index: %u\n"
		"Chunks Count: %llu\n",
		m_size,
		m_allocated,
		ToStringMemUsage( m_usage ),
		m_memTypeIndex,
		m_chunks.size() );

	for ( auto &chunk : m_chunks )
	{
		std::printf( "[ %u, %llu, %llu, %s ]\n", chunk.id, chunk.offset, chunk.size, ToStringAllocType( chunk.type ) );
	}
}

VulkanAllocator::VulkanAllocator() {}

VulkanAllocator::~VulkanAllocator()
{
	Shutdown();
}

void VulkanAllocator::Init()
{
	auto &gpu = GetVulkanContext().gpu;

	for ( uint32_t i = 0; i < gpu.memProps.memoryHeapCount; ++i )
	{
		auto &heap = gpu.memProps.memoryHeaps[ i ];
		if ( heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT )
		{
			m_deviceLocalMemoryBytes = heap.size;
			break;
		}
	}

	for ( uint32_t i = 0; i < gpu.memProps.memoryHeapCount; ++i )
	{
		auto &heap = gpu.memProps.memoryHeaps[ i ];
		if ( heap.flags == 0 )
		{
			m_hostVisibleMemoryBytes = heap.size;
			break;
		}
	}

	m_bufferImageGranularity = gpu.properties.limits.bufferImageGranularity;
}

void VulkanAllocator::Shutdown()
{
	for ( auto &blocks : m_blockChains )
	{
		for ( VulkanBlock *block : blocks )
		{
			delete block;
		}

		blocks.clear();
	}
}

vulkanAllocation_t VulkanAllocator::Alloc( vulkanAllocationType_t type,
										   vulkanMemoryUsage_t	  usage,
										   VkMemoryRequirements	  requirements )
{
	CHECK_PRED( type != VULKAN_ALLOCATION_TYPE_FREE );
	CHECK_PRED( usage != VULKAN_MEMORY_USAGE_UNKNOWN );

	vulkanAllocation_t allocation;

	auto &gpu = GetVulkanContext().gpu;

	uint32_t flagsRequired, flagsPreferred;
	UsageToMemPropsFlags( usage, flagsRequired, flagsPreferred );

	int memoryTypeId = FindMemoryType( &gpu.memProps, requirements.memoryTypeBits, flagsPreferred );
	if ( memoryTypeId == -1 )
	{
		memoryTypeId = FindMemoryType( &gpu.memProps, requirements.memoryTypeBits, flagsRequired );
	}
	CHECK_PRED( memoryTypeId != -1 );

	auto &blocks = m_blockChains[ memoryTypeId ];

	for ( auto &block : blocks )
	{
		if ( block->GetMemoryTypeIndex() != memoryTypeId )
		{
			// this should never happen
			CHECK_PRED( false );
			continue;
		}

		if ( block->Alloc( requirements.size, requirements.alignment, m_bufferImageGranularity, type, allocation ) )
		{
			// Print();
			return allocation;
		}
	}

	VkDeviceSize blockSize = usage != VULKAN_MEMORY_USAGE_GPU_ONLY
								 ? m_hostVisibleMemoryBytes / g_hostVisibleBlocksCount
								 : m_deviceLocalMemoryBytes / g_deviceLocalBlocksCount;

	auto *block = new VulkanBlock( memoryTypeId, blockSize, usage );
	block->Init();
	CHECK_PRED_MSG(
		block->Alloc( requirements.size, requirements.alignment, m_bufferImageGranularity, type, allocation ),
		"GPU Block allocation failed." );

	blocks.emplace_back( block );

	// Print();

	return allocation;
}

void VulkanAllocator::Free( vulkanAllocation_t &allocation )
{
	if ( allocation.block == nullptr )
	{
		return;
	}
	m_garbage.emplace_back( allocation );
	std::memset( &allocation, 0, sizeof( allocation ) );
}

void VulkanAllocator::EmptyGarbage()
{
	for ( auto &allocation : m_garbage )
	{
		auto *block = allocation.block;

		block->Free( allocation );
		if ( block->GetAllocatedSize() == 0 )
		{
			auto &blockChain = m_blockChains[ block->GetMemoryTypeIndex() ];

			auto pos = std::find( blockChain.cbegin(), blockChain.cend(), block );
			CHECK_PRED( pos != blockChain.cend() );

			blockChain.erase( pos );
			delete block;
		}
	}

	m_garbage.clear();
}

void VulkanAllocator::Print() const
{
	std::printf(
		"VulkanAllocator\n"
		"- - - - - - - -\n"
		"Device local memory bytes: %llu bytes\n"
		"Host visible memory bytes: %llu bytes\n\n"
		"Blocks:\n"
		"[id, offset, size, type]\n\n",
		m_deviceLocalMemoryBytes,
		m_hostVisibleMemoryBytes );

	for ( const auto &blocks : m_blockChains )
	{
		if ( blocks.size() == 0 )
		{
			continue;
		}

		std::printf( "****************\n" );
		for ( const auto &block : blocks )
		{
			block->Print();
		}
		std::printf( "****************\n" );
	}
}

} // namespace render
} // namespace vkRuna
