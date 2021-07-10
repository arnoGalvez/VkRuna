// Copyright (c) 2021 Arno Galvez

#include "Backend.h"

#include "VkBackend.h"

namespace vkRuna
{
namespace render
{
Backend &Backend::GetInstance()
{
	return VulkanBackend::GetInstance();
}

void Backend::OnWindowSizeChanged() {}
} // namespace render
} // namespace vkRuna
