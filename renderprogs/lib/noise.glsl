#ifndef _NOISE_LIB
#define _NOISE_LIB

vec3 uintToFloat01( uvec3 m )
{
    return uintBitsToFloat(0x3F800000u | (m & 0x007FFFFFu)) - 1.0;
}

float uintToFloat01( uint m )
{
    return uintBitsToFloat(0x3F800000u | (m & 0x007FFFFFu)) - 1.0;
}

uint pcg( uint v )
{
	uint state = v * 747796405u + 2891336453u;
	uint word  = ( ( state >> ( ( state >> 28u ) + 4u ) ) ^ state ) * 277803737u;
	return ( word >> 22u ) ^ word;
}

uvec3 pcg3d( uvec3 v )
{
	v = v * 1664525u + 1013904223u;
	v.x += v.y * v.z;
	v.y += v.z * v.x;
	v.z += v.x * v.y;
	v = v ^ ( v >> 16u );
	v.x += v.y * v.z;
	v.y += v.z * v.x;
	v.z += v.x * v.y;
	return v;
}

float hashwithoutsine13( vec3 p3 )
{
	p3 = fract( p3 * .1031 );
	p3 += dot( p3, p3.yzx + 33.33 );
	return fract( ( p3.x + p3.y ) * p3.z );
}

/////////////// K.jpg's Simplex-like Re-oriented 4-Point BCC Noise ///////////////
//////////////////// Output: vec4(dF/dx, dF/dy, dF/dz, value) ////////////////////

// Inspired by Stefan Gustavson's noise
vec4 permute( vec4 t )
{
	return t * ( t * 34.0 + 133.0 );
}

// Gradient set is a normalized expanded rhombic dodecahedron
vec3 grad( float hash )
{
	// Random vertex of a cube, +/- 1 each
	vec3 cube = mod( floor( hash / vec3( 1.0, 2.0, 4.0 ) ), 2.0 ) * 2.0 - 1.0;

	// Random edge of the three edges connected to that vertex
	// Also a cuboctahedral vertex
	// And corresponds to the face of its dual, the rhombic dodecahedron
	vec3 cuboct                  = cube;
	cuboct[ int( hash / 16.0 ) ] = 0.0;

	// In a funky way, pick one of the four points on the rhombic face
	float type  = mod( floor( hash / 8.0 ), 2.0 );
	vec3  rhomb = ( 1.0 - type ) * cube + type * ( cuboct + cross( cube, cuboct ) );

	// Expand it so that the new edges are the same length
	// as the existing ones
	vec3 grad = cuboct * 1.22474487139 + rhomb;

	// To make all gradients the same length, we only need to shorten the
	// second type of vector. We also put in the whole noise scale constant.
	// The compiler should reduce it into the existing floats. I think.
	grad *= ( 1.0 - 0.042942436724648037 * type ) * 32.80201376986577;

	return grad;
}

// BCC lattice split up into 2 cube lattices
vec4 bccNoiseBase( vec3 X )
{
	// First half-lattice, closest edge
	vec3 v1     = round( X );
	vec3 d1     = X - v1;
	vec3 score1 = abs( d1 );
	vec3 dir1   = step( max( score1.yzx, score1.zxy ), score1 );
	vec3 v2     = v1 + dir1 * sign( d1 );
	vec3 d2     = X - v2;

	// Second half-lattice, closest edge
	vec3 X2     = X + 144.5;
	vec3 v3     = round( X2 );
	vec3 d3     = X2 - v3;
	vec3 score2 = abs( d3 );
	vec3 dir2   = step( max( score2.yzx, score2.zxy ), score2 );
	vec3 v4     = v3 + dir2 * sign( d3 );
	vec3 d4     = X2 - v4;

	// Gradient hashes for the four points, two from each half-lattice
	vec4 hashes = permute( mod( vec4( v1.x, v2.x, v3.x, v4.x ), 289.0 ) );
	hashes      = permute( mod( hashes + vec4( v1.y, v2.y, v3.y, v4.y ), 289.0 ) );
	hashes      = mod( permute( mod( hashes + vec4( v1.z, v2.z, v3.z, v4.z ), 289.0 ) ), 48.0 );

	// Gradient extrapolations & kernel function
	vec4 a              = max( 0.5 - vec4( dot( d1, d1 ), dot( d2, d2 ), dot( d3, d3 ), dot( d4, d4 ) ), 0.0 );
	vec4 aa             = a * a;
	vec4 aaaa           = aa * aa;
	vec3 g1             = grad( hashes.x );
	vec3 g2             = grad( hashes.y );
	vec3 g3             = grad( hashes.z );
	vec3 g4             = grad( hashes.w );
	vec4 extrapolations = vec4( dot( d1, g1 ), dot( d2, g2 ), dot( d3, g3 ), dot( d4, g4 ) );

	// Derivatives of the noise
	vec3 derivative = -8.0 * mat4x3( d1, d2, d3, d4 ) * ( aa * a * extrapolations ) + mat4x3( g1, g2, g3, g4 ) * aaaa;

	// Return it all as a vec4
	return vec4( derivative, dot( aaaa, extrapolations ) );
}

// Use this if you don't want Z to look different from X and Y
vec4 bccNoiseClassic( vec3 X )
{
	// Rotate around the main diagonal. Not a skew transform.
	vec4 result = bccNoiseBase( dot( X, vec3( 2.0 / 3.0 ) ) - X );
	return vec4( dot( result.xyz, vec3( 2.0 / 3.0 ) ) - result.xyz, result.w );
}

// Use this if you want to show X and Y in a plane, and use Z for time, etc.
vec4 bccNoisePlaneFirst( vec3 X )
{
	// Rotate so Z points down the main diagonal. Not a skew transform.
	mat3 orthonormalMap = mat3( 0.788675134594813,
	                            -0.211324865405187,
	                            -0.577350269189626,
	                            -0.211324865405187,
	                            0.788675134594813,
	                            -0.577350269189626,
	                            0.577350269189626,
	                            0.577350269189626,
	                            0.577350269189626 );

	vec4 result = bccNoiseBase( orthonormalMap * X );
	return vec4( result.xyz * orthonormalMap, result.w );
}

// BCC lattice split up into 2 cube lattices
vec4 bccNoiseDerivativesPart(vec3 X) {
    vec3 b = floor(X);
    vec4 i4 = vec4(X - b, 2.5);
    
    // Pick between each pair of oppposite corners in the cube.
    vec3 v1 = b + floor(dot(i4, vec4(.25)));
    vec3 v2 = b + vec3(1, 0, 0) + vec3(-1, 1, 1) * floor(dot(i4, vec4(-.25, .25, .25, .35)));
    vec3 v3 = b + vec3(0, 1, 0) + vec3(1, -1, 1) * floor(dot(i4, vec4(.25, -.25, .25, .35)));
    vec3 v4 = b + vec3(0, 0, 1) + vec3(1, 1, -1) * floor(dot(i4, vec4(.25, .25, -.25, .35)));
    
    // Gradient hashes for the four vertices in this half-lattice.
    vec4 hashes = permute(mod(vec4(v1.x, v2.x, v3.x, v4.x), 289.0));
    hashes = permute(mod(hashes + vec4(v1.y, v2.y, v3.y, v4.y), 289.0));
    hashes = mod(permute(mod(hashes + vec4(v1.z, v2.z, v3.z, v4.z), 289.0)), 48.0);
    
    // Gradient extrapolations & kernel function
    vec3 d1 = X - v1; vec3 d2 = X - v2; vec3 d3 = X - v3; vec3 d4 = X - v4;
    vec4 a = max(0.75 - vec4(dot(d1, d1), dot(d2, d2), dot(d3, d3), dot(d4, d4)), 0.0);
    vec4 aa = a * a; vec4 aaaa = aa * aa;
    vec3 g1 = grad(hashes.x); vec3 g2 = grad(hashes.y);
    vec3 g3 = grad(hashes.z); vec3 g4 = grad(hashes.w);
    vec4 extrapolations = vec4(dot(d1, g1), dot(d2, g2), dot(d3, g3), dot(d4, g4));
    
    // Derivatives of the noise
    vec3 derivative = -8.0 * mat4x3(d1, d2, d3, d4) * (aa * a * extrapolations)
        + mat4x3(g1, g2, g3, g4) * aaaa;
    
    // Return it all as a vec4
    return vec4(derivative, dot(aaaa, extrapolations));
}

// Rotates domain, but preserve shape. Hides grid better in cardinal slices.
// Good for texturing 3D objects with lots of flat parts along cardinal planes.
vec4 bccNoiseDerivatives_XYZ(vec3 X) {
    X = dot(X, vec3(2.0/3.0)) - X;
    
    vec4 result = bccNoiseDerivativesPart(X) + bccNoiseDerivativesPart(X + 144.5);
    
    return vec4(dot(result.xyz, vec3(2.0/3.0)) - result.xyz, result.w);
}

// Gives X and Y a triangular alignment, and lets Z move up the main diagonal.
// Might be good for terrain, or a time varying X/Y plane. Z repeats.
vec4 bccNoiseDerivatives_PlaneFirst(vec3 X) {
    
    // Not a skew transform.
    mat3 orthonormalMap = mat3(
        0.788675134594813, -0.211324865405187, -0.577350269189626,
        -0.211324865405187, 0.788675134594813, -0.577350269189626,
        0.577350269189626, 0.577350269189626, 0.577350269189626);
    
    X = orthonormalMap * X;
    vec4 result = bccNoiseDerivativesPart(X) + bccNoiseDerivativesPart(X + 144.5);
    
    return vec4(result.xyz * orthonormalMap, result.w);
}

////////////////////////////////////////////////////////////////////////////////

/////////////// From https://github.com/Unity-Technologies/Graphics/tree/master/com.unity.visualeffectgraph ///////////////

void NoiseHash2D(vec2 gridcell, out vec4 hash_0, out vec4 hash_1)
{
    vec2 kOffset = vec2(26.0f, 161.0f);
    float kDomain = 71.0f;
    vec2 kLargeFloats = 1.0f / vec2(951.135664f, 642.949883f);

    vec4 P = vec4(gridcell.xy, gridcell.xy + 1.0f);
    P = P - floor(P * (1.0f / kDomain)) * kDomain;
    P += kOffset.xyxy;
    P *= P;
    P = P.xzxz * P.yyww;
    hash_0 = fract(P * kLargeFloats.x);
    hash_1 = fract(P * kLargeFloats.y);
}


vec4 Interpolation_C2_InterpAndDeriv_BS(vec2 x) { return x.xyxy * x.xyxy * (x.xyxy * (x.xyxy * (x.xyxy * vec2(6.0f, 0.0f).xxyy + vec2(-15.0f, 30.0f).xxyy) + vec2(10.0f, -60.0f).xxyy) + vec2(0.0f, 30.0f).xxyy); }

vec3 GeneratePerlinNoise2D(vec2 coordinate)
{
    // establish our grid cell and unit position
    vec2 i = floor(coordinate);
    vec4 f_fmin1 = coordinate.xyxy - vec4(i, i + 1.0f);

    // calculate the hash
    vec4 hash_x, hash_y;
    NoiseHash2D(i, hash_x, hash_y);

    // calculate the gradient results
    vec4 grad_x = hash_x - 0.49999f;
    vec4 grad_y = hash_y - 0.49999f;
    vec4 norm = inversesqrt(grad_x * grad_x + grad_y * grad_y);
    grad_x *= norm;
    grad_y *= norm;
    vec4 dotval = (grad_x * f_fmin1.xzxz + grad_y * f_fmin1.yyww);

    // convert our data to a more parallel format
    vec3 dotval0_grad0 = vec3(dotval.x, grad_x.x, grad_y.x);
    vec3 dotval1_grad1 = vec3(dotval.y, grad_x.y, grad_y.y);
    vec3 dotval2_grad2 = vec3(dotval.z, grad_x.z, grad_y.z);
    vec3 dotval3_grad3 = vec3(dotval.w, grad_x.w, grad_y.w);

    // evaluate common constants
    vec3 k0_gk0 = dotval1_grad1 - dotval0_grad0;
    vec3 k1_gk1 = dotval2_grad2 - dotval0_grad0;
    vec3 k2_gk2 = dotval3_grad3 - dotval2_grad2 - k0_gk0;

    // C2 Interpolation
    vec4 blend = Interpolation_C2_InterpAndDeriv_BS(f_fmin1.xy);

    // calculate final noise + deriv
    vec3 results = dotval0_grad0
        + blend.x * k0_gk0
        + blend.y * (k1_gk1 + blend.x * k2_gk2);

    results.yz += blend.zw * (vec2(k0_gk0.x, k1_gk1.x) + blend.yx * k2_gk2.xx);

    return results * 1.4142135623730950488016887242097f;  // scale to -1.0 -> 1.0 range  *= 1.0/sqrt(0.5)
}

#define NOISE2D GeneratePerlinNoise2D

// Compute Curl of vector field F = (F_x, F_y, F_z)
vec3 CurlNoise(vec3 pos, int octaves, float baseFreq, float persistence, float lacunarity)
{
	// Let pos = (x, y, z)
	// Let offset = (k1, k2, k3)
	// Let NOISE2D(a, b) = ( Noise_value, dNoise/da (a, b), dNoise/db (a, b) )
	// Let F_x = NOISE2D(k1 + z, k1 + y)
	// Let F_y = NOISE2D(k2 + x, k2 + z)
	// Let F_z = NOISE2D(k3 + y, k3 + x)
	// Then:
	// dF_x/dz (x, y, z) = F_x.y
	// dF_x/dy (x, y, z) = F_x.z
	// dF_y/dx (x, y, z) = F_y.y
	// dF_y/dz (x, y, z) = F_y.z
	// dF_z/dy (x, y, z) = F_z.y
	// dF_z/dx (x, y, z) = F_z.z

	const float k1 = 0.0, k2 = 100.0, k3 = 200.0;

	const vec2 samples[3] = vec2[3](
		k1 + vec2(pos.z, pos.y),
		k2 + vec2(pos.x, pos.z),
		k3 + vec2(pos.y, pos.x)
	);

	vec2 F_x = vec2(0.0);
	vec2 F_y = vec2(0.0);
	vec2 F_z = vec2(0.0);

	float a = 1.0;
	float f = baseFreq;
	float aTot = 0.0;
	
	for (int o = 0; o < octaves; ++o)
	{
		F_x += a * NOISE2D(f * samples[0]).yz;
		F_y += a * NOISE2D(f * samples[1]).yz;
		F_z += a * NOISE2D(f * samples[2]).yz;

		aTot += a;
		a *= persistence;
		f *= lacunarity;
	}

	vec3 curl = vec3(F_z.x - F_y.y, F_x.x - F_z.y, F_y.x - F_x.y) / aTot;

	return curl;
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////// Inigo Quilez functions ///////////////////////////


float InigoHash1( float n )
{
    return fract( n*17.0*fract( n*0.3183099 ) );
}


float InigoNoise( in vec3 x )
{
    vec3 p = floor(x);
    vec3 w = fract(x);
    
#if 1
    vec3 u = w*w*w*(w*(w*6.0-15.0)+10.0);
#else
    vec3 u = w*w*(3.0-2.0*w);
#endif

    float n = p.x + 317.0*p.y + 157.0*p.z;

#if 0
    float a = InigoHash1(n+0.000);
    float b = InigoHash1(n+1.000);
    float c = InigoHash1(n+317.0);
    float d = InigoHash1(n+318.0);
    float e = InigoHash1(n+157.0);
	float f = InigoHash1(n+158.0);
    float g = InigoHash1(n+474.0);
    float h = InigoHash1(n+475.0);
#else
	float a = uintToFloat01( pcg( floatBitsToUint(n+0.000) ) );
    float b = uintToFloat01( pcg( floatBitsToUint(n+1.000) ) );
    float c = uintToFloat01( pcg( floatBitsToUint(n+317.0) ) );
    float d = uintToFloat01( pcg( floatBitsToUint(n+318.0) ) );
    float e = uintToFloat01( pcg( floatBitsToUint(n+157.0) ) );
	float f = uintToFloat01( pcg( floatBitsToUint(n+158.0) ) );
    float g = uintToFloat01( pcg( floatBitsToUint(n+474.0) ) );
    float h = uintToFloat01( pcg( floatBitsToUint(n+475.0) ) );
#endif
    float k0 =   a;
    float k1 =   b - a;
    float k2 =   c - a;
    float k3 =   e - a;
    float k4 =   a - b - c + d;
    float k5 =   a - c - e + g;
    float k6 =   a - b - e + f;
    float k7 = - a + b + c - d + e - f - g + h;

    return (k0 + k1*u.x + k2*u.y + k3*u.z + k4*u.x*u.y + k5*u.y*u.z + k6*u.z*u.x + k7*u.x*u.y*u.z);
}

float InigoFBM(vec3 pos, float a, float f, int octaves, float persistence, float lacunarity)
{
	float ret = 0.0;

	for (int i = 0; i < octaves; ++i)
	{
		ret += a * InigoNoise(f * pos).x;

		
		a *= persistence;
		f *= lacunarity;
	}

	return ret;
}

#define INIGO_ANIMATE

vec2 InigoHash2( vec2 p )
{
	// texture based white noise
	//return textureLod( iChannel0, (p+0.5)/256.0, 0.0 ).xy;
	
    // procedural white noise	
	return fract(sin(vec2(dot(p,vec2(127.1,311.7)),dot(p,vec2(269.5,183.3))))*43758.5453);
}

vec3 InigoVoronoi( in vec2 x )
{
    vec2 n = floor(x);
    vec2 f = fract(x);

    //----------------------------------
    // first pass: regular voronoi
    //----------------------------------
	vec2 mg, mr;

    float md = 8.0;
    for( int j=-1; j<=1; j++ )
    for( int i=-1; i<=1; i++ )
    {
        vec2 g = vec2(float(i),float(j));
		vec2 o = InigoHash2( n + g );
		#ifdef INIGO_ANIMATE
        o = 0.5 + 0.5*sin( GetTime() + 6.2831*o );
        #endif	
        vec2 r = g + o - f;
        float d = dot(r,r);

        if( d<md )
        {
            md = d;
            mr = r;
            mg = g;
        }
    }

    //----------------------------------
    // second pass: distance to borders
    //----------------------------------
    md = 8.0;
    for( int j=-2; j<=2; j++ )
    for( int i=-2; i<=2; i++ )
    {
        vec2 g = mg + vec2(float(i),float(j));
		vec2 o = InigoHash2( n + g );
		#ifdef ANIMATE
        o = 0.5 + 0.5*sin( GetTime() + 6.2831*o );
        #endif	
        vec2 r = g + o - f;

        if( dot(mr-r,mr-r)>0.00001 )
        md = min( md, dot( 0.5*(mr+r), normalize(r-mr) ) );
    }

    return vec3( md, mr );
}
///////////////////////////////////////////////////////////////////////////

vec3 NoisePCG3D(vec3 x)
{
	uvec3 ux = floatBitsToUint(x);
	uvec3 hash = pcg3d(ux);
	// hash = hash >> 1;
	// return vec3(hash) * 2.328306436538696289e-10;
	return uintToFloat01(hash);
}

// vec4 signal(vec3 pos)
// {
// 	float v = sin(pos.x + pos.y + pos.z);
// 	vec3 grad = vec3(cos(pos.x + pos.y + pos.z));

// 	return vec4(grad, v);
// }

vec4 NoiseHarmonicsBCC(vec3 pos, float a, float f, int octaves, float persistence, float lacunarity)
{
	vec4 ret = vec4(0.0);
	float aTot = 0.0;

	for (int i = 0; i < octaves; ++i)
	{
		ret += a * bccNoiseClassic(f * pos);
		// ret += a * signal(f * pos);
		// ret.xyz *= f;

		aTot += a;

		a *= persistence;
		f *= lacunarity;
	}

	return ret /* / aTot */;
}

vec4 NoiseBCC(float x, float y, float z, float a, float f, int octaves, float persistence, float lacunarity)
{
	vec3 p = vec3(x, y, z);

	return NoiseHarmonicsBCC(p, a, f, octaves, persistence, lacunarity);
}

#endif