// Copyright (c) 2021 Arno Galvez

#pragma once

#include "external/cereal/cereal.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace vkRuna
{
enum ibFlags_t : uint16_t
{
	IBF_NONE	= 0,
	IBF_HIDDEN = 1u << 0
};

enum bindingType_t : uint16_t
{
	BT_UBO,
	BT_BUFFER,
	BT_SAMPLER2D,
	BT_SHARED_UBO,
	BT_COUNT,
	BT_UNKNOWN
};

// As of now, in a ubo, only vec4 and mat4 are supported, because of problematic memory alignments.
// (Don't blame me, even idTech 4.5 only supports vec4 !)
enum memberType_t
{
	MT_VEC4,
	MT_COLOR,
	MT_MAT4,
	MT_FLOAT_BUFFER,
	MT_INT_BUFFER,
	MT_COUNT,
	MT_UNKNOWN
};

uint32_t GetMemberTypeByteSize( memberType_t memberType );

struct memberDeclaration_t
{
	memberType_t type = memberType_t::MT_UNKNOWN;
	std::string	 name;

	bool operator==( const memberDeclaration_t &rhs ) const { return type == rhs.type && name == rhs.name; }
};

//#pragma warning( error : 4820 )
struct interfaceBlock_t
{
	ibFlags_t	  flags	  = IBF_NONE;
	bindingType_t type	  = BT_UNKNOWN;
	uint32_t	  binding = UINT32_MAX;
	// uint32_t						   byteSize = 0;
	std::vector< memberDeclaration_t > declarations;
	std::string						   name;

	bool	 HoldsUserVars() const { return type == BT_UBO && !( flags & IBF_HIDDEN ); }
	uint32_t GetByteSize() const;
};
//#pragma warning( disable : 4820 )

enum shaderStage_t : uint32_t
{
	SS_VERTEX	= 0,
	SS_FRAGMENT = 1,
	SS_COMPUTE	= 2,
	SS_COUNT	= 3,

	SS_VERTEX_BIT	= 1u << 0,
	SS_FRAGMENT_BIT = 1u << 1,
	SS_COMPUTE_BIT	= 1u << 2,

	SS_ALL_GRAPHICS,
	SS_ALL,

	SS_UNKNOWN = ~0u
};

const char *EnumToString( shaderStage_t stage );
const char *GetExtensionList( shaderStage_t stage );

//#pragma warning( error : 4820 )
struct shader_t
{
   public:
	shader_t() = default;
	~shader_t();

	void UpdateModule( const char *spvFile );
	void DestroyModule();

	bool IsValid() { return module != nullptr; }

	void *		  module = nullptr;
	std::string	  path {};
	shaderStage_t stage = SS_UNKNOWN;
};
//#pragma warning( disable : 4820 )

template< class Archive >
void serialize( Archive &ar, shader_t &obj )
{
	ar( ::cereal::make_nvp( "name", obj.path ), ::cereal::make_nvp( "stage", obj.stage ) );
}

} // namespace vkRuna
