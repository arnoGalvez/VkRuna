layout (location = 0) in vec4 color;
layout (location = 0) out vec4 fragColor;

void FragmentMain()
{
	vec2 uv = 2.0 * GetUV() - vec2(1.0);
	if (dot(uv, uv) > 1.0) {
		discard;
	}

	fragColor = color;
}
