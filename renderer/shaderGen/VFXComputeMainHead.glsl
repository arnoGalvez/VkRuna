	Particle_t initParticle;
	initParticle.life = (GetLifeMax() - GetLifeMin()) * Random(wanghash(GetParticleID())) + GetLifeMin();
	Init(initParticle);

	Particle_t updatedParticle;
	ReadParticleAttributes(updatedParticle);
	UpdateParticleLife(updatedParticle);
	Update(updatedParticle);

	// Write back
	int isAlive = int(GetLife() > 0.0);
	const int reviveToken = atomicAdd(vfxReviveCounter[0], -1 + isAlive);
	const float UpdateOrInit = float((isAlive == 0) && (reviveToken > 0)) ;// 1.0 for init, 0.0 for update
	const uint id = GetParticleID();
