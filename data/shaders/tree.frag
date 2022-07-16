/*
 * Copyright (C) 2022 by Sascha Willems - www.saschawillems.de
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#version 450 core
#extension GL_GOOGLE_include_directive : require

#include "includes/constants.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inViewVec;
layout (location = 3) in vec3 inLightVec;
layout (location = 4) in vec3 inViewPos;

layout (set = 0, binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 modelview;
	vec4 lightDir;
	vec4 layers[6];
} ubo;

layout (set = 1, binding = 0) uniform sampler2D samplerColorMap;

layout (set = 2, binding = 0) uniform UBOParams
{
	uint shadows;
	uint fog;
	vec4 fogColor;
} params;

layout (location = 0) out vec4 outFragColor;

#include "includes/fog.glsl"

void main(void)
{
	vec4 colorMap = texture(samplerColorMap, inUV);
//	float a = textureLod(samplerColorMap, inUV, 0.0).a;
//
//	if (a < 0.1 /*material.alphaMaskCutoff*/) {
//		discard;
//	}
//
	// Lighting
	float amb = 0.75;
	float shadow = 1.0;
	vec3 N = normalize(inNormal);
//	vec3 L = normalize(-ubo.lightDir.xyz);
//	float diffuse = dot(N, L);
//
	vec3 L = normalize(inLightVec);
	vec3 V = normalize(inViewVec);
	vec3 R = reflect(-L, N);
	vec3 diffuse = max(dot(N, L), amb).rrr;
	float specular = pow(max(dot(R, V), 0.0), 32.0);
	specular = 0.0f;
	vec3 color = vec3(diffuse * colorMap.rgb + specular);
//	color = colorMap.rgb;

	if (params.fog == 1) {
		outFragColor = vec4(applyFog(color), colorMap.a);
	} else {
		outFragColor = vec4(color, colorMap.a);
	}
}
