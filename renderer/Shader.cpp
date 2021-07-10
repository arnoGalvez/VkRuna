// Copyright (c) 2021 Arno Galvez

#include "renderer/Shader.h"

#include "platform/defines.h"

#include <array>

namespace vkRuna
{
static const std::array< uint32_t, MT_COUNT > NATIVE_TYPES_SIZES = { 4 * sizeof( float ),
																	 4 * sizeof( float ),
																	 16 * sizeof( float ),
																	 0,
																	 0 };

const char *vkRuna::EnumToString( shaderStage_t stage )
{
	switch ( stage )
	{
		case SS_VERTEX: return "Vertex shader";
		case SS_FRAGMENT: return "Fragment shader";
		case SS_COMPUTE: return "Compute shader";

		default: return "Unknown shader stage";
	}
}

uint32_t GetMemberTypeByteSize( memberType_t memberType )
{
	return NATIVE_TYPES_SIZES[ memberType ];
}

const char *GetExtensionList( shaderStage_t stage )
{
	switch ( stage )
	{
		case SS_VERTEX: return ".vert";
		case SS_FRAGMENT: return ".frag";
		case SS_COMPUTE: return ".comp";

		default: return ".*";
	}
}

uint32_t interfaceBlock_t::GetByteSize() const
{
	uint32_t byteSize = 0;

	for ( const memberDeclaration_t &md : declarations )
	{
		byteSize += GetMemberTypeByteSize( md.type );
	}

	return byteSize;
}

} // namespace vkRuna
