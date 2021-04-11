uniform mat4 LightMVP;  //world matrix

varying vec4 tPos;


vec4 getDistortFactor(in vec4 shadowPosition) {
    const float bias0 = 0.95f;
    const float bias1 = 1.0f - bias0;

    float factorDistance =  sqrt(shadowPosition.x * shadowPosition.x +
                                 shadowPosition.y * shadowPosition.y  );
    //float factorDistance =  length(shadowPosition.xy);
    float distortFactor = factorDistance * bias0 + bias1;

    shadowPosition.xyz *= vec3(vec2(1.0 / distortFactor),  .75);

    return shadowPosition;
}

void main() {


    tPos = getDistortFactor(LightMVP *  gl_Vertex );

    gl_Position = vec4(tPos.xyz, 1.0);
    gl_TexCoord[0].st = (gl_TextureMatrix[0] * gl_MultiTexCoord0).st;


}