#include "math.glsl"

const float PI = 3.1415926535;

layout (location = 0) out vec3 diffuse;

vec4 VertexMain(Particle_t particle)
{
	const vec4 vertex =  GetVertex();
	const float cubeLength = round(pow(GetCapacity(), 0.3333333));
	const float scale = 3.0 / cubeLength;

	mat4 TR = mat4(1.0);
	SetTranslation(TR, scale * particle.position);

	mat4 S = Scale(vec3(particle.scale * scale));
	mat4 TRS = TR * S;

	vec4 worldPos4 = TRS * vertex;
	vec4 viewPos4 = globals.v * worldPos4;
	vec4 screenPos = globals.p * viewPos4;

	diffuse = particle.diffuse;

	return screenPos;
}