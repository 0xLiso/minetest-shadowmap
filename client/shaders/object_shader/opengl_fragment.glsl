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

#if ENABLE_DYNAMIC_SHADOWS
	// shadow texture
	uniform sampler2D ShadowMapSampler;
	//shadow uniforms
	uniform vec3 v_LightDirection;
	uniform float f_textureresolution;
	uniform mat4 mWorld;
	uniform mat4 mShadowProj;
	uniform mat4 mShadowView;
	uniform mat4 mInvProj;
	uniform mat4 mInvWorldView;
	uniform vec2 vScreen;  //screen size w,h
	uniform int i_shadow_samples;
	uniform float f_shadow_strength;
	uniform float f_timeofday;
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
	
	#ifdef SHADOWS_PSM
		const float bias0 = 0.95;
		const float bias1 = 0.05; //1.0 - bias0;
		const float zdistorFactor = 0.2;

		vec4 getDistortFactor(in vec4 shadowPosition) {

		  float factorDistance =  sqrt(shadowPosition.x * shadowPosition.x +
		      shadowPosition.y * shadowPosition.y );
		  //float factorDistance =  length(shadowPosition.xy);
		  float distortFactor = factorDistance * bias0 + bias1;
		  shadowPosition.xyz *= vec3(vec2(1.0 / distortFactor), zdistorFactor);

		  return shadowPosition;
		}

		

		vec4 getWorldPosition(){
			vec4 positionNDCSpace = vec4(
				(gl_FragCoord.x / vScreen[0] - 0.5) * 2.0,
				(gl_FragCoord.y / vScreen[1] - 0.5) * 2.0,
				(gl_FragCoord.z - 0.5) * 2.0,
				1.0
			);

			vec4 positionCameraSpace = mInvProj * positionNDCSpace;
			positionCameraSpace = positionCameraSpace / positionCameraSpace.w;
			vec4 positionWorldSpace = mInvWorldView * positionCameraSpace;
			return positionWorldSpace;
		}


	#endif


	vec3 getShadowSpacePosition()
	{
		#ifdef SHADOWS_PSM
			vec4 positionShadowSpace = mShadowProj* mShadowView  * vec4(worldPosition,1.0); 
			positionShadowSpace = getDistortFactor(positionShadowSpace);
			positionShadowSpace.xyz = positionShadowSpace.xyz*0.5 +0.5;
			return positionShadowSpace.xyz;
		#else
			vec4 positionShadowSpace = mShadowProj* mShadowView * vec4(worldPosition,1.0); 
			return positionShadowSpace.xyz*0.5 +0.5;
		#endif

		
	}
	//custom smoothstep implementation because it's not defined in glsl1.2
	//	https://docs.gl/sl4/smoothstep
	float mtsmoothstep(in float edge0, in float edge1, in float x ){
		float t;
	    t = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
	    return t * t * (3.0 - 2.0 * t);
	}


	
	float getLinearDepth() {
			float near=1.0;
			float far=256.0;
	  return 2.0f * near * far / (far + near - (2.0f * gl_FragCoord.z - 1.0f) * (far - near));
	}
	float getHardShadow(sampler2D shadowsampler, vec2 smTexCoord, float realDistance)
	{
		float texDepth = texture2D(shadowsampler, smTexCoord.xy).r;
		float visibility = step(0.00000015f * getLinearDepth() +0.0000005,
			realDistance - texDepth);
		return visibility;
	}

	float getShadow(sampler2D shadowsampler, vec2 smTexCoord, float realDistance)
	{
		vec2 clampedpos;
		float visibility=0.0;//getHardShadow(shadowsampler, clampedpos.xy, realDistance);

		float texture_size= 1/(f_textureresolution*0.5);
		#if SHADOW_FILTER == 2
			#define PCFBOUND 3.5
			#define PCFSAMPLES 64.0
		#elif  SHADOW_FILTER == 1
			#define PCFBOUND 1.5
			#define PCFSAMPLES 16.0
		#else
			#define PCFBOUND 0.0
			#define PCFSAMPLES 1.0
		#endif
		float y;
		float x;
		for (y = -PCFBOUND ; y <=PCFBOUND ; y+=1.0)
			for (x = -PCFBOUND ; x <=PCFBOUND ; x+=1.0)
			{
				clampedpos = vec2(x,y)*texture_size + smTexCoord.xy;
				visibility += getHardShadow(shadowsampler, clampedpos.xy, realDistance);
			}
		
		return visibility/PCFSAMPLES;
	}


	#ifdef COLORED_SHADOWS
	vec3 getShadowColor(sampler2D shadowsampler, vec2 smTexCoord, float realDistance)
	{
		vec3 tcol = texture2D(shadowsampler, smTexCoord.xy).gba;
		return tcol;
	}	
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

#if ENABLE_DYNAMIC_SHADOWS
	
	float shadow_int=0.0;
	vec3 shadow_color=vec3(0.0,0.0,0.0);
	

	if(dot( -v_LightDirection , vNormal)  <= 0){
		shadow_int=1.0;
	}
	else {
		
		vec3 posInShadow=getShadowSpacePosition( );

		if(posInShadow.x>0.0&&posInShadow.x<1.0 &&
		   posInShadow.y>0.0&&posInShadow.y<1.0 &&
		   posInShadow.z>0.0&&posInShadow.z<1.0)
		{
			//float bias = 1.0 - clamp(dot( N , posInShadow.xyz), 0.0, 1.0);
			//bias = -0.0000005 - 0.00000005 * bias;

			shadow_int=getShadow(ShadowMapSampler, posInShadow.xy,
									posInShadow.z  );

			#ifdef COLORED_SHADOWS
				shadow_color=getShadowColor(ShadowMapSampler, posInShadow.xy,
									posInShadow.z  );			
			#endif

			
		}
	}
	float adj_shadow_strength = mtsmoothstep(0.20,0.25,
		f_timeofday)*(1.0-mtsmoothstep(0.7,0.8,f_timeofday) );

	
	shadow_int *= 1.0 - mtsmoothstep(200,500.0,vPosition.z);
	shadow_int  = 1.0 - (shadow_int*f_shadow_strength*adj_shadow_strength);
	
	col.rgb=shadow_int*col.rgb + ( shadow_color*shadow_int*0.25);
	
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
