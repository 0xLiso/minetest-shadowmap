uniform sampler2D ShadowMapClientMap0;
uniform sampler2D ShadowMapClientMap1;
uniform sampler2D ShadowMapClientMap2;

uniform sampler2D ShadowMapSamplerdynamic;

#ifdef COLORED_SHADOWS
uniform sampler2D ShadowMapClientMapTraslucent;
#endif


void main() {

#ifdef COLORED_SHADOWS
	vec3 first_depth = texture2D(ShadowMapClientMap0, gl_TexCoord[0].st).rgb;
	vec2 depth_splitdynamics = vec2(texture2D(ShadowMapSamplerdynamic, gl_TexCoord[2].st).r, 0.0);
	if (first_depth.r > depth_splitdynamics.r)
		first_depth.r = depth_splitdynamics;
	vec2 depth_color = texture2D(ShadowMapClientMapTraslucent, gl_TexCoord[1].st).rg;
	gl_FragColor = vec4(first_depth.r, first_depth.g, depth_color.r, depth_color.g);
#else
	vec3 first_depth=vec3(0.0);
	first_depth.r = texture2D(ShadowMapClientMap0, gl_TexCoord[0].st).r;
	float depth_splitdynamics = texture2D(ShadowMapSamplerdynamic, gl_TexCoord[2].st).r;
	first_depth.r = min(first_depth.r, depth_splitdynamics);
	first_depth.g = texture2D(ShadowMapClientMap1, gl_TexCoord[0].st).r;
	first_depth.b = texture2D(ShadowMapClientMap2, gl_TexCoord[0].st).r;
	gl_FragColor = vec4(first_depth.rgb, 1.0);
#endif

}
