#ifndef _MATH_LIB
#define _MATH_LIB

float Warp(float x, float t)
{
	return x / (exp(t) * (1.0 - x) + x );
}

float MapRange(float x, vec2 from, vec2 to)
{
	return ( (x - from.x) / (from.y - from.x) ) * (to.y - to.x) + to.x;
}

mat4 RotateX(float angle)
{
	float c = cos(angle);
	float s = sin(angle);
	vec4 X = vec4(1.0, 0.0, 0.0, 0.0);
	vec4 Y = vec4(0.0, c, s, 0.0);
	vec4 Z = vec4(0.0, -s, c, 0.0);
	vec4 T = vec4(0.0, 0.0, 0.0, 1.0);

	return mat4(X, Y, Z, T);
}

mat4 RotateY(float angle)
{
	float c = cos(angle);
	float s = sin(angle);
	vec4 X = vec4(c, 0.0, -s, 0.0);
	vec4 Y = vec4(0.0, 1.0, 0.0, 0.0);
	vec4 Z = vec4(s, 0.0, c, 0.0);
	vec4 T = vec4(0.0, 0.0, 0.0, 1.0);

	return mat4(X, Y, Z, T);
}

mat4 RotateZ(float angle)
{
	float c = cos(angle);
	float s = sin(angle);
	vec4 X = vec4(c, s, 0.0, 0.0);
	vec4 Y = vec4(-s, c, 0.0, 0.0);
	vec4 Z = vec4(0.0, 0.0, 1.0, 0.0);
	vec4 T = vec4(0.0, 0.0, 0.0, 1.0);

	return mat4(X, Y, Z, T);
}

mat4 Scale(vec3 scaling)
{
	vec4 X = vec4(scaling.x, 0.0, 0.0, 0.0);
	vec4 Y = vec4(0.0, scaling.y, 0.0, 0.0);
	vec4 Z = vec4(0.0, 0.0, scaling.z, 0.0);
	vec4 T = vec4(0.0, 0.0, 0.0, 1.0);

	return mat4(X, Y, Z, T);
}

void SetTranslation(inout mat4 TRS, vec3 translation)
{
	TRS[3].xyz = translation;
}

#endif