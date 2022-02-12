// Copyright (c) 2021 Arno Galvez

#include "renderer/RenderProgs.h"

#include "platform/Heap.h"
#include "platform/Sys.h"
#include "renderer/Buffer.h"
#include "renderer/Check.h"
#include "renderer/Image.h"
#include "renderer/VkAllocator.h"
#include "renderer/VkBackend.h"

#include <cstring>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <utility>

namespace vkRuna
{
using namespace sys;

shader_t::~shader_t()
{
	DestroyModule();
}

void shader_t::UpdateModule( const char *spvFile )
{
	std::vector< char > binary = ReadBinary< char >( spvFile );

	CHECK_PRED( !binary.empty() && ( ( binary.size() % 4 ) == 0 ) )

	DestroyModule();

	VkShaderModuleCreateInfo ci {};
	ci.sType	= VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ci.pNext	= nullptr;
	ci.flags	= 0;
	ci.codeSize = binary.size();
	ci.pCode	= reinterpret_cast< uint32_t * >( binary.data() );

	VkShaderModule *pModule = reinterpret_cast< VkShaderModule * >( &module );
	VK_CHECK( vkCreateShaderModule( render::GetVulkanContext().device, &ci, nullptr, pModule ) )
}

void shader_t::DestroyModule()
{
	VkShaderModule vkModule = static_cast< VkShaderModule >( module );

	if ( vkModule != VK_NULL_HANDLE )
	{
		vkDestroyShaderModule( render::GetVulkanContext().device, vkModule, nullptr );
		module = nullptr;
	}
}

namespace render
{
PipelineManager g_pipelineManager;

static const char *CACHE_DIR		= "renderCache";
static const char *GLSL_INCLUDE_DIR = "glsl/lib";

static const std::array< std::string, 3 > VALID_EXT = { "vert", "frag", "comp" };

static const std::array< VkShaderStageFlagBits, SS_COUNT > SS_VK_TYPES = { VK_SHADER_STAGE_VERTEX_BIT,
																		   VK_SHADER_STAGE_FRAGMENT_BIT,
																		   VK_SHADER_STAGE_COMPUTE_BIT };

static const std::array< VkDescriptorType, DS_COUNT > DS_VK_TYPES = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
																	  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
																	  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
																	  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER };

static const std::array< uint32_t, DS_COUNT > DS_POOL_SIZES = { 1 << 13, 1 << 13, 1 << 13, 256 };

struct vkGraphicsPipeline_t
{
	uint32_t												shaderCount { 0 };
	std::array< VkPipelineShaderStageCreateInfo, SS_COUNT > shaders {};
	VkPipelineVertexInputStateCreateInfo					vertexInput {};
	VkPipelineInputAssemblyStateCreateInfo					inputAssembly {};
	VkPipelineViewportStateCreateInfo						viewport {};
	VkPipelineRasterizationStateCreateInfo					rasterization {};
	VkPipelineMultisampleStateCreateInfo					multisample {};
	VkPipelineDepthStencilStateCreateInfo					depthStencil {};
	VkPipelineColorBlendAttachmentState						blendAttachment {};
	VkPipelineColorBlendStateCreateInfo						colorBlend {};
	uint32_t												dynamicStateCount { 0 };
	std::array< VkDynamicState, 3 >							dynamicStates {};
	VkPipelineDynamicStateCreateInfo						dynamic {};
};

void ValidatePipeline( pipelineProg_t &pp )
{
	if ( !pp.shaders[ SS_VERTEX ] && !pp.shaders[ SS_FRAGMENT ] && !pp.shaders[ SS_COMPUTE ] )
	{
		Error( "Pipeline error: no shader." );
		SetPipelineStatus( pp, pipelineStatus_t::NoShader );
		return;
	}

	if ( pp.shaders[ SS_VERTEX ] && pp.shaders[ SS_FRAGMENT ] && pp.shaders[ SS_COMPUTE ] )
	{
		Error( "Pipeline error: too many shaders !" );
		return;
	}

	if ( pp.shaders[ SS_VERTEX ] && pp.shaders[ SS_FRAGMENT ] )
	{
		if ( pp.shaders[ SS_VERTEX ]->IsValid() && pp.shaders[ SS_FRAGMENT ]->IsValid() )
		{
			SetPipelineStatus( pp, pipelineStatus_t::Ok );
			return;
		}
		else
		{
			Error( "Graphics pipeline error: one or more shader are invalid." );
			SetPipelineStatus( pp, pipelineStatus_t::Doomed );
			return;
		}
	}

	if ( pp.shaders[ SS_COMPUTE ] )
	{
		if ( pp.shaders[ SS_COMPUTE ]->IsValid() )
		{
			SetPipelineStatus( pp, pipelineStatus_t::Ok );
			return;
		}
		else
		{
			Error( "Compute pipeline error: invalid compute shader." );
			SetPipelineStatus( pp, pipelineStatus_t::Doomed );
			return;
		}
	}

	Error( "Pipeline error: not enough shaders allocated." );
	SetPipelineStatus( pp, pipelineStatus_t::Doomed );
}

void SetPipelineStatus( pipelineProg_t &pp, pipelineStatus_t status )
{
	pp.status = status;
}

descriptorSet_t BindingTypeToDescSet( bindingType_t type )
{
	CHECK_PRED( BT_COUNT == DS_COUNT );
	return static_cast< descriptorSet_t >( type );
}

bool FindInterfaceBlock( std::vector< interfaceBlock_t > &ibVec,
						 const std::string &			  name,
						 bindingType_t					  type,
						 ibFlags_t						  flags,
						 interfaceBlock_t **			  out )
{
	const auto predSame = [ & ]( const interfaceBlock_t &ib ) { return ib.name == name && ib.type == type; };

	auto it = std::find_if( ibVec.cbegin(), ibVec.cend(), predSame );
	if ( it != ibVec.cend() )
	{
		if ( out )
		{
			*out = &ibVec[ it - ibVec.cbegin() ];
		}
		return true;
	}

	return false;
}

// #TODO by construction, pp.interfaceBlocks is a set, so this function is now only useful to get ib of a certain type
std::vector< const interfaceBlock_t * > GetUniquePrivateUBOs( pipelineProg_t &pp )
{
	std::vector< const interfaceBlock_t * > uniqueBlocks;

	for ( interfaceBlock_t &ib : pp.interfaceBlocks )
	{
		if ( ib.type != BT_UBO )
		{
			continue;
		}

		uniqueBlocks.emplace_back( &ib );
	}

	return uniqueBlocks;
}

std::vector< const interfaceBlock_t * > PipelineManager::GetUniqueSharedBlocks( const pipelineProg_t &pp )
{
	std::vector< const interfaceBlock_t * > uniqueBlocks;

	for ( int ibb : pp.sharedInterfaceBlockBindings )
	{
		int sharedInterfaceBlockIndex = -1;

		{
			const auto predSameBinding = [ & ]( const interfaceBlock_t &interfaceBlock ) {
				return interfaceBlock.binding == ibb;
			};

			auto it = std::find_if( m_sharedBlocks.cbegin(), m_sharedBlocks.cend(), predSameBinding );
			if ( it == m_sharedBlocks.cend() )
			{
				FatalError( "GetUniqueSharedBlocks: binding %d not found.", ibb );
			}

			sharedInterfaceBlockIndex = static_cast< int >( it - m_sharedBlocks.cbegin() );
		}

		{
			const auto predSameBinding = [ & ]( const interfaceBlock_t *interfaceBlock ) {
				return interfaceBlock->binding == ibb;
			};

			if ( std::find_if( uniqueBlocks.cbegin(), uniqueBlocks.cend(), predSameBinding ) == uniqueBlocks.cend() )
			{
				uniqueBlocks.emplace_back( &m_sharedBlocks[ sharedInterfaceBlockIndex ] );
			}
		}
	}

	return uniqueBlocks;
}

inline std::string GetGLSLPath( const char *path )
{
	const char *fileName = std::strrchr( path, '\\' );
	if ( !fileName )
	{
		return std::string();
	}

	std::string out( CACHE_DIR );
	out += '\\';
	out += ( fileName + 1 );
	return out;
}

void SetDefaultState( uint64_t &stateBits )
{
	stateBits = 0;
	stateBits |= SRCBLEND_FACTOR_ONE;
	stateBits |= DSTBLEND_FACTOR_ZERO;
	stateBits |= STENCIL_OP_PASS_KEEP;
	stateBits |= STENCIL_OP_FAIL_KEEP;
	stateBits |= STENCIL_OP_ZFAIL_KEEP;
	stateBits |= STENCIL_COMPARE_OP_ALWAYS;
	stateBits |= DEPTH_COMPARE_OP_LESS_OR_EQUAL;
	stateBits |= CULL_MODE_NONE;
	stateBits |= POLYGON_MODE_FILL;
	stateBits |= COLOR_MASK_ALL_BITS | COLOR_MASK_A_BIT;
	stateBits |= PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	stateBits |= DEPTH_TEST_ENABLE;
	stateBits |= DEPTH_WRITE_ENABLE;
	// stateBits |= STENCIL_TEST_ENABLE;
}

inline uint64_t StateToVkHelper( uint64_t state, uint64_t enumSecond, uint64_t enumLast )
{
	uint64_t bsrEnumSecond	  = bsr( enumSecond );
	uint64_t flsShiftEnumLast = fls( enumLast ) >> bsrEnumSecond;
	uint64_t mask			  = ( flsShiftEnumLast - 1 ) | flsShiftEnumLast;

	return ( mask & ( state >> bsrEnumSecond ) );
}

VkBlendFactor StateToVkSrcBlend( uint64_t state )
{
	constexpr uint64_t mask = ( ffs( DSTBLEND_FACTOR_ONE ) - 1 );

	return static_cast< VkBlendFactor >( mask & state );
}

VkBlendFactor StateToVkDstBlend( uint64_t state )
{
	uint64_t dstBlend = StateToVkHelper( state, DSTBLEND_FACTOR_ONE, DSTBLEND_FACTOR_ONE_MINUS_SRC1_ALPHA );

	return static_cast< VkBlendFactor >( dstBlend );
}

VkBlendOp StateToVkBlendOp( uint64_t state )
{
	uint64_t blendOp = StateToVkHelper( state, BLEND_OP_SUBTRACT, BLEND_OP_MAX );

	return static_cast< VkBlendOp >( blendOp );
}

VkStencilOp StateToVkStencilOpFail( uint64_t state )
{
	uint64_t stencilOp = StateToVkHelper( state, STENCIL_OP_FAIL_ZERO, STENCIL_OP_FAIL_DECREMENT_AND_WRAP );

	return static_cast< VkStencilOp >( stencilOp );
}

VkStencilOp StateToVkStencilOpPass( uint64_t state )
{
	uint64_t stencilOp = StateToVkHelper( state, STENCIL_OP_PASS_ZERO, STENCIL_OP_PASS_DECREMENT_AND_WRAP );

	return static_cast< VkStencilOp >( stencilOp );
}

VkStencilOp StateToVkStencilOpDepthFail( uint64_t state )
{
	uint64_t stencilOp = StateToVkHelper( state, STENCIL_OP_ZFAIL_ZERO, STENCIL_OP_ZFAIL_DECREMENT_AND_WRAP );

	return static_cast< VkStencilOp >( stencilOp );
}

VkCompareOp StateToVkStencilCompare( uint64_t state )
{
	uint64_t stencilCompareOp = StateToVkHelper( state, STENCIL_COMPARE_OP_LESS, STENCIL_COMPARE_OP_ALWAYS );

	return static_cast< VkCompareOp >( stencilCompareOp );
}

VkCompareOp StateToVkDepthCompare( uint64_t state )
{
	uint64_t depthCompareOp = StateToVkHelper( state, DEPTH_COMPARE_OP_LESS, DEPTH_COMPARE_OP_ALWAYS );

	return static_cast< VkCompareOp >( depthCompareOp );
}

VkCullModeFlagBits StateToVkCullMode( uint64_t state )
{
	uint64_t cullMode = StateToVkHelper( state, CULL_MODE_FRONT_BIT, CULL_MODE_FRONT_AND_BACK );

	return static_cast< VkCullModeFlagBits >( cullMode );
}

VkPolygonMode StateToVkPolygonMode( uint64_t state )
{
	uint64_t polygonMode = StateToVkHelper( state, POLYGON_MODE_LINE, POLYGON_MODE_POINT );

	return static_cast< VkPolygonMode >( polygonMode );
}

// TODO: check StateToVkColorMask behavior
VkColorComponentFlagBits StateToVkColorMask( uint64_t state )
{
	uint64_t colorMask = StateToVkHelper( state, COLOR_MASK_R_BIT, COLOR_MASK_A_BIT );

	return static_cast< VkColorComponentFlagBits >( colorMask );
}

VkPrimitiveTopology StateToVkPrimitiveTopology( uint64_t state )
{
	uint64_t topology = StateToVkHelper( state, PRIMITIVE_TOPOLOGY_LINE_LIST, PRIMITIVE_TOPOLOGY_PATCH_LIST );

	return static_cast< VkPrimitiveTopology >( topology );
}

uint8_t StateToStencilRef( uint64_t state )
{
	constexpr uint64_t ffsRef = ffs( STENCIL_REF_BITS );
	constexpr uint64_t bsrRef = bsr( ffsRef );

	uint8_t ref = static_cast< uint8_t >( ( STENCIL_REF_BITS & state ) >> bsrRef );

	return ref;
}

uint8_t StateToStencilMask( uint64_t state )
{
	constexpr uint64_t ffsRef = ffs( STENCIL_MASK_BITS );
	constexpr uint64_t bsrRef = bsr( ffsRef );

	uint8_t ref = static_cast< uint8_t >( ( STENCIL_MASK_BITS & state ) >> bsrRef );

	return ref;
}

VkDeviceSize UpdateUBO( const std::vector< interfaceBlock_t > &ibVec,
						const char *						   varName,
						const float *						   values,
						Buffer *							   ubo )
{
	const VkDeviceSize minUniformBufferOffsetAlignment =
		GetVulkanContext().gpu.properties.limits.minUniformBufferOffsetAlignment;

	VkDeviceSize offset = 0;

	for ( const interfaceBlock_t &ib : ibVec )
	{
		for ( const memberDeclaration_t &uniform : ib.declarations )
		{
			VkDeviceSize byteSize = GetMemberTypeByteSize( uniform.type );
			if ( uniform.name == varName )
			{
				ubo->Update( byteSize, static_cast< const void * >( values ), offset );
				return byteSize;
			}

			offset += byteSize;
		}

		offset = Align( offset, minUniformBufferOffsetAlignment );
	}

	return 0;
}

static bool CompileShader( const std::string &shaderFile, const std::string &stage, const std::string &outFile )
{
	std::string cmdLine = RUNA_SHADER_COMPILER_PATH;
	cmdLine += " -I ";
	cmdLine += GLSL_INCLUDE_DIR;
	cmdLine += " -O -fshader-stage=";
	cmdLine += stage;
	cmdLine += " --target-env=vulkan1.2 -o \"";
	cmdLine += outFile;
	cmdLine += "\" \"";
	cmdLine += shaderFile;
	cmdLine += "\"";

	return ExecuteAndWait( const_cast< char * >( cmdLine.c_str() ) );
}

PipelineManager::PipelineManager() {}

PipelineManager::~PipelineManager()
{
	Shutdown();
}

void PipelineManager::Init()
{
	CreateDescriptorPool();
	CreatePipelineCache();

	m_sharedBlocksPool = new Buffer;
	m_sharedBlocksPool->Alloc( VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, BP_DYNAMIC, RENDERPROGS_SHARED_BLOCKS_POOL_SIZE );

	sysCallRet_t exitCode = Mkdir( CACHE_DIR );
	if ( exitCode != sysCallRet_t::SUCCESS && exitCode != sysCallRet_t::DIR_EXIST )
	{
		FatalError( "Could not create \"%s\" Directory", CACHE_DIR );
	}

	g_shaderLexer.Init();
}

void PipelineManager::Shutdown()
{
	g_shaderLexer.Shutdown();

	DestroyPipelineCache();
	DestroyDescriptorPool();

	m_pipelineProgs.clear();
	m_shaders.clear();
	m_sharedBlocks.clear();
	delete m_sharedBlocksPool;
	m_sharedBlocksPool			 = nullptr;
	m_sharedBlocksBindingCounter = 0;

	m_descriptorPool = VK_NULL_HANDLE;
	m_pipelineCache	 = VK_NULL_HANDLE;
}

void PipelineManager::AddSharedInterfaceBlock( interfaceBlock_t ib )
{
	// #TODO: check whether or not this block is already registered
	/*VkDeviceSize byteSize = 0;

	for ( const memberDeclaration_t &mb : ib.declarations )
	{
		byteSize += GetMemberTypeByteSize( mb.type );
	}*/

	ib.type	   = BT_SHARED_UBO;
	ib.binding = m_sharedBlocksBindingCounter++;
	m_sharedBlocks.emplace_back( std::move( ib ) );
}

void PipelineManager::SetSharedVar( size_t count, const char *const *varNames, const float *values )
{
	VkDeviceSize valuesOffset = 0;
	for ( size_t i = 0; i < count; ++i )
	{
		const char *name = varNames[ i ];

		VkDeviceSize writeSize = UpdateUBO( m_sharedBlocks, name, values + valuesOffset, m_sharedBlocksPool );

		if ( writeSize == 0 )
		{
			Error( "While updating shared UBO: variable \"%s\" not found.\n", name );
		}

		valuesOffset += writeSize / sizeof( float );
	}
}

bool PipelineManager::CreateEmptyPipelineProg( pipelineProg_t &out )
{
	DestroyPipelineProg( out );

	out.vertexBindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	SetDefaultState( out.stateBits );

	return true;
}

bool PipelineManager::CreateGraphicsPipeline( const char *	  vertexShader,
											  const char *	  fragmentShader,
											  pipelineProg_t &out )
{
	uint64_t state = 0;
	SetDefaultState( state );
	return CreateGraphicsPipeline( vertexShader, fragmentShader, state, out );
}

bool PipelineManager::CreateGraphicsPipeline( const char *	  vertexShader,
											  const char *	  fragmentShader,
											  const uint64_t  state,
											  pipelineProg_t &out )
{
	const char *  shaders[] = { vertexShader, fragmentShader };
	shaderStage_t stages[]	= { SS_VERTEX, SS_FRAGMENT };

	DestroyPipelineProg( out );

	if ( !LoadShaders( out, ARRAY_SIZE( shaders ), stages, shaders ) )
	{
		return false;
	}
	out.vertexBindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	UpdateState( out, state );

	CreateGraphicsPipeline( out );

	return true;
}

bool PipelineManager::CreateComputePipeline( const char *computeShader, pipelineProg_t &out )
{
	DestroyPipelineProg( out );

	const shaderStage_t computeStage = SS_COMPUTE;
	if ( !LoadShaders( out, 1, &computeStage, &computeShader ) )
	{
		return false;
	}

	CreateComputePipeline( out );

	return true;
}

bool PipelineManager::CreateComputePipeline( const char *computeShader, std::string &shaderCode, pipelineProg_t &out )
{
	DestroyPipelineProg( out );

	const shaderStage_t computeStage = SS_COMPUTE;
	if ( !LoadShaders( out, 1, &computeStage, &computeShader, &shaderCode ) )
	{
		return false;
	}

	CreateComputePipeline( out );

	return true;
}

void PipelineManager::CreateDepthPrepassPipeline( pipelineProg_t &dpp, const pipelineProg_t &srcpp )
{
	if ( srcpp.GetStatus() != pipelineStatus_t::Ok )
	{
		Error( "While creating depth prepass pipeline: source pipeline is in a bad state." );
		CHECK_PRED( false );
		return;
	}

	dpp.status		   = srcpp.GetStatus();
	dpp.stateBits	   = srcpp.stateBits;
	dpp.pipelineLayout = srcpp.pipelineLayout;
	dpp.descriptorSets = srcpp.descriptorSets;

	vkGraphicsPipeline_t vkgp {};
	GetVulkanGraphicsPipelineInfo( srcpp, vkgp );

	vkgp.shaderCount = 0;
	for ( int i = 0; i < SS_COUNT; ++i )
	{
		auto &shader = srcpp.shaders[ i ];
		if ( shader == nullptr || shader->stage != SS_VERTEX )
		{
			continue;
		}

		VkPipelineShaderStageCreateInfo &stage = vkgp.shaders[ vkgp.shaderCount++ ];
		stage.sType							   = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage.pNext							   = nullptr;
		stage.flags							   = 0;
		stage.stage							   = SS_VK_TYPES[ shader->stage ]; // #TODO Dangerous
		stage.module						   = static_cast< VkShaderModule >( shader->module );
		stage.pName							   = "main";
		stage.pSpecializationInfo			   = nullptr;

		break; // only get vertex shader
	}

	vkgp.depthStencil.depthTestEnable  = VK_TRUE;
	vkgp.depthStencil.depthWriteEnable = VK_TRUE;
	vkgp.depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;

	VkGraphicsPipelineCreateInfo pipelineCI {};
	pipelineCI.sType			   = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCI.pNext			   = nullptr;
	pipelineCI.flags			   = 0;
	pipelineCI.stageCount		   = vkgp.shaderCount;
	pipelineCI.pStages			   = vkgp.shaders.data();
	pipelineCI.pVertexInputState   = &vkgp.vertexInput;
	pipelineCI.pInputAssemblyState = &vkgp.inputAssembly;
	pipelineCI.pTessellationState  = nullptr;
	pipelineCI.pViewportState	   = &vkgp.viewport;
	pipelineCI.pRasterizationState = &vkgp.rasterization;
	pipelineCI.pMultisampleState   = &vkgp.multisample;
	pipelineCI.pDepthStencilState  = &vkgp.depthStencil;
	pipelineCI.pColorBlendState	   = &vkgp.colorBlend;
	pipelineCI.pDynamicState	   = &vkgp.dynamic;
	pipelineCI.layout			   = dpp.pipelineLayout;
	pipelineCI.renderPass		   = GetVulkanContext().renderPass;
	pipelineCI.subpass			   = 0;
	pipelineCI.basePipelineHandle  = VK_NULL_HANDLE;
	pipelineCI.basePipelineIndex   = -1;

	DestroyPipelineHandle( dpp );

	VkDevice &device = GetVulkanContext().device;
	VK_CHECK( vkCreateGraphicsPipelines( device, m_pipelineCache, 1, &pipelineCI, nullptr, &dpp.pipeline ) );
}

void PipelineManager::RegisterEvent( pipelineProg_t &pp, std::unique_ptr< Event > ev )
{
	if ( !pp.events )
	{
		pp.events = new std::vector< std::unique_ptr< Event > >;
	}

	pp.events->emplace_back( std::move( ev ) );
}

void PipelineManager::BindGraphicsPipeline( VkCommandBuffer cmdBuffer, pipelineProg_t &graphicsPipeline )
{
	CHECK_PRED( graphicsPipeline.GetStatus() == pipelineStatus_t::Ok );

	if ( graphicsPipeline.pipeline == VK_NULL_HANDLE )
	{
		CreateGraphicsPipeline( graphicsPipeline );
	}

	vkCmdBindDescriptorSets( cmdBuffer,
							 VK_PIPELINE_BIND_POINT_GRAPHICS,
							 graphicsPipeline.pipelineLayout,
							 0,
							 static_cast< uint32_t >( graphicsPipeline.descriptorSets.size() ),
							 graphicsPipeline.descriptorSets.data(),
							 0,
							 nullptr );

	vkCmdBindPipeline( cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.pipeline );
}

void PipelineManager::BindComputePipeline( VkCommandBuffer cmdBuffer, pipelineProg_t &computePipeline )
{
	CHECK_PRED( computePipeline.GetStatus() == pipelineStatus_t::Ok );

	if ( computePipeline.pipeline == VK_NULL_HANDLE )
	{
		CreateComputePipeline( computePipeline );
	}

	vkCmdBindDescriptorSets( cmdBuffer,
							 VK_PIPELINE_BIND_POINT_COMPUTE,
							 computePipeline.pipelineLayout,
							 0,
							 static_cast< uint32_t >( computePipeline.descriptorSets.size() ),
							 computePipeline.descriptorSets.data(),
							 0,
							 nullptr );

	vkCmdBindPipeline( cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline.pipeline );
}

void PipelineManager::UpdateUBOs( pipelineProg_t &	 pp,
								  size_t			 count,
								  const char *const *varNames,
								  const size_t *	 byteSizes,
								  const float *		 values )
{
	VkDeviceSize valuesOffset = 0;
	for ( size_t i = 0; i < count; ++i )
	{
		const char *name = varNames[ i ];

		VkDeviceSize writeSize = byteSizes[ i ];

		/*writeSize = */ UpdateUBO( pp.interfaceBlocks, name, values + valuesOffset, pp.uboPool );

		/*if ( writeSize == 0 )
		{
			Error( "While updating UBO: variable \"%s\" not found.\n", name );
		}*/

		// Log( "%ul", writeSize );

		valuesOffset += writeSize / sizeof( float );
	}
}

void PipelineManager::UpdateImages( pipelineProg_t &		  pp,
									size_t					  count,
									const std::string *const *varNames,
									const Image *const *	  images )
{
	auto &device = GetVulkanContext().device;

	std::vector< VkWriteDescriptorSet >	 wdsVec;
	std::vector< VkDescriptorImageInfo > diiVec;

	{
		VkWriteDescriptorSet wds {};
		wds.sType			 = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		wds.pNext			 = nullptr;
		wds.dstSet			 = pp.descriptorSets[ DS_SAMPLER ];
		wds.dstArrayElement	 = 0;
		wds.descriptorCount	 = 1;
		wds.descriptorType	 = DS_VK_TYPES[ DS_SAMPLER ];
		wds.pBufferInfo		 = nullptr;
		wds.pTexelBufferView = nullptr;

		for ( size_t i = 0; i < count; ++i )
		{
			const Image &image		= *( images[ i ] );
			bool		 found		= false;
			const auto	 ImageFound = [ & ]( const interfaceBlock_t &ib ) {
				  return ib.type == BT_SAMPLER2D && ib.name == *( varNames[ i ] );
			};
			// auto       it            = std::find_if( pp.interfaceBlocks.cbegin(), pp.interfaceBlocks.cend(),
			// predFindImage
			// );
			for ( const interfaceBlock_t &ib : pp.interfaceBlocks )
			{
				if ( ImageFound( ib ) )
				{
					wds.dstBinding = ib.binding;
					wdsVec.emplace_back( wds );

					VkDescriptorImageInfo dii {};
					dii.sampler		= image.GetSampler();
					dii.imageView	= image.GetView();
					dii.imageLayout = image.GetLayout();
					diiVec.emplace_back( dii );

					found = true;
					break;
				}
			}
			if ( !found )
			{
				Error( "While updating image: variable \"%s\" not found.\n", varNames[ i ]->c_str() );
			}
		}
	}

	for ( size_t i = 0; i < wdsVec.size(); ++i )
	{
		VkWriteDescriptorSet &		 wds = wdsVec[ i ];
		const VkDescriptorImageInfo &dii = diiVec[ i ];

		wds.pImageInfo = &dii;
	}

	vkUpdateDescriptorSets( device, static_cast< uint32_t >( wdsVec.size() ), wdsVec.data(), 0, nullptr );
}

void PipelineManager::UpdateBuffers( pipelineProg_t &	  pp,
									 size_t				  count,
									 const char *const *  varNames,
									 const Buffer *const *buffers )
{
	auto &device = GetVulkanContext().device;

	std::vector< VkWriteDescriptorSet >	  wdsVec;
	std::vector< VkDescriptorBufferInfo > dbiVec;

	{
		VkWriteDescriptorSet wds {};
		wds.sType			 = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		wds.pNext			 = nullptr;
		wds.dstSet			 = pp.descriptorSets[ DS_BUFFER ];
		wds.dstArrayElement	 = 0;
		wds.descriptorCount	 = 1;
		wds.descriptorType	 = DS_VK_TYPES[ DS_BUFFER ];
		wds.pImageInfo		 = nullptr;
		wds.pTexelBufferView = nullptr;

		for ( size_t i = 0; i < count; ++i )
		{
			const Buffer &buffer			= *( buffers[ i ] );
			bool		  found				= false;
			const auto	  IsCorrespondingIB = [ & ]( const interfaceBlock_t &ib ) {
				   return ib.type == BT_BUFFER && ( std::strcmp( ib.name.c_str(), varNames[ i ] ) == 0 );
			};

			// auto       it         = std::find_if( pp.interfaceBlocks.cbegin(), pp.interfaceBlocks.cend(), predBuffer
			// );
			for ( const interfaceBlock_t &ib : pp.interfaceBlocks )
			{
				if ( IsCorrespondingIB( ib ) )
				{
					wds.dstBinding = ib.binding;
					wdsVec.emplace_back( wds );

					VkDescriptorBufferInfo dbi {};
					dbi.buffer = buffer.GetHandle();
					dbi.offset = 0;
					dbi.range  = VK_WHOLE_SIZE; // buffer.GetSize() is incorrect, since allocated size is greater than
												// size specified upon creation
					dbiVec.emplace_back( dbi );

					found = true;
					break;
				}
			}

			if ( !found )
			{
				Error( "While updating buffer: variable \"%s\" not found.\n", varNames[ i ] );
			}
		}
	}

	for ( size_t i = 0; i < wdsVec.size(); ++i )
	{
		VkWriteDescriptorSet &		  wds = wdsVec[ i ];
		const VkDescriptorBufferInfo &dbi = dbiVec[ i ];

		wds.pBufferInfo = &dbi;
	}

	vkUpdateDescriptorSets( device, static_cast< uint32_t >( wdsVec.size() ), wdsVec.data(), 0, nullptr );
}

bool PipelineManager::LoadShaders( pipelineProg_t &		pp,
								   size_t				count,
								   const shaderStage_t *shaderStages,
								   const char *const *	paths )
{
	DestroyPipelineHandle( pp );

	std::vector< std::string > shaderCodes;
	shaderCodes.reserve( count );

	for ( size_t i = 0; i < count; ++i )
	{
		const char *path	 = paths[ i ];
		std::string fileName = path;

		const size_t period_pos = fileName.rfind( '.' );
		if ( std::string::npos == period_pos )
		{
			Error( "File \"%s\" has no extension.", fileName.c_str() );
			SetPipelineStatus( pp, pipelineStatus_t::Doomed );
			return false;
		}

		std::string fileExt( fileName.cbegin() + period_pos + 1, fileName.cend() );
		if ( std::find( VALID_EXT.cbegin(), VALID_EXT.cend(), fileExt ) == VALID_EXT.cend() )
		{
			Error( "File \"%s\": extension not supported", fileName.c_str() );
			SetPipelineStatus( pp, pipelineStatus_t::Doomed );
			return false;
		}

		std::ifstream istrm( fileName, std::ios::ate | std::ios::in );
		if ( !istrm.is_open() )
		{
			Error( "File \"%s\" not found.", fileName.c_str() );
			SetPipelineStatus( pp, pipelineStatus_t::Doomed );
			return false;
		}

		auto		fileSize = istrm.tellg();
		std::string shaderCode( fileSize, ' ' );
		istrm.seekg( 0, std::ios::beg );
		istrm.read( &shaderCode[ 0 ], fileSize );

		shaderCodes.emplace_back( std::move( shaderCode ) );
	}

	return LoadShaders( pp, count, shaderStages, paths, shaderCodes.data() );
}

bool PipelineManager::LoadShaders( pipelineProg_t &		pp,
								   size_t				count,
								   const shaderStage_t *shaderStages,
								   const char *const *	paths,
								   std::string *		shaderCodes )
{
	struct ShaderCompileInfo
	{
		std::vector< std::unique_ptr< ShaderTokenizer > > tokenizers;
		std::string										  glslCode;
		std::string										  glslPath;
		std::string										  stageStr;
	};

	DestroyPipelineHandle( pp );
	DestroyResourceBindings( pp );

	std::vector< ShaderCompileInfo > shaderCompileInfoVec( count );

	for ( size_t i = 0; i < count; ++i )
	{
		shaderStage_t shaderStage = shaderStages[ i ];
		const char *  shaderPath  = paths[ i ];
		std::string & code		  = shaderCodes[ i ];

		if ( pp.events )
		{
			for ( std::unique_ptr< Event > &ev : *pp.events )
			{
				if ( ev->IsOfType( EV_BEFORE_SHADER_PARSING ) )
				{
					if ( !ev->Call( 0, &code, shaderStage ) )
					{
						Error( "Pre parsing shader %s failed.", shaderPath );
						SetPipelineStatus( pp, pipelineStatus_t::ShaderNotCompiled );
						return false;
					}
				}
			}
		}

		std::array< std::unique_ptr< ShaderTokenizer >, 3 > exprTorkenizers = {
			std::make_unique< ResourceExprTokenizer >(),
			std::make_unique< GlobalsTokenizer >(),
			std::make_unique< ComputeShaderOptsTokenizer >()
		};

		if ( !g_shaderLexer.Parse( code,
								   exprTorkenizers.size(),
								   exprTorkenizers.data(),
								   shaderCompileInfoVec[ i ].tokenizers,
								   true ) )
		{
			Error( "Parsing shader %s failed.", shaderPath );
			SetPipelineStatus( pp, pipelineStatus_t::ShaderNotCompiled );
			return false;
		}

		for ( const auto &tokenizer : shaderCompileInfoVec[ i ].tokenizers )
		{
			parsedObjectAction_t actions = tokenizer->GetActions();
			if ( actions & POA_BIND_IB_SCOPE_PIPELINE )
			{
				auto ib = static_cast< interfaceBlock_t * >( tokenizer->GetActionParams() );
				BindInterfaceBlock( pp, *ib );
			}
			if ( actions & POA_BIND_SHARED_IB )
			{
				auto sib = static_cast< interfaceBlock_t * >( tokenizer->GetActionParams() );
				BindSharedInterfaceBlock( pp, *sib );
			}
		}
	}

	for ( size_t i = 0; i < count; ++i )
	{
		for ( const auto &tokenizer : shaderCompileInfoVec[ i ].tokenizers )
		{
			parsedObjectAction_t actions = tokenizer->GetActions();
			if ( actions & POA_BIND_IB_SCOPE_PIPELINE )
			{
				auto			  ib = static_cast< interfaceBlock_t * >( tokenizer->GetActionParams() );
				interfaceBlock_t *duplicateIB;
				if ( ib->HoldsUserVars() &&
					 FindInterfaceBlock( pp.interfaceBlocks, ib->name, ib->type, ib->flags, &duplicateIB ) )
				{
					*ib = *duplicateIB;
				}
			}
		}

		g_shaderLexer.Combine( shaderCompileInfoVec[ i ].tokenizers, shaderCompileInfoVec[ i ].glslCode );

		// Output GLSL code
		{
			std::string	 glslPath	= GetGLSLPath( paths[ i ] ); // #TODO remove call ?
			const size_t period_pos = glslPath.rfind( '.' );
			if ( std::string::npos == period_pos )
			{
				Error( "File \"%s\" has no extension.", glslPath.c_str() );
				SetPipelineStatus( pp, pipelineStatus_t::Doomed );
				return false;
			}
			shaderCompileInfoVec[ i ].stageStr = std::string( glslPath.cbegin() + period_pos + 1, glslPath.cend() );
			shaderCompileInfoVec[ i ].glslPath = glslPath + ".glsl";

			std::ofstream ostrm( shaderCompileInfoVec[ i ].glslPath, std::ios_base::out | std::ios_base::trunc );
			if ( !ostrm.is_open() )
			{
				Error( "Could not create %s.", shaderCompileInfoVec[ i ].glslPath.c_str() );
				SetPipelineStatus( pp, pipelineStatus_t::Doomed );
				return false;
			}

			ostrm.write( shaderCompileInfoVec[ i ].glslCode.c_str(), shaderCompileInfoVec[ i ].glslCode.size() );
		}

		std::string spirvFile = shaderCompileInfoVec[ i ].glslPath;
		spirvFile += ".spv";

		// Compile to spir-V
		{
			if ( !CompileShader( shaderCompileInfoVec[ i ].glslPath, shaderCompileInfoVec[ i ].stageStr, spirvFile ) )
			{
				Error( "Compiling %s failed.", spirvFile.c_str() );
				SetPipelineStatus( pp, pipelineStatus_t::ShaderNotCompiled );
				return false;
			}
		}

		// Create shader module
		{
			const shaderStage_t shaderStage = shaderStages[ i ];
			const char *		shaderPath	= paths[ i ];

			auto &shader = pp.shaders[ shaderStage ];

			if ( shader == nullptr )
			{
				shader		  = std::make_unique< shader_t >();
				shader->stage = shaderStage;
			}
			else
			{
				shader->DestroyModule();
			}

			shader->path = shaderPath;

			shader->UpdateModule( spirvFile.c_str() );
		}
	}

	FinalizeShadersUpdate( pp );

	ValidatePipeline( pp );
	return true;
}

void PipelineManager::FinalizeShadersUpdate( pipelineProg_t &pp )
{
	AllocUBOs( pp );

	UpdateResourceBindings( pp );

	UpdateDescriptorSetUBO( pp );
}

void PipelineManager::UpdateResourceBindings( pipelineProg_t &pp )
{
	auto &device = GetVulkanContext().device;

	// Create descriptor sets layouts
	{
		std::array< std::vector< VkDescriptorSetLayoutBinding >, DS_COUNT > dslbVecTable;

		for ( interfaceBlock_t &ib : pp.interfaceBlocks )
		{
			descriptorSet_t setId = BindingTypeToDescSet( ib.type );

			std::vector< VkDescriptorSetLayoutBinding > &dslbVec = dslbVecTable[ setId ];

			const auto predSameBinding = [ & ]( const VkDescriptorSetLayoutBinding &elt ) {
				return elt.binding == ib.binding;
			};
			if ( std::find_if( dslbVec.cbegin(), dslbVec.cend(), predSameBinding ) == dslbVec.cend() )
			{
				VkDescriptorSetLayoutBinding dslb {};
				dslb.binding			= ib.binding;
				dslb.descriptorType		= DS_VK_TYPES[ setId ];
				dslb.descriptorCount	= 1; // Arrays not handled
				dslb.stageFlags			= VK_SHADER_STAGE_ALL;
				dslb.pImmutableSamplers = nullptr;

				dslbVec.emplace_back( dslb );
			}
		}

		for ( int sibBinding : pp.sharedInterfaceBlockBindings )
		{
			const descriptorSet_t setId = BindingTypeToDescSet( BT_SHARED_UBO );

			std::vector< VkDescriptorSetLayoutBinding > &dslbVec = dslbVecTable[ setId ];

			const auto predSameBinding = [ & ]( const VkDescriptorSetLayoutBinding &elt ) {
				return elt.binding == sibBinding;
			};
			if ( std::find_if( dslbVec.cbegin(), dslbVec.cend(), predSameBinding ) == dslbVec.cend() )
			{
				VkDescriptorSetLayoutBinding dslb {};
				dslb.binding			= sibBinding;
				dslb.descriptorType		= DS_VK_TYPES[ setId ];
				dslb.descriptorCount	= 1; // Arrays not handled
				dslb.stageFlags			= VK_SHADER_STAGE_ALL;
				dslb.pImmutableSamplers = nullptr;

				dslbVec.emplace_back( dslb );
			}
		}

		DestroyDescriptorSetLayouts( pp );

		for ( size_t i = 0; i < dslbVecTable.size(); ++i )
		{
			const std::vector< VkDescriptorSetLayoutBinding > &dslbVec = dslbVecTable[ i ];

			VkDescriptorSetLayoutCreateInfo dsCI {};
			dsCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			dsCI.pNext = nullptr;
			dsCI.flags = 0;

			dsCI.bindingCount = static_cast< uint32_t >( dslbVec.size() );
			dsCI.pBindings	  = dslbVec.data();

			VK_CHECK( vkCreateDescriptorSetLayout( device, &dsCI, nullptr, &pp.descriptorSetLayouts[ i ] ) );
		}
	}

	// TODO add shared blocks handling

	// Create pipeline layout
	{
		DestroyPipelineLayout( pp );

		VkPipelineLayoutCreateInfo plCI {};
		plCI.sType				 = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		plCI.pNext				 = nullptr;
		plCI.flags				 = 0;
		plCI.setLayoutCount		 = static_cast< uint32_t >( pp.descriptorSetLayouts.size() );
		plCI.pSetLayouts		 = pp.descriptorSetLayouts.data();
		plCI.pPushConstantRanges = 0;
		plCI.pPushConstantRanges = nullptr;

		VK_CHECK( vkCreatePipelineLayout( device, &plCI, nullptr, &pp.pipelineLayout ) );

		FreeDescriptorSets( pp );
		AllocDescriptorSets( pp );
	}
}

void PipelineManager::DestroyResourceBindings( pipelineProg_t &pp )
{
	FreeUBOs( pp );
	FreeDescriptorSets( pp );
	DestroyDescriptorSetLayouts( pp );
	DestroyPipelineLayout( pp );
	pp.interfaceBlocks.clear();
	pp.sharedInterfaceBlockBindings.clear();
	pp.pipelineLayout = VK_NULL_HANDLE;
	ResetCounters( pp );
}

void PipelineManager::ResetCounters( pipelineProg_t &pp )
{
	std::memset( pp.resourceCounters.data(), 0, sizeof( pp.resourceCounters ) );
}

void PipelineManager::UpdateVertexDesc( pipelineProg_t &						 pp,
										uint32_t								 stride,
										VkVertexInputRate						 inputRate,
										uint32_t								 attriutebDescCount,
										const VkVertexInputAttributeDescription *attributeDescs )
{
	VkVertexInputBindingDescription &bindingDesc = pp.vertexBindingDesc;
	bindingDesc.binding							 = 0;
	bindingDesc.stride							 = stride;
	bindingDesc.inputRate						 = inputRate;

	pp.vertexAttributeDescs.resize( attriutebDescCount );

	for ( uint32_t i = 0; i < attriutebDescCount; ++i )
	{
		VkVertexInputAttributeDescription &attribDesc = pp.vertexAttributeDescs[ i ];

		attribDesc		   = attributeDescs[ i ];
		attribDesc.binding = 0;
	}
}

void PipelineManager::UpdateState( pipelineProg_t &pp, uint64_t state )
{
	DestroyPipelineHandle( pp );
	pp.stateBits = state;
}

byte *PipelineManager::GetUBOPtr( const pipelineProg_t &pp, int interfaceBlockIndex )
{
	const VkDeviceSize minUniformBufferOffsetAlignment =
		GetVulkanContext().gpu.properties.limits.minUniformBufferOffsetAlignment;

	size_t offset = 0;

	for ( int i = 0; i < interfaceBlockIndex; ++i )
	{
		const interfaceBlock_t &ib = pp.interfaceBlocks[ i ];
		if ( ib.type == BT_UBO )
		{
			offset += ib.GetByteSize();
			offset = Align( offset, minUniformBufferOffsetAlignment );
		}
	}

	return static_cast< byte * >( pp.uboPool->GetPointer() ) + offset;
}

NO_DISCARD bool PipelineManager::Reload( pipelineProg_t &pp )
{
	bool shadersReloaded = ReloadShaders( pp );

	if ( shadersReloaded && pp.serializedValues )
	{
		DeserializeInterfaceBlocks( pp, *pp.serializedValues );
	}

	return shadersReloaded;
}

bool PipelineManager::ReloadShaders( pipelineProg_t &pp )
{
	shaderStage_t stages[ SS_COUNT ];
	char const *  paths[ SS_COUNT ];
	size_t		  count = 0;

	for ( int i = 0; i < SS_COUNT; ++i )
	{
		auto &shader = pp.shaders[ i ];
		if ( shader == nullptr )
		{
			continue;
		}

		stages[ count ] = shader->stage;
		paths[ count ]	= shader->path.c_str();

		++count;
	}

	return LoadShaders( pp, count, stages, paths );
}

void PipelineManager::ClearSerializedValues( pipelineProg_t &pp )
{
	pp.serializedValues = nullptr;
}

void PipelineManager::DestroyPipelineProg( pipelineProg_t &pp )
{
	const shaderStage_t stage = SS_ALL;

	DestroyShaders( pp, 1, &stage );
	DestroyResourceBindings( pp );
	DestroyPipelineHandle( pp );

	std::memset( &pp.vertexBindingDesc, 0, sizeof( pp.vertexBindingDesc ) );

	pp.vertexAttributeDescs.clear();
	pp.stateBits = 0;

	delete pp.events;
	pp.events = nullptr;

	pp.serializedValues = nullptr;
}

void PipelineManager::DestroyPipelineProgKeepResources( pipelineProg_t &pp )
{
	DestroyPipelineHandle( pp );
}

void PipelineManager::DestroyShaders( pipelineProg_t &pp, size_t count, const shaderStage_t *shaderStages )
{
	for ( size_t i = 0; i < count; ++i )
	{
		shaderStage_t stage = shaderStages[ i ];
		if ( stage < SS_COUNT )
		{
			pp.shaders[ stage ] = nullptr;
		}
		else if ( stage == SS_ALL_GRAPHICS )
		{
			for ( int s = SS_VERTEX; s < SS_COMPUTE; ++s )
			{
				pp.shaders[ s ] = nullptr;
			}
		}
		else if ( stage == SS_ALL )
		{
			for ( int s = SS_VERTEX; s < SS_COUNT; ++s )
			{
				pp.shaders[ s ] = nullptr;
			}

			return;
		}
	}
}

void PipelineManager::FreeUBOs( pipelineProg_t &pp )
{
	delete pp.uboPool;
	pp.uboPool = nullptr;
}

void PipelineManager::GetVulkanGraphicsPipelineInfo( const pipelineProg_t &pp, vkGraphicsPipeline_t &vkgp )
{
	// Shaders
	/*std::array< VkPipelineShaderStageCreateInfo, SS_COUNT > stages;
	uint32_t												stagesCount = 0;*/
	vkgp.shaderCount = 0;
	for ( int i = 0; i < SS_COUNT; ++i )
	{
		auto &shader = pp.shaders[ i ];
		if ( shader == nullptr )
		{
			continue;
		}

		VkPipelineShaderStageCreateInfo &stage = vkgp.shaders[ vkgp.shaderCount++ ];
		stage.sType							   = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage.pNext							   = nullptr;
		stage.flags							   = 0;
		stage.stage							   = SS_VK_TYPES[ shader->stage ]; // #TODO Dangerous
		stage.module						   = static_cast< VkShaderModule >( shader->module );
		stage.pName							   = "main";
		stage.pSpecializationInfo			   = nullptr;
	}

	// Vertex input
	// VkPipelineVertexInputStateCreateInfo vertexInputState {};
	vkgp.vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vkgp.vertexInput.pNext = nullptr;
	vkgp.vertexInput.flags = 0;
	vkgp.vertexInput.vertexBindingDescriptionCount =
		uint32_t( std::min< size_t >( 1, pp.vertexAttributeDescs.size() ) );
	vkgp.vertexInput.pVertexBindingDescriptions		 = &pp.vertexBindingDesc;
	vkgp.vertexInput.vertexAttributeDescriptionCount = uint32_t( pp.vertexAttributeDescs.size() );
	vkgp.vertexInput.pVertexAttributeDescriptions	 = pp.vertexAttributeDescs.data();

	// Input assembly
	// VkPipelineInputAssemblyStateCreateInfo inputAssemblyState {};
	vkgp.inputAssembly.sType				  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	vkgp.inputAssembly.pNext				  = nullptr;
	vkgp.inputAssembly.flags				  = 0;
	vkgp.inputAssembly.topology				  = StateToVkPrimitiveTopology( pp.stateBits );
	vkgp.inputAssembly.primitiveRestartEnable = VK_FALSE;

	// Viewport state
	// VkPipelineViewportStateCreateInfo viewportState {};
	vkgp.viewport.sType			= VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vkgp.viewport.pNext			= nullptr;
	vkgp.viewport.flags			= 0;
	vkgp.viewport.viewportCount = 1;
	vkgp.viewport.pViewports	= nullptr;
	vkgp.viewport.scissorCount	= 1;
	vkgp.viewport.pScissors		= nullptr;

	// Rasterization
	// VkPipelineRasterizationStateCreateInfo rasterizationState {};
	vkgp.rasterization.sType				   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	vkgp.rasterization.pNext				   = nullptr;
	vkgp.rasterization.flags				   = 0;
	vkgp.rasterization.depthClampEnable		   = VK_FALSE;
	vkgp.rasterization.rasterizerDiscardEnable = VK_FALSE;
	vkgp.rasterization.polygonMode			   = StateToVkPolygonMode( pp.stateBits );
	vkgp.rasterization.cullMode				   = StateToVkCullMode( pp.stateBits );
	vkgp.rasterization.frontFace			   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	vkgp.rasterization.depthBiasEnable		   = VK_FALSE;
	vkgp.rasterization.depthBiasConstantFactor = 0.0f;
	vkgp.rasterization.depthBiasClamp		   = 0.0f;
	vkgp.rasterization.depthBiasSlopeFactor	   = 1.0f;
	vkgp.rasterization.lineWidth			   = 1.0f;

	// Multisampling. Note: to activate it, do not forget the device level extension
	// VkPipelineMultisampleStateCreateInfo multisampleState {};
	vkgp.multisample.sType				   = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	vkgp.multisample.pNext				   = nullptr;
	vkgp.multisample.flags				   = 0;
	vkgp.multisample.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
	vkgp.multisample.sampleShadingEnable   = VK_FALSE;
	vkgp.multisample.minSampleShading	   = 1.0f;
	vkgp.multisample.pSampleMask		   = nullptr;
	vkgp.multisample.alphaToCoverageEnable = VK_FALSE;

	// Depth / Stencil
	// VkPipelineDepthStencilStateCreateInfo depthStencilState {};
	vkgp.depthStencil.sType					= VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	vkgp.depthStencil.pNext					= nullptr;
	vkgp.depthStencil.flags					= 0;
	vkgp.depthStencil.depthTestEnable		= ( pp.stateBits & DEPTH_TEST_ENABLE ) == DEPTH_TEST_ENABLE;
	vkgp.depthStencil.depthWriteEnable		= ( pp.stateBits & DEPTH_WRITE_ENABLE ) == DEPTH_WRITE_ENABLE;
	vkgp.depthStencil.depthCompareOp		= StateToVkDepthCompare( pp.stateBits );
	vkgp.depthStencil.depthBoundsTestEnable = GetVulkanContext().gpu.features.depthBounds;
	vkgp.depthStencil.stencilTestEnable		= ( pp.stateBits & STENCIL_TEST_ENABLE ) == STENCIL_TEST_ENABLE;
	vkgp.depthStencil.front.failOp			= StateToVkStencilOpFail( pp.stateBits );
	vkgp.depthStencil.front.passOp			= StateToVkStencilOpPass( pp.stateBits );
	vkgp.depthStencil.front.depthFailOp		= StateToVkStencilOpDepthFail( pp.stateBits );
	vkgp.depthStencil.front.compareOp		= StateToVkStencilCompare( pp.stateBits );
	vkgp.depthStencil.front.compareMask		= StateToStencilMask( pp.stateBits );
	vkgp.depthStencil.front.writeMask		= 0xFFFFFFFF;
	vkgp.depthStencil.front.reference		= StateToStencilRef( pp.stateBits );
	vkgp.depthStencil.back					= vkgp.depthStencil.front;
	vkgp.depthStencil.minDepthBounds		= 0.0f;
	vkgp.depthStencil.maxDepthBounds		= 1.0f;

	// Blending
	// VkPipelineColorBlendAttachmentState blendAttachment {};
	vkgp.blendAttachment.srcColorBlendFactor = StateToVkSrcBlend( pp.stateBits );
	vkgp.blendAttachment.dstColorBlendFactor = StateToVkDstBlend( pp.stateBits );
	vkgp.blendAttachment.blendEnable		 = !( vkgp.blendAttachment.srcColorBlendFactor == VK_BLEND_FACTOR_ONE &&
										  vkgp.blendAttachment.dstColorBlendFactor == VK_BLEND_FACTOR_ZERO );
	vkgp.blendAttachment.colorBlendOp		 = StateToVkBlendOp( pp.stateBits );
	vkgp.blendAttachment.srcAlphaBlendFactor = vkgp.blendAttachment.srcColorBlendFactor;
	vkgp.blendAttachment.dstAlphaBlendFactor = vkgp.blendAttachment.dstColorBlendFactor;
	vkgp.blendAttachment.alphaBlendOp		 = vkgp.blendAttachment.colorBlendOp;
	vkgp.blendAttachment.colorWriteMask		 = StateToVkColorMask( pp.stateBits );

	// VkPipelineColorBlendStateCreateInfo colorBlendState {};
	vkgp.colorBlend.sType			= VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	vkgp.colorBlend.attachmentCount = 1;
	vkgp.colorBlend.pAttachments	= &vkgp.blendAttachment;

	// Dynamic states
	// std::array< VkDynamicState, 3 > dynamics	  = { VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT };
	// uint32_t						dynamicsCount = 2;
	vkgp.dynamicStateCount	= 2;
	vkgp.dynamicStates[ 0 ] = VK_DYNAMIC_STATE_SCISSOR;
	vkgp.dynamicStates[ 1 ] = VK_DYNAMIC_STATE_VIEWPORT;
	if ( GetVulkanContext().gpu.features.depthBounds )
	{
		vkgp.dynamicStates[ vkgp.dynamicStateCount++ ] = VK_DYNAMIC_STATE_DEPTH_BOUNDS;
	}

	// VkPipelineDynamicStateCreateInfo dynamicState {};
	vkgp.dynamic.sType			   = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	vkgp.dynamic.pNext			   = nullptr;
	vkgp.dynamic.flags			   = 0;
	vkgp.dynamic.dynamicStateCount = vkgp.dynamicStateCount;
	vkgp.dynamic.pDynamicStates	   = vkgp.dynamicStates.data();
}

void PipelineManager::CreateGraphicsPipeline( pipelineProg_t &pp )
{
	vkGraphicsPipeline_t vkgp {};

	GetVulkanGraphicsPipelineInfo( pp, vkgp );

	VkGraphicsPipelineCreateInfo pipelineCI {};
	pipelineCI.sType			   = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCI.pNext			   = nullptr;
	pipelineCI.flags			   = 0;
	pipelineCI.stageCount		   = vkgp.shaderCount;
	pipelineCI.pStages			   = vkgp.shaders.data();
	pipelineCI.pVertexInputState   = &vkgp.vertexInput;
	pipelineCI.pInputAssemblyState = &vkgp.inputAssembly;
	pipelineCI.pTessellationState  = nullptr;
	pipelineCI.pViewportState	   = &vkgp.viewport;
	pipelineCI.pRasterizationState = &vkgp.rasterization;
	pipelineCI.pMultisampleState   = &vkgp.multisample;
	pipelineCI.pDepthStencilState  = &vkgp.depthStencil;
	pipelineCI.pColorBlendState	   = &vkgp.colorBlend;
	pipelineCI.pDynamicState	   = &vkgp.dynamic;
	pipelineCI.layout			   = pp.pipelineLayout;
	pipelineCI.renderPass		   = GetVulkanContext().renderPass;
	pipelineCI.subpass			   = 0;
	pipelineCI.basePipelineHandle  = VK_NULL_HANDLE;
	pipelineCI.basePipelineIndex   = -1;

	DestroyPipelineHandle( pp );

	VkDevice &device = GetVulkanContext().device;
	VK_CHECK( vkCreateGraphicsPipelines( device, m_pipelineCache, 1, &pipelineCI, nullptr, &pp.pipeline ) );
}

void PipelineManager::CreateComputePipeline( pipelineProg_t &pp )
{
	auto &device = GetVulkanContext().device;

	VkComputePipelineCreateInfo pipelineCI {};
	pipelineCI.sType					 = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineCI.pNext					 = nullptr;
	pipelineCI.flags					 = 0;
	pipelineCI.stage.sType				 = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipelineCI.stage.pNext				 = nullptr;
	pipelineCI.stage.flags				 = 0;
	pipelineCI.stage.stage				 = SS_VK_TYPES[ SS_COMPUTE ];
	pipelineCI.stage.module				 = static_cast< VkShaderModule >( pp.shaders[ SS_COMPUTE ]->module );
	pipelineCI.stage.pName				 = "main";
	pipelineCI.stage.pSpecializationInfo = nullptr;
	pipelineCI.layout					 = pp.pipelineLayout;
	pipelineCI.basePipelineHandle		 = VK_NULL_HANDLE;
	pipelineCI.basePipelineIndex		 = -1;

	DestroyPipelineHandle( pp );

	VK_CHECK( vkCreateComputePipelines( device, m_pipelineCache, 1, &pipelineCI, nullptr, &pp.pipeline ) );
}

void BindResource( pipelineProg_t &pp, interfaceBlock_t &interfaceBlock, uint32_t &counter )
{
	interfaceBlock_t *duplicateIB;

	if ( FindInterfaceBlock( pp.interfaceBlocks,
							 interfaceBlock.name,
							 interfaceBlock.type,
							 interfaceBlock.flags,
							 &duplicateIB ) )
	{
		interfaceBlock.binding = duplicateIB->binding;

		if ( duplicateIB->HoldsUserVars() )
		{
			for ( const memberDeclaration_t &md : interfaceBlock.declarations )
			{
				if ( std::find( duplicateIB->declarations.cbegin(), duplicateIB->declarations.cend(), md ) ==
					 duplicateIB->declarations.cend() )
				{
					duplicateIB->declarations.emplace_back( md );
				}
			}
		}
	}
	else
	{
		interfaceBlock.binding = counter++;

		pp.interfaceBlocks.emplace_back( interfaceBlock );
	}
}

void PipelineManager::BindInterfaceBlock( pipelineProg_t &pp, interfaceBlock_t &interfaceBlock )
{
	switch ( interfaceBlock.type )
	{
		case BT_UBO:
		{
			BindUBO( pp, interfaceBlock );
			break;
		}
		case BT_SAMPLER2D:
		{
			BindSampler( pp, interfaceBlock );
			break;
		}

		case BT_BUFFER:
		{
			BindBuffer( pp, interfaceBlock );
			break;
		}

		default:
		{
			FatalError( "When binding interface block: Unknown interface block type." );
			break;
		}
	}
}

void PipelineManager::BindSharedInterfaceBlock( pipelineProg_t &pp, interfaceBlock_t &sharedInterfaceBlock )
{
	CHECK_PRED( sharedInterfaceBlock.type == BT_SHARED_UBO );
	int binding = GetSharedBlockBinding( sharedInterfaceBlock );
	CHECK_PRED( binding >= 0 );
	sharedInterfaceBlock.binding = binding;
	// #TODO: Manage duplicates
	if ( std::find( pp.sharedInterfaceBlockBindings.cbegin(), pp.sharedInterfaceBlockBindings.cend(), binding ) ==
		 pp.sharedInterfaceBlockBindings.cend() )
	{
		pp.sharedInterfaceBlockBindings.emplace_back( binding );
	}
}

void PipelineManager::BindUBO( pipelineProg_t &pp, interfaceBlock_t &interfaceBlock )
{
	BindResource( pp, interfaceBlock, pp.resourceCounters[ DS_UBO ] );
}

void PipelineManager::BindSampler( pipelineProg_t &pp, interfaceBlock_t &interfaceBlock )
{
	BindResource( pp, interfaceBlock, pp.resourceCounters[ DS_SAMPLER ] );
}

void PipelineManager::BindBuffer( pipelineProg_t &pp, interfaceBlock_t &interfaceBlock )
{
	BindResource( pp, interfaceBlock, pp.resourceCounters[ DS_BUFFER ] );
}

int PipelineManager::GetSharedBlockBinding( const interfaceBlock_t &sharedBlock )
{
	for ( const interfaceBlock_t &sib : m_sharedBlocks )
	{
		if ( sharedBlock.name == sib.name )
		{
			return sib.binding;
		}
	}

	return -1;
}

void PipelineManager::UpdateDescriptorSetUBO( pipelineProg_t &pp )
{
	auto &device = GetVulkanContext().device;

	const VkDeviceSize minUniformBufferOffsetAlignment =
		GetVulkanContext().gpu.properties.limits.minUniformBufferOffsetAlignment;

	Buffer *uboPool = pp.uboPool;
	if ( !uboPool )
	{
		return; // #TODO risky early exit ?
	}

	std::vector< const interfaceBlock_t * > privateUBOs = GetUniquePrivateUBOs( pp );
	std::vector< const interfaceBlock_t * > sharedUBOs	= GetUniqueSharedBlocks( pp );

	if ( privateUBOs.empty() && sharedUBOs.empty() )
	{
		return;
	}

	std::vector< VkWriteDescriptorSet >	  wdsVec;
	std::vector< VkDescriptorBufferInfo > dbiVec;

	VkWriteDescriptorSet wds {};
	wds.sType			 = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	wds.pNext			 = nullptr;
	wds.dstSet			 = pp.descriptorSets[ DS_UBO ];
	wds.dstArrayElement	 = 0;
	wds.descriptorCount	 = 1;
	wds.descriptorType	 = DS_VK_TYPES[ DS_UBO ];
	wds.pImageInfo		 = nullptr;
	wds.pTexelBufferView = nullptr;

	// Private UBOs
	{
		VkDescriptorBufferInfo dbi {};
		dbi.buffer = uboPool->GetHandle();

		VkDeviceSize offset = 0;
		for ( const interfaceBlock_t *ib : privateUBOs )
		{
			VkDeviceSize ibByteSize = 0;
			for ( const memberDeclaration_t &uniform : ib->declarations )
			{
				ibByteSize += GetMemberTypeByteSize( uniform.type );
			}

			const VkDeviceSize offsetAlignment =
				GetVulkanContext().gpu.properties.limits.minUniformBufferOffsetAlignment;
			if ( offset % offsetAlignment )
			{
				FatalError(
					"VkDescriptorBufferInfo: incorrectly aligned offset. Specified was %llu bytes, min alignment is "
					"%llu "
					"bytes.",
					offset,
					offsetAlignment );
			}

			dbi.offset = offset;
			dbi.range  = ibByteSize;
			dbiVec.emplace_back( dbi );

			offset += ibByteSize;
			offset = Align( offset, minUniformBufferOffsetAlignment );

			wds.dstBinding = ib->binding;
			wdsVec.emplace_back( wds );
		}
	}

	// Shared UBOs
	{
		wds.dstSet		   = pp.descriptorSets[ DS_SHARED_BUFFER ];
		wds.descriptorType = DS_VK_TYPES[ DS_SHARED_BUFFER ];

		VkDescriptorBufferInfo dbi {};
		dbi.buffer = m_sharedBlocksPool->GetHandle();

		VkDeviceSize offset = 0;
		for ( const interfaceBlock_t *ib : sharedUBOs )
		{
			VkDeviceSize ibByteSize = 0;
			for ( const memberDeclaration_t &uniform : ib->declarations )
			{
				ibByteSize += GetMemberTypeByteSize( uniform.type );
			}

			const VkDeviceSize offsetAlignment =
				GetVulkanContext().gpu.properties.limits.minUniformBufferOffsetAlignment;
			if ( offset % offsetAlignment )
			{
				FatalError(
					"VkDescriptorBufferInfo: incorrectly aligned offset. Specified was %llu bytes, min alignment is "
					"%llu "
					"bytes.",
					offset,
					offsetAlignment );
			}

			dbi.offset = offset;
			dbi.range  = ibByteSize;
			dbiVec.emplace_back( dbi );

			offset += ibByteSize;
			offset = Align( offset, minUniformBufferOffsetAlignment );

			wds.dstBinding = ib->binding;
			wdsVec.emplace_back( wds );
		}
	}

	for ( size_t i = 0; i < wdsVec.size(); ++i )
	{
		VkWriteDescriptorSet &		  wds = wdsVec[ i ];
		const VkDescriptorBufferInfo &dbi = dbiVec[ i ];

		wds.pBufferInfo = &dbi;
	}
	// #TODO make sure that this function is not called when the corresponding desc set is being used in a cmd buffer
	vkUpdateDescriptorSets( device, static_cast< uint32_t >( wdsVec.size() ), wdsVec.data(), 0, nullptr );
}

void PipelineManager::AllocUBOs( pipelineProg_t &pp )
{
	const VkDeviceSize minUniformBufferOffsetAlignment =
		GetVulkanContext().gpu.properties.limits.minUniformBufferOffsetAlignment;

	FreeUBOs( pp );

	VkDeviceSize allocSize = 0;

	for ( const interfaceBlock_t &ib : pp.interfaceBlocks )
	{
		if ( ib.type != BT_UBO )
		{
			continue;
		}

		allocSize = Align( allocSize, minUniformBufferOffsetAlignment );
		allocSize += ib.GetByteSize();
	}

	if ( allocSize > 0 )
	{
		pp.uboPool = new Buffer;
		pp.uboPool->Alloc( VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, BP_DYNAMIC, allocSize );
	}
}

void PipelineManager::AllocDescriptorSets( pipelineProg_t &pp )
{
	auto &device = GetVulkanContext().device;

	VkDescriptorSetAllocateInfo allocInfo {};
	allocInfo.sType				 = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext				 = nullptr;
	allocInfo.descriptorPool	 = m_descriptorPool;
	allocInfo.descriptorSetCount = static_cast< uint32_t >( pp.descriptorSetLayouts.size() );
	allocInfo.pSetLayouts		 = pp.descriptorSetLayouts.data();

	VK_CHECK( vkAllocateDescriptorSets( device, &allocInfo, pp.descriptorSets.data() ) );
}

void PipelineManager::FreeDescriptorSets( pipelineProg_t &pp )
{
	auto &device = GetVulkanContext().device;
	if ( pp.descriptorSets.size() > 0 && pp.descriptorSets[ 0 ] != VK_NULL_HANDLE )
	{
		vkFreeDescriptorSets( device,
							  m_descriptorPool,
							  static_cast< uint32_t >( pp.descriptorSets.size() ),
							  pp.descriptorSets.data() );
		std::memset( pp.descriptorSets.data(), 0, pp.descriptorSets.size() * sizeof( pp.descriptorSets[ 0 ] ) );
	}
}

void PipelineManager::DestroyDescriptorSetLayouts( pipelineProg_t &pp )
{
	auto &device = GetVulkanContext().device;

	for ( VkDescriptorSetLayout &dsl : pp.descriptorSetLayouts )
	{
		if ( dsl != VK_NULL_HANDLE )
		{
			vkDestroyDescriptorSetLayout( device, dsl, nullptr );
			dsl = VK_NULL_HANDLE;
		}
	}
}

void PipelineManager::DestroyPipelineLayout( pipelineProg_t &pp )
{
	auto &device = GetVulkanContext().device;

	if ( pp.pipelineLayout != VK_NULL_HANDLE )
	{
		vkDestroyPipelineLayout( device, pp.pipelineLayout, nullptr );
		pp.pipelineLayout = VK_NULL_HANDLE;
	}
}

void PipelineManager::DestroyPipelineHandle( pipelineProg_t &pp )
{
	auto &device = GetVulkanContext().device;

	if ( pp.pipeline != VK_NULL_HANDLE )
	{
		vkDestroyPipeline( device, pp.pipeline, nullptr );
		pp.pipeline = VK_NULL_HANDLE;
	}
}

void PipelineManager::CreateDescriptorPool()
{
	auto &device = GetVulkanContext().device;

	DestroyDescriptorPool();

	std::array< VkDescriptorPoolSize, DS_COUNT > poolSizes;
	uint32_t									 maxSets = 0;
	for ( int setId = 0; setId < DS_COUNT; ++setId )
	{
		VkDescriptorPoolSize &poolSize = poolSizes[ setId ];

		poolSize.type			 = DS_VK_TYPES[ setId ];
		poolSize.descriptorCount = DS_POOL_SIZES[ setId ];

		maxSets += poolSize.descriptorCount;
	}

	VkDescriptorPoolCreateInfo dspCI {};
	dspCI.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	dspCI.pNext			= nullptr;
	dspCI.flags			= VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	dspCI.maxSets		= maxSets;
	dspCI.poolSizeCount = static_cast< uint32_t >( poolSizes.size() );
	dspCI.pPoolSizes	= poolSizes.data();

	VK_CHECK( vkCreateDescriptorPool( device, &dspCI, nullptr, &m_descriptorPool ) );
}

void PipelineManager::DestroyDescriptorPool()
{
	auto &device = GetVulkanContext().device;

	if ( m_descriptorPool != VK_NULL_HANDLE )
	{
		vkDestroyDescriptorPool( device, m_descriptorPool, nullptr );
		m_descriptorPool = VK_NULL_HANDLE;
	}
}

void PipelineManager ::CreatePipelineCache()
{
	auto &device = GetVulkanContext().device;

	DestroyPipelineCache();

	VkPipelineCacheCreateInfo pipelineCacheCreateInfo {};
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	VK_CHECK( vkCreatePipelineCache( device, &pipelineCacheCreateInfo, nullptr, &m_pipelineCache ) );
}

void PipelineManager::DestroyPipelineCache()
{
	auto &device = GetVulkanContext().device;

	if ( m_pipelineCache != VK_NULL_HANDLE )
	{
		vkDestroyPipelineCache( device, m_pipelineCache, nullptr );
		m_pipelineCache = VK_NULL_HANDLE;
	}
}

vkRuna::SerializableData SerializeUserVar( const memberDeclaration_t &declaration, const float *value )
{
	SerializableData suv;
	suv.key = declaration.name;

	switch ( declaration.type )
	{
		case MT_VEC4:
		case MT_COLOR:
			// case MT_MAT4:
			{
				suv.type  = declaration.type == MT_COLOR ? SVT_COLOR : SVT_FLOAT;
				suv.count = declaration.type == MT_MAT4 ? 16 : 4;

				float *storage = new float[ suv.count ];
				for ( int i = 0; i < suv.count; i++ )
				{
					storage[ i ] = value[ i ];
				}
				suv.value = storage;

				break;
			}
		default:
		{
			FatalError( "Unserializable user type %d.", declaration.type );
			break;
		}
	}

	return suv;
}

void DeserializeUserVar( const SerializableData &serializedData,
						 std::string &			 varName,
						 size_t &				 byteSize,
						 std::vector< float > &	 value )
{
	varName = serializedData.key;
	switch ( serializedData.type )
	{
		case SVT_FLOAT:
		case SVT_COLOR:
		{
			byteSize   = serializedData.count * sizeof( float );
			float *vec = static_cast< float * >( serializedData.value );
			for ( int i = 0; i < serializedData.count; ++i )
			{
				value.emplace_back( vec[ i ] );
			}
			break;
		}
		default:
		{
			FatalError( "Un-deserializable type %d.", serializedData.type );
			break;
		}
	}
}

std::vector< vkRuna::SerializableData > SerializeInterfaceBlocks( const render::pipelineProg_t &pp )
{
	std::vector< SerializableData > suvVec;
	VkDeviceSize					offset = 0;

	for ( int i = 0; i < pp.interfaceBlocks.size(); ++i )
	{
		const interfaceBlock_t &ib = pp.interfaceBlocks[ i ];

		if ( !ib.HoldsUserVars() )
		{
			continue;
		}

		const byte *uboPtr = g_pipelineManager.GetUBOPtr( pp, i );

		for ( const memberDeclaration_t &uniform : ib.declarations )
		{
			const float *	 f	 = reinterpret_cast< const float * >( uboPtr + offset );
			SerializableData suv = SerializeUserVar( uniform, f );
			suvVec.emplace_back( suv );

			offset += GetMemberTypeByteSize( uniform.type );
		}
	}

	return suvVec;
}

void DeserializeInterfaceBlocks( pipelineProg_t &pp, const std::vector< SerializableData > &suvVec )
{
	std::vector< std::string > userVarNames;
	userVarNames.resize( suvVec.size() );
	std::vector< size_t > userVarByteSizes;
	userVarByteSizes.resize( suvVec.size() );
	std::vector< float > values;
	values.reserve( 4 * suvVec.size() );

	size_t declarationIndex = 0;
	for ( const SerializableData &serializedData : suvVec )
	{
		DeserializeUserVar( serializedData,
							userVarNames[ declarationIndex ],
							userVarByteSizes[ declarationIndex ],
							values );
		++declarationIndex;
	}

	std::vector< const char * > userVarNamesPtrs;
	userVarNamesPtrs.reserve( userVarNames.size() );
	for ( const std::string &varName : userVarNames )
	{
		userVarNamesPtrs.emplace_back( varName.c_str() );
	}

	g_pipelineManager.UpdateUBOs( pp,
								  userVarNamesPtrs.size(),
								  userVarNamesPtrs.data(),
								  userVarByteSizes.data(),
								  values.data() );
}

vkRuna::render::pipelineStatus_t pipelineProg_t::GetStatus() const
{
	return status;
}

} // namespace render

} // namespace vkRuna
