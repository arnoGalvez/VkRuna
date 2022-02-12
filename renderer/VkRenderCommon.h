// Copyright (c) 2021 Arno Galvez

#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

// Hey look ! I just had a shit in my code:
extern "C"
{
	struct VkMemoryBarrier;
	struct VkImageMemoryBarrier;
	struct VkBufferMemoryBarrier;
}

namespace vkRuna
{
namespace render
{
class Buffer;
struct pipelineProg_t;

struct drawSurf_t
{
	Buffer * vertexBuffer		= nullptr;
	uint64_t vertexBufferOffset = 0;	   // byte offset
	Buffer * indexBuffer		= nullptr; // uint16
	uint64_t indexBufferOffset	= 0;	   // byte offset
	// uint64_t transformHandle	= 0;
	uint32_t instanceCount = 0;
	union
	{
		uint32_t vertexCount = 0;
		uint32_t indexCount;
	};

	void Zero() { std::memset( this, 0, sizeof( *this ) ); }
};

struct gpuBarrier_t
{
	uint32_t							 srcStageMask	 = 0;
	uint32_t							 dstStageMask	 = 0;
	uint32_t							 dependencyFlags = 0;
	std::vector< VkMemoryBarrier >		 globalBarriers;
	std::vector< VkBufferMemoryBarrier > bufferBarriers;
	std::vector< VkImageMemoryBarrier >	 imageBarriers;
};

enum gpuCmdType_t : uint16_t
{
	CT_GRAPHIC,
	CT_COMPUTE,
	CT_BARRIER,
	CT_UI,
	CT_UNKNOWN
};

struct gpuCmd_t
{
	gpuCmdType_t			  type = CT_UNKNOWN;
	drawSurf_t				  drawSurf {};
	std::array< uint32_t, 3 > groupCountDim = { 0, 0, 0 };
	pipelineProg_t *		  pipeline		= nullptr;
	void *					  obj			= nullptr;
};

} // namespace render
} // namespace vkRuna
