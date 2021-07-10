// Copyright (c) 2021 Arno Galvez

#include "ShaderLexer.h"

#include "platform/Sys.h"
#include "renderer/Check.h"
#include "renderer/RenderProgs.h"
#include "renderer/VFX.h"

#include <array>
#include <iostream>
#include <thread>

// CE: Custom Expression
#define CE_BEG	   "${beg"
#define CE_END	   "end}"
#define CE_REG_BEG "\\$\\s*\\{\\s*beg\\s+"
#define CE_REG_END "\\s*end\\s*\\}"

#define CE_NAME					 "\\s*\\w+\\s*"
#define CE_ASSIGNMENT			 "\\s*\\w+\\s*\\=\\s*\\w+\\s*"
#define CE_NAME_COMMA			 CE_NAME "\\s*,\\s*"
#define CE_ASSIGNMENT_COMMA		 CE_ASSIGNMENT "\\s*,\\s*"
#define CE_MEMBER_DECLARATION	 "\\s*\\w+\\s+[a-zA-Z_]\\w*(\\[\\]|)\\s*;\\s*"
#define CE_MEMBER_DECLARATION_CG "\\s*(\\w+)\\s+([a-zA-Z_]\\w*)(\\[\\]|)\\s*;\\s*"
#define CE_LAYOUT_DECL                         \
	"(layout\\s*\\("                           \
	"((" CE_NAME_COMMA "|" CE_ASSIGNMENT_COMMA \
	")*"                                       \
	"(" CE_NAME "|" CE_ASSIGNMENT              \
	"))"                                       \
	"\\)|)\\s*"

#define CE_FLAGS       \
	"(\\["             \
	"((" CE_NAME_COMMA \
	")*"               \
	"(" CE_NAME        \
	"))"               \
	"\\]|)\\s*"

namespace vkRuna
{
using namespace render;
using namespace sys;

ShaderLexer		 g_shaderLexer;
interfaceBlock_t GlobalsTokenizer::ib;

static const std::array< const std::string, 3 >		   VALID_BINDING_TYPES = { "uniform", "buffer", "sampler2D" };
static const std::array< const std::string, MT_COUNT > USER_TYPES = { "vec4", "color", "mat4", "float[]", "int[]" };
static const std::array< const std::string, MT_COUNT > USER_TO_NATIVE_TYPES				= { "vec4",
																				"vec4",
																				"mat4",
																				"float",
																				"int" };
static const std::array< const std::string, MT_COUNT > USER_TO_NATIVE_TYPES_NAME_SUFFIX = { "", "", "", "[]", "[]" };

enum captureGroup_t
{
	CG_RES_FLAGS						 = 2,
	CG_RES_LAYOUT_ARGS					 = 6,
	CG_RES_INTERFACE_BLOCK_LAYOUT_SUFFIX = 9,
	CG_RES_INTERFACE_BLOCK_NAME			 = 10,
	CG_RES_MEMBER_DECL					 = 11,

	CG_RES_SAMPLER_LAYOUT_SUFFIX = 9,
	CG_RES_SAMPLER_NAME			 = 10,

	CG_RES_USER_DECL = 5,

	CG_RES_MEMBER_DECL_TYPE	  = 1,
	CG_RES_MEMBER_DECL_NAME	  = 2,
	CG_RES_MEMBER_DECL_BUFFER = 3,
};

static const std::regex CE_DECL_REGEX( CE_REG_BEG "([\\S\\s]*?)" CE_REG_END, std::regex::optimize );

std::regex ResourceExprTokenizer::UBO_REGEX( CE_FLAGS CE_LAYOUT_DECL
											 "(uniform\\s+"
											 "(\\w+)\\s+"
											 "\\{"
											 "((" CE_MEMBER_DECLARATION
											 ")+)"
											 "\\s*\\}"
											 "\\s*;);*",
											 std::regex::optimize );

std::regex ResourceExprTokenizer::BUFFER_REGEX( CE_FLAGS CE_LAYOUT_DECL
												"(buffer\\s+"
												"(\\w+)\\s+"
												"\\{"
												"((" CE_MEMBER_DECLARATION
												")+)"
												"\\s*\\}"
												"\\s*;);*",
												std::regex::optimize );

std::regex ResourceExprTokenizer::SAMPLER_2D_REGEX( CE_FLAGS CE_LAYOUT_DECL "(uniform\\s+sampler2D\\s+(\\w+)\\s*;);*",
													std::regex::optimize );

std::regex ResourceExprTokenizer::USER_VARS_REGEX( CE_FLAGS "((" CE_MEMBER_DECLARATION ")+)", std::regex::optimize );

bool ExtractBindingType( const std::string &s, bindingType_t &bindingType )
{
	auto it = std::find( VALID_BINDING_TYPES.cbegin(), VALID_BINDING_TYPES.cend(), s );
	if ( it == VALID_BINDING_TYPES.cend() )
	{
		Error( "unknown binding type %s", s.c_str() );
		return false;
	}

	bindingType = static_cast< bindingType_t >( it - VALID_BINDING_TYPES.cbegin() );
	return true;
}

ibFlags_t ExtractFlags( std::string flagsList )
{
	const std::regex flags_regex( "(\\w+)" );

	uint16_t	flags = IBF_NONE;
	std::smatch sm;
	while ( std::regex_search( flagsList, sm, flags_regex ) )
	{
		if ( sm[ 1 ].str() == "private" )
		{
			flags |= IBF_HIDDEN;
		}

		flagsList = sm.suffix();
	}

	return static_cast< ibFlags_t >( flags );
}

bool ExtractMemberDeclarations( std::string list, interfaceBlock_t &ib )
{
	const std::regex member_declaration_regex( CE_MEMBER_DECLARATION_CG, std::regex::optimize );

	std::smatch sm;
	while ( std::regex_search( list, sm, member_declaration_regex ) )
	{
		memberDeclaration_t declaration;

		std::string type = sm[ CG_RES_MEMBER_DECL_TYPE ];
		if ( sm.length( CG_RES_MEMBER_DECL_BUFFER ) != 0 )
		{
			type += "[]";
		}
		std::string name = sm[ CG_RES_MEMBER_DECL_NAME ];

		auto it = std::find( USER_TYPES.cbegin(), USER_TYPES.cend(), type );
		if ( it == USER_TYPES.cend() )
		{
			Error( "When parsing: unknown type %s", type.c_str() );
			return false;
		}

		declaration.type = static_cast< memberType_t >( it - USER_TYPES.cbegin() );
		declaration.name = name;

		ib.declarations.emplace_back( declaration );
		// ib.byteSize += ShaderLexer::GetMemberTypeByteSize( declaration.type );

		list = sm.suffix();
	}

	return true;
}

int UpdateLayout( char *buff, size_t size, const char *layoutArgs, int set, int binding )
{
	int writeSize = std::snprintf( nullptr,
								   0,
								   "\nlayout (%s%sset = %d, binding = %d) ",
								   layoutArgs,
								   ( std::strlen( layoutArgs ) != 0 ) ? "," : "",
								   set,
								   binding );
	writeSize += 1;

	if ( writeSize > size )
	{
		return -1;
	}
	return std::snprintf( buff,
						  size,
						  "\nlayout (%s%sset = %d, binding = %d) ",
						  layoutArgs,
						  ( std::strlen( layoutArgs ) != 0 ) ? "," : "",
						  set,
						  binding );
}

void AddShaderCode( const char *shaderCodeFilePath, std::string &out )
{
	try
	{
		out += ReadFile( shaderCodeFilePath );
		out += "\n\n";
	}
	catch ( const std::ios::failure &e )
	{
		FatalError( e.what() );
	}
}

const char *ShaderLexer::MemberTypeToStr( memberType_t memberType )
{
	return USER_TYPES[ memberType ].c_str();
}

void ShaderLexer::Init()
{
	g_pipelineManager.AddSharedInterfaceBlock( GlobalsTokenizer::GetInterfaceBlock() );
}

void ShaderLexer::Shutdown() {}

NO_DISCARD bool ShaderLexer::Parse( std::string &									   shaderCode,
									size_t											   tokenizersCount,
									std::unique_ptr< ShaderTokenizer > *			   tokenizers,
									std::vector< std::unique_ptr< ShaderTokenizer > > &out,
									bool errorOnExpressionNotFound /*= false */ )
{
	try
	{
		std::smatch ce_sm;

		for ( ; std::regex_search( shaderCode, ce_sm, CE_DECL_REGEX ); shaderCode = ce_sm.suffix() /*, r[ 0 ] = '\0'*/ )
		{
			{
				std::unique_ptr< PassthroughTokenizer > pt( new PassthroughTokenizer );
				pt->Scan( ce_sm.prefix().str() );
				out.emplace_back( std::move( pt ) );
			}

			std::string expr		= ce_sm[ 1 ];
			bool		exprMatched = false;
			for ( size_t i = 0; i < tokenizersCount; ++i )
			{
				auto &tokenizer = tokenizers[ i ];
				if ( tokenizer->Scan( expr ) )
				{
					auto newTokenizer = tokenizer->NewInstance();
					out.emplace_back( std::move( tokenizer ) );
					tokenizer = std::move( newTokenizer );

					exprMatched = true;
					break;
				}
			}

			if ( errorOnExpressionNotFound && !exprMatched )
			{
				Error( "When parsing: unknown custom block:\n%s", expr.c_str() );
				return false;
			}

			if ( !exprMatched )
			{
				std::unique_ptr< PassthroughTokenizer > pt( new PassthroughTokenizer );
				pt->Scan( ce_sm.str( 0 ) );
				out.emplace_back( std::move( pt ) );
			}
		}

		{
			std::unique_ptr< PassthroughTokenizer > pt( new PassthroughTokenizer );
			pt->Scan( std::move( shaderCode ) );
			out.emplace_back( std::move( pt ) );
		}
	}
	catch ( std::regex_error &e )
	{
		Error( e.what() );
		return false;
	}

	return true;
}

// bool ShaderLexer::ParseAllExpr( std::string &shaderCode, std::vector< std::unique_ptr< ShaderTokenizer > > &out )
//{
//	std::array< std::unique_ptr< ShaderTokenizer >, 3 > exprTorkenizers = {
//		std::make_unique< ResourceExprTokenizer >(),
//		std::make_unique< GlobalsTokenizer >(),
//		std::make_unique< ComputeShaderOptsTokenizer >()
//	};
//
//	return ParseSpecific( shaderCode, static_cast< int >( exprTorkenizers.size() ), exprTorkenizers.data(), out, true );
//}

void ShaderLexer::Combine( const std::vector< std::unique_ptr< ShaderTokenizer > > &tokenizers, std::string &out )
{
	std::string r;
	for ( auto &st : tokenizers )
	{
		st->Evaluate( r );
		out += r;
		r.clear(); // #TODO: memsetzero instead
	}
}

bool ResourceExprTokenizer::Scan( const std::string &text )
{
	std::smatch sm;
	if ( std::regex_match( text, sm, UBO_REGEX ) )
	{
		m_ib.flags = ExtractFlags( sm[ CG_RES_FLAGS ] );
		m_ib.type  = BT_UBO;
		m_ib.name  = sm[ CG_RES_INTERFACE_BLOCK_NAME ];
		if ( !ExtractMemberDeclarations( sm[ CG_RES_MEMBER_DECL ].str(), m_ib ) )
		{
			return false;
		}

		m_declarationsBlock = sm[ CG_RES_INTERFACE_BLOCK_LAYOUT_SUFFIX ].str();
		m_layoutArgs		= sm[ CG_RES_LAYOUT_ARGS ].str();
		return true;
	}

	if ( std::regex_match( text, sm, BUFFER_REGEX ) )
	{
		m_ib.flags = ExtractFlags( sm[ CG_RES_FLAGS ] );
		m_ib.type  = BT_BUFFER;
		m_ib.name  = sm[ CG_RES_INTERFACE_BLOCK_NAME ];
		if ( !ExtractMemberDeclarations( sm[ CG_RES_MEMBER_DECL ].str(), m_ib ) )
		{
			return false;
		}

		if ( m_ib.declarations.size() != 1 )
		{
			Error( "Parsing error: buffer interface blocks must declare a single float/int array." );
			return false;
		}

		memberType_t type = m_ib.declarations[ 0 ].type;
		if ( !( type == MT_FLOAT_BUFFER || type == MT_INT_BUFFER ) )
		{
			Error( "Parsing error: buffer interface blocks must declare a single float/int array." );
			return false;
		}

		m_declarationsBlock = sm[ CG_RES_INTERFACE_BLOCK_LAYOUT_SUFFIX ].str();
		m_layoutArgs		= sm[ CG_RES_LAYOUT_ARGS ].str();
		return true;
	}

	if ( std::regex_match( text, sm, SAMPLER_2D_REGEX ) )
	{
		m_ib.flags = ExtractFlags( sm[ CG_RES_FLAGS ] );
		m_ib.type  = BT_SAMPLER2D;
		m_ib.name  = sm[ CG_RES_SAMPLER_NAME ];

		m_declarationsBlock = sm[ CG_RES_SAMPLER_LAYOUT_SUFFIX ];
		m_layoutArgs		= sm[ CG_RES_LAYOUT_ARGS ].str();
		return true;
	}

	if ( std::regex_match( text, sm, USER_VARS_REGEX ) )
	{
		m_ib.flags = ExtractFlags( sm[ CG_RES_FLAGS ] );
		m_ib.type  = BT_UBO;
		m_ib.name  = "_userVariables";

		if ( !ExtractMemberDeclarations( sm[ CG_RES_USER_DECL ], m_ib ) )
		{
			return false;
		}

		m_declarationsBlock = "uniform ";
		m_declarationsBlock += m_ib.name;
		m_declarationsBlock += " {\n\t" + sm[ CG_RES_USER_DECL ].str() + "\n};";

		return true;
	}

	return false;
}

bool ResourceExprTokenizer::Evaluate( std::string &out )
{
	std::array< char, 2048 > buff;

	out += "//////// Var Begin ////////";

	int writeLen =
		UpdateLayout( buff.data(), buff.size(), m_layoutArgs.c_str(), BindingTypeToDescSet( m_ib.type ), m_ib.binding );

	if ( writeLen < 0 )
	{
		FatalError( "Error during ResourceExpr evaluation: write size too long." );
	}

	out += buff.data();
	// #TODO
	// out += m_declarationsBlock;
	out += VALID_BINDING_TYPES[ m_ib.type ];
	out += " ";
	out += m_ib.name;
	out += " {\n";
	for ( const memberDeclaration_t &md : m_ib.declarations )
	{
		out += '\t' + USER_TO_NATIVE_TYPES[ md.type ] + ' ' + md.name + USER_TO_NATIVE_TYPES_NAME_SUFFIX[ md.type ] +
			   ";\n";
	}
	out += "};\n";

	out += "//////// Var end ////////\n";

	return true;
}

bool AddUBODeclaration( const char *uboName, const interfaceBlock_t &ib, std::string &out )
{
	// fill everything using ib
	std::string declarations;
	declarations.reserve( 64 );
	for ( const memberDeclaration_t &md : ib.declarations )
	{
		declarations += "\t" + USER_TO_NATIVE_TYPES[ md.type ] + " " + md.name +
						USER_TO_NATIVE_TYPES_NAME_SUFFIX[ md.type ] + ";\n";
	}

	std::array< char, 512 > buff;

	std::snprintf( buff.data(),
				   buff.size(),
				   "layout (set = %d, binding = %d) uniform _%s_ {\n%s} %s;\n",
				   BindingTypeToDescSet( ib.type ),
				   ib.binding,
				   uboName,
				   declarations.c_str(),
				   ib.name.c_str() );

	out += buff.data();

	return true;
}

std::regex GlobalsTokenizer::scanRegex( "\\s*globals\\s*", std::regex::optimize | std::regex::icase );

const char *GlobalsTokenizer::FUNCTIONS_PATH = "shaderGen/GlobalsFunctions.glsl";

bool GlobalsTokenizer::Scan( const std::string &text )
{
	return std::regex_match( text, scanRegex );
}
bool GlobalsTokenizer::Evaluate( std::string &out )
{
	out += "//////// Globals Begin ////////\n\n";
	bool ret = AddUBODeclaration( GetUBOName(), ib, out );
	AddShaderCode( FUNCTIONS_PATH, out );
	out += "\n\n//////// Globals End ////////\n";

	return ret;
}

const vkRuna::interfaceBlock_t &GlobalsTokenizer::GetInterfaceBlock()
{
	ib.flags = IBF_HIDDEN;
	ib.type	 = BT_SHARED_UBO;
	ib.name	 = GetUBOName();
	// ib.byteSize = 2 * ShaderLexer::GetMemberTypeByteSize( MT_MAT4 ) + ShaderLexer::GetMemberTypeByteSize( MT_VEC4 );

	ib.declarations.resize( 4 );

	memberDeclaration_t *md;

	md		 = &ib.declarations[ 0 ];
	md->type = MT_MAT4;
	md->name = GetProjStr();

	md		 = &ib.declarations[ 1 ];
	md->type = MT_MAT4;
	md->name = GetViewStr();

	md		 = &ib.declarations[ 2 ];
	md->type = MT_VEC4;
	md->name = GetDeltaFrameStr();

	md		 = &ib.declarations[ 3 ];
	md->type = MT_VEC4;
	md->name = GetTimeStr();

	return ib;
}

const std::regex VFXTokenizer::VFX_DEFINITIONS_REGEX( "\\s*VFX definitions\\s*",
													  std::regex::optimize | std::regex::icase );
const std::regex VFXTokenizer::VFX_MAIN_REGEX( "\\s*VFX main\\s*", std::regex::optimize | std::regex::icase );

const char *VFXTokenizer::COMMON_CODE_PATH = "shaderGen/VFXCommonCode.glsl";

const char *VFXTokenizer::COMPUTE_CODE_PATH = "shaderGen/VFXComputeCode.glsl";
const char *VFXTokenizer::COMPUTE_MAIN_PATH = "shaderGen/VFXComputeMainHead.glsl";

const char *VFXTokenizer::VERTEX_CODE_PATH = "shaderGen/VFXVertexCode.glsl";
const char *VFXTokenizer::VERTEX_MAIN_PATH = "shaderGen/VFXVertexMain.glsl";
const char *VFXTokenizer::VERTEX_QUAD_PATH = "shaderGen/primitives/VFXVertexQuad.glsl";
const char *VFXTokenizer::VERTEX_CUBE_PATH = "shaderGen/primitives/VFXVertexCube.glsl";

const char *VFXTokenizer::FRAGMENT_CODE_PATH = "shaderGen/VFXFragmentCode.glsl";
const char *VFXTokenizer::FRAGMENT_MAIN_PATH = "shaderGen/VFXFragmentMain.glsl";
const char *VFXTokenizer::FRAGMENT_QUAD_PATH = "shaderGen/primitives/VFXFragmentQuad.glsl";
const char *VFXTokenizer::FRAGMENT_CUBE_PATH = "shaderGen/primitives/VFXFragmentCube.glsl";

void VFXTokenizer::GetBufferInterfaceBlockName( const char *bufferName, char *out, int outSize )
{
	std::snprintf( out, outSize, "_%sBuffer", bufferName );
}

VFXTokenizer::VFXTokenizer( render::VFX *vfx, shaderStage_t stage )
	: m_vfx( vfx )
	, m_stage( stage )
{
}

void VFXTokenizer::AddIncludeDirectives( std::string &out )
{
	out += "#include \"rand.glsl\"\n\n";
}

void VFXTokenizer::AddBuffersDefinitions( std::string &out )
{
	std::array< char, 256 > buff;

	const char *fmt = CE_BEG " [private] buffer _%sBuffer { %s _%s[]; }; " CE_END "\n";

	for ( int i = 0; i < m_vfx->m_attributesCount; ++i )
	{
		const VFX::VFXBuffer_t &vfxBuffer = m_vfx->m_attributesBuffers[ i ];

		std::snprintf( buff.data(),
					   buff.size(),
					   fmt,
					   vfxBuffer.name,
					   VFX::TypeIndexToStr( vfxBuffer.dataType ),
					   vfxBuffer.name );
		out += buff.data();
	}
}

void VFXTokenizer::AddUBODefinition( std::string &out )
{
	std::array< char, 512 > buff;

	const char *fmt = "\n" CE_BEG " [private] uniform _vfxUBO {\n%s\n}; " CE_END "\n";

	char uboDecl[ buff.size() ];
	m_vfx->GetUBOMembers( uboDecl, buff.size() );
	std::snprintf( buff.data(), buff.size(), fmt, uboDecl );

	out += buff.data();
}

void VFXTokenizer::AddRevivalCounterDefinition( std::string &out )
{
	std::array< char, 256 > buff;

	const char *fmt = CE_BEG " [private] buffer _%sBuffer { %s %s[]; }; " CE_END;

	char revivalCounterName[ 128 ];
	VFX::GetRevivalCounterName( revivalCounterName, 128 );
	std::snprintf( buff.data(), buff.size(), fmt, revivalCounterName, "int", revivalCounterName );

	out += buff.data();
}

bool VFXTokenizer::Scan( const std::string &text )
{
	if ( std::regex_match( text, VFX_DEFINITIONS_REGEX ) )
	{
		m_match = DEFINITIONS;
		return true;
	}

	if ( std::regex_match( text, VFX_MAIN_REGEX ) )
	{
		m_match = MAIN;
		return true;
	}

	return false;
}

bool VFXTokenizer::Evaluate( std::string &out )
{
	if ( !m_vfx )
	{
		FatalError( "When evaluating VFXTokenizer: No VFX attached." );
	}

	if ( m_match == DEFINITIONS )
	{
		out += "//////// VFX Definitions Begin ////////\n";

		AddIncludeDirectives( out );
		AddBuffersDefinitions( out );
		AddRevivalCounterDefinition( out );
		AddUBODefinition( out );
		AddParticleStructDefinition( out );
		AddCommonFunctions( out );

		switch ( m_stage )
		{
			case SS_VERTEX:
			{
				AddVertexShaderDefinitions( out );
				break;
			}
			case SS_FRAGMENT:
			{
				AddFragmentShaderDefinitions( out );
				break;
			}
			case SS_COMPUTE:
			{
				AddComputeShaderDefinitions( out );
				break;
			}
			default:
			{
				CHECK_PRED( false );
				return false;
			}
		}

		out += "//////// VFX Definitions End ////////\n";

		return true;
	}

	if ( m_match == MAIN )
	{
		out += "//////// VFX Main Begin ////////\n\n";

		switch ( m_stage )
		{
			case SS_VERTEX:
			{
				AddVertexShaderMain( out );
				break;
			}
			case SS_FRAGMENT:
			{
				AddFragmentShaderMain( out );
				break;
			}
			case SS_COMPUTE:
			{
				AddComputeShaderMain( out );
				break;
			}
			default:
			{
				CHECK_PRED( false );
				return false;
			}
		}

		out += "\n//////// VFX Main End ////////\n";

		return true;
	}

	return false;
}

void VFXTokenizer::AddParticleStructDefinition( std::string &out )
{
	out += "struct Particle_t\n{\n";

	for ( int i = 0; i < m_vfx->m_attributesCount; ++i )
	{
		const VFX::VFXBuffer_t &vfxBuffer = m_vfx->m_attributesBuffers[ i ];
		std::string				vfxBufferType;
		vfxBuffer.GetGLSLType( vfxBufferType );
		out += '\t' + vfxBufferType + ' ' + vfxBuffer.name + ";\n";
	}

	out += "};\n\n";
}

void VFXTokenizer::AddReadParticleAttributesFunc( std::string &out )
{
	out += "void ReadParticleAttributes(out Particle_t particle) {\n";
	out += "\tconst uint id = GetParticleID();\n";

	for ( int i = 0; i < m_vfx->m_attributesCount; ++i )
	{
		const VFX::VFXBuffer_t &vfxBuffer = m_vfx->m_attributesBuffers[ i ];

		for ( int j = 0; j < vfxBuffer.arity; ++j )
		{
			std::string vfxBufferElt = "_";
			vfxBufferElt += vfxBuffer.name;
			vfxBufferElt += '[' + std::to_string( vfxBuffer.arity ) + " * id + " + std::to_string( j ) + "]";

			char component = 0;
			switch ( j )
			{
				case 0: component = 'x'; break;
				case 1: component = 'y'; break;
				case 2: component = 'z'; break;
				case 3: component = 'w'; break;
				default:
				{
					FatalError( "Buffer arity was %d, expected value in range [1, 4].", vfxBuffer.arity );
					break;
				}
			}

			out += "\tparticle.";
			out += vfxBuffer.name;
			out += '.';
			out += component;
			out += " = ";
			out += vfxBufferElt;
			out += ";\n";
		}
	}

	out += "}\n\n";
}

void VFXTokenizer::AddCommonFunctions( std::string &out )
{
	AddShaderCode( COMMON_CODE_PATH, out );
}

void VFXTokenizer::AddComputeShaderDefinitions( std::string &out )
{
	AddShaderCode( COMPUTE_CODE_PATH, out );
}

void VFXTokenizer::AddComputeShaderMain( std::string &out )
{
	std::string mainHead = "void main()\n{\n";
	AddShaderCode( COMPUTE_MAIN_PATH, mainHead );

	std::string mainTail;

	for ( int i = 0; i < m_vfx->m_attributesCount; ++i )
	{
		const VFX::VFXBuffer_t &vfxBuffer = m_vfx->m_attributesBuffers[ i ];

		mainTail += "\t{\n";

		std::string vfxBufferType;
		vfxBuffer.GetGLSLType( vfxBufferType );

		mainTail += "\t\t" + vfxBufferType + " _attribute = mix(updatedParticle." + vfxBuffer.name + ", initParticle." +
					vfxBuffer.name + ", UpdateOrInit);\n";

		for ( int j = 0; j < vfxBuffer.arity; ++j )
		{
			std::string vfxBufferElt = "_";
			vfxBufferElt += vfxBuffer.name;
			vfxBufferElt += '[' + std::to_string( vfxBuffer.arity ) + " * id + " + std::to_string( j ) + "]";

			char component = 0;
			switch ( j )
			{
				case 0: component = 'x'; break;
				case 1: component = 'y'; break;
				case 2: component = 'z'; break;
				case 3: component = 'w'; break;
				default:
				{
					FatalError( "Buffer arity was %d, expected value in range [1, 4].", vfxBuffer.arity );
					break;
				}
			}

			mainTail += "\t\t";
			mainTail += vfxBufferElt + " = _attribute.";
			mainTail += component;
			mainTail += ";\n";
		}

		mainTail += "\t}\n";
	}

	mainTail += "}\n";

	std::string updateLifeFunc =
		"void UpdateParticleLife(inout Particle_t particle) {\n"
		"\tparticle.life = "
		"particle.life - mix( 0.0, globals.deltaFrame.x, float( particle.life > 0.0 ) );\n"
		"}\n\n";

	std::string readParticleAttributesFunc;
	AddReadParticleAttributesFunc( readParticleAttributesFunc );

	out += readParticleAttributesFunc;
	out += updateLifeFunc;
	out += mainHead;
	out += mainTail;
}

void VFXTokenizer::AddVertexShaderDefinitions( std::string &out )
{
	static_assert( VFX_RP_COUNT == 2, "Unhandled vfx render primitive." );

	switch ( m_vfx->m_renderPrimitive )
	{
		case VFX_RP_QUAD:
		{
			AddShaderCode( VERTEX_QUAD_PATH, out );
			break;
		}
		case VFX_RP_CUBE:
		{
			AddShaderCode( VERTEX_CUBE_PATH, out );
			break;
		}
		default:
		{
			Error( "Unknown VFX render primitive." );
			break;
		}
	}

	AddShaderCode( VERTEX_CODE_PATH, out );
	AddReadParticleAttributesFunc( out );
}

void VFXTokenizer::AddVertexShaderMain( std::string &out )
{
	AddShaderCode( VERTEX_MAIN_PATH, out );
}

void VFXTokenizer::AddFragmentShaderDefinitions( std::string &out )
{
	static_assert( VFX_RP_COUNT == 2, "Unhandled vfx render primitive." );

	switch ( m_vfx->m_renderPrimitive )
	{
		case VFX_RP_QUAD:
		{
			AddShaderCode( FRAGMENT_QUAD_PATH, out );
			break;
		}
		case VFX_RP_CUBE:
		{
			AddShaderCode( FRAGMENT_CUBE_PATH, out );
			break;
		}
		default:
		{
			Error( "Unknown VFX render primitive." );
			break;
		}
	}
	AddShaderCode( FRAGMENT_CODE_PATH, out );
}

void VFXTokenizer::AddFragmentShaderMain( std::string &out )
{
	AddShaderCode( FRAGMENT_MAIN_PATH, out );
}

std::regex ComputeShaderOptsTokenizer::scanRegex( "\\s*compute\\s*options\\s*",
												  std::regex::optimize | std::regex::icase );

bool ComputeShaderOptsTokenizer::Scan( const std::string &text )
{
	return std::regex_match( text, scanRegex );
}

bool ComputeShaderOptsTokenizer::Evaluate( std::string &out )
{
	std::array< char, 256 > buff;
	std::snprintf( buff.data(),
				   buff.size(),
				   "layout (local_size_x = %d, local_size_y = %d, local_size_z = %d) in;",
				   render::COMPUTE_GROUP_SIZE_X,
				   render::COMPUTE_GROUP_SIZE_Y,
				   render::COMPUTE_GROUP_SIZE_Z );

	out += buff.data();

	return true;
}

} // namespace vkRuna
