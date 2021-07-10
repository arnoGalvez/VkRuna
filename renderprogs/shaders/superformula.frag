#include "utils.glsl"

layout (location = 0) in vec4 worldPos;

layout (location = 0) out vec4 fragColor;

void FragmentMain()
{
	vec3 color = hsv2rgb( vec3( worldPos.z + 2.4 * GetTime(), 0.8, 0.8 ) );
	fragColor = vec4(color, 1.0);
}