#version 450 core

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

layout (location = 0) out vec2 outUV;

layout (set = 0, binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 modelview;
} ubo;

layout(push_constant) uniform PushConsts {
	mat4 scale;
	vec4 clipPlane;
	int _dummy;
} pushConsts;

void main(void)
{
	gl_Position = ubo.projection * ubo.modelview * pushConsts.scale * vec4(inPos.xyz, 1.0);
	outUV = inUV;
	outUV.t = 1.0f - outUV.t;
}
