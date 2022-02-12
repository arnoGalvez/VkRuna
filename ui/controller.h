// Copyright (c) 2021 Arno Galvez

#pragma once

#include "platform/defines.h"
#include "renderer/RenderConfig.h"
#include "renderer/Shader.h"
#include "renderer/vfxtypes.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace vkRuna
{
struct SerializableData;
namespace render
{
class VFX;
struct pipelineProg_t;

} // namespace render

class PipelineController
{
   public:
	struct shaderView_t
	{
		shaderView_t()
			: stage( SS_UNKNOWN )
		{
		}
		shaderView_t( shaderStage_t stage )
			: stage( stage )
		{
		}
		shaderView_t( shaderStage_t stage, const std::string &path )
			: stage( stage )
			, path( path )
		{
		}

		std::string	  path {};
		shaderStage_t stage = SS_UNKNOWN;
	};

	struct gpuVarView_t
	{
		std::string	 name {};
		memberType_t type  = MT_UNKNOWN;
		float *		 value = nullptr;

		float *GetPtr() const { return value; }
	};

   public:
	PipelineController( std::weak_ptr< render::pipelineProg_t > pipeline, uint32_t stageBits );
	bool IsValid() { return m_isValid; }

	NO_DISCARD bool Reload();

	void SetPipeline( std::weak_ptr< render::pipelineProg_t > pipeline, uint32_t stageBits );

	NO_DISCARD bool SetShader( shaderStage_t stage, const char *path );

	const std::vector< shaderView_t > &GetShaderViews() const { return m_shaderViews; }
	const std::vector< gpuVarView_t > &GetGpuVarViews() const { return m_gpuVarViews; }

   private:
	void ExtractGPUVarViews( std::shared_ptr< render::pipelineProg_t > pp );

   private:
	std::weak_ptr< render::pipelineProg_t >			   m_pipeline {};
	std::vector< shaderView_t >						   m_shaderViews {};
	std::vector< gpuVarView_t >						   m_gpuVarViews {};
	std::unique_ptr< std::vector< SerializableData > > m_userValues {};

	bool m_isValid = false;
};

class VFXController
{
   public:
	enum attribute_t
	{
		Float,
		Vec2,
		Vec3,
		Vec4,

		Int,
		iVec2,
		iVec3,
		iVec4,

		COUNT
	};

	struct vfxBufferView_t
	{
		vfxBufferView_t()
			: dataType( Float )
		{
		}

		vfxBufferView_t( const std::string &name )
			: name( name )
		{
		}

		vfxBufferView_t( const std::string &name, attribute_t dataType )
			: name( name )
			, dataType( dataType )
		{
		}
		static int MaxNameSize() { return render::VFX_MAX_BUFFER_NAME_LENGTH; }
		int8_t	   GetArity() const;

		std::string name {};
		attribute_t dataType = Float;
	};

   public:
	VFXController( std::weak_ptr< render::VFX > vfx );

	bool Reload();
	bool Save();
	bool SaveAs( const char *path );

	void						 SetVFX( std::weak_ptr< render::VFX > vfx );
	std::weak_ptr< render::VFX > GetVFX() { return m_vfx; }

	PipelineController &GetComputeController() { return m_computePipController; }
	PipelineController &GetGraphicsController() { return m_graphicsPipController; }

	bool							AddBuffer( const vfxBufferView_t &buffer );
	bool							RemoveBuffer( size_t i );
	std::vector< vfxBufferView_t > &GetBuffers() { return m_attributeBufferViews; }
	const char *					GetName() const;
	double *						GetSpawnRatePtr();
	bool *							GetInfiniteSpawnRatePtr();
	uint32_t *						GetCapacityPtr() { return &m_capacity; }
	float *							GetLifeMinPtr() { return &m_lifeMin; }
	float *							GetLifeMaxPtr() { return &m_lifeMax; }
	vfxRenderPrimitive_t &			GetRenderPrimitiveRef() { return m_renderPrimitive; }

   private:
	static void BufferViewInfoToInternalBufferInfo( const vfxBufferView_t &bufferView,
													vfxBufferData_t &	   bufferType,
													int8_t &			   arity );
	static void InternalBufferInfoToBufferViewInfo( vfxBufferData_t	 bufferType,
													int8_t			 arity,
													vfxBufferView_t &bufferView );

	PipelineController m_computePipController;
	PipelineController m_graphicsPipController;

	std::weak_ptr< render::VFX >   m_vfx {};
	uint32_t					   m_capacity		 = 0;
	float						   m_lifeMin		 = 0.0f;
	float						   m_lifeMax		 = 1.0f;
	vfxRenderPrimitive_t		   m_renderPrimitive = VFX_RP_QUAD;
	std::vector< vfxBufferView_t > m_attributeBufferViews {};
};

const char *EnumToString( VFXController::attribute_t attribute );

} // namespace vkRuna
