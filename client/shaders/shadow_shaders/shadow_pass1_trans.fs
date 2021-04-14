uniform sampler2D ColorMapSampler;
uniform int idx;
uniform float MapResolution;
varying vec4 tPos;

float getLinearDepth(in float depth) {
  float near = 0.1;
  float far  = 20000.0;
  return 2.0 * near * far / (far + near - (2.0 * depth - 1.0) * (far - near));
}

// encode vec3 rgb to 1 float 
// ripped from https://stackoverflow.com/questions/6893302/decode-rgb-value-to-single-float-without-bit-shift-in-glsl

    float packColor(vec4 v) {
        return dot( v, vec4(1.0, 1/255.0, 1/65025.0, 1/16581375.0) );
    }

void main() {
    vec4 col = texture2D(ColorMapSampler, gl_TexCoord[0].xy);
    if (col.a == 0.0) {
        discard;
    }
    
    float depth = 0.5 + (getLinearDepth(tPos.z) ) * 0.5;
    // ToDo: Liso: Apply movement on waving plants
    // depth in [0, 1] for texture
    float packedcolor = packColor(col);
    col.rgb*=col.a;
    gl_FragColor = vec4(depth,col.r,col.g,col.b);
}
