// Copyright (c) 2021 Arno Galvez

#pragma once

namespace vkRuna
{
namespace render
{
static const int SWAPCHAIN_BUFFERING_LEVEL	   = 3;
static const int COMPUTE_CHAIN_BUFFERING_LEVEL = 3;
static const int GPU_MAIL_BUFFERING_LEVEL	   = 3;

static const int RENDERPROGS_SHARED_BLOCKS_POOL_SIZE = 512;

static const int VFX_MAX_BUFFERS			= 8;
static const int VFX_MAX_BUFFER_NAME_LENGTH = 61;

static const int VULKAN_FILL_BUFFER_ALIGNMENT = 4;

static const int COMPUTE_GROUP_SIZE_X = 32;
static const int COMPUTE_GROUP_SIZE_Y = 1;
static const int COMPUTE_GROUP_SIZE_Z = 1;

} // namespace render
} // namespace vkRuna
