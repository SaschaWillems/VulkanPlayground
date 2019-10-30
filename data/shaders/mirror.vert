#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

layout (binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 model;
	vec4 cameraPos;
	float time;
} ubo;

layout (location = 0) out vec2 outUV;
layout (location = 1) out vec4 outPos;
layout (location = 2) out vec3 outNormal;
layout (location = 3) out vec3 outEyePos;

void main() 
{
	outUV = inUV;
	outPos = ubo.projection * ubo.model * vec4(inPos.xyz, 1.0);
	outNormal = inNormal;
	gl_Position = outPos;		
	outEyePos = -ubo.cameraPos.xyz - vec3(ubo.model * vec4(inPos.xyz, 1.0));
}
