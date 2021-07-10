// Copyright (c) 2021 Arno Galvez

#pragma once

#include "external/cereal/types/array.hpp"
#include "external/cereal/types/memory.hpp"
#include "external/cereal/types/vector.hpp"
#include "external/vulkan/vulkan.hpp"
#include "platform/Serializable.h"
#include "platform/Sys.h"
#include "platform/defines.h"
#include "renderer/Buffer.h"
#include "renderer/RenderConfig.h"
#include "renderer/Shader.h"
#include "renderer/ShaderLexer.h"
#include "renderer/State.h"
#include "rnLib/Event.h"
#include "rnLib/Math.h"

#include <array>
#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <vector>

namespace vkRuna
{
namespace render
{
class Buffer;
class Image;
class VulkanBackend;

struct pipelineProg_t;
struct vkGraphicsPipeline_t;

enum descriptorSet_t
{
	DS_UBO,
	DS_BUFFER,
	DS_SAMPLER,
	DS_SHARED_BUFFER,
	DS_COUNT
};

std::string GetGLSLPath( const char *shader );

descriptorSet_t BindingTypeToDescSet( bindingType_t type );

class PipelineManager
{
	NO_COPY_NO_ASSIGN( PipelineManager )
   public:
	PipelineManager();
	~PipelineManager();

	void Init();
	void Shutdown();

   public:
	void AddSharedInterfaceBlock( interfaceBlock_t ib );
	void SetSharedVar( size_t count, const char *const *varNames, const float *values );

   public:
	NO_DISCARD bool CreateEmptyPipelineProg( pipelineProg_t &out );
	// TODO: manage null pointers
	NO_DISCARD bool CreateGraphicsPipeline( const char *vertexShader, const char *fragmentShader, pipelineProg_t &out );
	NO_DISCARD bool CreateGraphicsPipeline( const char *	vertexShader,
											const char *	fragmentShader,
											const uint64_t	state,
											pipelineProg_t &out );
	NO_DISCARD bool CreateComputePipeline( const char *computeShader, pipelineProg_t &out );
	NO_DISCARD bool CreateComputePipeline( const char *computeShader, std::string &shaderCode, pipelineProg_t &out );

	void CreateDepthPrepassPipeline( pipelineProg_t &dpp, const pipelineProg_t &srcpp );

	// void SerializePipeline( const pipelineProg_t &pp, SerializableData &out );
	// void DeserializePipeline( const SerializableData &in, pipelineProg_t &pp );
	// // also creates it

	void RegisterEvent( pipelineProg_t &pp, std::unique_ptr< Event > ev );

	void BindGraphicsPipeline( VkCommandBuffer cmdBuffer, pipelineProg_t &graphicsPipeline );
	// void BindGraphicsPipeline( VkCommandBuffer cmdBuffer, int cacheIndex );
	void BindComputePipeline( VkCommandBuffer cmdBuffer, pipelineProg_t &computePipeline );
	// void BindComputePipeline( VkCommandBuffer cmdBuffer, int cacheIndex );

	void UpdateUBOs( pipelineProg_t &	pp,
					 size_t				count,
					 const char *const *varNames,
					 const size_t *		byteSizes,
					 const float *		values );
	void UpdateImages( pipelineProg_t &			 pp,
					   size_t					 count,
					   const std::string *const *varNames,
					   const Image *const *		 images );
	void UpdateBuffers( pipelineProg_t &pp, size_t count, const char *const *varNames, const Buffer *const *buffers );

	NO_DISCARD bool LoadShaders( pipelineProg_t &	  pp,
								 size_t				  count,
								 const shaderStage_t *shaderStages,
								 const char *const *  paths );
	NO_DISCARD bool LoadShaders( pipelineProg_t &	  pp,
								 size_t				  count,
								 const shaderStage_t *shaderStages,
								 const char *const *  names,
								 std::string *		  shaderCodes );
	// void UpdateShader( pipelineProg_t &pp, size_t count, int
	// *shaderCacheIndices );
	void UpdateVertexDesc( pipelineProg_t &							pp,
						   uint32_t									stride,
						   VkVertexInputRate						inputRate,
						   uint32_t									attriutebDescCount,
						   const VkVertexInputAttributeDescription *attributeDescs );
	void UpdateState( pipelineProg_t &pp, uint64_t state );

	byte *GetUBOPtr( const pipelineProg_t &pp, int interfaceBlockIndex );

	NO_DISCARD bool Reload( pipelineProg_t &pp );
	NO_DISCARD bool ReloadShaders( pipelineProg_t &pp );

	// Cached objects should be marked somehow, at least because there is no way
	// to copy a VkShaderModule. Consider cached objects immutable ?
	// => or move the pp from its origin into the cache.
	// int CachePipelineProg( pipelineProg_t &&pp );
	// int CacheShader( const pipelineProg_t &pp, shaderStage_t shaderStage );
	// bool RetrievePipelineProg( int index, pipelineProg_t &pp );
	// bool RetrieveShader( int index, pipelineProg_t &pp );

	// const pipelineProg_t &GetCachedPipeline( int pipelineCacheIndex );
	void ClearSerializedValues( pipelineProg_t &pp );
	void DestroyPipelineProg( pipelineProg_t &pp );
	void DestroyPipelineProgKeepResources( pipelineProg_t &pp );
	void DestroyShaders( pipelineProg_t &pp, size_t count, const shaderStage_t *shaderStages );

   private:
	friend class ShaderLexer;

	void GetVulkanGraphicsPipelineInfo( const pipelineProg_t &pp, vkGraphicsPipeline_t &vkgp );
	void CreateGraphicsPipeline( pipelineProg_t &pp );
	void CreateComputePipeline( pipelineProg_t &pp );

	void BindInterfaceBlock( pipelineProg_t &pp, interfaceBlock_t &interfaceBlock );
	void BindSharedInterfaceBlock( pipelineProg_t &pp, interfaceBlock_t &sharedInterfaceBlock );
	void BindUBO( pipelineProg_t &pp, interfaceBlock_t &interfaceBlock );
	void BindSampler( pipelineProg_t &pp, interfaceBlock_t &interfaceBlock );
	void BindBuffer( pipelineProg_t &pp, interfaceBlock_t &interfaceBlock );

	int										GetSharedBlockBinding( const interfaceBlock_t &sharedBlock );
	std::vector< const interfaceBlock_t * > GetUniqueSharedBlocks( const pipelineProg_t &pp );

	void UpdateDescriptorSetUBO( pipelineProg_t &pp );

	void AllocUBOs( pipelineProg_t &pp );
	void FreeUBOs( pipelineProg_t &pp );

	void UpdateResourceBindings( pipelineProg_t &pp );
	void DestroyResourceBindings( pipelineProg_t &pp );
	void ResetCounters( pipelineProg_t &pp );
	void AllocDescriptorSets( pipelineProg_t &pp );
	void FreeDescriptorSets( pipelineProg_t &pp );
	void DestroyDescriptorSetLayouts( pipelineProg_t &pp );
	void DestroyPipelineLayout( pipelineProg_t &pp );

	void FinalizeShadersUpdate( pipelineProg_t &pp );

	void DestroyPipelineHandle( pipelineProg_t &pp );

	void CreateDescriptorPool();
	void DestroyDescriptorPool();

	void CreatePipelineCache();
	void DestroyPipelineCache();

   private:
	friend class VulkanBackend;

   private:
	std::vector< pipelineProg_t > m_pipelineProgs; // holds cached pipelines
	std::vector< shader_t >		  m_shaders;	   // holds cached shaders

	std::vector< interfaceBlock_t > m_sharedBlocks;
	Buffer *						m_sharedBlocksPool			 = nullptr;
	uint32_t						m_sharedBlocksBindingCounter = 0;

	VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
	VkPipelineCache	 m_pipelineCache  = VK_NULL_HANDLE;

   private:
	VkDescriptorPool GetDescriptorPool() { return m_descriptorPool; }
	VkPipelineCache	 GetPipelineCache() { return m_pipelineCache; }
};

extern PipelineManager g_pipelineManager;

enum class pipelineStatus_t : uint8_t
{
	Unknown,
	Ok,
	ShaderNotCompiled,
	NoShader,
	Doomed
};

void ValidatePipeline( pipelineProg_t &pp );
void SetPipelineStatus( pipelineProg_t &pp, pipelineStatus_t status );

//#pragma warning( error : 4820 )
struct pipelineProg_t
{
	DEFAULT_RVAL_CONSTRUCT_ASSIGN( pipelineProg_t )

	~pipelineProg_t() { g_pipelineManager.DestroyPipelineProg( *this ); }
	pipelineProg_t() { SetPipelineStatus( *this, pipelineStatus_t::Unknown ); }

	pipelineStatus_t GetStatus() const;

	pipelineStatus_t						status = pipelineStatus_t::Unknown;
	std::vector< interfaceBlock_t >			interfaceBlocks {};
	Buffer *								uboPool = nullptr;
	std::array< VkDescriptorSet, DS_COUNT > descriptorSets {};
	VkPipelineLayout						pipelineLayout = VK_NULL_HANDLE;
	VkPipeline								pipeline	   = VK_NULL_HANDLE;

	std::array< std::unique_ptr< shader_t >, SS_COUNT > shaders {};
	std::vector< int >									sharedInterfaceBlockBindings {};

	std::array< VkDescriptorSetLayout, DS_COUNT > descriptorSetLayouts {};

	std::array< uint32_t, DS_COUNT > resourceCounters {}; // #TODO one counter could be enough ?

	VkVertexInputBindingDescription					 vertexBindingDesc {};
	std::vector< VkVertexInputAttributeDescription > vertexAttributeDescs {};

	uint64_t stateBits = 0;

	std::vector< std::unique_ptr< Event > > *		   events = nullptr;
	std::unique_ptr< std::vector< SerializableData > > serializedValues;
};
//#pragma warning( disable : 4820 )

std::vector< SerializableData > SerializeInterfaceBlocks( const render::pipelineProg_t &pp );
void DeserializeInterfaceBlocks( pipelineProg_t &pp, const std::vector< SerializableData > &suvVec );

template< class Archive >
void save( Archive &ar, const render::pipelineProg_t &pp )
{
	ar( ::cereal::make_nvp( "shaders", pp.shaders ), ::cereal::make_nvp( "state", pp.stateBits ) );

	// User resources values (#TODO only float values are handled at the moment)
	{
		std::vector< SerializableData > suvVec;
		if ( pp.GetStatus() == pipelineStatus_t::Ok )
		{
			suvVec = SerializeInterfaceBlocks( pp );
		}
		ar( ::cereal::make_nvp( "userVars", suvVec ) );

		for ( SerializableData &suv : suvVec )
		{
			suv.Clear();
		}
	}
}

template< class Archive >
void load( Archive &ar, render::pipelineProg_t &pp )
{
	ar( ::cereal::make_nvp( "shaders", pp.shaders ) );

	uint64_t state = 0;
	ar( ::cereal::make_nvp( "state", state ) );
	g_pipelineManager.UpdateState( pp, state );

	pp.serializedValues = std::make_unique< std::vector< SerializableData > >();
	ar( ::cereal::make_nvp( "userVars", *pp.serializedValues ) );
}

} //  namespace render

} // namespace vkRuna
