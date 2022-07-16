/*
 * Copyright (C) 2022 by Sascha Willems - www.saschawillems.de
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#version 450 core

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

// Instanced attributes
layout (location = 3) in vec3 instancePos;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outViewVec;
layout (location = 3) out vec3 outLightVec;
layout (location = 4) out vec3 outViewPos;

layout (set = 0, binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 modelview;
	vec4 lightDir;
} ubo;

layout(push_constant) uniform PushConsts {
	mat4 scale;
	vec4 clipPlane;
	int _dummy;
	layout(offset = 96) vec3 pos;
} pushConsts;

void main(void)
{
	outUV = inUV;
	outNormal = inNormal;

	vec4 pos = vec4(inPos, 1.0);
	//pos.xyz += pushConsts.pos.xyz;
	pos.xyz += instancePos + pushConsts.pos;
	if (pushConsts.scale[1][1] < 0) {
		pos.y *= -1.0f;
	}
	gl_Position = ubo.projection * ubo.modelview * pos;

	outViewVec = -pos.xyz;
	outLightVec = normalize(ubo.lightDir.xyz + outViewVec);
	outViewPos = (ubo.modelview * vec4(pos.xyz, 1.0)).xyz;

	// Clip against reflection plane
	if (length(pushConsts.clipPlane) != 0.0)  {
		gl_ClipDistance[0] = dot(pos, pushConsts.clipPlane);
	} else {
		gl_ClipDistance[0] = 0.0f;
	}
}
