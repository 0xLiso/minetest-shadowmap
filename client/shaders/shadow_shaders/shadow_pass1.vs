uniform mat4 LightMVP;  //world matrix

varying vec4 tPos;

 
void main() {
 
    
    tPos = LightMVP *  vec4(gl_Vertex.xyz,1.0);
    
    gl_Position=vec4(tPos.xyz,1.0);
    gl_TexCoord[0].xy = gl_MultiTexCoord0.xy;

    
}