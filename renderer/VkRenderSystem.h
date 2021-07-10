// Copyright (c) 2021 Arno Galvez

#pragma once

#include "renderer/RenderSystem.h"

#include <vector>

namespace vkRuna
{
namespace render
{
class VkRenderSystem : public RenderSystem
{
   public:
	void Init() final;
	void Shutdown() final;

	void BeginFrame() final;
	void EndFrame() final;

	int GetRenderCmds( gpuCmd_t **firstCmd ) final;
	int GetPreRenderCmds( gpuCmd_t **firstCmd ) final;

	void SetUBOVar( int count, const char *const *vars, const float *values ) final;

   private:
	std::vector< gpuCmd_t > m_renderCmds;
};

} // namespace render
} // namespace vkRuna
