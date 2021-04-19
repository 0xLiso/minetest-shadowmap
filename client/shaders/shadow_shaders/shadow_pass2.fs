
uniform sampler2D ShadowMapClientMap;
#ifdef COLORED_SHADOWS
uniform sampler2D ShadowMapClientMapTraslucent;
#endif
uniform sampler2D ShadowMapSamplerdynamic;

void main() {

#ifdef COLORED_SHADOWS
    float depth_map = texture2D(ShadowMapClientMap, gl_TexCoord[0].st ).r ;
    float depth_splitdynamics = texture2D(ShadowMapSamplerdynamic, gl_TexCoord[2].st ).r ;
    vec4 depth_color = texture2D(ShadowMapClientMapTraslucent, gl_TexCoord[1].st ) ;
    float first_depth = min(depth_map, depth_splitdynamics);
    if ( false) {
        gl_FragColor = vec4(depth_color.r, depth_color.g, depth_color.b, depth_color.a);
    } else {
        gl_FragColor = vec4(first_depth, depth_color.g, depth_color.b, depth_color.a);
    }
#else
    float depth_map = texture2D(ShadowMapClientMap, gl_TexCoord[0].st ).r ;
    float depth_splitdynamics = texture2D(ShadowMapSamplerdynamic, gl_TexCoord[2].st ).r ;

    float first_depth = min(depth_map, depth_splitdynamics);

    gl_FragColor = vec4(first_depth, 0.0, 0.0, 1.0);
#endif

}
