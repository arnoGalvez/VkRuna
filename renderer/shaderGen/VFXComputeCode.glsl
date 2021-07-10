uint GetParticleID()
{
	return gl_GlobalInvocationID.x;
}

float GetLife()
{
	return _life[GetParticleID()];
}

void Init(inout Particle_t particle);
void Update(inout Particle_t particle);
