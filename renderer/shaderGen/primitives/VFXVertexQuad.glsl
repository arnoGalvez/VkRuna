//////// Quad Primitive Begin ////////

#define PRIMITIVE_QUAD

layout (location = 16) out vec2 _quadUV;

int GetParticleID()
{
	const int vertId = gl_VertexIndex;
	const int particleId = (vertId >> 2) + gl_InstanceIndex;
	return particleId;
}

vec4 GetVertex()
{
	const int vertId = gl_VertexIndex;

	vec3 offset;
	offset.x = 0;
	offset.y = (vertId & 2) >> 1;
	offset.z = vertId & 1;
	offset -= 0.5;

	return vec4(offset, 1.0);
}

vec2 GetUV()
{
	const int vertId = gl_VertexIndex;

	vec2 uv;
	uv.x = (vertId & 2) >> 1;
	uv.y = vertId & 1;

	return uv;
}

vec3 GetNormal()
{
	return vec3(1.0, 0.0, 0.0);
}

void PerPrimitiveFunc()
{
	_quadUV = GetUV();
}

//////// Quad Primitive End ////////
