uniform sampler2D ColorMapSampler;
uniform int idx;
uniform float MapResolution;
varying vec4 tPos;

float getLinearDepth(in float depth) {
  float near=0.1;
  float far =20000.0;
  return 2.0f * near * far / (far + near - (2.0f * depth - 1.0f) * (far - near));
}

void main() {
    float alpha = texture2D(ColorMapSampler, gl_TexCoord[0].xy).a;

    if (alpha < 0.5) {
        discard;
    }
    float depth =  0.5 + (getLinearDepth(tPos.z) )*0.5;
    // ToDo: Liso: Apply movement on waving plants

    // depth in [0, 1] for texture

    gl_FragColor = vec4(depth, 0.0, 0.0, 1.0);

}
