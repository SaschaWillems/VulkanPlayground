/*
 * Copyright (C) 2022 by Sascha Willems - www.saschawillems.de
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#version 450 core
#extension GL_GOOGLE_include_directive : require

layout (location = 0) in vec2 inUV;

layout (set = 1, binding = 0) uniform sampler2D samplerColorMap;

layout (location = 0) out vec4 outFragColor;

#include "includes/fog.glsl"

void main(void)
{
	vec4 color = texture(samplerColorMap, inUV);

//	if (color.a < 0.5 /*material.alphaMaskCutoff*/) {
//		discard;
//	}

	outFragColor.rgb = applyFog(color.rgb);
	outFragColor.a = color.a;
}
