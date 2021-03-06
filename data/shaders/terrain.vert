#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

layout (set = 0, binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 modelview;
	vec4 lightDir;
} ubo;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outViewVec;
layout (location = 3) out vec3 outLightVec;
layout (location = 4) out vec3 outEyePos;
layout (location = 5) out vec3 outViewPos;
layout (location = 6) out vec3 outPos;

layout(push_constant) uniform PushConsts {
	mat4 scale;
	vec4 clipPlane;
	uint shadows;
} pushConsts;

void main(void)
{
	outUV = inUV;
	outNormal = inNormal;
	vec4 pos = vec4(inPos, 1.0);
	if (pushConsts.scale[1][1] < 0) {
		pos.y *= -1.0f;
	}
	gl_Position = ubo.projection * ubo.modelview * pos;
	outPos = pos.xyz;
	outViewVec = -pos.xyz;
	outLightVec = normalize(ubo.lightDir.xyz + outViewVec);
	outEyePos = vec3(ubo.modelview * pos);
	outViewPos = (ubo.modelview * vec4(pos.xyz, 1.0)).xyz;

	// Clip against reflection plane
	if (length(pushConsts.clipPlane) != 0.0)  {
		gl_ClipDistance[0] = dot(pos, pushConsts.clipPlane);
	} else {
		gl_ClipDistance[0] = 0.0f;
	}
}