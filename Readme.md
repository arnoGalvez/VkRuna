# VkRuna - A simple shader based VFX editor ✨

![](images/swarm-sample.png)

This VFX was generated using the compute, vertex and fragment shaders hereinafter:

```glsl
/////////////////////////// Compute Shader ///////////////////////////
#include "noise.glsl"
#include "utils.glsl"

${beg
	vec4 sphereScale;
	vec4 velocityScale;
	vec4 noiseAmplFreqPersLac;
end}

void Init(inout Particle_t particle)
{
	const uint id = GetParticleID();
	const float cubeLength = ceil( pow(GetCapacity(), 0.333333) );
	const vec3 noiseParams = vec3( CubePosSeq( int(id), int(cubeLength) ) ); 

	particle.position = sphereScale.xyz * normalize( bccNoiseClassic( noiseParams / (cubeLength * 0.4) + vec3(1.0) ).xyz );
	particle.velocity = vec3(0.0);
}

void Update(inout Particle_t updatedParticle)
{	
	updatedParticle.velocity = velocityScale.xyz * CurlNoise(updatedParticle.position, 6, noiseAmplFreqPersLac.y, noiseAmplFreqPersLac.z, noiseAmplFreqPersLac.w);
	updatedParticle.position = updatedParticle.position + GetDeltaFrame() * updatedParticle.velocity;
}

/////////////////////////// Vertex Shader ///////////////////////////
#include "utils.glsl"
#include "math.glsl"

${beg
	vec4 scale;
	vec4 warpT;
	color colorBeg;
	color colorEnd;
end}

layout (location = 0) out vec4 color;

vec4 VertexMain(Particle_t particle)
{
	mat4 rotation = AlignYAxis(globals.v, particle.position, particle.velocity);
	vec4 vertex = GetVertex();
	vec4 worldPosition = rotation * (GetLife() * vec4(vec3(scale), 1.0) * vertex);
	
	vec4 screenPosition = globals.p * globals.v * worldPosition;

	color.rgb =  mix( colorEnd.rgb, colorBeg.rgb, Warp( GetLife() * (1.0 / GetLifeMax()), warpT.x ) );
	color.a = 1.0;	

	return screenPosition;
}

/////////////////////////// Fragment Shader ///////////////////////////
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

```
## Building the tool

### Prerequisites

* Windows 10
* [CMake](https://cmake.org/download/)
* A 64 bits toolchain (eg x64, coming with Visual Studio 2019)

### Building
1. Generate a build configuration with CMake, e.g.:
```
cmake -S <path-to-source> -B <path-to-build> -G "Visual Studio 16 2019"
```
2. Open the project in your IDE, build and run !

## Utilisation

See the wiki.

## Samples

![](images/cubes-sample.png)
![](images/twist-sample.png)

## About The Project

This project is part of my peregrination through the world of explicit rendering API and real time rendering. My goal was to make a simple GPU based VFX system. I first set out to recreate one of the example from Unity's VFX graph, the [particle swarm](images/unity-swarm.png). Then, the renderer now being able to do simple VFX rendering, I moved onto making it a simple tool enabling real time modifications of the VFXs through UI exposed variables and on the fly shader recompiling.

I layed the foundation of this project using Pawel Lapinski's [API without secrets](https://software.intel.com/content/www/us/en/develop/articles/api-without-secrets-introduction-to-vulkan-part-1.html) tutorials, and then moved on to Dustin Land's [Vulkan blog series](https://www.fasterthan.life/blog). With Pawel I learnt the basics of Vulkan, and with Dustin I learnt the process of integrating a Vulkan rendering backend. I used my knowledge from a previous internship at Unity to design the UX for the VFX tool itself. Building the foundations was tedious, but the catharsis of seing my first animated particles was quite worth it !

The following SDK and libraries were used:
* [Vulkan](https://vulkan.lunarg.com/sdk/home)
* [glm](https://glm.g-truc.net/0.9.9/index.html)
* [Cereal](https://uscilab.github.io/cereal/)
* [Dear ImGui](https://github.com/ocornut/imgui)

## Contact

Arno Galvez - [Linkedin](https://www.linkedin.com/in/arnogalvez/)

Project Link: [https://github.com/arnoGalvez/vkRuna](https://github.com/arnoGalvez/vkRuna)


