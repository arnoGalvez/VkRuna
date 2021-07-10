vec3 rgb2hsv( vec3 c )
{
	vec4 K = vec4( 0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0 );
	vec4 p = mix( vec4( c.bg, K.wz ), vec4( c.gb, K.xy ), step( c.b, c.g ) );
	vec4 q = mix( vec4( p.xyw, c.r ), vec4( c.r, p.yzx ), step( p.x, c.r ) );

	float d = q.x - min( q.w, q.y );
	float e = 1.0e-10;
	return vec3( abs( q.z + ( q.w - q.y ) / ( 6.0 * d + e ) ), d / ( q.x + e ), q.x );
}

vec3 hsv2rgb( vec3 c )
{
	vec4 K = vec4( 1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0 );
	vec3 p = abs( fract( c.xxx + K.xyz ) * 6.0 - K.www );
	return c.z * mix( K.xxx, clamp( p - K.xxx, 0.0, 1.0 ), c.y );
}

ivec3 CubePosSeq(int k, int cubeLength)
{
	ivec3 pos;
	pos.x = k % cubeLength;
	pos.y = (k / cubeLength) % cubeLength;
	pos.z = (k / (cubeLength * cubeLength)) % cubeLength;

	return pos;
}

mat4 AlignYAxis(mat4 cameraMat, vec3 position, vec3 axis)
{
	mat3 cameraToWorld = transpose( mat3( cameraMat ) );
	vec3 cameraWorldPos = -(cameraToWorld * cameraMat[3].xyz);
	
	vec3 rotX, rotY, rotZ;
	rotY = normalize(axis);
	rotX = normalize( cameraWorldPos - position );
	rotZ = cross(rotX, rotY);
	rotX = cross(rotY, rotZ);

	mat3 rotation = mat3( rotX, rotY, rotZ );
	mat4 ret = mat4( rotation );
	ret[3].xyz = position;

	return ret;
}
