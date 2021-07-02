uniform mat4 LightMVP; // world matrix
varying vec4 tPos;


vec4 getPerspectiveFactor(in vec4 shadowPosition)
{
	return shadowPosition;
}


void main()
{
	vec4 pos = LightMVP * gl_Vertex;

	tPos = getPerspectiveFactor(LightMVP * gl_Vertex);

	gl_Position = vec4(tPos.xyz, 1.0);
	gl_TexCoord[0].st = gl_MultiTexCoord0.st;
}
