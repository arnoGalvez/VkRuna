// Copyright (c) 2021 Arno Galvez

#include "renderer/RenderSystem.h"

#include "renderer/VkRenderSystem.h"

namespace vkRuna
{
namespace render
{
RenderSystem &RenderSystem::GetInstance()
{
	return *g_renderSystem;
}

} // namespace render
} // namespace vkRuna