#define ENABLE_DYNAMIC_SHADOWS_VMS 1

uniform sampler2D ColorMapSampler;
uniform int idx;
uniform float MapResolution;
varying vec4 tPos;
void main() {
float alpha = texture2D(ColorMapSampler, gl_TexCoord[0].xy).a;

if(alpha <0.75){
    discard;
}   
float depth = 0.5+(0.5*tPos.z); 

#ifdef ENABLE_DYNAMIC_SHADOWS_VMS

	float dx = dFdx(depth);
    float dy = dFdy(depth);
    float moment2 = depth * depth + 0.25 * (dx * dx + dy * dy);
    gl_FragColor = vec4(depth, moment2, 0.0, 1.0);
#else
   // ToDo: Liso: Apply movement on waving plants
    
    // depth in [0, 1] for texture
       
	gl_FragColor = vec4(depth,0.0,0.0,1.0);

#endif
}
