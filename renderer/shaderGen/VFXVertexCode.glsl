float GetLife()
{
	return _life[GetParticleID()];
}

bool ExitIfDead()
{
	return (GetParticleID() >= int(GetCapacity())) || (GetLife() <= 0.0);
}
