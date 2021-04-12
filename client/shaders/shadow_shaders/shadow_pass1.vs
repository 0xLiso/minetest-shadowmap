uniform mat4 LightMVP;  //world matrix
uniform float animationTimer;
varying vec4 tPos;
 

vec4 getDistortFactor(in vec4 shadowPosition) {

  const float bias0 = 0.9f;
  const float bias1 = 1.0f - bias0;

  float factorDistance =  sqrt(shadowPosition.x * shadowPosition.x +
  							   shadowPosition.y * shadowPosition.y );
  //float factorDistance =  length(shadowPosition.xy);
  float distortFactor = factorDistance * bias0 + bias1;

    shadowPosition.xyz *= vec3(vec2(1.0 / distortFactor), .75);

  return shadowPosition;
}

vec4 getDistortFactorv2(in vec4 shadowPosition) {
  const float DistortPower = 7.0f;
  const float SHADOW_MAP_BIAS = 0.9f;
  vec2 p=shadowPosition.xy;
  p = abs(p);
  p = p * p * p;
  float distordLengh=pow(p.x + p.y, 1.0f / 3.0f);
  float len = 1e-6 + distordLengh;
  distordLengh =  (1.0f - SHADOW_MAP_BIAS) + len * SHADOW_MAP_BIAS;
  vec2 distortedcoords =  shadowPosition.xy / min(distordLengh, 1.0f);

  return vec4(distortedcoords.xy,shadowPosition.z * 0.2,1.0);
}
void main() {
	vec2 varTexCoord = (gl_TextureMatrix[0] * gl_MultiTexCoord0).st;

    tPos = getDistortFactor(LightMVP *   gl_Vertex);

    gl_Position = vec4(tPos.xyz, 1.0);
    gl_TexCoord[0].st = varTexCoord;


}