uniform sampler2D ColorMapSampler;
uniform int idx;
uniform float MapResolution;
varying vec4 tPos;

float getLinearDepth(in float depth) {
    float near = 0.1;
    float far  = 20000.0;
    return 2.0 * near * far / (far + near - (2.0 * depth - 1.0) * (far - near));
}


void main() {
    vec4 col = texture2D(ColorMapSampler, gl_TexCoord[0].st);

    float depth = 0.5 + (getLinearDepth(tPos.z) ) * 0.5;
    // ToDo: Liso: Apply movement on waving plants
    // depth in [0, 1] for texture

    //col.rgb = col.a == 1.0 ? vec3(1.0) : col.rgb;
    col.rgb*=col.a;
    gl_FragColor = vec4( depth, col.r, col.g, col.b);
}
