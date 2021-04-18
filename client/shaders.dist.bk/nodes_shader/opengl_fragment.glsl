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

	varying vec3 N;
#endif

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

const float fogStart = FOG_START;
const float fogShadingParameter = 1.0 / ( 1.0 - fogStart);


#ifdef ENABLE_DYNAMIC_SHADOWS
		 
	const vec2[64] poissonDisk = vec2[64]
	(
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

	

	const float bias0 = 0.9;
	const float bias1 = 0.1; //1.0 - bias0;
	const float zdistorFactor = 0.5;

	vec4 getDistortFactor(in vec4 shadowPosition) {
	  float factorDistance =  sqrt(shadowPosition.x * shadowPosition.x +
	      shadowPosition.y * shadowPosition.y );
	  //float factorDistance =  length(shadowPosition.xy);
	  float distortFactor = factorDistance * bias0 + bias1;
	  shadowPosition.xyz *= vec3(vec2(1.0 / distortFactor), zdistorFactor);

	  return shadowPosition;
	}

	vec3 getShadowSpacePosition(in vec4 pos)
	{
		vec4 positionShadowSpace = mShadowProj* mShadowView * mWorld * pos; 
		positionShadowSpace = getDistortFactor(positionShadowSpace);
		positionShadowSpace.xyz = positionShadowSpace.xyz*0.5 +0.5;
		return positionShadowSpace.xyz;
	}

	vec3 getShadowSpacePosition2(in vec4 pos)
	{
		vec4 positionShadowSpace = mShadowProj* mShadowView * mWorld * pos; 
		positionShadowSpace.xyz = positionShadowSpace.xyz*0.5 +0.5;
		return positionShadowSpace.xyz;
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

	/*
		custom smoothstep implementation because it's not defined in glsl1.2
		https://docs.gl/sl4/smoothstep
	*/
	float mtsmoothstep(in float edge0, in float edge1, in float x ){

		float t;  /* Or genDType t; */
	    t = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
	    return t * t * (3.0 - 2.0 * t);
	}


	#ifdef COLORED_SHADOWS

		vec4 getHardShadowColor(sampler2D shadowsampler, vec2 smTexCoord, float realDistance)
		{
			vec4 texDepth = texture2D(shadowsampler, smTexCoord.xy);
			return (realDistance > texDepth.r) ? vec4(1.0,texDepth.gba) : vec4(0.0);
		}
		vec4 getShadowColor(sampler2D shadowsampler, vec2 smTexCoord, float realDistance)
		{

			int init_offset = int(floor(mod(((smTexCoord.x * 34.0) + 1.0) * smTexCoord.y, 64-i_shadow_samples)));
			int end_offset = i_shadow_samples + init_offset;
			vec2 clampedpos=smTexCoord.xy;
			vec4 visibility=getHardShadowColor(shadowsampler, clampedpos.xy, realDistance);
			
			for ( int x=init_offset; x<end_offset; x++)
			{
				clampedpos = smTexCoord.xy + (poissonDisk[x]/(f_textureresolution/4.0) );
				visibility += getHardShadowColor(shadowsampler, clampedpos.xy, realDistance);
			}
			vec4 result = visibility/float(i_shadow_samples+1);
			return  result ;
		}
	#else
		float getHardShadow(sampler2D shadowsampler, vec2 smTexCoord, float realDistance)
		{
			float texDepth = texture2D(shadowsampler, smTexCoord.xy).r;
			return (realDistance > texDepth) ? 1.0 : 0.0;
		}

		float getShadow(sampler2D shadowsampler, vec2 smTexCoord, float realDistance)
		{

			int init_offset = int(floor(mod(((smTexCoord.x * 34.0) + 1.0) * smTexCoord.y, 64-i_shadow_samples)));
			int end_offset = i_shadow_samples + init_offset;
			vec2 clampedpos=smTexCoord.xy;
			float visibility=getHardShadow(shadowsampler, clampedpos.xy, realDistance);

			for ( int x=init_offset; x<end_offset; x++)
			{
				clampedpos = smTexCoord.xy + (poissonDisk[x]/(f_textureresolution/2.0) );
				visibility += getHardShadow(shadowsampler, clampedpos.xy, realDistance);
			}
			
			return visibility/float(i_shadow_samples+1);
		}


	#endif

#endif
 
#ifdef ENABLE_TONE_MAPPING

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

#if ENABLE_DYNAMIC_SHADOWS && DRAW_TYPE!=NDT_TORCHLIKE
	
	#ifdef COLORED_SHADOWS
		vec4 shadow_int =  vec4(0.0,0.0,0.0,0.0);
	#else
		float shadow_int=0.0;
	#endif

	if(dot( -v_LightDirection , N )  <= 0){
		#ifdef COLORED_SHADOWS
			shadow_int = vec4(1.0,0.0,0.0,0.0);
		#else
			shadow_int=1.0;
		#endif
	}
	else {
		vec4 posInWorld = getWorldPosition() ;
		vec3 posInShadow=getShadowSpacePosition( posInWorld );
		//if(posInShadow.x>0.0&&posInShadow.x<1.0&&posInShadow.y>0.0&&posInShadow.y<1.0)
		{
			float bias = 1.0 - clamp(dot( N , posInShadow.xyz), 0.0, 1.0);
			bias = -0.0000005 - 0.00000005 * bias;

			#ifdef COLORED_SHADOWS
				shadow_int=getShadowColor(ShadowMapSampler, posInShadow.xy,
									posInShadow.z  + bias  );
			#else
				shadow_int=getShadow(ShadowMapSampler, posInShadow.xy,
									posInShadow.z  + bias  );
			#endif

	

		}
		 


	}
	float adj_shadow_strength = mtsmoothstep(0.20,0.25,f_timeofday)*(1.0-mtsmoothstep(0.7,0.8,f_timeofday) );
	
	//ccol[cIdx]=0.15;

	#ifdef COLORED_SHADOWS
		shadow_int.r  = 1.0 - (shadow_int.r*f_shadow_strength*adj_shadow_strength);
		col.rgb=  col.rgb*shadow_int.r + shadow_int.gba*shadow_int.r;
	#else
		shadow_int  =  1.0  - (shadow_int*f_shadow_strength*adj_shadow_strength);
		col.rgb*=shadow_int;
	#endif
	
	//col = clamp(vec4((col.rgb-shadow_int),col.a),0.0,1.0);
#endif

#ifdef ENABLE_TONE_MAPPING
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
	
	col = vec4(col.rgb , base.a);
	 
	gl_FragColor = col;
}
