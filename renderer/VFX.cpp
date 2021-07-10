// Copyright (c) 2021 Arno Galvez

#include "VFX.h"

#include "external/cereal/archives/json.hpp"
#include "game/Game.h"
#include "platform/Sys.h"
#include "platform/Window.h"
#include "renderer/Check.h"
#include "renderer/RenderProgs.h"
#include "renderer/VkBackend.h"
#include "rnLib/Event.h"
#include "rnLib/Math.h"

#include <cmath>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

namespace vkRuna
{
using namespace sys;

const char *EnumToString( vfxRenderPrimitive_t rp )
{
	static_assert( VFX_RP_COUNT == 2, "Unhandled render primitive" );
	switch ( rp )
	{
		case VFX_RP_QUAD: return "Quad";
		case VFX_RP_CUBE: return "Cube";
		default: return "Unknown VFX render primitive";
	}
}

const char *EnumToString( vfxBufferData_t bd )
{
	static_assert( VFX_BD_COUNT == 2, "Unhandled vfx buffer data type" );
	switch ( bd )
	{
		case VFX_BD_FLOAT: return "float";
		case VFX_BD_INT: return "int";
		default: return "Unknown VFX buffer data type";
	}
}

namespace render
{
const char *VFX::SHADER_PARTICLE_TO_REVIVE = "vfxReviveCounter";
const char *VFX::SHADER_PARTICLE_CAPACITY  = "vfxCapacity";
const char *VFX::SHADER_PARTICLES_LIFE_MIN = "vfxLifeMin";
const char *VFX::SHADER_PARTICLES_LIFE_MAX = "vfxLifeMax";
const char *VFX::SHADER_PAD_0			   = "vfxPad_0";

const char *VFX::VERTEX_HEADER_PATH	  = "shaderGen/VFXVertexHeader.glsl";
const char *VFX::VERTEX_FOOTER_PATH	  = "shaderGen/VFXVertexFooter.glsl";
const char *VFX::FRAGMENT_HEADER_PATH = "shaderGen/VFXFragmentHeader.glsl";
const char *VFX::FRAGMENT_FOOTER_PATH = "shaderGen/VFXFragmentFooter.glsl";
const char *VFX::COMPUTE_HEADER_PATH  = "shaderGen/VFXComputeHeader.glsl";
const char *VFX::COMPUTE_FOOTER_PATH  = "shaderGen/VFXComputeFooter.glsl";

static_assert( VFX_BD_COUNT == 2, "Unhandled new VFX buffer data type." );
static const std::array< const std::string, VFX_BD_COUNT > VFX_BUFFER_VALID_TYPES = { "float", "int" };
static const std::array< uint64_t, VFX_BD_COUNT > VFX_BUFFER_TYPES_TO_ELT_SIZE	  = { sizeof( float ), sizeof( int ) };

static_assert( VFX_RP_COUNT == 2, "Unhandled new VFX rendering primitive." );
static const std::array< uint32_t, VFX_RP_COUNT > VFX_RP_TO_NUM_VERTICES = { 2 * 3, 6 * 2 * 3 };

VFXManager g_vfxManager;

VFX::~VFX()
{
	Clear();
}

VFX::VFX( const char *file )
	: m_isValid( false )
	, m_path( file )
	, m_computePipeline( std::make_shared< pipelineProg_t >() )
	, m_graphicsPipeline( std::make_shared< pipelineProg_t >() )
{
	Load( file );
}

void VFX::Load( const char *path )
{
	SetPath( path );
	Log( "Loading VFX %s", path );
	m_isValid = LoadFromJSON( m_path.c_str() );
	Log( "Loading done." );
}

bool VFX::Save( const char *path )
{
	return SaveToJson( path );
}

void VFX::ReloadBuffers()
{
	AllocBuffers();
	InitBarriers();
}

bool VFX::GetComputeCmd( gpuCmd_t &computeCmd )
{
	if ( !IsValid() )
	{
		return false;
	}

	uint32_t groupCountX = m_capacity / COMPUTE_GROUP_SIZE_X;

	computeCmd.type				  = CT_COMPUTE;
	computeCmd.groupCountDim[ 0 ] = groupCountX;
	computeCmd.groupCountDim[ 1 ] = 1;
	computeCmd.groupCountDim[ 2 ] = 1;
	computeCmd.pipeline			  = m_computePipeline.get();

	return true;
}

bool VFX::InsertRenderCmds( std::vector< gpuCmd_t > &renderCmds )
{
	// #TODO return 2 renderCmd, as right now if there is more than one instance, the last one may be partially wasted,
	// because the number of particles is not a multiple of nbIndices.

	if ( !IsValid() )
	{
		return false;
	}

	uint32_t nbIndices = GetIndicesCount();

	if ( m_indexBuffer.GetAllocSize() == 0 )
	{
		uint16_t *indices = new uint16_t[ nbIndices ];
		switch ( m_renderPrimitive )
		{
			case vkRuna::VFX_RP_QUAD:
			{
				indices[ 0 ] = 0b11;
				indices[ 1 ] = 0b10;
				indices[ 2 ] = 0b01;

				indices[ 3 ] = 0b01;
				indices[ 4 ] = 0b10;
				indices[ 5 ] = 0b00;

				break;
			}
			case vkRuna::VFX_RP_CUBE:
			{
				// 0bxyz
				const uint16_t A = 0b101;
				const uint16_t B = 0b111;
				const uint16_t C = 0b110;
				const uint16_t D = 0b100;
				const uint16_t E = 0b001;
				const uint16_t F = 0b011;
				const uint16_t G = 0b010;
				const uint16_t H = 0b000;

				indices[ 0 ] = A;
				indices[ 1 ] = B;
				indices[ 2 ] = C;
				indices[ 3 ] = A;
				indices[ 4 ] = C;
				indices[ 5 ] = D;

				indices[ 6 ]  = G;
				indices[ 7 ]  = F;
				indices[ 8 ]  = E;
				indices[ 9 ]  = G;
				indices[ 10 ] = E;
				indices[ 11 ] = H;

				indices[ 12 ] = B;
				indices[ 13 ] = F;
				indices[ 14 ] = G;
				indices[ 15 ] = B;
				indices[ 16 ] = G;
				indices[ 17 ] = C;

				indices[ 18 ] = E;
				indices[ 19 ] = A;
				indices[ 20 ] = D;
				indices[ 21 ] = E;
				indices[ 22 ] = D;
				indices[ 23 ] = H;

				indices[ 24 ] = F;
				indices[ 25 ] = B;
				indices[ 26 ] = A;
				indices[ 27 ] = F;
				indices[ 28 ] = A;
				indices[ 29 ] = E;

				indices[ 30 ] = H;
				indices[ 31 ] = D;
				indices[ 32 ] = C;
				indices[ 33 ] = H;
				indices[ 34 ] = C;
				indices[ 35 ] = G;

				break;
			}
			default: break;
		}

		m_indexBuffer.Alloc( VK_BUFFER_USAGE_INDEX_BUFFER_BIT, BP_STATIC, nbIndices * sizeof( uint16_t ), indices );
		delete[] indices;
	}
	gpuCmd_t renderCmd;
	renderCmd.type = CT_GRAPHIC;

	const uint32_t particlesPerInstance = nbIndices / VFX_RP_TO_NUM_VERTICES[ m_renderPrimitive ];
	renderCmd.drawSurf.Zero();
	renderCmd.drawSurf.indexBuffer		 = &m_indexBuffer;
	renderCmd.drawSurf.indexBufferOffset = 0;
	renderCmd.drawSurf.instanceCount	 = m_capacity;
	renderCmd.drawSurf.indexCount		 = nbIndices;

	renderCmd.drawSurf.instanceCount = m_capacity;
	renderCmd.drawSurf.indexCount	 = VFX_RP_TO_NUM_VERTICES[ m_renderPrimitive ];

	if ( m_depthPrepassPipeline )
	{
		renderCmd.pipeline = m_depthPrepassPipeline.get();
		renderCmds.emplace_back( renderCmd );
	}

	renderCmd.pipeline = m_graphicsPipeline.get();
	renderCmds.emplace_back( renderCmd );

	return true;
}

int VFX::BarriersUpdateToRender( VkBufferMemoryBarrier **barriers )
{
	if ( !IsValid() )
	{
		return 0;
	}

	*barriers = m_barriersUpdateToRender.data();
	return m_attributesCount;
}

int VFX::BarrierRenderToUpdate( VkBufferMemoryBarrier **barriers )
{
	if ( !IsValid() )
	{
		return 0;
	}

	*barriers = m_barriersRenderToUpdate.data();
	return m_attributesCount;
}

void VFX::Update( double deltaFrame )
{
	if ( !IsValid() )
	{
		return;
	}

	m_reviveAcc += deltaFrame * m_spawnRate;

	const int reviveCounterLeftovers = std::max< int >( 0, *( static_cast< int * >( m_revivalCounter.GetPointer() ) ) );
	const double toRevive			 = trunc( m_reviveAcc );

	int n = reviveCounterLeftovers + static_cast< int >( toRevive );
	n *= deltaFrame != 0; // #TODO WTF ???
	m_revivalCounter.Update( sizeof( n ), &n );

	m_reviveAcc -= toRevive;
}

bool VFX::LoadFromJSON( const char *path )
{
	m_isValid = false;

	if ( !g_pipelineManager.CreateEmptyPipelineProg( *m_computePipeline ) )
	{
		return false;
	}
	if ( !g_pipelineManager.CreateEmptyPipelineProg( *m_graphicsPipeline ) )
	{
		return false;
	}

	{
		EventOnShaderRead::Func f =
			std::bind( &VFX::ParseCustomVars, this, std::placeholders::_1, std::placeholders::_2 );
		std::unique_ptr< Event > onShaderRead = std::make_unique< EventOnShaderRead >( std::move( f ) );
		g_pipelineManager.RegisterEvent( *m_computePipeline, std::move( onShaderRead ) );
	}
	{
		EventOnShaderRead::Func f =
			std::bind( &VFX::ParseCustomVars, this, std::placeholders::_1, std::placeholders::_2 );
		std::unique_ptr< Event > onShaderRead = std::make_unique< EventOnShaderRead >( std::move( f ) );
		g_pipelineManager.RegisterEvent( *m_graphicsPipeline, std::move( onShaderRead ) );
	}

	{
		std::ifstream json( path );
		if ( !json.is_open() )
		{
			Error( "Could not open VFX \"%s\".", path );
			return false;
		}

		try
		{
			cereal::JSONInputArchive deserializationArchive( json );
			deserializationArchive( *this );
		}
		catch ( const std::exception &e )
		{
			Error( e.what() );
			return false;
		}
	}

	SetPath( path );

	{
		std::filesystem::path currentPath = std::filesystem::current_path();
		std::filesystem::current_path( sys::ExtractDirPath( GetPath() ) );

		if ( m_computePipeline )
		{
			std::string &computePath = m_computePipeline->shaders[ SS_COMPUTE ]->path;
			if ( !computePath.empty() )
			{
				computePath = std::filesystem::canonical( computePath ).string();
			}
		}

		if ( m_graphicsPipeline )
		{
			std::string &vertexPath = m_graphicsPipeline->shaders[ SS_VERTEX ]->path;
			if ( !vertexPath.empty() )
			{
				vertexPath = std::filesystem::canonical( vertexPath ).string();
			}

			std::string &fragmentPath = m_graphicsPipeline->shaders[ SS_FRAGMENT ]->path;
			if ( !fragmentPath.empty() )
			{
				fragmentPath = std::filesystem::canonical( fragmentPath ).string();
			}
		}

		std::filesystem::current_path( currentPath );
	}

	ReloadBuffers();

	InitPipelines();

	BindBuffers();

	bool pipelinesValid = m_computePipeline->GetStatus() == pipelineStatus_t::Ok &&
						  m_graphicsPipeline->GetStatus() == pipelineStatus_t::Ok;

	if ( pipelinesValid )
	{
		g_pipelineManager.ClearSerializedValues( *m_graphicsPipeline );
		g_pipelineManager.ClearSerializedValues( *m_computePipeline );
	}

	m_isValid = pipelinesValid;

	return m_isValid;
}

bool VFX::SaveToJson( const char *path )
{
	std::string vfxDir = sys::ExtractDirPath( m_path );
	std::string computePath;
	std::string vertexPath;
	std::string fragmentPath;

	if ( m_computePipeline )
	{
		computePath = m_computePipeline->shaders[ SS_COMPUTE ]->path;
		if ( !computePath.empty() )
		{
			m_computePipeline->shaders[ SS_COMPUTE ]->path = std::filesystem::relative( computePath, vfxDir ).string();
		}
	}

	if ( m_graphicsPipeline )
	{
		vertexPath = m_graphicsPipeline->shaders[ SS_VERTEX ]->path;
		if ( !vertexPath.empty() )
		{
			m_graphicsPipeline->shaders[ SS_VERTEX ]->path = std::filesystem::relative( vertexPath, vfxDir ).string();
		}

		fragmentPath = m_graphicsPipeline->shaders[ SS_FRAGMENT ]->path;
		if ( !fragmentPath.empty() )
		{
			m_graphicsPipeline->shaders[ SS_FRAGMENT ]->path =
				std::filesystem::relative( fragmentPath, vfxDir ).string();
		}
	}

	std::ofstream out( GetPath() );

	if ( !out.is_open() )
	{
		Error( "While Saving VFX: could not open file %s for writing.", path );
		return false;
	}

	try
	{
		cereal::JSONOutputArchive archive( out );
		archive( *this );
	}
	catch ( const std::exception &e )
	{
		Error( e.what() );
		return false;
	}

	if ( m_computePipeline )
	{
		m_computePipeline->shaders[ SS_COMPUTE ]->path = computePath;
	}

	if ( m_graphicsPipeline )
	{
		m_graphicsPipeline->shaders[ SS_VERTEX ]->path	 = vertexPath;
		m_graphicsPipeline->shaders[ SS_FRAGMENT ]->path = fragmentPath;
	}

	return true;
}

uint32_t VFX::GetIndicesCount()
{
	return VFX_RP_TO_NUM_VERTICES[ m_renderPrimitive ];
}

void VFX::AllocBuffers()
{
	m_attributesCount	  = 0;
	m_userAttributesCount = 0;
	for ( VFXBuffer_t &vfxBuffer : m_attributesBuffers )
	{
		if ( !vfxBuffer.IsValid() )
		{
			break;
		}

		VkDeviceSize arity = vfxBuffer.arity;
		vfxBuffer.buffer.Alloc( VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
								BP_STATIC,
								Align< VkDeviceSize >( arity * VFX_BUFFER_TYPES_TO_ELT_SIZE[ vfxBuffer.dataType ] *
														   static_cast< VkDeviceSize >( m_capacity ),
													   VULKAN_FILL_BUFFER_ALIGNMENT ) );

		++m_attributesCount;
		++m_userAttributesCount;
	}

	{
		VFXBuffer_t &vfxBuffer = m_attributesBuffers[ m_userAttributesCount ];

		vfxBuffer.dataType = VFX_BD_FLOAT;
		vfxBuffer.arity	   = 1;
		strcpy( vfxBuffer.name, "life" );
		vfxBuffer.buffer.Alloc(
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			BP_STATIC,
			Align< VkDeviceSize >( vfxBuffer.arity * VFX_BUFFER_TYPES_TO_ELT_SIZE[ vfxBuffer.dataType ] *
									   static_cast< VkDeviceSize >( m_capacity ),
								   VULKAN_FILL_BUFFER_ALIGNMENT ) );

		++m_attributesCount;
	}

	uint32_t toRevive = 0;
	m_revivalCounter.Alloc( VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, BP_DYNAMIC, sizeof( toRevive ), &toRevive );

	g_vfxManager.MemsetZeroVFX( *this ); // #TODO move to allocbuffers

	InitBarriers();
}

void VFX::BindBuffers()
{
	/*if ( !CheckPipelines() )
	{
					return;
	}*/

	// Update gpu local buffers
	{
		const int maxBuffNameSize = VFX_MAX_BUFFER_NAME_LENGTH + 32;

		std::array< char[ maxBuffNameSize ], VFX_MAX_BUFFERS + 1 > bufferNames;
		std::array< const char *, VFX_MAX_BUFFERS + 1 >			   bufferNamesPtrs;
		std::array< const Buffer *, VFX_MAX_BUFFERS + 1 >		   bufferHandles;

		for ( int i = 0; i < m_attributesCount; ++i )
		{
			const VFXBuffer_t &vfxBuffer = m_attributesBuffers[ i ];

			VFXTokenizer::GetBufferInterfaceBlockName( vfxBuffer.name, bufferNames[ i ], maxBuffNameSize );
			bufferNamesPtrs[ i ] = bufferNames[ i ];
			bufferHandles[ i ]	 = &vfxBuffer.buffer;
		}

		// graphics pipeline buffers
		if ( m_graphicsPipeline->GetStatus() == pipelineStatus_t::Ok )
		{
			g_pipelineManager.UpdateBuffers( *m_graphicsPipeline,
											 m_attributesCount,
											 bufferNamesPtrs.data(),
											 bufferHandles.data() );
		}

		// compute pipeline buffers
		if ( m_computePipeline->GetStatus() == pipelineStatus_t::Ok )
		{
			VFXTokenizer::GetBufferInterfaceBlockName( SHADER_PARTICLE_TO_REVIVE,
													   bufferNames[ m_attributesCount ],
													   maxBuffNameSize );
			bufferNamesPtrs[ m_attributesCount ] = bufferNames[ m_attributesCount ];
			bufferHandles[ m_attributesCount ]	 = &m_revivalCounter;

			g_pipelineManager.UpdateBuffers( *m_computePipeline,
											 m_attributesCount + 1,
											 bufferNamesPtrs.data(),
											 bufferHandles.data() );
		}
	}

	// Update ubos
	{
		const std::array< const char *, 3 > uboVarNames = { SHADER_PARTICLE_CAPACITY,
															SHADER_PARTICLES_LIFE_MIN,
															SHADER_PARTICLES_LIFE_MAX };

		const std::array< size_t, 3 > byteSizes = { size_t( GetMemberTypeByteSize( MT_VEC4 ) ),
													size_t( GetMemberTypeByteSize( MT_VEC4 ) ),
													size_t( GetMemberTypeByteSize( MT_VEC4 ) ) };

		std::array< float, uboVarNames.size() * 4 > values;

		float capacity = static_cast< float >( m_capacity );
		values[ 0 ] = values[ 1 ] = values[ 2 ] = values[ 3 ] = capacity;
		values[ 4 ] = values[ 5 ] = values[ 6 ] = values[ 7 ] = m_lifeMin;
		values[ 8 ] = values[ 9 ] = values[ 10 ] = values[ 11 ] = m_lifeMax;

		if ( m_computePipeline->GetStatus() == pipelineStatus_t::Ok )
		{
			g_pipelineManager.UpdateUBOs( *m_computePipeline,
										  uboVarNames.size(),
										  uboVarNames.data(),
										  byteSizes.data(),
										  values.data() );
		}
		if ( m_graphicsPipeline->GetStatus() == pipelineStatus_t::Ok )
		{
			g_pipelineManager.UpdateUBOs( *m_graphicsPipeline,
										  uboVarNames.size(),
										  uboVarNames.data(),
										  byteSizes.data(),
										  values.data() );
		}
	}
}

bool VFX::CheckPipelines()
{
	if ( m_graphicsPipeline->GetStatus() != pipelineStatus_t::Ok ||
		 m_computePipeline->GetStatus() != pipelineStatus_t::Ok )
	{
		Error( "VFX error: corrupted pipelines." );
		return false;
	}

	return true;
}

void VFX::InitBarriers()
{
	/*if ( !CheckPipelines() )
	{
					return;
	}*/

	std::memset( m_barriersUpdateToRender.data(),
				 0,
				 m_barriersUpdateToRender.size() * sizeof( m_barriersUpdateToRender[ 0 ] ) );
	for ( int i = 0; i < m_attributesCount; ++i )
	{
		VFXBuffer_t &vfxBuffer = m_attributesBuffers[ i ];

		VkBufferMemoryBarrier &barrier = m_barriersUpdateToRender[ i ];
		barrier.sType				   = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		barrier.pNext				   = nullptr;
		barrier.srcAccessMask		   = VK_ACCESS_MEMORY_WRITE_BIT;
		barrier.dstAccessMask		   = VK_ACCESS_SHADER_READ_BIT;
		barrier.srcQueueFamilyIndex	   = GetVulkanContext().graphicsFamilyId;
		barrier.dstQueueFamilyIndex	   = GetVulkanContext().graphicsFamilyId;
		barrier.buffer				   = vfxBuffer.buffer.GetHandle();
		barrier.offset				   = 0;
		barrier.size				   = VK_WHOLE_SIZE;
	}

	std::memset( m_barriersRenderToUpdate.data(),
				 0,
				 m_barriersRenderToUpdate.size() * sizeof( m_barriersRenderToUpdate[ 0 ] ) );
	for ( int i = 0; i < m_attributesCount; ++i )
	{
		VFXBuffer_t &vfxBuffer = m_attributesBuffers[ i ];

		VkBufferMemoryBarrier &barrier = m_barriersRenderToUpdate[ i ];
		barrier.sType				   = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		barrier.pNext				   = nullptr;
		barrier.srcAccessMask		   = VK_ACCESS_SHADER_READ_BIT;
		barrier.dstAccessMask		   = VK_ACCESS_MEMORY_WRITE_BIT;
		barrier.srcQueueFamilyIndex	   = GetVulkanContext().graphicsFamilyId;
		barrier.dstQueueFamilyIndex	   = GetVulkanContext().graphicsFamilyId;
		barrier.buffer				   = vfxBuffer.buffer.GetHandle();
		barrier.offset				   = 0;
		barrier.size				   = VK_WHOLE_SIZE;
	}
}

void VFX::InitPipelines()
{
	/*uint64_t state = m_graphicsPipeline->stateBits;
	if ( m_renderPrimitive == VFX_RP_QUAD )
	{
		state &= ~DST_BLEND_FACTOR_MASK;
		state |= DSTBLEND_FACTOR_ONE;
		state &= ~DEPTH_TEST_ENABLE;
	}
	else
	{
		state &= ~DST_BLEND_FACTOR_MASK;
		state |= DSTBLEND_FACTOR_ZERO;
		state |= DEPTH_TEST_ENABLE;
	}
	g_pipelineManager.UpdateState( *m_graphicsPipeline, state );*/
	SetRenderPrimitive( m_renderPrimitive );

	if ( !g_pipelineManager.Reload( *m_graphicsPipeline ) )
	{
		Error( "Failed to initialize graphics pipeline for VFX %s", GetPath().c_str() );
	}
	if ( !g_pipelineManager.Reload( *m_computePipeline ) )
	{
		Error( "Failed to initialize compute pipeline for VFX %s", GetPath().c_str() );
	}

	SetupRenderpass();
}

void VFX::SetupRenderpass()
{
	m_depthPrepassPipeline = nullptr;

	static_assert( VFX_RP_COUNT == 2, "Unhandled VFX render primitive" );
	switch ( m_renderPrimitive )
	{
		case vkRuna::VFX_RP_QUAD: break;
		case vkRuna::VFX_RP_CUBE:
		{
			if ( m_graphicsPipeline->GetStatus() == pipelineStatus_t::Ok )
			{
				m_depthPrepassPipeline =
					std::unique_ptr< pipelineProg_t, depthPrepassDeleter_t >( new pipelineProg_t,
																			  depthPrepassDeleter_t() );

				g_pipelineManager.CreateDepthPrepassPipeline( *m_depthPrepassPipeline, *m_graphicsPipeline );
			}
			break;
		}
		default: CHECK_PRED( false ); break;
	}
}

void VFX::SetRenderPrimitive( vfxRenderPrimitive_t renderPrimitive )
{
	m_renderPrimitive = renderPrimitive;

	uint64_t state = m_graphicsPipeline->stateBits;
	if ( m_renderPrimitive == VFX_RP_QUAD )
	{
		state = StateSetDstBlend( state, DSTBLEND_FACTOR_ONE );
		state = StateSetDepthTest( state, false );
		state = StateSetDepthWrite( state, false );
		state = StateSetCullMode( state, CULL_MODE_NONE );
	}
	else
	{
		state = StateSetDstBlend( state, DSTBLEND_FACTOR_ZERO );
		state = StateSetDepthTest( state, true );
		state = StateSetDepthWrite( state, false );
		state = StateSetDepthOp( state, DEPTH_COMPARE_OP_EQUAL );
		state = StateSetCullMode( state, CULL_MODE_BACK_BIT );
	}
	g_pipelineManager.UpdateState( *m_graphicsPipeline, state );
}

NO_DISCARD bool VFX::ParseCustomVars( std::string *shaderCode, shaderStage_t shaderStage )
{
	AddShaderCodeHeaderAndFooter( *shaderCode, shaderStage );

	std::unique_ptr< ShaderTokenizer > vfxTokenizer = std::make_unique< VFXTokenizer >( this, shaderStage );
	std::vector< std::unique_ptr< ShaderTokenizer > > tokenizerOut;

	if ( !g_shaderLexer.Parse( *shaderCode, 1, &vfxTokenizer, tokenizerOut ) )
	{
		return false;
	}
	shaderCode->clear();
	g_shaderLexer.Combine( tokenizerOut, *shaderCode );

	return true;
}

void VFX::AddShaderCodeHeaderAndFooter( std::string &shaderCode, shaderStage_t stage )
{
	std::string header;
	std::string footer;
	switch ( stage )
	{
		case vkRuna::SS_VERTEX:
		{
			header = ReadFile( VERTEX_HEADER_PATH );
			footer = ReadFile( VERTEX_FOOTER_PATH );
			break;
		}
		case vkRuna::SS_FRAGMENT:
		{
			header = ReadFile( FRAGMENT_HEADER_PATH );
			footer = ReadFile( FRAGMENT_FOOTER_PATH );
			break;
		}
		case vkRuna::SS_COMPUTE:
		{
			header = ReadFile( COMPUTE_HEADER_PATH );
			footer = ReadFile( COMPUTE_FOOTER_PATH );
			break;
		}

		default:
		{
			CHECK_PRED( false );
			break;
		}
	}

	shaderCode = header + shaderCode + footer;
}

int VFX::GetUBOMembers( char *buffer, size_t bufferSize ) const
{
	// #TODO pad according to gpu.limits.minBufferOffset
	int test = sizeof( char ) * std::snprintf( nullptr,
											   0,
											   "\tvec4 %s;\n\tvec4 %s;\n\tvec4 %s;\n\tvec4 %s;",
											   SHADER_PARTICLE_CAPACITY,
											   SHADER_PARTICLES_LIFE_MIN,
											   SHADER_PARTICLES_LIFE_MAX,
											   SHADER_PAD_0 );

	if ( size_t( test + 1 ) > bufferSize )
	{
		CHECK_PRED( false );
	}

	return std::snprintf( buffer,
						  bufferSize,
						  "\tvec4 %s;\n\tvec4 %s;\n\tvec4 %s;\n\tvec4 %s;",
						  SHADER_PARTICLE_CAPACITY,
						  SHADER_PARTICLES_LIFE_MIN,
						  SHADER_PARTICLES_LIFE_MAX,
						  SHADER_PAD_0 );
}

void VFX::Clear()
{
	FreeBuffers();
	m_computePipeline	   = nullptr;
	m_graphicsPipeline	   = nullptr;
	m_depthPrepassPipeline = nullptr;
}

void VFX::FreeBuffers()
{
	for ( VFXBuffer_t &b : m_attributesBuffers )
	{
		b.Free();
	}

	m_indexBuffer.Free(); // #TODO given how this function is called, doing this
						  // here is odd
}

const char *VFX::TypeIndexToStr( int vfxBufferTypeIndex )
{
	CHECK_PRED( vfxBufferTypeIndex >= 0 && vfxBufferTypeIndex < VFX_BUFFER_VALID_TYPES.size() );
	return VFX_BUFFER_VALID_TYPES[ vfxBufferTypeIndex ].c_str();
}

int VFX::GetRevivalCounterName( char *buffer, int bufferSize )
{
	return std::snprintf( buffer, bufferSize, "%s", SHADER_PARTICLE_TO_REVIVE );
}

VFXManager::~VFXManager()
{
	Shutdown();
}

VFXManager::VFXManager() {}

void VFXManager::Init()
{
	m_initPipeline = new pipelineProg_t();
}

void VFXManager::Shutdown()
{
	delete m_initPipeline;
	m_initPipeline = nullptr;

	m_vfxContainer.clear();
	m_preRenderCmds.clear();
	m_barriers.clear();
}

VFXManager::VFXContent_t VFXManager::AddVFXFromFile( const char *file )
{
	auto vfx = MakeVFX( file );
	m_vfxContainer.emplace_back( vfx );

	return vfx;
}

void VFXManager::RemoveVFX( VFXContent_t vfx )
{
	if ( vfx )
	{
		auto last = std::remove( m_vfxContainer.begin(), m_vfxContainer.end(), vfx );
		m_vfxContainer.erase( last, m_vfxContainer.end() );
	}
}

int VFXManager::GetPreRenderCmds( gpuCmd_t **cmds )
{
	m_preRenderCmds.clear();
	m_barriers.clear();

	double deltaFrame = g_game->GetDeltaFrame();
	// std::cout << deltaFrame << '\n';

	// Get memory barriers
	for ( VFXContent_t &vfx : m_vfxContainer )
	{
		if ( !vfx->IsValid() )
		{
			continue;
		}

		vfx->Update( deltaFrame );

		{
			gpuBarrier_t barrier;
			barrier.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			barrier.dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

			VkBufferMemoryBarrier *vkBarriers;
			int					   barriersCount = vfx->BarrierRenderToUpdate( &vkBarriers );
			for ( int i = 0; i < barriersCount; ++i )
			{
				barrier.bufferBarriers.emplace_back( vkBarriers[ i ] );
			}

			m_barriers.emplace_back( std::move( barrier ) );
		}

		{
			gpuBarrier_t barrier;
			barrier.srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			barrier.dstStageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;

			VkBufferMemoryBarrier *vkBarriers;
			int					   count = vfx->BarriersUpdateToRender( &vkBarriers );
			for ( int i = 0; i < count; ++i )
			{
				barrier.bufferBarriers.emplace_back( vkBarriers[ i ] );
			}

			m_barriers.emplace_back( std::move( barrier ) );
		}
	}

	// Create cmds
	size_t barrierIndex = 0;
	for ( VFXContent_t &vfx : m_vfxContainer )
	{
		if ( !vfx->IsValid() )
		{
			continue;
		}

		gpuBarrier_t &barrierRenderToUpdate = m_barriers[ 2 * barrierIndex ];
		gpuBarrier_t &barrierUpdateToRender = m_barriers[ 2 * barrierIndex + 1 ];
		++barrierIndex;

		{
			gpuCmd_t barrierCmd;
			barrierCmd.type = CT_BARRIER;
			barrierCmd.obj	= &barrierRenderToUpdate;
			m_preRenderCmds.emplace_back( barrierCmd );
		}

		{
			gpuCmd_t computeCmd;
			vfx->GetComputeCmd( computeCmd );
			m_preRenderCmds.emplace_back( computeCmd );
		}

		{
			gpuCmd_t barrierCmd;
			barrierCmd.type = CT_BARRIER;
			barrierCmd.obj	= &barrierUpdateToRender;
			m_preRenderCmds.emplace_back( barrierCmd );
		}
	}

	*cmds = m_preRenderCmds.data();

	return static_cast< int >( m_preRenderCmds.size() );
}

int VFXManager::GetRenderCmds( gpuCmd_t **cmds )
{
	m_renderCmds.clear();

	for ( VFXContent_t &vfx : m_vfxContainer )
	{
		if ( !vfx->IsValid() )
		{
			continue;
		}

		vfx->InsertRenderCmds( m_renderCmds );
	}

	*cmds = m_renderCmds.data();

	return static_cast< int >( m_renderCmds.size() );
}

VFXManager::VFXContent_t VFXManager::MakeVFX( const char *file )
{
	return std::make_shared< VFX >( file );
}

void VFXManager::MemsetZeroVFX( VFX &vfx )
{
	for ( int i = 0; i < vfx.m_attributesCount; ++i )
	{
		VFX::VFXBuffer_t &vfxBuffer = vfx.m_attributesBuffers[ i ];
		vfxBuffer.Fill( 0 );
	}
}

void VFX::VFXBuffer_t::GetGLSLType( std::string &out ) const
{
	if ( arity == 1 )
	{
		switch ( dataType )
		{
			case vkRuna::VFX_BD_FLOAT: out = "float"; break;
			case vkRuna::VFX_BD_INT: out = "int"; break;
			default: break;
		}
	}
	else
	{
		switch ( dataType )
		{
			case vkRuna::VFX_BD_INT: out = "i"; break;
			default: break;
		}

		out += "vec";
		out += std::to_string( arity );
	}
}

void VFX::VFXBuffer_t::Free()
{
	buffer.Free();
	arity = -1;
}

void VFX::VFXBuffer_t::Fill( uint32_t data )
{
	buffer.Fill( 0 );
}

void VFX::depthPrepassDeleter_t::operator()( pipelineProg_t *pp )
{
	g_pipelineManager.DestroyPipelineProgKeepResources( *pp );
}

} // namespace render
} // namespace vkRuna
