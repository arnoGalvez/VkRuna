//////// Cube Primitive Begin ////////

#define PRIMITIVE_CUBE

layout (location = 16) out vec3 _cubeVertOffset;
layout (location = 17) out flat int _cubeFaceId;

int GetParticleID()
{
	const int vertId = gl_VertexIndex;
	const int particleId = (vertId >> 3) + gl_InstanceIndex;
	return particleId;
}

vec4 GetVertex()
{
	const int vertId = gl_VertexIndex;

	vec3 offset;
	offset.x = (vertId & 4) >> 2;
	offset.y = (vertId & 2) >> 1;
	offset.z = vertId & 1;
	offset -= 0.5;

	return vec4(offset, 1.0);
}

vec2 GetUV()
{
	return vec2(0);
}

vec3 GetNormal()
{
	const int vertId = gl_VertexIndex;

	vec3 offset;
	offset.x = (vertId & 4) >> 2;
	offset.y = (vertId & 2) >> 1;
	offset.z = vertId & 1;
	offset -= 0.5;

	const float invSqrt075 = 1.1547005383792515290182975610039;

	return offset * invSqrt075;
}

void PerPrimitiveFunc()
{
	const int vertId = gl_VertexIndex;
	_cubeVertOffset = GetVertex().xyz + vec3(0.5);
	_cubeFaceId = (vertId & 7) % 3;
	//_cubeFaceId = (vertId & 7) % 5;
}


//////// Cube Primitive End ////////
