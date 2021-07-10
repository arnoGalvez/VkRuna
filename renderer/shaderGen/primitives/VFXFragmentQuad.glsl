#define PRIMITIVE_QUAD

layout (location = 16) in vec2 _quadUV;

vec2 GetUV()
{
	return _quadUV;
}
