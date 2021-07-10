// Copyright (c) 2021 Arno Galvez

#pragma once

#include "external/vulkan/vulkan.hpp"
#include "platform/defines.h"
#include "renderer/VkAllocator.h"

#include <string>

namespace vkRuna
{
namespace render
{
enum textureType_t
{
	TT_UNDEFINED,
	TT_1D,
	TT_2D,
	TT_3D,
	TT_CUBE,
	TT_DEPTH
};

struct imageOpts_t
{
	textureType_t	  type		 = TT_UNDEFINED;
	VkFormat		  format	 = VK_FORMAT_UNDEFINED;
	uint32_t		  width		 = 0;
	uint32_t		  height	 = 0;
	uint32_t		  depth		 = 1;
	uint32_t		  mipLevels	 = 1;
	VkImageUsageFlags usageFlags = 0;
};

struct samplerOpts_t
{
	VkFilter			 filter		 = VK_FILTER_LINEAR;
	VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	VkBorderColor		 borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
};

class Image
{
	NO_COPY_NO_ASSIGN( Image );

   public:
	Image();
	~Image();

   public:
	void AllocImage( const imageOpts_t &imageOpts, const samplerOpts_t &samplerOpts );
	void ClearVulkanResources();

	void Upload( const VkOffset3D &offset,
				 const VkExtent3D &dimensions,
				 uint32_t		   mipLevel,
				 uint32_t		   firstDimLength,
				 uint32_t		   secondDimLength,
				 uint32_t		   bytesPerTexel,
				 byte *			   img );

   public:
	const std::string &GetName() const { return m_name; }
	VkFormat		   GetFormat() const { return m_opts.format; }
	VkImage			   GetHandle() const { return m_image; }
	VkImageView		   GetView() const { return m_view; }
	VkSampler		   GetSampler() const { return m_sampler; }
	VkImageLayout	   GetLayout() const { return m_layout; }

   private:
	void CreateSampler();

   private:
	std::string m_name;

	imageOpts_t	  m_opts;
	samplerOpts_t m_samplerOpts;

	VkFormat	  m_format	= VK_FORMAT_UNDEFINED;
	VkImage		  m_image	= VK_NULL_HANDLE;
	VkImageView	  m_view	= VK_NULL_HANDLE;
	VkSampler	  m_sampler = VK_NULL_HANDLE;
	VkImageLayout m_layout	= VK_IMAGE_LAYOUT_UNDEFINED;

	vulkanAllocation_t m_allocation;
};
} // namespace render
} // namespace vkRuna