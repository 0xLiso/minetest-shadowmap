uniform sampler2D ColorMapSampler;
varying vec4 tPos;

void main() {
    vec4 col = texture2D(ColorMapSampler, gl_TexCoord[0].st);

    float depth = 0.5 + tPos.z * 0.5;
    // ToDo: Liso: Apply movement on waving plants
    // depth in [0, 1] for texture

    //col.rgb = col.a == 1.0 ? vec3(1.0) : col.rgb;
    col.rgb*=col.a<0.8?col.a:0.0;
    gl_FragColor = vec4( depth, col.r, col.g, col.b);
}
