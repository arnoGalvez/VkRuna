${beg 
	vec4 scale;
end}

layout (location = 0) out vec4 worldPos;

vec4 VertexMain(Particle_t particle)
{
	const vec4 vertex = GetVertex();
	worldPos = vec4(scale.xyz, 1.0) * vertex + vec4(particle.position, 0.0);
	const vec4 screenPos = globals.p * globals.v * worldPos;

	return screenPos;
}