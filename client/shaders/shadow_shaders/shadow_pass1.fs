uniform sampler2D ColorMapSampler;
uniform int idx;
uniform float MapResolution;
varying vec4 tPos;


 
void main() {
	float alpha = texture2D(ColorMapSampler, gl_TexCoord[0].xy).a;

	if(alpha <0.9){
	    discard;
	}   
	float depth = 0.5+(tPos.z/MapResolution)*0.5; 
   // ToDo: Liso: Apply movement on waving plants
    
    // depth in [0, 1] for texture
       
	gl_FragColor = vec4(depth,0.0,0.0,1.0);

}
