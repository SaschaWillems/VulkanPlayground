#version 450

layout (set = 0, binding = 1) uniform sampler2D samplerHeight; 
layout (set = 0, binding = 2) uniform sampler2DArray samplerLayers;
layout (set = 0, binding = 3) uniform sampler2DArray shadowMap;
layout (set = 0, binding = 4) uniform sampler2D samplerGradient;

layout (set = 0, binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 modelview;
	vec4 lightDir;
	vec4 layers[6];
} ubo;

#define SHADOW_MAP_CASCADE_COUNT 4
#define ambient 0.2

layout (binding = 5) uniform UBOCSM {
	vec4 cascadeSplits;
	mat4 cascadeViewProjMat[SHADOW_MAP_CASCADE_COUNT];
	mat4 inverseViewMat;
	vec4 lightDir;
} uboCSM;

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
layout (location = 7) flat in vec3 inColor;
layout (location = 8) in float inTerrainHeight;

layout (location = 0) out vec4 outFragColor;

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 
);

float textureProj(vec4 shadowCoord, vec2 offset, uint cascadeIndex)
{
	float shadow = 1.0;
	float bias = 0.005;

	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 ) {
		float dist = texture(shadowMap, vec3(shadowCoord.st + offset, cascadeIndex)).r;
		if (shadowCoord.w > 0 && dist < shadowCoord.z - bias) {
			shadow = ambient;
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

vec3 sampleTerrainLayer()
{
	vec3 color = vec3(0.0);
	
	// Get height from displacement map
	//float height = textureLod(samplerHeight, inUV, 0.0).r * 255.0;

	float height = inTerrainHeight;
	
	for (int i = 0; i < ubo.layers.length(); i++) {
		float start = ubo.layers[i].x - ubo.layers[i].y / 2.0;
		float end = ubo.layers[i].x + ubo.layers[i].y / 2.0;

		float range = end - start;
		float weight = (range - abs(height - end)) / range;
		weight = max(0.0, weight);
		color += weight * texture(samplerLayers, vec3(inUV * 32.0, i)).rgb;
	}

	return color;
}

float fog(float density)
{
	const float LOG2 = -1.442695;
	float dist = gl_FragCoord.z / gl_FragCoord.w * 0.1;
	float d = density * dist;
	return 1.0 - clamp(exp2(d * d * LOG2), 0.0, 1.0);
}

void main()
{
	// Get cascade index for the current fragment's view position
	uint cascadeIndex = 0;
	for(uint i = 0; i < SHADOW_MAP_CASCADE_COUNT - 1; ++i) {
		if(inViewPos.z < uboCSM.cascadeSplits[i]) {	
			cascadeIndex = i + 1;
		}
	}

	// Depth compare for shadowing
	vec4 shadowCoord = (biasMat * uboCSM.cascadeViewProjMat[cascadeIndex]) * vec4(inPos, 1.0);	

	float shadow = 0;
	bool enablePCF = false;
	if (pushConsts.shadows > 0) {
		if (enablePCF) {
			shadow = filterPCF(shadowCoord / shadowCoord.w, cascadeIndex);
		} else {
			shadow = textureProj(shadowCoord / shadowCoord.w, vec2(0.0), cascadeIndex);
		}
		if (inPos.y > 0.0f) {
			shadow = 1.0f;
		}
	} else {
		shadow =  1.0f;
	}

	const vec3 fogColor = vec3(0.47, 0.5, 0.67);
	// Directional light
	vec3 N = normalize(inNormal);
	vec3 L = normalize(-ubo.lightDir.xyz);
	float diffuse = dot(N, L);
//	vec3 color = (ambient.rrr + (shadow) * (diffuse/* + specular*/)) * texture(samplerHeight, inUV).rgb; //* sampleTerrainLayer();
	vec3 color = (ambient.rrr + (diffuse/* + specular*/)) * sampleTerrainLayer();
	//vec3 color = (ambient.rrr + (diffuse/* + specular*/)) * texture(samplerHeight, inUV).rgb; //* sampleTerrainLayer();
	outFragColor = vec4(color, 1.0);
//	color = sampleTerrainLayer();
	//outFragColor.rgb = (ambient.rrr + (shadow) * (diffuse/* + specular*/)) * inColor;
//	outFragColor.rgb = 
	// Apply fog
	outFragColor.rgb = mix(color, fogColor, fog(0.05));

	// Color cascades (if enabled)
	bool colorCascades = false;
	if (colorCascades) {
		switch(cascadeIndex) {
			case 0 : 
				outFragColor.rgb *= vec3(1.0f, 0.25f, 0.25f);
				break;
			case 1 : 
				outFragColor.rgb *= vec3(0.25f, 1.0f, 0.25f);
				break;
			case 2 : 
				outFragColor.rgb *= vec3(0.25f, 0.25f, 1.0f);
				break;
			case 3 : 
				outFragColor.rgb *= vec3(1.0f, 1.0f, 0.25f);
				break;
		}
	}

//	outFragColor.rgb = texture(samplerHeight, inUV).rgb;
//	outFragColor.rgb = inColor;
//	outFragColor.rgb = vec3(diffuse);

//	outFragColor.rgb = texture(samplerGradient, vec2(inTerrainHeight, 0.5)).rgb;

}