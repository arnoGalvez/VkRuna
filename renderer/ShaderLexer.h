// Copyright (c) 2021 Arno Galvez

#pragma once

#include "platform/defines.h"
#include "renderer/Shader.h"

#include <memory>
#include <regex>
#include <string>
#include <vector>

namespace vkRuna
{
namespace render
{
class VFX;
}

enum parsedObjectAction_t : uint32_t
{
	POA_NONE				   = 0,
	POA_BIND_IB_SCOPE_PIPELINE = 1u << 0,
	POA_BIND_SHARED_IB		   = 1u << 1
};

class ShaderTokenizer
{
   public:
	virtual ~ShaderTokenizer() = default;

	virtual std::unique_ptr< ShaderTokenizer > NewInstance() const = 0;

	virtual bool				 Scan( const std::string &text ) = 0;
	virtual bool				 Scan( std::string &&text ) { return false; }
	virtual bool				 Evaluate( std::string &out ) = 0;
	virtual parsedObjectAction_t GetActions()				  = 0;
	virtual void *				 GetActionParams()			  = 0;
};

class ShaderLexer
{
	NO_COPY_NO_ASSIGN( ShaderLexer )

   public:
	static const char *MemberTypeToStr( memberType_t memberType );

   public:
	ShaderLexer() = default;
	void Init();
	void Shutdown();

	NO_DISCARD bool Parse( std::string &									  shaderCode,
								   size_t											  tokenizersCount,
								   std::unique_ptr< ShaderTokenizer > *				  tokenizers,
								   std::vector< std::unique_ptr< ShaderTokenizer > > &out,
								   bool errorOnExpressionNotFound = false );
	// NO_DISCARD bool ParseAllExpr( std::string &shaderCode, std::vector< std::unique_ptr< ShaderTokenizer > > &out );
	void Combine( const std::vector< std::unique_ptr< ShaderTokenizer > > &tokenizers, std::string &out );
};

extern ShaderLexer g_shaderLexer;

class PassthroughTokenizer : public ShaderTokenizer
{
	BASE_CLASS( ShaderTokenizer )

   public:
	~PassthroughTokenizer() = default;
	std::unique_ptr< ShaderTokenizer > NewInstance() const { return std::unique_ptr< PassthroughTokenizer >(); }

	inline bool			 Scan( const std::string &text ) final;
	inline bool			 Scan( std::string &&text ) final;
	inline bool			 Evaluate( std::string &out ) final;
	parsedObjectAction_t GetActions() final { return POA_NONE; }
	void *				 GetActionParams() final { return nullptr; }

   private:
	std::string t;
};

class ResourceExprTokenizer : public ShaderTokenizer
{
	BASE_CLASS( ShaderTokenizer )
   public:
	~ResourceExprTokenizer() = default;
	std::unique_ptr< ShaderTokenizer > NewInstance() const { return std::make_unique< ResourceExprTokenizer >(); }

	bool				 Scan( const std::string &text ) final;
	bool				 Evaluate( std::string &out ) final;
	parsedObjectAction_t GetActions() final { return POA_BIND_IB_SCOPE_PIPELINE; }
	void *				 GetActionParams() final { return &m_ib; }

   private:
	static std::regex UBO_REGEX;
	static std::regex BUFFER_REGEX;
	static std::regex SAMPLER_2D_REGEX;
	static std::regex USER_VARS_REGEX;

	// std::smatch      sm;
	std::string		 m_layoutArgs;
	std::string		 m_declarationsBlock;
	interfaceBlock_t m_ib;
};

class GlobalsTokenizer : public ShaderTokenizer
{
	BASE_CLASS( ShaderTokenizer )
   public:
	static const interfaceBlock_t &GetInterfaceBlock();
	static const char *			   GetUBOName() { return "globals"; }
	static const char *			   GetProjStr() { return "p"; }
	static const char *			   GetViewStr() { return "v"; }
	static const char *			   GetDeltaFrameStr() { return "deltaFrame"; }
	static const char *			   GetTimeStr() { return "time"; }

   public:
	~GlobalsTokenizer() = default;
	std::unique_ptr< ShaderTokenizer > NewInstance() const { return std::make_unique< GlobalsTokenizer >(); }

	bool				 Scan( const std::string &text ) final;
	bool				 Evaluate( std::string &out ) final;
	parsedObjectAction_t GetActions() final { return POA_BIND_SHARED_IB; }
	void *				 GetActionParams() final { return &ib; };

   private:
	static std::regex		scanRegex;
	static interfaceBlock_t ib;
	static const char * FUNCTIONS_PATH;
};

class ComputeShaderOptsTokenizer : public ShaderTokenizer
{
	BASE_CLASS( ShaderTokenizer )

   public:
	~ComputeShaderOptsTokenizer() = default;
	std::unique_ptr< ShaderTokenizer > NewInstance() const { return std::make_unique< ComputeShaderOptsTokenizer >(); }

	bool				 Scan( const std::string &text ) final;
	bool				 Evaluate( std::string &out ) final;
	parsedObjectAction_t GetActions() final { return POA_NONE; }
	void *				 GetActionParams() final { return nullptr; };

   private:
	static std::regex scanRegex;
};

class VFXTokenizer : public ShaderTokenizer
{
   public:
	static void GetBufferInterfaceBlockName( const char *bufferName, char *out, int outSize );

	VFXTokenizer( render::VFX *vfx, shaderStage_t stage );
	std::unique_ptr< ShaderTokenizer > NewInstance() const final
	{
		return std::unique_ptr< VFXTokenizer >( new VFXTokenizer( m_vfx, m_stage ) );
	}
	bool Scan( const std::string &text ) final;
	bool Evaluate( std::string &out ) final;

	parsedObjectAction_t GetActions() final { return POA_NONE; }
	void *				 GetActionParams() final { return nullptr; }

	void SetVFX( render::VFX *vfx ) { m_vfx = vfx; }

   private:
	void AddIncludeDirectives( std::string &out );
	void AddBuffersDefinitions( std::string &out );
	void AddUBODefinition( std::string &out );
	void AddRevivalCounterDefinition( std::string &out );

	void AddParticleStructDefinition( std::string &out );
	void AddReadParticleAttributesFunc( std::string &out );

	void AddCommonFunctions( std::string &out );

	void AddComputeShaderDefinitions( std::string &out );
	void AddComputeShaderMain( std::string &out );

	void AddVertexShaderDefinitions( std::string &out );
	void AddVertexShaderMain( std::string &out );

	void AddFragmentShaderDefinitions( std::string &out );
	void AddFragmentShaderMain( std::string &out );

   private:
	static const std::regex VFX_DEFINITIONS_REGEX;
	static const std::regex VFX_MAIN_REGEX;

	static const char *COMMON_CODE_PATH;

	static const char *COMPUTE_CODE_PATH;
	static const char *COMPUTE_MAIN_PATH;

	static const char *VERTEX_CODE_PATH;
	static const char *VERTEX_MAIN_PATH;
	static const char *VERTEX_QUAD_PATH;
	static const char *VERTEX_CUBE_PATH;

	static const char *FRAGMENT_MAIN_PATH;
	static const char *FRAGMENT_CODE_PATH;
	static const char *FRAGMENT_QUAD_PATH;
	static const char *FRAGMENT_CUBE_PATH;

	enum match_t
	{
		UNKNOWN,
		DEFINITIONS,
		MAIN
	};

	match_t		  m_match = UNKNOWN;
	render::VFX * m_vfx	  = nullptr;
	shaderStage_t m_stage = SS_UNKNOWN;
};

inline bool PassthroughTokenizer::Scan( const std::string &text )
{
	t = text;
	return true;
}

inline bool PassthroughTokenizer::Scan( std::string &&text )
{
	t = text;
	return true;
}

inline bool PassthroughTokenizer::Evaluate( std::string &out )
{
	out = t;
	return true;
}

} // namespace vkRuna
