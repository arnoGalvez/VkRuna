#include "math.glsl"

const float PI = 3.1415926535;

layout (location = 0) flat out int id;
layout (location = 1) out vec3 worldPosition;
// layout (location = 2) out vec3 viewDir;
// layout (location = 3) out vec3 normalInterp;

vec4 VertexMain(Particle_t particle)
{
	const float cubeSize = 1.0;
	const float invCapacity = 1.0 / GetCapacity();
	const float shapeHeight = 3.0;
	const vec4 vertex = GetVertex();
	
	float wave = 0.9 * sin ( 2.0 * PI * ((particle.position.z) + 0.5) + GetTime() * 0.9 ) + 1.8;
	float h = particle.position.z / 3.0;
	wave *= clamp( min(Warp(h, -2.0), Warp(1.0 - h, -2.0)), 0.0, 1.0);
	vec3 scaling = vec3(0.5, 0.5, shapeHeight / (GetCapacity() - 1.0)) * vec3( vec2( wave ), 1.0 );
	
	mat4 TR = RotateZ(GetTime() * 0.5 + 2.0 * PI * particle.position.z);
	SetTranslation(TR, particle.position);
	mat4 S = Scale(scaling);
	mat4 TRS = TR * S;

	vec4 worldPosition4 = TRS * vertex;
	vec4 viewPosition4 = globals.v * worldPosition4;
	vec4 srceePosition = globals.p * viewPosition4;

	id = GetParticleID();
	worldPosition = worldPosition4.xyz;
	// mat3 cameraToWorld = transpose( mat3( globals.v ) );
	// vec3 cameraWorldPos = -(cameraToWorld * globals.v[3].xyz);
	// viewDir = worldPosition - cameraWorldPos;

	// mat3 normalTransform = mat3(TR);
	// normalInterp = normalTransform * GetNormal();

	return srceePosition;
}