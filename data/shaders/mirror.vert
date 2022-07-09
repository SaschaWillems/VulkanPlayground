#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

layout (binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 model;
	vec4 cameraPos;
	vec4 lightDir;
	float time;
} ubo;

layout(push_constant) uniform PushConsts {
	mat4 scale;
	vec4 clipPlane;
	uint shadows;
	layout(offset = 96) vec3 pos;
} pushConsts;

layout (location = 0) out vec2 outUV;
layout (location = 1) out vec4 outPos;
layout (location = 2) out vec3 outNormal;
layout (location = 3) out vec3 outEyePos;
layout (location = 5) out vec3 outViewPos;
layout (location = 6) out vec3 outLPos;

void main() 
{
	outUV = inUV;
	vec3 pos = inPos;
	pos.xz *= 24.1 / 2.0;
	pos.xz += pushConsts.pos.xz;
	outPos = ubo.projection * ubo.model * vec4(pos, 1.0);
	outLPos = pos;
	outNormal = inNormal;
	outEyePos = ubo.cameraPos.xyz - pos;
	outViewPos = (ubo.model * vec4(pos, 1.0)).xyz;
	gl_Position = outPos;		
}
