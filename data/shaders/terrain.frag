/*
 * Copyright (C) 2022 by Sascha Willems - www.saschawillems.de
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#version 450
#extension GL_GOOGLE_include_directive : require

#include "includes/constants.glsl"

layout (set = 0, binding = 1) uniform sampler2D samplerHeight; 
layout (set = 0, binding = 2) uniform sampler2DArray samplerLayers;
layout (set = 0, binding = 3) uniform sampler2DArray shadowMap;

layout (set = 0, binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 modelview;
	vec4 lightDir;
	vec4 layers[6];
} ubo;

layout (set = 0, binding = 4) uniform UBOCSM {
	vec4 cascadeSplits;
	mat4 cascadeViewProjMat[SHADOW_MAP_CASCADE_COUNT];
	mat4 inverseViewMat;
	vec4 lightDir;
} uboCSM;

layout (set = 1, binding = 0) uniform UBOParams
{
	uint shadows;
	uint fog;
	vec4 fogColor;
} params;

layout(push_constant) uniform PushConsts {
	mat4 scale;
	vec4 clipPlane;
	uint shadows;
} pushConsts;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inViewVec;
layout (location = 3) in vec3 inLightVec;
layout (location = 4) in vec3 inEyePos;
layout (location = 5) in vec3 inViewPos;
layout (location = 6) in vec3 inPos;
layout (location = 7) in float inTerrainHeight;

layout (location = 0) out vec4 outFragColor;

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 
);

#include "includes/fog.glsl"

float textureProj(vec4 shadowCoord, vec2 offset, uint cascadeIndex)
{
	float shadow = 0.0;
	float bias = 0.005;
	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 ) {
		float dist = texture(shadowMap, vec3(shadowCoord.st + offset, cascadeIndex)).r;
		if (shadowCoord.w > 0 && dist < shadowCoord.z - bias) {
			shadow = 1.0;
		}
	}
	return shadow;

}

float filterPCF(vec4 sc, uint cascadeIndex)
{
	ivec2 texDim = textureSize(shadowMap, 0).xy;
	float scale = 0.75;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 1;
	
	for (int x = -range; x <= range; x++) {
		for (int y = -range; y <= range; y++) {
			shadowFactor += textureProj(sc, vec2(dx*x, dy*y), cascadeIndex);
			count++;
		}
	}
	return shadowFactor / count;
}

vec3 triPlanarBlend(vec3 worldNormal){
	vec3 blending = abs(worldNormal);
	blending = normalize(max(blending, 0.00001));
	float b = (blending.x + blending.y + blending.z);
	blending /= vec3(b, b, b);
	return blending;
}

vec3 sampleTerrainLayer()
{
	vec3 color = vec3(0.0);
	float texRepeat = 0.125f;
	vec3 blend = triPlanarBlend(inNormal);
	for (int i = 0; i < ubo.layers.length(); i++) {
		float start = ubo.layers[i].x - ubo.layers[i].y / 2.0;
		float end = ubo.layers[i].x + ubo.layers[i].y / 2.0;
		float range = end - start;
		float weight = (range - abs(inTerrainHeight - end)) / range;
		weight = max(0.0, weight);
		// Triplanar mapping
		vec3 xaxis = texture(samplerLayers, vec3(inPos.yz * texRepeat, i)).rgb;
		vec3 yaxis = texture(samplerLayers, vec3(inPos.xz * texRepeat, i)).rgb;
		vec3 zaxis = texture(samplerLayers, vec3(inPos.xy * texRepeat, i)).rgb;
		vec3 texColor = xaxis * blend.x + yaxis * blend.y + zaxis * blend.z;
		color += weight * texColor;
		
//		vec3 uv = vec3(inUV * 32.0, 1);
//		color += weight * texture(samplerLayers, uv).rgb;
	}
	return color;
}

void main()
{

	// Shadows
	float shadow = 0.0f;
	bool enablePCF = false;
	if (params.shadows > 0) {
		// Get cascade index for the current fragment's view position
		uint cascadeIndex = 0;
		for(uint i = 0; i < SHADOW_MAP_CASCADE_COUNT - 1; ++i) {
			if(inViewPos.z < uboCSM.cascadeSplits[i]) {	
				cascadeIndex = i + 1;
			}
		}
		// Depth compare for shadowing
		vec4 shadowCoord = (biasMat * uboCSM.cascadeViewProjMat[cascadeIndex]) * vec4(inPos, 1.0);	
		if (enablePCF) {
			shadow = filterPCF(shadowCoord / shadowCoord.w, cascadeIndex);
		} else {
			shadow = textureProj(shadowCoord / shadowCoord.w, vec2(0.0), cascadeIndex);
		}
	}

	// Directional light
	vec3 N = normalize(inNormal);
	vec3 L = normalize(-ubo.lightDir.xyz);
	float diffuse = dot(N, L);
	vec3 color = (ambient + (1.0 - shadow) * diffuse) * sampleTerrainLayer();

	if (params.fog == 1) {
		outFragColor = vec4(applyFog(color), 1.0);
	} else {
		outFragColor = vec4(color, 1.0);
	}

}