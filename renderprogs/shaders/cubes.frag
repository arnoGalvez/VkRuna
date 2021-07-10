layout (location = 0) in vec3 diffuse;

layout (location = 0) out vec4 fragColor;

void FragmentMain()
{
	fragColor = vec4(diffuse, 1.0);
}
