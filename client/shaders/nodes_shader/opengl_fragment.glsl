#extension GL_ARB_gpu_shader5 : enable
uniform sampler2D baseTexture;

uniform vec4 skyBgColor;
uniform float fogDistance;
uniform vec3 eyePosition;
uniform vec2 vScreen;

uniform mat4 mInvWorldViewProj;

// The cameraOffset is the current center of the visible world.
uniform vec3 cameraOffset;
uniform float animationTimer;
#ifdef ENABLE_DYNAMIC_SHADOWS
	// shadow texture
	uniform sampler2D ShadowMapSampler;
	//shadow uniforms
	uniform mat4 mShadowWorldViewProj0;
	uniform mat4 mShadowWorldViewProj1;
	uniform mat4 mShadowWorldViewProj2;
	uniform vec4 mShadowCsmSplits;
	uniform vec3 v_LightDirection;
	uniform float f_textureresolution;
	uniform float f_brightness;
	uniform mat4 mWorldView;
	uniform mat4 mWorldViewProj;
	uniform mat4 m_worldView;
	uniform mat4 mWorld;

	uniform mat4 mShadowProj;
	uniform mat4 mShadowView;
	uniform mat4 mInvProj;
	uniform mat4 mInvWorldView;


	uniform vec3 vCamPos;

	varying vec4 P;
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



	vec3 rgb2hsl( in vec4 c )
	{
	    const float epsilon = 0.00000001;
	    float cmin = min( c.r, min( c.g, c.b ) );
	    float cmax = max( c.r, max( c.g, c.b ) );
	    float cd   = cmax - cmin;
	    vec3 hsl = vec3(0.0);
	    hsl.z = (cmax + cmin) / 2.0;
	    hsl.y = mix(cd / (cmax + cmin + epsilon), cd / (epsilon + 2.0 - (cmax + cmin)), step(0.5, hsl.z));

	    vec3 a = vec3(1.0 - step(epsilon, abs(cmax - c)));
	    a = mix(vec3(a.x, 0.0, a.z), a, step(0.5, 2.0 - a.x - a.y));
	    a = mix(vec3(a.x, a.y, 0.0), a, step(0.5, 2.0 - a.x - a.z));
	    a = mix(vec3(a.x, a.y, 0.0), a, step(0.5, 2.0 - a.y - a.z));
	    
	    hsl.x = dot( vec3(0.0, 2.0, 4.0) + ((c.gbr - c.brg) / (epsilon + cd)), a );
	    hsl.x = (hsl.x + (1.0 - step(0.0, hsl.x) ) * 6.0 ) / 6.0;
	    return hsl;
	}

	float getLinearDepth(in float depth) {
	  float near=0.1;
	  float far =20000.0;
	  return 2.0f * near * far / (far + near - (2.0f * depth - 1.0f) * (far - near));
	}

	 
const vec2[128] poissonDisk = vec2[128](
    vec2(0. , 0.),
    vec2(0.2538637f, -0.589553f),
    vec2(0.6399639f, -0.6070346f),
    vec2(0.1431894f, -0.8152663f),
    vec2(0.5930731f, -0.7948953f),
    vec2(0.6914624f, -0.3480401f),
    vec2(0.4279022f, -0.4768359f),
    vec2(0.8242062f, -0.508942f),
    vec2(0.01053669f, -0.4866286f),
    vec2(-0.1108985f, -0.7414401f),
    vec2(0.03328848f, -0.9812139f),
    vec2(-0.2678958f, -0.3206359f),
    vec2(0.25712f, -0.229964f),
    vec2(-0.02783006f, -0.2600488f),
    vec2(-0.2917352f, -0.6411636f),
    vec2(-0.4032183f, -0.8573055f),
    vec2(-0.6612689f, -0.7354062f),
    vec2(-0.5676314f, -0.5411444f),
    vec2(-0.2168807f, -0.9072415f),
    vec2(-0.5580572f, -0.09704394f),
    vec2(-0.5138885f, -0.3027371f),
    vec2(-0.1932104f, -0.09702744f),
    vec2(-0.3822881f, -0.01384046f),
    vec2(0.8748599f, -0.1630837f),
    vec2(-0.522255f, 0.2585554f),
    vec2(-0.749154f, -0.08459146f),
    vec2(-0.749154f, -0.08459146f),
    vec2(-0.6647733f, 0.129063f),
    vec2(-0.8998289f, -0.2349087f),
    vec2(-0.8098084f, -0.5461301f),
    vec2(0.5121568f, 0.00675085f),
    vec2(0.1070659f, -0.05260961f),
    vec2(0.3009415f, 0.1365128f),
    vec2(0.5151741f, -0.1867349f),
    vec2(-0.9284627f, -0.007728597f),
    vec2(-0.2198475f, 0.3018067f),
    vec2(-0.07589716f, 0.09244914f),
    vec2(0.721417f, 0.01370876f),
    vec2(0.6517887f, 0.1998482f),
    vec2(0.4209776f, 0.3226621f),
    vec2(0.9295521f, 0.1595292f),
    vec2(0.8101555f, 0.3356059f),
    vec2(0.6216043f, 0.4737987f),
    vec2(-0.7957394f, 0.4460461f),
    vec2(-0.578917f, 0.5065681f),
    vec2(-0.3760341f, 0.4722787f),
    vec2(0.1558616f, 0.3765588f),
    vec2(0.4568439f, 0.655364f),
    vec2(0.08923677f, 0.1941438f),
    vec2(0.1930917f, 0.5782562f),
    vec2(-0.07713082f, 0.5275764f),
    vec2(0.4766026f, 0.8639814f),
    vec2(-0.7173501f, 0.6784452f),
    vec2(-0.8751968f, 0.2121847f),
    vec2(0.8041916f, 0.5765353f),
    vec2(0.2870654f, 0.9436792f),
    vec2(0.6502987f, 0.7152798f),
    vec2(-0.2637711f, 0.7050315f),
    vec2(-0.03864802f, 0.7925433f),
    vec2(-0.1051485f, 0.9776039f),
    vec2(-0.3079708f, 0.9433341f),
    vec2(-0.5206522f, 0.6986488f),
    vec2(0.08988898f, 0.9506541f),
    vec2(0.2821491f, 0.7465457f),
     vec2(-0.613392, 0.617481),
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
     vec2(-0.178564, -0.596057));

	float getShadowv2(sampler2D shadowsampler, vec2 smTexCoord, float realDistance ,int cIdx) {
	    float texDepth = texture2D(shadowsampler, smTexCoord.xy )[cIdx];
		return ( realDistance  >  texDepth  ) ?  1.0  :0.0 ;
	}


 float getShadow(sampler2D shadowsampler, vec2 smTexCoord, float realDistance ,int cIdx) {
		float nsamples=128.0;
	    vec2 clampedpos;
		
		float visibility= getShadowv2(shadowsampler, smTexCoord.xy, realDistance ,  cIdx);
		
	    for (int i = 1; i <  nsamples ; i++){
	        clampedpos = smTexCoord.xy + ( poissonDisk[i]/f_textureresolution);
            visibility += getShadowv2(shadowsampler, clampedpos.xy, realDistance ,  cIdx) ;
	    }
	    
	    return  visibility/ nsamples  ;
	}




	vec4 getDistortFactor(in vec4 shadowPosition) {
		
	  const float bias0 = 0.9f;
	  const float bias1 = 1.0f - bias0;

	  float factorDistance =  sqrt(shadowPosition.x * shadowPosition.x +
	  							   shadowPosition.y * shadowPosition.y );
	  //float factorDistance =  length(shadowPosition.xy);
	  float distortFactor = factorDistance * bias0 + bias1;

	    shadowPosition.xyz *= vec3(vec2(1.0 / distortFactor), .75);

	  return shadowPosition;
	}

	vec4 getDistortFactorv2(in vec4 shadowPosition) {
	  const float DistortPower = 7.0f;
	  const float SHADOW_MAP_BIAS = 0.9f;
	  vec2 p=shadowPosition.xy;
	  p = abs(p);
	  p = p * p * p;
	  float distordLengh=pow(p.x + p.y, 1.0f / 3.0f);
	  float len = 1e-6 + distordLengh;
	  distordLengh =  (1.0f - SHADOW_MAP_BIAS) + len * SHADOW_MAP_BIAS;
	  vec2 distortedcoords =  shadowPosition.xy / min(distordLengh, 1.0f);

	  return vec4(distortedcoords.xy,shadowPosition.z * 0.2,1.0);
	}

	vec3 getShadowSpacePosition(in vec4 pos,in mat4 shadowMVP) {

	  vec4 positionShadowSpace = mShadowProj* mShadowView * mWorld * pos; 
	  positionShadowSpace = getDistortFactor(positionShadowSpace);
	  positionShadowSpace.xy = positionShadowSpace.xy*0.5 +0.5;
	  positionShadowSpace.z = getLinearDepth(positionShadowSpace.z);
	  positionShadowSpace.z = positionShadowSpace.z*0.5 + 0.5;
	  return positionShadowSpace.xyz;
	}

	vec4 getWorldPosition(){
		vec4 positionNDCSpace = vec4(2.0f * gl_FragCoord.xy - 1.0f,
									 2.0f * gl_FragCoord.z - 1.0f,
									 1.0f);

		positionNDCSpace = vec4(
	        (gl_FragCoord.x / vScreen[0] - 0.5) * 2.0,
	        (gl_FragCoord.y / vScreen[1] - 0.5) * 2.0,
	        (gl_FragCoord.z - 0.5) * 2.0,
	        1.0);

	  vec4 positionCameraSpace = mInvProj * positionNDCSpace;

	  positionCameraSpace = positionCameraSpace / positionCameraSpace.w;

	  vec4 positionWorldSpace = mInvWorldView * positionCameraSpace;

	  return positionWorldSpace;

	}



 

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
		float shadow_int =0.0;
		
		float diffuseLight = dot(normalize(-v_LightDirection),normalize(N)) ;

		 

	    float bias = max(0.0005 * (1.0 - diffuseLight), 0.000005) ;  
	     
	    float NormalOffsetScale= 2.0+2.0/f_textureresolution;
	    float SlopeScale = abs(1-diffuseLight);
		NormalOffsetScale*=SlopeScale;
	    vec3 posNormalbias = P.xyz + N.xyz*NormalOffsetScale;
		diffuseLight=clamp(diffuseLight+0.2,0.5,1.0);
		float shadow_int0 =0.0;
		float shadow_int1 =0.0;
		float shadow_int2 =0.0;

		//float brightness = rgb2hsl(col).b;//(col.r+col.g+col.b)/3.0;
		
		

   		bias =  0.0000005 ;
 		//bias=0.0f;
        
        if(dot(normalize(-v_LightDirection),normalize(N))  <= 0){
        	shadow_int0=1.0f;
        }
		else {
			vec4 posInWorld = getWorldPosition() ;
			vec3 posInShadow=getShadowSpacePosition( posInWorld ,mShadowWorldViewProj0);
			if(posInShadow.x>0.0&&posInShadow.x<1.0&&posInShadow.y>0.0&&posInShadow.y<1.0)
			{
				bias = 1.0 - clamp(dot(normalize(N), posInShadow.xyz), 0.0, 1.0);
				bias = 0.0000000200 + 0.00000002 * bias;
				shadow_int0=getShadow(ShadowMapSampler, posInShadow.xy,
										posInShadow.z  + bias ,0);
			}
		}

		 
		//shadow_int = shadow_int0;
		//shadow_int -= brightness;
		shadow_int  = 1.0 - shadow_int0*0.35;
		
		
		//ccol[cIdx]=0.15;
		 diffuseLight=1.0;
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
	#if ENABLE_DYNAMIC_SHADOWS && DRAW_TYPE!=NDT_TORCHLIKE
	col = vec4(col.rgb * shadow_int, base.a);
	#else
	col = vec4(col.rgb , base.a);
	#endif
	gl_FragColor = col;
}
