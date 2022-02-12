// Copyright (c) 2021 Arno Galvez

#include "ui/Controller.h"

#include "renderer/Check.h"
#include "renderer/RenderProgs.h"
#include "renderer/VFX.h"
#include "rnLib/Math.h"

namespace vkRuna
{
using namespace sys;

PipelineController::PipelineController( std::weak_ptr< render::pipelineProg_t > pipeline, uint32_t stageBits )
{
	std::shared_ptr< render::pipelineProg_t > pp = pipeline.lock();
	if ( pp )
	{
		if ( pp->serializedValues )
		{
			m_userValues  = std::make_unique< std::vector< SerializableData > >();
			*m_userValues = *pp->serializedValues;

			render::g_pipelineManager.ClearSerializedValues( *pp );
		}
	}

	SetPipeline( pipeline, stageBits );
}

bool PipelineController::Reload()
{
	m_isValid = false;

	std::shared_ptr< render::pipelineProg_t > pp = m_pipeline.lock();
	if ( pp )
	{
		std::vector< shaderStage_t > shaderStages;
		shaderStages.reserve( m_shaderViews.size() );
		std::vector< const char * > shaderPaths;
		shaderPaths.reserve( m_shaderViews.size() );

		size_t countShadersToUpdate = 0;
		for ( size_t i = 0; i < m_shaderViews.size(); ++i )
		{
			if ( m_shaderViews[ i ].path.size() > 0 )
			{
				shaderPaths.emplace_back( m_shaderViews[ i ].path.c_str() );
				shaderStages.emplace_back( m_shaderViews[ i ].stage );
				++countShadersToUpdate;
			}
		}
		if ( countShadersToUpdate > 0 )
		{
			if ( pp->GetStatus() == render::pipelineStatus_t::Ok )
			{
				if ( !m_userValues )
				{
					m_userValues = std::make_unique< std::vector< SerializableData > >();
				}
				*m_userValues = std::move( render::SerializeInterfaceBlocks( *pp ) );
			}

			if ( !render::g_pipelineManager.LoadShaders( *pp,
														 countShadersToUpdate,
														 shaderStages.data(),
														 shaderPaths.data() ) )
			{
				return false;
			}

			if ( m_userValues )
			{
				render::DeserializeInterfaceBlocks( *pp, *m_userValues );
				m_userValues = nullptr;
			}

			ExtractGPUVarViews( pp );

			m_isValid = pp->GetStatus() == render::pipelineStatus_t::Ok;
		}
	}

	return m_isValid;
}

void PipelineController::SetPipeline( std::weak_ptr< render::pipelineProg_t > pipeline, uint32_t stageBits )
{
	m_pipeline = pipeline;
	m_shaderViews.clear();
	m_gpuVarViews.clear();
	m_isValid = false;

	auto pp = m_pipeline.lock();

	if ( !pp )
	{
		CHECK_PRED( false );
		return;
	}

	m_isValid = pp->GetStatus() == render::pipelineStatus_t::Ok;

	bool shaderNamesValid = m_isValid || pp->GetStatus() == render::pipelineStatus_t::ShaderNotCompiled;

	if ( stageBits & SS_COMPUTE_BIT )
	{
		shaderView_t sv( SS_COMPUTE );
		if ( shaderNamesValid && pp->shaders[ SS_COMPUTE ] )
		{
			sv.path = pp->shaders[ SS_COMPUTE ]->path;
		}
		m_shaderViews.emplace_back( std::move( sv ) );
	}

	if ( stageBits & SS_VERTEX_BIT )
	{
		shaderView_t sv( SS_VERTEX );
		if ( shaderNamesValid && pp->shaders[ SS_VERTEX ] )
		{
			sv.path = pp->shaders[ SS_VERTEX ]->path;
		}
		m_shaderViews.emplace_back( std::move( sv ) );
	}

	if ( stageBits & SS_FRAGMENT_BIT )
	{
		shaderView_t sv( SS_FRAGMENT );
		if ( shaderNamesValid && pp->shaders[ SS_FRAGMENT ] )
		{
			sv.path = pp->shaders[ SS_FRAGMENT ]->path;
		}
		m_shaderViews.emplace_back( std::move( sv ) );
	}

	if ( m_isValid )
	{
		ExtractGPUVarViews( pp );
	}
}

bool PipelineController::SetShader( shaderStage_t stage, const char *path )
{
	for ( shaderView_t &sv : m_shaderViews )
	{
		if ( sv.stage == stage )
		{
			sv.path = path;
			return true;
		}
	}

	return false;
}

void PipelineController::ExtractGPUVarViews( std::shared_ptr< render::pipelineProg_t > pp )
{
	m_gpuVarViews.clear();

	for ( int i = 0; i < pp->interfaceBlocks.size(); ++i )
	{
		const interfaceBlock_t &ib = pp->interfaceBlocks[ i ];

		if ( !ib.HoldsUserVars() )
		{
			continue;
		}

		byte *buffer = render::g_pipelineManager.GetUBOPtr( *pp, i );
		for ( const memberDeclaration_t &uniform : ib.declarations )
		{
			gpuVarView_t gpuVarView;
			gpuVarView.name	 = uniform.name;
			gpuVarView.type	 = uniform.type;
			gpuVarView.value = reinterpret_cast< float * >( buffer );

			m_gpuVarViews.emplace_back( gpuVarView );

			buffer += GetMemberTypeByteSize( uniform.type );
		}
	}
}

VFXController::VFXController( std::weak_ptr< render::VFX > vfx )
	: m_computePipController( !vfx.expired() ? vfx.lock()->m_computePipeline : nullptr, SS_COMPUTE_BIT )
	, m_graphicsPipController( !vfx.expired() ? vfx.lock()->m_graphicsPipeline : nullptr,
							   SS_VERTEX_BIT | SS_FRAGMENT_BIT )
{
	SetVFX( vfx );
}

bool VFXController::Reload()
{
	auto vfxPtr = m_vfx.lock();

	if ( !vfxPtr )
	{
		Error( "Reload failed." );
		return false;
	}

	Log( "Reloading VFX %s", vfxPtr->GetPath().c_str() );

	m_capacity = Align< uint32_t >( m_capacity, render::COMPUTE_GROUP_SIZE_X );

	// #TODO and what about VFX::m_reviveAcc ?? this whole function is garbage

	// #TODO private method to set capacity, that deallocates index buffer (wait
	// no, no need for that)
	vfxPtr->SetCapacity( m_capacity );
	vfxPtr->SetLifeMin( m_lifeMin );
	vfxPtr->SetLifeMax( m_lifeMax );
	vfxPtr->SetRenderPrimitive( m_renderPrimitive );

	vfxPtr->FreeBuffers();
	for ( size_t i = 0; i < m_attributeBufferViews.size(); ++i )
	{
		BufferViewInfoToInternalBufferInfo( m_attributeBufferViews[ i ],
											vfxPtr->m_attributesBuffers[ i ].dataType,
											vfxPtr->m_attributesBuffers[ i ].arity );

		std::strncpy( vfxPtr->m_attributesBuffers[ i ].name,
					  m_attributeBufferViews[ i ].name.c_str(),
					  render::VFX_MAX_BUFFER_NAME_LENGTH );
	}

	// #TODO the following code is messy and quite error prone. This behavior is
	// similar to VFX::loadfromjson, hence it should be factorized.

	vfxPtr->ReloadBuffers();

	// vfxPtr->InitPipelines();// #TODO no need for this call ? Unless pipeline
	// state needs to be updated !!!

	bool ret = true;
	ret &= m_computePipController.Reload();
	ret &= m_graphicsPipController.Reload();

	vfxPtr->BindBuffers();
	vfxPtr->SetupRenderpass();
	// vfxPtr->InitBarriers();
	vfxPtr->m_isValid = ret; // #TODO private method CheckValidity

	if ( !ret )
	{
		Error( "Error during reload." );
	}

	Log( "Reloading done." );
	return ret;
}

bool VFXController::Save()
{
	auto vfxPtr = m_vfx.lock();

	if ( vfxPtr )
	{
		bool success = vfxPtr->Save( vfxPtr->GetPath().c_str() );
		if ( success )
		{
			Log( "VFX saved to %s", vfxPtr->GetPath().c_str() );
		}
		return success;
	}

	return false;
}

bool VFXController::SaveAs( const char *path )
{
	auto vfxPtr = m_vfx.lock();

	if ( vfxPtr )
	{
		vfxPtr->SetPath( path );
		return Save();
	}

	return false;
}

void VFXController::SetVFX( std::weak_ptr< render::VFX > vfx )
{
	m_vfx = vfx;

	auto vfxPtr = m_vfx.lock();

	if ( vfxPtr )
	{
		m_capacity		  = vfxPtr->GetCapacity();
		m_lifeMin		  = vfxPtr->GetLifeMin();
		m_lifeMax		  = vfxPtr->GetLifeMax();
		m_renderPrimitive = vfxPtr->GetRenderPrimitive();

		m_attributeBufferViews.clear();
		m_attributeBufferViews.reserve( render::VFX_MAX_BUFFERS );
		const std::array< render::VFX::VFXBuffer_t, render::VFX_MAX_BUFFERS > &vfxBuffers = vfxPtr->m_attributesBuffers;
		for ( int i = 0; i < vfxPtr->m_userAttributesCount; ++i )
		{
			const render::VFX::VFXBuffer_t &vfxBuffer = vfxBuffers[ i ];

			if ( !vfxBuffer.IsValid() )
			{
				break;
			}

			vfxBufferView_t bv( vfxBuffer.name );
			InternalBufferInfoToBufferViewInfo( vfxBuffer.dataType, vfxBuffer.arity, bv );

			m_attributeBufferViews.emplace_back( std::move( bv ) );
		}
	}
}

bool VFXController::AddBuffer( const vfxBufferView_t &buffer )
{
	if ( m_attributeBufferViews.size() < render::VFX_MAX_BUFFERS )
	{
		m_attributeBufferViews.emplace_back( buffer );
		return true;
	}

	Error( "Could not add buffer to vfx %s. Maximum buffer count is %d.", GetName(), render::VFX_MAX_BUFFERS );

	return false;
}

bool VFXController::RemoveBuffer( size_t i )
{
	if ( i >= 0 && i < m_attributeBufferViews.size() )
	{
		m_attributeBufferViews.erase( m_attributeBufferViews.begin() + i );
		return true;
	}

	return false;
}

const char *VFXController::GetName() const
{
	auto vfxPtr = m_vfx.lock();

	if ( vfxPtr )
	{
		return vfxPtr->GetPath().c_str();
	}

	return "???";
}

double *VFXController::GetSpawnRatePtr()
{
	auto vfxPtr = m_vfx.lock();

	if ( vfxPtr )
	{
		return &vfxPtr->m_spawnRate;
	}

	return nullptr;
}

bool *VFXController::GetInfiniteSpawnRatePtr()
{
	auto vfxPtr = m_vfx.lock();

	if ( vfxPtr )
	{
		return &vfxPtr->m_infiniteSpawnRate;
	}

	return nullptr;
}

void VFXController::BufferViewInfoToInternalBufferInfo( const vfxBufferView_t &bv,
														vfxBufferData_t &	   bufferType,
														int8_t &			   arity )
{
	switch ( bv.dataType )
	{
		case VFXController::Float:
		case VFXController::Vec2:
		case VFXController::Vec3:
		case VFXController::Vec4:
		{
			bufferType = VFX_BD_FLOAT;
			break;
		}
		case VFXController::Int:
		case VFXController::iVec2:
		case VFXController::iVec3:
		case VFXController::iVec4:
		{
			bufferType = VFX_BD_INT;
			break;
		}
		default:
		{
			CHECK_PRED( false );
			break;
		}
	}

	arity = bv.GetArity();
}

void VFXController::InternalBufferInfoToBufferViewInfo( vfxBufferData_t bufferType, int8_t arity, vfxBufferView_t &bv )
{
	switch ( bufferType )
	{
		case vkRuna::VFX_BD_FLOAT:
		{
			bv.dataType = attribute_t( int( attribute_t::Float ) + int( arity ) - 1 );
			break;
		}
		case vkRuna::VFX_BD_INT:

		{
			bv.dataType = attribute_t( int( attribute_t::Int ) + int( arity ) - 1 );
			break;
		}

		default:
		{
			CHECK_PRED( false );
			break;
		}
	}
}

int8_t VFXController::vfxBufferView_t::GetArity() const
{
	static_assert( attribute_t::COUNT == 8, "VFXController::vfxBufferView_t::attribute_t::COUNT changed." );
	return ( int8_t( dataType ) % 4u ) + 1;
}

const char *EnumToString( VFXController::attribute_t attribute )
{
	switch ( attribute )
	{
		case VFXController::Float: return "Float";
		case VFXController::Vec2: return "Vec2";
		case VFXController::Vec3: return "Vec3";
		case VFXController::Vec4: return "Vec4";
		case VFXController::Int: return "Int";
		case VFXController::iVec2: return "iVec2";
		case VFXController::iVec3: return "iVec3";
		case VFXController::iVec4: return "iVec4";
		case VFXController::COUNT: return "COUNT";
		default: return "???";
	}
}

} // namespace vkRuna
