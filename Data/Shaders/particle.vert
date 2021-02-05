#version 450
#include "ubos.glsl"

layout(location = 0) in vec4 inPosUv;
layout(location = 1) in vec4 inst_Pos;			// xyz - Particle position in world space, w - unused
layout(location = 2) in vec4 inst_Color;

layout(location = 0) out vec2 uv;
layout(location = 1) out vec4 particleColor;

void main()
{
	particleColor = inst_Color;

	mat4 modelMatrix = mat4(1.0);										// Constructor set all values in the diagonal to 1 (identity)
	modelMatrix[3].xyzw = vec4(inst_Pos.xyz, 1.0);
	
	mat4 modelView = viewMatrix * modelMatrix;
	
	modelView[0][0] = 0.5; 
	modelView[0][1] = 0.0;
	modelView[0][2] = 0.0;

	// Second colunm
	modelView[1][0] = 0.0;
	modelView[1][1] = 0.5;
	modelView[1][2] = 0.0;

	// Third colunm
	modelView[2][0] = 0.0;
	modelView[2][1] = 0.0;
	modelView[2][2] = 0.5;
	
	uv = inPosUv.zw;
	
    gl_Position = projectionMatrix * modelView * vec4(inPosUv.xy, 0.0, 1.0);
}