uniform sampler2D baseTexture;

uniform vec4 emissiveColor;
uniform vec4 skyBgColor;
uniform float fogDistance;
uniform vec3 eyePosition;

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

varying vec3 P;
varying vec3 N;
#endif


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



#ifdef DYNAMIC_SHADOWS_VMS

float linstep(float low,float high,float v){
    return clamp((v-low)/(high-low),0.0,1.0);

 }

 float getShadow(sampler2D shadowsampler,vec2 texCoords, float RealDist ,int cIdx) {
    vec2 shadTexCol = texture2D(shadowsampler, texCoords.xy ).ra;

    float p= step(RealDist,shadTexCol.x);
    float variance = min(max(shadTexCol.y - shadTexCol.x*shadTexCol.x,0.00002),1.0);
    float d = RealDist - shadTexCol.x;
    float pMax = linstep(0.9,1.0,variance / (variance +  d * d));

    return 1-min(max(p,pMax),1.0);
}


#else 


	vec2 poissonDisk[4] = vec2[](
	  vec2( -0.94201624, -0.39906216 ),
	  vec2( 0.94558609, -0.76890725 ),
	  vec2( -0.094184101, -0.92938870 ),
	  vec2( 0.34495938, 0.29387760 )
	);
	vec2 offsetArray[16]=vec2[](
		 vec2(0.0, 0.0),
	     vec2(0.0, 1.0),
	     vec2(1.0, 0.0),
	     vec2(1.0, 1.0),
	     vec2(-2.0, 0.0),
	     vec2(0.0, -2.0),
	     vec2(2.0, -2.0),
	     vec2(-2.0, 2.0),
	     vec2(3.0, 0.0),
	     vec2(0.0, 3.0),
	     vec2(3.0, 3.0),
	     vec2(-3.0, -3.0),
	     vec2(-4.0, 0.0),
	     vec2(0.0, -4.0),
	     vec2(4.0, -4.0),
	     vec2(-4.0, 4.0));
	float getShadow(sampler2D shadowsampler, vec2 smTexCoord, float realDistance ,int cIdx) {
	     
		float nsamples=16;
	    float visibility=0.0;

	    
	    
	    for (int i = 0; i < nsamples; i++){
	        vec2 clampedpos = smTexCoord + (offsetArray[i] /vec2(f_textureresolution));

	        //clampedpos=clamp(clampedpos.xy, vec2(0.0, 0.0), vec2(1.0, 1.0));         
	        if ( texture2D( shadowsampler, clampedpos.xy )[cIdx] < realDistance ){
	            visibility += 1.0 / nsamples;
	        }        
	        
	    }
	    
	    return pow(visibility ,2.2);
	}

#endif
	float getShadow2(sampler2D shadowsampler, vec2 smTexCoord, float realDistance ,int cIdx) {
	    float texDepth = texture2D(shadowsampler, smTexCoord.xy )[cIdx];
 
	    return (realDistance  > texDepth ) ?  1.0  :0.0 ;
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

	vec4 col = vec4(color.rgb, base.a);

	col.rgb *= varColor.rgb;

	col.rgb *= emissiveColor.rgb * vIDiff;

#if ENABLE_DYNAMIC_SHADOWS && DRAW_TYPE!=NDT_TORCHLIKE
		float shadow_int =0.0;
		vec3 ccol=vec3(0.0);
		float diffuseLight = dot(normalize(-v_LightDirection),normalize(N)) ;

		 

	    float bias = max(0.0005 * (1.0 - diffuseLight), 0.000005) ;  
	    bias=0;
	    float NormalOffsetScale= 2.0+2.0/f_textureresolution;
	    float SlopeScale = abs(1-diffuseLight);
		NormalOffsetScale*=SlopeScale;
	    vec3 posNormalbias = P.xyz + N.xyz*NormalOffsetScale;
		diffuseLight=clamp(diffuseLight+0.2,0.5,1.0);
		 
		
		float brightness = rgb2hsl(col).b;//(col.r+col.g+col.b)/3.0;
		
		vec3 posInWorld= gl_TexCoord[3].xyz ;
			     
		vec4 posInShadow= 0.5+0.5*mShadowWorldViewProj0*vec4(posNormalbias.xyz,1.0);
		if(posInShadow.x>=0.0&&posInShadow.x<=1.0&&posInShadow.y>=0.0&&posInShadow.y<=1.0)
		shadow_int=getShadow(ShadowMapSampler, posInShadow.xy, posInShadow.z +bias ,0);
		

		 
		//shadow_int -= brightness;
		shadow_int *= 0.25;
		
		//ccol[cIdx]=0.15;
		diffuseLight=1.0;
		col = clamp(vec4((col.rgb-shadow_int)*diffuseLight+ccol,col.a),0.0,1.0);
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

	gl_FragColor = vec4(col.rgb, base.a);
}
