// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// DO NOT #INCLUDE IN YOUR SHADERS
// (Already included)
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!


uint wanghash(uint x)
{
	x = (x ^ 61) ^ (x >> 16);
	x *= 9;
	x = x ^ (x >> 4);
	x *= 0x27d4eb2d;
	x = x ^ (x >> 15);
	return x;
}

uint RandomUInt(uint seed)
{
	// Xorshift32
	seed ^= (seed << 13);
	seed ^= (seed >> 17);
	seed ^= (seed << 5);

	return seed;
}

float Random(uint seed)
{
	return float(RandomUInt(seed)) / 4294967297.0;
}
