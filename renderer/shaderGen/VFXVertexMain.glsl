void main()
{
	if (ExitIfDead()) {
		gl_Position = vec4(vec3(-10), 1.0);
		return;
	}

	Particle_t particle;
	ReadParticleAttributes(particle);

	PerPrimitiveFunc();

	gl_Position = VertexMain(particle);	
}
