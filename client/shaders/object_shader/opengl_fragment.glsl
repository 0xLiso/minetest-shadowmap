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
	//shadow uniforms
	uniform vec3 v_LightDirection;
	uniform float f_textureresolution;
	uniform mat4 m_ShadowViewProj;
	uniform float f_shadowfar;
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
	
	#ifdef SHADOWS_PSM
    const float bias0 = 0.94;
    
    const float zPersFactor = 0.25;

    vec4 getPerspectiveFactor(in vec4 shadowPosition) {
      float bias1 = 1.0 - bias0;
      float pDistance =  sqrt(shadowPosition.x * shadowPosition.x +
          shadowPosition.y * shadowPosition.y );
      float pFactor = pDistance * bias0 + bias1;
      shadowPosition.xyz *= vec3(vec2(1.0 / pFactor), zPersFactor);

      return shadowPosition;
    }
	#endif


	//assuming near is allways 1.0
	float getLinearDepth() {
		//float near=1.0;
		//float far=f_shadowfar;
	  	//return 2.0f * near * far / (far + near - (2.0f * gl_FragCoord.z - 1.0f) * (far - near));
	  	return  2.0f * f_shadowfar / (f_shadowfar + 1.0 - (2.0 * gl_FragCoord.z - 1.0) * (f_shadowfar - 1.0));
	}

	vec3 getLightSpacePosition()
	{	
		vec4 pLightSpace;
		//some NDT have normals to 0, so we need to handle it :(
		if(f_normal_length<0.01){
			pLightSpace = m_ShadowViewProj  * vec4(worldPosition+0.000000005  ,1.0); 
		}
		else{
			float offsetScale = (0.03* getLinearDepth()+ normalOffsetScale) ;
			pLightSpace = m_ShadowViewProj  * vec4(worldPosition+  offsetScale* normalize(vNormal) ,1.0); 
		}

		#ifdef SHADOWS_PSM
			pLightSpace = getPerspectiveFactor(pLightSpace);
		#endif
		return pLightSpace.xyz*0.5 +0.5;
		
	}


	//custom smoothstep implementation because it's not defined in glsl1.2
	//	https://docs.gl/sl4/smoothstep
	float mtsmoothstep(in float edge0, in float edge1, in float x ){
		float t;
	    t = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
	    return t * t * (3.0 - 2.0 * t);
	}


	

	float getHardShadow(sampler2D shadowsampler, vec2 smTexCoord, float realDistance)
	{
		float texDepth = texture2D(shadowsampler, smTexCoord.xy).r;
		float visibility = step(0.0 ,realDistance - texDepth);
		return visibility;
	}

	float getShadow(sampler2D shadowsampler, vec2 smTexCoord, float realDistance)
	{
		vec2 clampedpos;
		float visibility=0.0;

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
		//basic PCF filter. we can explorer Poisson filter
		for (y = -PCFBOUND ; y <=PCFBOUND ; y+=1.0)
			for (x = -PCFBOUND ; x <=PCFBOUND ; x+=1.0)
		{
			clampedpos = vec2(x,y)*texture_size + smTexCoord.xy;
			visibility += getHardShadow(shadowsampler, clampedpos.xy, realDistance);
		}
		
		return visibility/PCFSAMPLES;
	}


	#ifdef COLORED_SHADOWS
	// c_precision of 128 fits within 7 base-10 digits
	const float c_precision = 128.0;
	const float c_precisionp1 = c_precision + 1.0;
	 
	float packColor(vec3 color) {
	   
	    return floor(color.r * c_precision + 0.5) 
	        + floor(color.b * c_precision + 0.5) * c_precisionp1
	        + floor(color.g * c_precision + 0.5) * c_precisionp1 * c_precisionp1;
	}

	vec3 unpackColor(float value) {
	    vec3 color;
	    color.r = mod(value, c_precisionp1) / c_precision;
	    color.b = mod(floor(value / c_precisionp1), c_precisionp1) / c_precision;
	    color.g = floor(value / (c_precisionp1 * c_precisionp1)) / c_precision;
	    return color;
	}


	
	vec4 getHardShadowColor(sampler2D shadowsampler, vec2 smTexCoord, float realDistance)
	{
		vec4 texDepth = texture2D(shadowsampler, smTexCoord.xy).rgba;

		float visibility = step(0.0,realDistance - texDepth.r);
		vec4 result = vec4(visibility,unpackColor(texDepth.g));
		if(visibility<0.1){
			visibility = step(0.0,	realDistance - texDepth.b);
			result = vec4(visibility,unpackColor(texDepth.a));
		}
		return result;

	}

	vec4 getShadowColor(sampler2D shadowsampler, vec2 smTexCoord, float realDistance)
	{
	 
		vec2 clampedpos;
		vec4 visibility=vec4(0.0);

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
		//basic PCF filter. we can explorer Poisson filter
		for (y = -PCFBOUND ; y <=PCFBOUND ; y+=1.0)
			for (x = -PCFBOUND ; x <=PCFBOUND ; x+=1.0)
		{
			clampedpos = vec2(x,y)*texture_size + smTexCoord.xy;
			visibility += getHardShadowColor(shadowsampler, clampedpos.xy, realDistance);

		}
		
		return visibility/PCFSAMPLES;
	 
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
	
	//check if the surface doesn't have normal, like billboards
	//if the surface is pointing backwards light, it's in shadow
	
	{
		
		vec3 posinLightSpace=getLightSpacePosition( );

		#ifdef COLORED_SHADOWS
			vec4 visibility=getShadowColor(ShadowMapSampler, posinLightSpace.xy,
									posinLightSpace.z  );			
			shadow_int=visibility.r;
			shadow_color=visibility.gba;
		#else
			shadow_int=getShadow(ShadowMapSampler, posinLightSpace.xy,
									posinLightSpace.z  );
		#endif

			//shadow_int*= 1.0 - mtsmoothstep(0.7,0.9, length(posinLightSpace-vec3(0.5)));

	}
	if( f_normal_length!=0 && cosLight<= 0){
		shadow_int=clamp(shadow_int + (0.3 - cosLight),0.0,1.0);
	}

	shadow_int  = 1.0 - (shadow_int*adj_shadow_strength);
	shadow_color *= adj_shadow_strength ;
	
	col.rgb=col.rgb*shadow_int+shadow_color;
	
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
