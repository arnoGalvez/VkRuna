// Copyright (c) 2021 Arno Galvez

#pragma once

#include "external/cereal/cereal.hpp"
#include "external/cereal/types/array.hpp"
#include "external/cereal/types/memory.hpp"
#include "external/vulkan/vulkan.hpp"
#include "platform/Serializable.h"
#include "platform/defines.h"
#include "renderer/Buffer.h"
#include "renderer/RenderConfig.h"
#include "renderer/Shader.h"
#include "renderer/VkRenderCommon.h"
#include "renderer/vfxtypes.h"

#include <array>
#include <cstdint>
#include <memory>

namespace cereal
{
class access;
}

namespace vkRuna
{
class VFXTokenizer;
class VFXController;
namespace render
{
struct pipelineProg_t;

class VFX : public ISerializable
{
	NO_COPY_NO_ASSIGN( VFX )

   private:
	friend class VFXManager;
	friend class vkRuna::VFXTokenizer;
	friend class vkRuna::VFXController;
	friend class cereal::access;

   public:
	virtual ~VFX();
	VFX( const char *file );

	void			Load( const char *path ) override;
	NO_DISCARD bool Save( const char *path ) override;
	void			ReloadBuffers();
	bool			IsValid() const { return m_isValid; }

	// #TODO some resources would need to be updated after this call
	// void SetRenderingPrimitive( vfxRenderPrimitive_t primitive ) { m_renderPrimitive = primitive; }

	void Update( double deltaFrame );

	bool GetComputeCmd( gpuCmd_t &computeCmd );
	bool InsertRenderCmds( std::vector< gpuCmd_t > &renderCmds );

	int BarriersUpdateToRender( VkBufferMemoryBarrier **barriers );
	int BarrierRenderToUpdate( VkBufferMemoryBarrier **barriers );

	const auto &		 GetPath() const { return m_path; }
	uint32_t			 GetCapacity() const { return m_capacity; }
	float				 GetLifeMin() const { return m_lifeMin; }
	float				 GetLifeMax() const { return m_lifeMax; }
	vfxRenderPrimitive_t GetRenderPrimitive() const { return m_renderPrimitive; }
	const auto &		 GetComputePipeline() const { return m_computePipeline; }
	const auto &		 GetGraphicsPipeline() const { return m_graphicsPipeline; }

   private:
	static const char *	  TypeIndexToStr( int vfxBufferTypeIndex );
	static NO_DISCARD int GetRevivalCounterName( char *buffer, int bufferSize );

	template< class Archive >
	void serialize( Archive &ar );

	bool			LoadFromJSON( const char *path );
	NO_DISCARD bool SaveToJson( const char *path );

	uint32_t GetIndicesCount();

	void AllocBuffers();
	void BindBuffers();

	NO_DISCARD bool CheckPipelines();

	void InitBarriers();
	void InitPipelines();
	void SetupRenderpass();

	void SetPath( const char *path ) { m_path = path; }
	void SetCapacity( uint32_t capacity ) { m_capacity = capacity; }
	void SetLifeMin( float lifeMin ) { m_lifeMin = lifeMin; }
	void SetLifeMax( float lifeMax ) { m_lifeMax = lifeMax; }
	void SetRenderPrimitive( vfxRenderPrimitive_t renderPrimitive );

	NO_DISCARD bool ParseCustomVars( std::string *shaderCode, shaderStage_t shaderStage );
	void			AddShaderCodeHeaderAndFooter( std::string &shaderCode, shaderStage_t stage );
	int				GetUBOMembers( char *buffer, size_t bufferSize ) const;

	/*void			AddShaderMain( const char *mainFilePath, std::string &out );
	void			AddVertexShaderMain( std::string &out );
	void			AddFragmentShaderMain( std::string &out );
	void			AddComputeShaderMain( std::string &out );*/

	void Clear();
	void FreeBuffers();

   private:
	static const char *SHADER_PARTICLE_TO_REVIVE;
	static const char *SHADER_PARTICLE_CAPACITY;
	static const char *SHADER_PARTICLES_LIFE_MIN;
	static const char *SHADER_PARTICLES_LIFE_MAX;
	static const char *SHADER_PAD_0;

	static const char *VERTEX_HEADER_PATH;
	static const char *VERTEX_FOOTER_PATH;
	static const char *FRAGMENT_HEADER_PATH;
	static const char *FRAGMENT_FOOTER_PATH;
	static const char *COMPUTE_HEADER_PATH;
	static const char *COMPUTE_FOOTER_PATH;

	struct VFXBuffer_t
	{
		vfxBufferData_t dataType = VFX_BD_FLOAT;
		int8_t			arity	 = -1;
		char			name[ VFX_MAX_BUFFER_NAME_LENGTH + 1 ] {};
		Buffer			buffer;

		template< class Archive >
		void serialize( Archive &ar )
		{
			ar( dataType, arity, name );
		}

		bool IsValid() const { return arity >= 0; }
		void GetGLSLType( std::string &out ) const;
		void Free();
		void Fill( uint32_t data );
	};

	struct depthPrepassDeleter_t
	{
		void operator()( pipelineProg_t *pp );
	};

	bool		m_isValid = false;
	std::string m_path {};

	std::shared_ptr< pipelineProg_t >						 m_computePipeline {};
	std::shared_ptr< pipelineProg_t >						 m_graphicsPipeline {};
	std::unique_ptr< pipelineProg_t, depthPrepassDeleter_t > m_depthPrepassPipeline {};

	uint32_t m_capacity	 = 1;
	double	 m_spawnRate = 0.0;
	float	 m_lifeMin	 = 0.0f;
	float	 m_lifeMax	 = 1.0f;

	vfxRenderPrimitive_t m_renderPrimitive = VFX_RP_CUBE;
	Buffer				 m_indexBuffer {}; // Could be shared by all VFX ?

	Buffer m_revivalCounter {};
	double m_reviveAcc = 0.0;

	int										   m_userAttributesCount = 0;
	int										   m_attributesCount	 = 0;
	std::array< VFXBuffer_t, VFX_MAX_BUFFERS > m_attributesBuffers {};
	// std::array< VFXBuffer_t, 1 >						 m_hiddenAttributesBuffers {};
	std::array< VkBufferMemoryBarrier, VFX_MAX_BUFFERS > m_barriersUpdateToRender {};
	std::array< VkBufferMemoryBarrier, VFX_MAX_BUFFERS > m_barriersRenderToUpdate {};
};

class VFXManager
{
	NO_COPY_NO_ASSIGN( VFXManager )
   private:
	friend class VFX;

   public:
	using VFXContent_t	 = std::shared_ptr< VFX >;
	using VFXContainer_t = std::vector< VFXContent_t >;

   public:
	static const char *GetPreferredDir() { return "."; }

	~VFXManager();
	VFXManager();

	void Init();
	void Shutdown();

	// VFXContent_t          AddVFX( uint32_t capacity, float spawnRate );
	VFXContent_t	AddVFXFromFile( const char *file );
	void			RemoveVFX( VFXContent_t vfx );
	VFXContainer_t &GetContainer() { return m_vfxContainer; }

	int GetPreRenderCmds( gpuCmd_t **cmds );
	int GetRenderCmds( gpuCmd_t **cmds );

   private:
	// VFXContent_t MakeVFX( uint32_t capacity, float spawnRate );
	VFXContent_t MakeVFX( const char *file );
	void		 MemsetZeroVFX( VFX &vfx );

   private:
	pipelineProg_t *m_initPipeline = nullptr;

	VFXContainer_t m_vfxContainer;

	std::vector< gpuCmd_t >		m_preRenderCmds;
	std::vector< gpuCmd_t >		m_renderCmds;
	std::vector< gpuBarrier_t > m_barriers;
};

extern VFXManager g_vfxManager;

// template< class Archive >
// void VFX::serialize( Archive &ar )
//{
//	REDO FUNC;
//
//	ar( CEREAL_NVP( m_capacity ),
//		CEREAL_NVP( m_spawnRate ),
//		CEREAL_NVP( m_attributesBuffers ),
//		CEREAL_NVP( m_attributesCount ) );
//
//	ar( CEREAL_NVP( *m_computePipeline ) );
//
//	ar( CEREAL_NVP( *m_graphicsPipeline ) );
//}

template< class Archive >
void VFX::serialize( Archive &ar )
{
	ar( CEREAL_NVP( m_capacity ),
		CEREAL_NVP( m_spawnRate ),
		CEREAL_NVP( m_lifeMin ),
		CEREAL_NVP( m_lifeMax ),
		CEREAL_NVP( m_renderPrimitive ) );
	ar( CEREAL_NVP( m_userAttributesCount ) );
	for ( int i = 0; i < m_userAttributesCount; ++i )
	{
		ar( m_attributesBuffers[ i ] );
	}

	ar( CEREAL_NVP( *m_computePipeline ) );
	ar( CEREAL_NVP( *m_graphicsPipeline ) );
}

} // namespace render
} // namespace vkRuna
