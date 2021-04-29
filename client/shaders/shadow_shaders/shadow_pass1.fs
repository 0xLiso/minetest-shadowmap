uniform sampler2D ColorMapSampler;
varying vec4 tPos;

// we have to check the colored shadows too, 
// because some solid blocks are already translucent :/
// but IDK yet how to extract that info.
 #ifdef COLORED_SHADOWS

//none yet

#endif

void main() {
    vec4 col = texture2D(ColorMapSampler, gl_TexCoord[0].st);


    if (col.a < 0.70) {
            discard;
    }
 
    float depth = 0.5 + tPos.z * 0.5;
    // ToDo: Liso: Apply movement on waving plants
    gl_FragColor = vec4( depth, 0.0, 0.0, 1.0);
 
}
