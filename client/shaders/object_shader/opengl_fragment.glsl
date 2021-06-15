uniform sampler2D baseTexture;

uniform vec4 emissiveColor;
uniform vec4 skyBgColor;
uniform float fogDistance;
uniform vec3 eyePosition;


varying vec3 vNormal;
varying vec3 vPosition;
varying vec3 worldPosition;
varying lowp vec4 varColor;
#ifdef GL_ES
varying mediump vec2 varTexCoord;
#else
centroid varying vec2 varTexCoord;
#endif

varying vec3 eyeVec;
varying float vIDiff;

const float e = 2.718281828459;
const float BS = 10.0;
const float fogStart = FOG_START;
const float fogShadingParameter = 1.0 / (1.0 - fogStart);

#ifdef ENABLE_DYNAMIC_SHADOWS
	// shadow texture
	uniform sampler2D ShadowMapSampler;
	// shadow uniforms
	uniform vec3 v_LightDirection;
	uniform float f_textureresolution;
	uniform mat4 m_ShadowViewProj;
	uniform float f_shadowfar;
	uniform float f_shadownear;
	uniform float f_timeofday;
	varying float normalOffsetScale;
	varying float adj_shadow_strength;
	varying float cosLight;
	varying float f_normal_length;
#endif

#if ENABLE_TONE_MAPPING
/* Hable's UC2 Tone mapping parameters
	A = 0.22;
	B = 0.30;
	C = 0.10;
	D = 0.20;
	E = 0.01;
	F = 0.30;
	W = 11.2;
	equation used:  ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F
*/

vec3 uncharted2Tonemap(vec3 x)
{
	return ((x * (0.22 * x + 0.03) + 0.002) / (x * (0.22 * x + 0.3) + 0.06)) - 0.03333;
}

vec4 applyToneMapping(vec4 color)
{
	color = vec4(pow(color.rgb, vec3(2.2)), color.a);
	const float gamma = 1.6;
	const float exposureBias = 5.5;
	color.rgb = uncharted2Tonemap(exposureBias * color.rgb);
	// Precalculated white_scale from
	//vec3 whiteScale = 1.0 / uncharted2Tonemap(vec3(W));
	vec3 whiteScale = vec3(1.036015346);
	color.rgb *= whiteScale;
	return vec4(pow(color.rgb, vec3(1.0 / gamma)), color.a);
}
#endif

#ifdef ENABLE_DYNAMIC_SHADOWS
const float bias0 = 0.9;
const float zPersFactor = 0.5;
const float bias1 = 1.0 - bias0;

vec4 getPerspectiveFactor(in vec4 shadowPosition)
{
	float pDistance = length(shadowPosition.xy);
	float pFactor = pDistance * bias0 + bias1;
	shadowPosition.xyz *= vec3(vec2(1.0 / pFactor), zPersFactor);

	return shadowPosition;
}

// assuming near is always 1.0
float getLinearDepth()
{

	return 2.0 * f_shadownear*f_shadowfar / (f_shadowfar + f_shadownear - (2.0 * gl_FragCoord.z - 1.0) * (f_shadowfar - f_shadownear));
}

vec3 getLightSpacePosition()
{
	vec4 pLightSpace;
	float normalBias = 0.0005 * getLinearDepth() * cosLight + normalOffsetScale;
	pLightSpace = m_ShadowViewProj * vec4(worldPosition + normalBias * normalize(vNormal), 1.0);
	pLightSpace = getPerspectiveFactor(pLightSpace);
	return pLightSpace.xyz * 0.5 + 0.5;
}

#ifdef COLORED_SHADOWS

// c_precision of 128 fits within 7 base-10 digits
const float c_precision = 128.0;
const float c_precisionp1 = c_precision + 1.0;

float packColor(vec3 color)
{
	return floor(color.b * c_precision + 0.5)
		+ floor(color.g * c_precision + 0.5) * c_precisionp1
		+ floor(color.r * c_precision + 0.5) * c_precisionp1 * c_precisionp1;
}

vec3 unpackColor(float value)
{
	vec3 color;
	color.b = mod(value, c_precisionp1) / c_precision;
	color.g = mod(floor(value / c_precisionp1), c_precisionp1) / c_precision;
	color.r = floor(value / (c_precisionp1 * c_precisionp1)) / c_precision;
	return color;
}

vec4 getHardShadowColor(sampler2D shadowsampler, vec2 smTexCoord, float realDistance)
{
	vec4 texDepth = texture2D(shadowsampler, smTexCoord.xy).rgba;

	float visibility = step(0.0, (realDistance-2e-5) - texDepth.r);
	vec4 result = vec4(visibility, vec3(0.0,0.0,0.0));//unpackColor(texDepth.g));
	if (visibility < 0.1) {
		visibility = step(0.0, (realDistance-2e-5) - texDepth.r);
		result = vec4(visibility, unpackColor(texDepth.a));
	}
	return result;
}

#else

float getHardShadow(sampler2D shadowsampler, vec2 smTexCoord, float realDistance)
{
	float texDepth = texture2D(shadowsampler, smTexCoord.xy).r;
	float visibility = step(0.0, (realDistance-2e-5) - texDepth);

	return visibility;
}

#endif

#if SHADOW_FILTER == 2
	#define PCFBOUND 3.5
	#define PCFSAMPLES 64.0
#elif SHADOW_FILTER == 1
	#define PCFBOUND 1.5
	#if defined(POISSON_FILTER)
		#define PCFSAMPLES 32.0
	#else
		#define PCFSAMPLES 16.0
	#endif
#else
	#define PCFBOUND 0.0
	#if defined(POISSON_FILTER)
		#define PCFSAMPLES 4.0
	#else
		#define PCFSAMPLES 1.0
	#endif
#endif

#ifdef POISSON_FILTER
const vec2[64] poissonDisk = vec2[64](
	vec2(0.170019, -0.040254),
	vec2(-0.299417, 0.791925),
	vec2(0.645680, 0.493210),
	vec2(-0.651784, 0.717887),
	vec2(0.421003, 0.027070),
	vec2(-0.817194, -0.271096),
	vec2(-0.705374, -0.668203),
	vec2(0.977050, -0.108615),
	vec2(0.063326, 0.142369),
	vec2(0.203528, 0.214331),
	vec2(-0.667531, 0.326090),
	vec2(-0.098422, -0.295755),
	vec2(-0.885922, 0.215369),
	vec2(0.566637, 0.605213),
	vec2(0.039766, -0.396100),
	vec2(0.751946, 0.453352),
	vec2(0.078707, -0.715323),
	vec2(-0.075838, -0.529344),
	vec2(0.724479, -0.580798),
	vec2(0.222999, -0.215125),
	vec2(-0.467574, -0.405438),
	vec2(-0.248268, -0.814753),
	vec2(0.354411, -0.887570),
	vec2(0.175817, 0.382366),
	vec2(0.487472, -0.063082),
	vec2(0.355476, 0.025357),
	vec2(-0.084078, 0.898312),
	vec2(0.488876, -0.783441),
	vec2(0.470016, 0.217933),
	vec2(-0.696890, -0.549791),
	vec2(-0.149693, 0.605762),
	vec2(0.034211, 0.979980),
	vec2(0.503098, -0.308878),
	vec2(-0.016205, -0.872921),
	vec2(0.385784, -0.393902),
	vec2(-0.146886, -0.859249),
	vec2(0.643361, 0.164098),
	vec2(0.634388, -0.049471),
	vec2(-0.688894, 0.007843),
	vec2(0.464034, -0.188818),
	vec2(-0.440840, 0.137486),
	vec2(0.364483, 0.511704),
	vec2(0.034028, 0.325968),
	vec2(0.099094, -0.308023),
	vec2(0.693960, -0.366253),
	vec2(0.678884, -0.204688),
	vec2(0.001801, 0.780328),
	vec2(0.145177, -0.898984),
	vec2(0.062655, -0.611866),
	vec2(0.315226, -0.604297),
	vec2(-0.780145, 0.486251),
	vec2(-0.371868, 0.882138),
	vec2(0.200476, 0.494430),
	vec2(-0.494552, -0.711051),
	vec2(0.612476, 0.705252),
	vec2(-0.578845, -0.768792),
	vec2(-0.772454, -0.090976),
	vec2(0.504440, 0.372295),
	vec2(0.155736, 0.065157),
	vec2(0.391522, 0.849605),
	vec2(-0.620106, -0.328104),
	vec2(0.789239, -0.419965),
	vec2(-0.545396, 0.538133),
	vec2(-0.178564, -0.596057)
);

#ifdef COLORED_SHADOWS

vec4 getShadowColor(sampler2D shadowsampler, vec2 smTexCoord, float realDistance)
{
	vec2 clampedpos;
	vec4 visibility = vec4(0.0);

	float texture_size = 1.0 / (f_textureresolution * 0.5);
	int init_offset = int(floor(mod(((smTexCoord.x * 34.0) + 1.0) * smTexCoord.y, 64.0-PCFSAMPLES)));
	int end_offset = int(PCFSAMPLES) + init_offset;

	for (int x = init_offset; x < end_offset; x++) {
		clampedpos = poissonDisk[x] * texture_size * SOFTSHADOWRADIUS + smTexCoord.xy;
		visibility += getHardShadowColor(shadowsampler, clampedpos.xy, realDistance);
	}

	return visibility / PCFSAMPLES;
}

#else

float getShadow(sampler2D shadowsampler, vec2 smTexCoord, float realDistance)
{
	vec2 clampedpos;
	float visibility = 0.0;

	float texture_size = 1.0 / (f_textureresolution * 0.5);
	int init_offset = int(floor(mod(((smTexCoord.x * 34.0) + 1.0) * smTexCoord.y, 64.0-PCFSAMPLES)));
	int end_offset = int(PCFSAMPLES) + init_offset;

	for (int x = init_offset; x < end_offset; x++) {
		clampedpos = poissonDisk[x] * texture_size * SOFTSHADOWRADIUS + smTexCoord.xy;
		visibility += getHardShadow(shadowsampler, clampedpos.xy, realDistance);
	}

	return visibility / PCFSAMPLES;
}

#endif

#else
/* poisson filter disabled */

#ifdef COLORED_SHADOWS

vec4 getShadowColor(sampler2D shadowsampler, vec2 smTexCoord, float realDistance)
{
	vec2 clampedpos;
	vec4 visibility = vec4(0.0);
	float sradius=0.0;
	if( PCFBOUND>0)
		sradius = SOFTSHADOWRADIUS / PCFBOUND;  
	float texture_size = 1.0 / (f_textureresolution * 0.5);
	float y, x;
	// basic PCF filter
	for (y = -PCFBOUND; y <= PCFBOUND; y += 1.0)
	for (x = -PCFBOUND; x <= PCFBOUND; x += 1.0) {
		clampedpos = vec2(x,y) * texture_size* sradius +  smTexCoord.xy;
		visibility += getHardShadowColor(shadowsampler, clampedpos.xy, realDistance);
	}

	return visibility / PCFSAMPLES;
}

#else
float getShadow(sampler2D shadowsampler, vec2 smTexCoord, float realDistance)
{
	vec2 clampedpos;
	float visibility = 0.0;
	float sradius=0.0;
	if( PCFBOUND>0)
		sradius = SOFTSHADOWRADIUS / PCFBOUND;  
	
	float texture_size = 1.0 / (f_textureresolution * 0.5);
	float y, x;
	// basic PCF filter
	for (y = -PCFBOUND; y <= PCFBOUND; y += 1.0)
	for (x = -PCFBOUND; x <= PCFBOUND; x += 1.0) {
		clampedpos =  vec2(x,y) * texture_size * sradius + smTexCoord.xy;
		visibility += getHardShadow(shadowsampler, clampedpos.xy, realDistance);
	}

	return visibility / PCFSAMPLES;
}

#endif

#endif
#endif

void main(void)
{
	vec3 color;
	vec2 uv = varTexCoord.st;
	vec4 base = texture2D(baseTexture, uv).rgba;

#ifdef USE_DISCARD
	// If alpha is zero, we can just discard the pixel. This fixes transparency
	// on GPUs like GC7000L, where GL_ALPHA_TEST is not implemented in mesa,
	// and also on GLES 2, where GL_ALPHA_TEST is missing entirely.
	if (base.a == 0.0) {
		discard;
	}
#endif

	color = base.rgb;
	vec4 col = vec4(color.rgb, base.a);
	col.rgb *= varColor.rgb;
	col.rgb *= emissiveColor.rgb * vIDiff;

#ifdef ENABLE_DYNAMIC_SHADOWS
	float shadow_int = 0.0;
	vec3 shadow_color = vec3(0.0, 0.0, 0.0);
	vec3 posLightSpace = getLightSpacePosition();

#ifdef COLORED_SHADOWS
	vec4 visibility = getShadowColor(ShadowMapSampler, posLightSpace.xy, posLightSpace.z);
	shadow_int = visibility.r;
	shadow_color = visibility.gba;
#else
	shadow_int = getShadow(ShadowMapSampler, posLightSpace.xy, posLightSpace.z);
#endif

	if (f_normal_length != 0 && cosLight <= 0.001) {
		shadow_int = clamp(shadow_int + 0.5 * abs(cosLight), 0.0, 1.0);
	}

	shadow_int = 1.0 - (shadow_int * adj_shadow_strength);

	col.rgb = mix(shadow_color, col.rgb, shadow_int) * shadow_int;
#endif



#if ENABLE_TONE_MAPPING
	col = applyToneMapping(col);
#endif

	// Due to a bug in some (older ?) graphics stacks (possibly in the glsl compiler ?),
	// the fog will only be rendered correctly if the last operation before the
	// clamp() is an addition. Else, the clamp() seems to be ignored.
	// E.g. the following won't work:
	//      float clarity = clamp(fogShadingParameter
	//		* (fogDistance - length(eyeVec)) / fogDistance), 0.0, 1.0);
	// As additions usually come for free following a multiplication, the new formula
	// should be more efficient as well.
	// Note: clarity = (1 - fogginess)
	float clarity = clamp(fogShadingParameter
		- fogShadingParameter * length(eyeVec) / fogDistance, 0.0, 1.0);
	col = mix(skyBgColor, col, clarity);

	gl_FragColor = vec4(col.rgb, base.a);
}
