uniform mat4 LightMVP;  //world matrix
varying vec4 tPos;
 
const float bias0 = 0.9;
const float bias1 = 0.1; //1.0 - bias0;
const float zdistorFactor = 0.5;

vec4 getDistortFactor(in vec4 shadowPosition) {

  float factorDistance =  sqrt(shadowPosition.x * shadowPosition.x +
      shadowPosition.y * shadowPosition.y );
  //float factorDistance =  length(shadowPosition.xy);
  float distortFactor = factorDistance * bias0 + bias1;
  shadowPosition.xyz *= vec3(vec2(1.0 / distortFactor), zdistorFactor);

  return shadowPosition;
}

void main() {
	  tPos = getDistortFactor(LightMVP *   gl_Vertex);

    gl_Position = vec4(tPos.xyz, 1.0);
    gl_TexCoord[0].st = gl_MultiTexCoord0.st;
}