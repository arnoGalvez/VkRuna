// Copyright (c) 2021 Arno Galvez

#include "renderer/VkRenderSystem.h"

#include "renderer/RenderProgs.h"
#include "renderer/VFX.h"
#include "renderer/uiBackend.h"

#include <cstring>

namespace vkRuna
{
namespace render
{
VkRenderSystem		g_vkRenderSystem;
RenderSystem *const g_renderSystem = &g_vkRenderSystem;

void VkRenderSystem::Init()
{
	g_vfxManager.Init();

	m_renderCmds.resize( 8 );
}

void VkRenderSystem::Shutdown()
{
	g_vfxManager.Shutdown();
}

void VkRenderSystem::BeginFrame()
{
	// g_uiBackend.BeginFrame();
}

void VkRenderSystem::EndFrame()
{
	g_uiBackend.EndFrame();
}

int VkRenderSystem::GetRenderCmds( gpuCmd_t **firstCmd )
{
	gpuCmd_t *vfxCmds;
	const int vfxCount = g_vfxManager.GetRenderCmds( &vfxCmds );

	auto *uiRenderData = g_uiBackend.GetDrawData();

	const int cmdCount = vfxCount + ( uiRenderData != nullptr );

	if ( cmdCount > m_renderCmds.size() )
	{
		m_renderCmds.resize( cmdCount );
	}

	int i = 0;
	// for ( ; i < vfxCount; ++i ) {
	//	std::memcpy( &m_renderCmds[ i ], &vfxCmds[ i ], sizeof( m_renderCmds[ i ] ) );
	//}

	std::memcpy( &m_renderCmds[ i ], &vfxCmds[ i ], vfxCount * sizeof( m_renderCmds[ i ] ) );
	i = vfxCount;

	if ( uiRenderData )
	{
		m_renderCmds[ i ].type = CT_UI;
		m_renderCmds[ i ].obj  = uiRenderData;
	}

	*firstCmd = m_renderCmds.data();

	return cmdCount;
}

int VkRenderSystem::GetPreRenderCmds( gpuCmd_t **firstCmd )
{
	return g_vfxManager.GetPreRenderCmds( firstCmd );
}

void VkRenderSystem::SetUBOVar( int count, const char *const *vars, const float *values )
{
	g_pipelineManager.SetSharedVar( static_cast< size_t >( count ), vars, values );
}

} // namespace render
} // namespace vkRuna
