// Copyright (c) 2021 Arno Galvez

#include "Image.h"

#include "renderer/Check.h"
#include "renderer/GPUMailManager.h"
#include "renderer/VkBackend.h"

#ifdef max
	#undef max
#endif

#include <limits>

namespace vkRuna
{
namespace render
{
VkImageType GetImageType( textureType_t textureType_t )
{
	switch ( textureType_t )
	{
		case TT_1D: return VK_IMAGE_TYPE_1D;
		case TT_2D:
		case TT_CUBE:
		case TT_DEPTH: return VK_IMAGE_TYPE_2D;
		case TT_3D: return VK_IMAGE_TYPE_3D;
		case TT_UNDEFINED:
		default: CHECK_PRED( false ) return VK_IMAGE_TYPE_MAX_ENUM;
	}
}

VkImageViewType GetViewType( textureType_t textureType )
{
	switch ( textureType )
	{
		case TT_1D: return VK_IMAGE_VIEW_TYPE_1D;
		case TT_2D: return VK_IMAGE_VIEW_TYPE_2D;
		case TT_3D: return VK_IMAGE_VIEW_TYPE_3D;
		case TT_CUBE: return VK_IMAGE_VIEW_TYPE_CUBE;
		case TT_DEPTH: return VK_IMAGE_VIEW_TYPE_2D;
		case TT_UNDEFINED:
		default: CHECK_PRED( false ); return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
	}
}

Image::Image() {}

Image::~Image()
{
	ClearVulkanResources();
}

void Image::AllocImage( const imageOpts_t &imageOpts, const samplerOpts_t &samplerOpts )
{
	auto &vulkanContext = GetVulkanContext();

	m_opts		  = imageOpts;
	m_samplerOpts = samplerOpts;

	CreateSampler();

	VkImageCreateInfo imageCI {};
	imageCI.sType				  = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCI.pNext				  = nullptr;
	imageCI.flags				  = m_opts.type == TT_CUBE ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
	imageCI.imageType			  = GetImageType( m_opts.type );
	imageCI.format				  = m_opts.format;
	imageCI.extent.width		  = m_opts.width;
	imageCI.extent.height		  = m_opts.height;
	imageCI.extent.depth		  = m_opts.depth;
	imageCI.mipLevels			  = m_opts.mipLevels;
	imageCI.arrayLayers			  = m_opts.type == TT_CUBE ? 6 : 1;
	imageCI.samples				  = VK_SAMPLE_COUNT_1_BIT;
	imageCI.tiling				  = VK_IMAGE_TILING_OPTIMAL;
	imageCI.usage				  = m_opts.usageFlags;
	imageCI.sharingMode			  = VK_SHARING_MODE_EXCLUSIVE;
	imageCI.queueFamilyIndexCount = 0;
	imageCI.pQueueFamilyIndices	  = nullptr;
	imageCI.initialLayout		  = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CHECK( vkCreateImage( vulkanContext.device, &imageCI, nullptr, &m_image ) );

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements( vulkanContext.device, m_image, &memoryRequirements );

	m_allocation = g_vulkanAllocator.Alloc( VULKAN_ALLOCATION_TYPE_IMAGE_OPTIMAL,
											VULKAN_MEMORY_USAGE_GPU_ONLY,
											memoryRequirements );

	VK_CHECK( vkBindImageMemory( vulkanContext.device, m_image, m_allocation.deviceMemory, m_allocation.offset ) );

	const VkComponentMapping componentMapping = { VK_COMPONENT_SWIZZLE_R,
												  VK_COMPONENT_SWIZZLE_G,
												  VK_COMPONENT_SWIZZLE_B,
												  VK_COMPONENT_SWIZZLE_A };
	VkImageViewCreateInfo	 imageViewCI {};
	imageViewCI.sType	   = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCI.pNext	   = nullptr;
	imageViewCI.flags	   = 0;
	imageViewCI.image	   = m_image;
	imageViewCI.viewType   = GetViewType( m_opts.type );
	imageViewCI.format	   = m_opts.format;
	imageViewCI.components = componentMapping;
	imageViewCI.subresourceRange.aspectMask =
		m_opts.type == TT_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	imageViewCI.subresourceRange.baseMipLevel	= 0;
	imageViewCI.subresourceRange.levelCount		= m_opts.mipLevels;
	imageViewCI.subresourceRange.baseArrayLayer = 0;
	imageViewCI.subresourceRange.layerCount		= m_opts.type == TT_CUBE ? 6 : 1;

	VK_CHECK( vkCreateImageView( vulkanContext.device, &imageViewCI, nullptr, &m_view ) );
}

void Image::ClearVulkanResources()
{
	auto &device = GetVulkanContext().device;

	if ( m_view != VK_NULL_HANDLE )
	{
		vkDestroyImageView( device, m_view, nullptr );
		m_view = VK_NULL_HANDLE;
	}

	m_allocation.block->Free( m_allocation );

	if ( m_image != VK_NULL_HANDLE )
	{
		vkDestroyImage( device, m_image, nullptr );
		m_image = VK_NULL_HANDLE;
	}

	if ( m_sampler != VK_NULL_HANDLE )
	{
		vkDestroySampler( device, m_sampler, nullptr );
		m_sampler = VK_NULL_HANDLE;
	}
}

void Image::Upload( const VkOffset3D &offset,
					const VkExtent3D &dimensions,
					uint32_t		  mipLevel,
					uint32_t		  firstDimLength,
					uint32_t		  secondDimLength,
					uint32_t		  bytesPerTexel,
					byte *			  img )
{
	CHECK_PRED( m_opts.type == TT_1D || m_opts.type == TT_2D || m_opts.type == TT_3D );
	CHECK_PRED( dimensions.width > 0 && dimensions.height > 0 && dimensions.depth > 0 && firstDimLength > 0 );

	VkBuffer		mailBuffer		 = VK_NULL_HANDLE;
	VkDeviceSize	mailBufferOffset = UINT64_MAX;
	VkCommandBuffer mailCmdBuffer	 = VK_NULL_HANDLE;
	g_gpuMail.Submit( bytesPerTexel * dimensions.width * dimensions.height * dimensions.depth,
					  16,
					  img,
					  mailBuffer,
					  mailBufferOffset,
					  mailCmdBuffer );

	VkBufferImageCopy bufferImageCopy {};
	bufferImageCopy.bufferOffset	  = mailBufferOffset;
	bufferImageCopy.bufferRowLength	  = firstDimLength;
	bufferImageCopy.bufferImageHeight = secondDimLength;
	bufferImageCopy.imageSubresource.aspectMask =
		m_opts.type == TT_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	bufferImageCopy.imageSubresource.mipLevel		= mipLevel;
	bufferImageCopy.imageSubresource.baseArrayLayer = 0; // TODO: add support for texture arrays
	bufferImageCopy.imageSubresource.layerCount		= 1;
	bufferImageCopy.imageOffset						= offset;
	bufferImageCopy.imageExtent						= dimensions;

	VkImageMemoryBarrier imgBarrier {};
	imgBarrier.sType						   = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imgBarrier.pNext						   = nullptr;
	imgBarrier.srcAccessMask				   = 0;
	imgBarrier.dstAccessMask				   = VK_ACCESS_TRANSFER_WRITE_BIT;
	imgBarrier.oldLayout					   = VK_IMAGE_LAYOUT_UNDEFINED;
	imgBarrier.newLayout					   = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imgBarrier.srcQueueFamilyIndex			   = VK_QUEUE_FAMILY_IGNORED;
	imgBarrier.dstQueueFamilyIndex			   = VK_QUEUE_FAMILY_IGNORED;
	imgBarrier.image						   = m_image;
	imgBarrier.subresourceRange.aspectMask	   = bufferImageCopy.imageSubresource.aspectMask;
	imgBarrier.subresourceRange.baseMipLevel   = 0;
	imgBarrier.subresourceRange.levelCount	   = VK_REMAINING_MIP_LEVELS;
	imgBarrier.subresourceRange.baseArrayLayer = 0;
	imgBarrier.subresourceRange.layerCount	   = VK_REMAINING_ARRAY_LAYERS;

	vkCmdPipelineBarrier( mailCmdBuffer,
						  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
						  VK_PIPELINE_STAGE_TRANSFER_BIT,
						  0,
						  0,
						  nullptr,
						  0,
						  nullptr,
						  1,
						  &imgBarrier );

	vkCmdCopyBufferToImage( mailCmdBuffer,
							mailBuffer,
							m_image,
							VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							1,
							&bufferImageCopy );

	imgBarrier.oldLayout	 = imgBarrier.newLayout;
	imgBarrier.newLayout	 = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imgBarrier.srcAccessMask = imgBarrier.dstAccessMask;
	imgBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier( mailCmdBuffer,
						  VK_PIPELINE_STAGE_TRANSFER_BIT,
						  VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
						  0,
						  0,
						  nullptr,
						  0,
						  nullptr,
						  1,
						  &imgBarrier );

	m_layout = imgBarrier.newLayout;
}

void Image::CreateSampler()
{
	VkSamplerCreateInfo samplerCI {};
	samplerCI.sType		= VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCI.pNext		= nullptr;
	samplerCI.flags		= 0;
	samplerCI.magFilter = m_samplerOpts.filter;
	samplerCI.minFilter = m_samplerOpts.filter;
	samplerCI.mipmapMode =
		m_samplerOpts.filter == VK_FILTER_NEAREST ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCI.addressModeU			  = m_samplerOpts.addressMode;
	samplerCI.addressModeV			  = m_samplerOpts.addressMode;
	samplerCI.addressModeW			  = m_samplerOpts.addressMode;
	samplerCI.mipLodBias			  = 0.0f;
	samplerCI.anisotropyEnable		  = VK_FALSE;
	samplerCI.maxAnisotropy			  = std::numeric_limits< float >::max();
	samplerCI.compareEnable			  = m_opts.type == TT_DEPTH;
	samplerCI.compareOp				  = m_opts.type == TT_DEPTH ? VK_COMPARE_OP_LESS_OR_EQUAL : VK_COMPARE_OP_ALWAYS;
	samplerCI.minLod				  = 0.0f;
	samplerCI.maxLod				  = std::numeric_limits< float >::max();
	samplerCI.borderColor			  = m_samplerOpts.borderColor;
	samplerCI.unnormalizedCoordinates = VK_FALSE;

	auto &vulkanContext = GetVulkanContext();

	VK_CHECK( vkCreateSampler( vulkanContext.device, &samplerCI, nullptr, &m_sampler ) );
}

} // namespace render
} // namespace vkRuna