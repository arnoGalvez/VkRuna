// Copyright (c) 2021 Arno Galvez

#pragma once

namespace vkRuna
{
enum vfxRenderPrimitive_t
{
	VFX_RP_QUAD,
	VFX_RP_CUBE,

	VFX_RP_COUNT
};

enum vfxBufferData_t : int8_t
{
	VFX_BD_FLOAT,
	VFX_BD_INT,

	VFX_BD_COUNT
};

const char *EnumToString( vfxRenderPrimitive_t rp );
const char *EnumToString( vfxBufferData_t bd );

} // namespace vkRuna
