#include "utils.glsl"
#include "math.glsl"

${beg
	vec4 scale;
	vec4 warpT;
	color colorBeg;
	color colorEnd;
end}

layout (location = 0) out vec4 color;

vec4 VertexMain(Particle_t particle)
{
	mat4 rotation = AlignYAxis(globals.v, particle.position, particle.velocity);
	vec4 vertex = GetVertex();
	vec4 worldPosition = rotation * (GetLife() * vec4(vec3(scale), 1.0) * vertex);
	
	vec4 screenPosition = globals.p * globals.v * worldPosition;

	color.rgb =  mix( colorEnd.rgb, colorBeg.rgb, Warp( GetLife() * (1.0 / GetLifeMax()), warpT.x ) );
	color.a = 1.0;	

	return screenPosition;
}
