#include "utils.glsl"
#include "noise.glsl"
#include "brianSharpeNoise.glsl"
#include "math.glsl"

layout (location = 0) flat in int id;
layout (location = 1) in vec3 worldPosition;

layout (location = 0) out vec4 fragColor;

${beg 
	color color_1;
	color color_2;
	color color_3;
	color color_4;
	vec4 noise_lacunarity_persistence;
	vec4 noise_2_lacunarity_persistence;
end}

#define NOISE_HARMONICS NoiseHarmonicsBCC

vec4 pattern( 	vec3 pos,
				float a,
				float f,
				int octaves,
				float persistence_1,
				float lacunarity_1,
				float persistence_2,
				float lacunarity_2,
				float persistence_3,
				float lacunarity_3,
				out vec3 q,
				out vec3 r )
{

	const float q_s = 2.0;
	const float r_s = 1.0;

	vec4 A = NOISE_HARMONICS( pos + vec3(0.0,0.0,0.0), a, f, octaves, persistence_1, lacunarity_1 );
	vec4 B = NOISE_HARMONICS( pos + vec3(1.2,1.3,1.6), a, f, octaves, persistence_1, lacunarity_1 );
	vec4 C = NOISE_HARMONICS( pos + vec3(2.2,1.8,1.8), a, f, octaves, persistence_1, lacunarity_1 );

    q = vec3(A.w, B.w, C.w);

    vec4 D = NOISE_HARMONICS( pos + q_s * q + vec3(1.7,5.9,-0.3), a, f, octaves, persistence_2, lacunarity_2 );
	vec4 E = NOISE_HARMONICS( pos + q_s * q + vec3(2.4,3.1,0.4), a, f, octaves, persistence_2, lacunarity_2 );
	vec4 F = NOISE_HARMONICS( pos + q_s * q + vec3(2.6,2.3,1.5), a, f, octaves, persistence_2, lacunarity_2 );

    r = vec3(D.w, E.w, F.w);

    vec4 G = NOISE_HARMONICS( pos + r_s * r, a, f, octaves, persistence_3, lacunarity_3 );

    vec4 pat = vec4(0.0);
    pat.w = G.w;

    vec3 d_f2f1_1 = vec3(0.0);
    d_f2f1_1.x = r_s * D.x * q_s * A.x + r_s * D.y * q_s * B.x + r_s * D.z * q_s * C.x;
    d_f2f1_1.y = r_s * D.x * q_s * A.y + r_s * D.y * q_s * B.y + r_s * D.z * q_s * C.y;
    d_f2f1_1.z = r_s * D.x * q_s * A.z + r_s * D.y * q_s * B.z + r_s * D.z * q_s * C.z;

    vec3 d_f2f1_2 = vec3(0.0);
    d_f2f1_2.x = r_s * E.x * q_s * A.x + r_s * E.y * q_s * B.x + r_s * E.z * q_s * C.x;
    d_f2f1_2.y = r_s * E.x * q_s * A.y + r_s * E.y * q_s * B.y + r_s * E.z * q_s * C.y;
    d_f2f1_2.z = r_s * E.x * q_s * A.z + r_s * E.y * q_s * B.z + r_s * E.z * q_s * C.z;

    vec3 d_f2f1_3 = vec3(0.0);
    d_f2f1_3.x = r_s * F.x * q_s * A.x + r_s * F.y * q_s * B.x + r_s * F.z * q_s * C.x;
    d_f2f1_3.y = r_s * F.x * q_s * A.y + r_s * F.y * q_s * B.y + r_s * F.z * q_s * C.y;
    d_f2f1_3.z = r_s * F.x * q_s * A.z + r_s * F.y * q_s * B.z + r_s * F.z * q_s * C.z;


    vec3 d_f3f2f1 = vec3(0.0);
    d_f3f2f1.x = G.x * d_f2f1_1.x + G.y * d_f2f1_2.x + G.z * d_f2f1_3.x;
    d_f3f2f1.y = G.x * d_f2f1_1.y + G.y * d_f2f1_2.y + G.z * d_f2f1_3.y;
    d_f3f2f1.z = G.x * d_f2f1_1.z + G.y * d_f2f1_2.z + G.z * d_f2f1_3.z;

    pat.xyz = d_f3f2f1;

    return pat;
}

void FragmentMain()
{
	vec3 p = 1.0 * worldPosition + 0.01*vec3(globals.time.x);
	float a = .6;
	float f = 1.0;
	int octaves = 1;
	float persistence_1 = noise_lacunarity_persistence.y;
	float lacunarity_1 = noise_lacunarity_persistence.x;
	float persistence_2 = noise_lacunarity_persistence.w;
	float lacunarity_2 = noise_lacunarity_persistence.z;
	float persistence_3 = noise_2_lacunarity_persistence.y;
	float lacunarity_3 = noise_2_lacunarity_persistence.x;
	
	vec3 q = vec3(0.0);
	vec3 r = vec3(0.0);
	vec4 pat = pattern(p, a, f, octaves, persistence_1, lacunarity_1, persistence_2, lacunarity_2, persistence_3, lacunarity_3, q, r);
	float noise = pat.w;
	vec3 grad = pat.xyz;
	
	vec3 color = mix( color_1.rgb, color_2.rgb, Warp(0.5*noise+0.5, 0.5) );
	color = mix( color, color_3.rgb, clamp(length(q) / sqrt(3), 0.0, 1.0) );
	color = mix( color, color_4.rgb, r.z) ;

	vec3 normal = normalize(vec3(1, -grad.y, -grad.z));
	vec3 light = normalize( vec3( 1.0, 0.0, 0.0 ) );
	float diffuse =  2.0 * clamp( dot( normal, light ), 0.0, 1.0 );
	color *= diffuse;

	fragColor = vec4(color, 1.0);
}
