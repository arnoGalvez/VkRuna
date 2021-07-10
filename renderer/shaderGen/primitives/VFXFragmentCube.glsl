//////// Cube Primitive Begin ////////

#define PRIMITIVE_CUBE

#define CUBE_FACE_XY 0
#define CUBE_FACE_XZ 1
#define CUBE_FACE_YZ 2

layout (location = 16) in vec3 _cubeVertOffset;
layout (location = 17) in flat int _cubeFaceId;

int GetFaceID()
{
	return _cubeFaceId;
}

vec2 GetUV()
{
	vec3 o = _cubeVertOffset;
	const int faceId = GetFaceID();

	if (faceId == CUBE_FACE_YZ)
		return vec2(o.y, o.z);
	else if (faceId == CUBE_FACE_XZ)
		return vec2(o.x, o.z);
	else 
		return vec2(o.x, o.y);
}

vec3 GetNormal()
{
	vec3 o = _cubeVertOffset;
	const int faceId = GetFaceID();

	if (faceId == CUBE_FACE_YZ)
		return vec3(float(o.x > 0.1) - float(o.x < 0.1), 0.0, 0.0);
	else if (faceId == CUBE_FACE_XZ)
		return vec3(0.0, float(o.y > 0.1) - float(o.y < 0.1), 0.0);
	else 
		return vec3(0.0, 0.0, float(o.z > 0.1) - float(o.z < 0.1));
}

//////// Cube Primitive End ////////
