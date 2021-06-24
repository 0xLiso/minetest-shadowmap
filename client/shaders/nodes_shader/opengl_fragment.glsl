uniform sampler2D baseTexture;

uniform vec4 skyBgColor;
uniform float fogDistance;
uniform vec3 eyePosition;

// The cameraOffset is the current center of the visible world.
uniform vec3 cameraOffset;
uniform float animationTimer;
#ifdef ENABLE_DYNAMIC_SHADOWS
	// shadow texture
	uniform sampler2D ShadowMapSampler;
	// shadow uniforms
	uniform vec3 v_LightDirection;
	uniform float f_textureresolution;
	uniform mat4 m_ShadowViewProj;
	uniform mat4 m_InvProj;
	uniform mat4 m_InvView;
	uniform float f_shadowfar;
	uniform float f_shadownear;
	uniform vec2 v_screen_size;
	varying float normalOffsetScale;
	varying float adj_shadow_strength;
	varying float cosLight;
	varying float f_normal_length;
	varying vec4 v_LightSpace;
#endif


varying vec3 vNormal;
varying vec3 vPosition;
// World position in the visible world (i.e. relative to the cameraOffset.)
// This can be used for many shader effects without loss of precision.
// If the absolute position is required it can be calculated with
// cameraOffset + worldPosition (for large coordinates the limits of float
// precision must be considered).
varying vec3 worldPosition;
varying lowp vec4 varColor;
#ifdef GL_ES
varying mediump vec2 varTexCoord;
#else
centroid varying vec2 varTexCoord;
#endif
varying vec3 eyeVec;
varying float nightRatio;

const float fogStart = FOG_START;
const float fogShadingParameter = 1.0 / ( 1.0 - fogStart);



#ifdef ENABLE_DYNAMIC_SHADOWS
const float bias0 = 0.9;
const float zPersFactor = 1.0/4.0;
const float bias1 = 1.0 - bias0 + 1e-6;




vec4 getWorldSpacePosition() {

     // Convert screen coordinates to normalized device coordinates (NDC)
    vec4 ndc = vec4(
        (gl_FragCoord.x / v_screen_size.x - 0.5) * 2.0,
        (gl_FragCoord.y / v_screen_size.y - 0.5) * 2.0,
        (gl_FragCoord.z - 0.5) * 2.0,
        1.0);

    // Convert NDC throuch inverse clip coordinates to view coordinates
    vec4 clip = m_InvView * m_InvProj *  ndc;
    vec4 vertex = vec4((clip / clip.w).xyz,1.0);
  	return vertex;
}

vec4 getPerspectiveFactor(in vec4 shadowPosition)
{   
	float lnz = sqrt(shadowPosition.x*shadowPosition.x+shadowPosition.y*shadowPosition.y);

	float pf=mix(1.0, lnz * 1.165, bias0);
	
	float pFactor =1.0/pf;
	shadowPosition.xyz *= vec3(vec2(pFactor), zPersFactor);

	return shadowPosition;
}


// assuming near is always 1.0
float getLinearDepth(vec4 fragposition)
{	return  2.0 * f_shadownear*f_shadowfar / (f_shadowfar + f_shadownear - ( fragposition.z-cameraOffset.z ) * (f_shadowfar - f_shadownear));
 	return  2.0 * gl_DepthRange.near*gl_DepthRange.far / (gl_DepthRange.far + gl_DepthRange.near - (0.5 * gl_FragCoord.z +0.5) * (gl_DepthRange.far - gl_DepthRange.near));
}

vec3 getLightSpacePosition()
{
	vec4 pLightSpace;
	vec4 worldpos = getWorldSpacePosition();
	// some drawtypes have zero normals, so we need to handle it :(
	#if DRAW_TYPE == NDT_PLANTLIKE
	pLightSpace = m_ShadowViewProj * worldpos;
	#else
	vec3 adjustedBias = ( 0.00005 * getLinearDepth(worldpos) + normalOffsetScale)  *normalize(vNormal) ;
	pLightSpace = m_ShadowViewProj * vec4(worldpos.xyz +  adjustedBias, 1.0);
	#endif

	if(pLightSpace.x<-1.0 || pLightSpace.x>1.0 ||
		pLightSpace.y<-1.0 || pLightSpace.y>1.0)
	{
		return vec3(-2);
	}

	pLightSpace = getPerspectiveFactor(pLightSpace);
	pLightSpace.xyz=pLightSpace.xyz * 0.5 + 0.5;
	return pLightSpace.xyz;
}
// custom smoothstep implementation because it's not defined in glsl1.2
// https://docs.gl/sl4/smoothstep
float mtsmoothstep(in float edge0, in float edge1, in float x)
{
	float t = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
	return t * t * (3.0 - 2.0 * t);
}



float getHardShadow(sampler2D shadowsampler, vec2 smTexCoord, float realDistance)
{
	float texDepth = texture2DLod(shadowsampler, smTexCoord.xy,2.0).r;
	float visibility = step(0.0, realDistance - texDepth);
	return visibility;
}

const float PCFSAMPLES=64.0;
const float PCFBOUND=3.5;
float getShadow(sampler2D shadowsampler, vec2 smTexCoord, float realDistance)
{
	vec2 clampedpos;
	float visibility = 0.0;
	float sradius=0.0;
		
	float texture_size = 2.0/f_textureresolution;
	float y, x;
	// basic PCF filter
	for (y = -PCFBOUND; y <= PCFBOUND; y += 1.0)
	for (x = -PCFBOUND; x <= PCFBOUND; x += 1.0) {
		clampedpos =  vec2(x,y)*texture_size + smTexCoord.xy;
		visibility += getHardShadow(shadowsampler, clampedpos.xy, realDistance);
	}

	if(visibility<PCFSAMPLES*.25){
		return 0.0;
	}

	return visibility / PCFSAMPLES;
}
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
	vec4 col = vec4(color.rgb * varColor.rgb, 1.0);

#ifdef ENABLE_DYNAMIC_SHADOWS
	float shadow_int = 0.0;
	vec3 shadow_color = vec3(0.0, 0.0, 0.0);
	vec3 posLightSpace = getLightSpacePosition();
	posLightSpace = v_LightSpace.xyz;
	float distance_rate = (1 - pow(clamp(2.0 * length(posLightSpace.xy - 0.5),0.0,1.0), 20.0));
	float f_adj_shadow_strength = max(adj_shadow_strength-mtsmoothstep(.95,1.,  posLightSpace.z  ),0.0);
	//float f_adj_shadow_strength = max(adj_shadow_strength,0.0);
	
	if (distance_rate > 1e-7 && posLightSpace.x>-1e-7) {
	
 
		shadow_int = getHardShadow(ShadowMapSampler, posLightSpace.xy, posLightSpace.z );
		shadow_int *= distance_rate;
		shadow_int *= 1.0 - nightRatio;


	}

	if (f_normal_length != 0 && cosLight == 0.0) {
		shadow_int = clamp(1.0-nightRatio, 0.0, 1.0);
	} 

	shadow_int = 1.0 - (shadow_int * f_adj_shadow_strength);
	
	col.rgb = mix(shadow_color,col.rgb,shadow_int)*shadow_int;
	// col.r = 0.5 * clamp(getPenumbraRadius(ShadowMapSampler, posLightSpace.xy, posLightSpace.z, 1.0) / SOFTSHADOWRADIUS, 0.0, 1.0) + 0.5 * col.r;
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
	col = vec4(col.rgb, base.a);
	/* shadow frustum debug
	if(posLightSpace.x<=1.0 && posLightSpace.x>= 0.0 &&
		posLightSpace.y<=1.0 && posLightSpace.y>= 0.0 ){
		col.r+=.60;
	}else{
		col.g+=.6;
	}*/

	gl_FragColor = col;
}
