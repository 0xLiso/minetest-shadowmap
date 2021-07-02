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
	uniform mat4 m_ShadowViewProj0;
	uniform mat4 m_ShadowViewProj1;
	uniform mat4 m_ShadowViewProj2;
	uniform vec3 v_shadow_splits;
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

vec4 getPerspectiveFactor(in vec4 shadowPosition)
{
	return shadowPosition;
}


vec3 getLightSpacePosition(in mat4 shadow_split_view_proj)
{
	vec4 pLightSpace;
	// some drawtypes have zero normals, so we need to handle it :(
	#if DRAW_TYPE == NDT_PLANTLIKE
	pLightSpace = shadow_split_view_proj * vec4(worldPosition, 1.0);
	pLightSpace.z+=0.005;
	#else
	float offsetScale = normalOffsetScale ;
	pLightSpace = shadow_split_view_proj * vec4(worldPosition + offsetScale * normalize(vNormal), 1.0);
	#endif
	pLightSpace = getPerspectiveFactor(pLightSpace);
	return pLightSpace.xyz * 0.5 + 0.5;
}

float getHardShadow(sampler2D shadowsampler, vec2 smTexCoord, float realDistance, int splitId)
{
	float texDepth = texture2D(shadowsampler, smTexCoord.xy)[splitId];
	float visibility = step(0.0, realDistance - texDepth);
	return visibility;
}

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

	int cIdx = 2;

    vec3 posLightSpace;
    float z_view = -eyeVec.z;
    if(z_view<=v_shadow_splits[0]){
    	posLightSpace = getLightSpacePosition(m_ShadowViewProj0);
    	cIdx=0;
    }
    else if(z_view<=v_shadow_splits[1]){
    	posLightSpace = getLightSpacePosition(m_ShadowViewProj1);
    	cIdx=1;
    }
    else{
    	posLightSpace = getLightSpacePosition(m_ShadowViewProj2);
    }

	
	if(posLightSpace.x>=0 && posLightSpace.x<=1.0 &&
		posLightSpace.y>=0 && posLightSpace.y<=1.0)
	{
		shadow_int = getHardShadow(ShadowMapSampler, posLightSpace.xy, posLightSpace.z,cIdx);
	}

	if (f_normal_length != 0 && cosLight <= 0.001) {
		//shadow_int = clamp(shadow_int + abs(cosLight) , 0.0, 1.0);
		shadow_int =1.0;
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
	//col.rgb=vec3(-cosLight);
	gl_FragColor = vec4(col.rgb, base.a);
}
