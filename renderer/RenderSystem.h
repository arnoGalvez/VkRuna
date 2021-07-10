// Copyright (c) 2021 Arno Galvez

#pragma once

namespace vkRuna
{
namespace render
{
struct gpuCmd_t;

class RenderSystem
{
   public:
	virtual ~RenderSystem() {}

	static RenderSystem &GetInstance();

	virtual void Init()		= 0;
	virtual void Shutdown() = 0;

	virtual void BeginFrame() = 0;
	virtual void EndFrame()	  = 0;

	virtual int GetRenderCmds( gpuCmd_t **firstCmd )	= 0;
	virtual int GetPreRenderCmds( gpuCmd_t **firstCmd ) = 0;

	virtual void SetUBOVar( int count, const char *const *vars, const float *values ) = 0;
};

extern RenderSystem *const g_renderSystem;
} // namespace render
} // namespace vkRuna
