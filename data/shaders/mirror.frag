#version 450

#define SHADOW_MAP_CASCADE_COUNT 4
#define ambient 0.3

layout (binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 model;
	vec4 cameraPos;
	vec4 lightDir;
	float time;
} ubo;

layout (binding = 5) uniform UBOCSM {
	vec4 cascadeSplits;
	mat4 cascadeViewProjMat[SHADOW_MAP_CASCADE_COUNT];
	mat4 inverseViewMat;
	vec3 lightDir;
} uboCSM;

layout (set = 0, binding = 1) uniform sampler2D samplerRefraction;
layout (set = 0, binding = 2) uniform sampler2D samplerReflection;
layout (set = 0, binding = 3) uniform sampler2D samplerWaterNormalMap;
layout (set = 0, binding = 4) uniform sampler2DArray shadowMap;

layout (location = 0) in vec2 inUV;
layout (location = 1) in vec4 inPos;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec3 inEyePos;
layout (location = 5) in vec3 inViewPos;
layout (location = 6) in vec3 inLPos;

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

float shadowMapping()
{
	// Get cascade index for the current fragment's view position
	uint cascadeIndex = 0;
	for(uint i = 0; i < SHADOW_MAP_CASCADE_COUNT - 1; ++i) {
		if(inViewPos.z < uboCSM.cascadeSplits[i]) {	
			cascadeIndex = i + 1;
		}
	}

	// Depth compare for shadowing
	vec4 shadowCoord = (biasMat * uboCSM.cascadeViewProjMat[cascadeIndex]) * vec4(inLPos, 1.0);	

	float shadow = 0;
	return filterPCF(shadowCoord / shadowCoord.w, cascadeIndex);
}

void main() 
{
	// @todo: fog

	const vec4 tangent = vec4(1.0, 0.0, 0.0, 0.0);
	const vec4 viewNormal = vec4(0.0, -1.0, 0.0, 0.0);
	const vec4 bitangent = vec4(0.0, 0.0, 1.0, 0.0);
	const float distortAmount = 0.05;

	vec4 tmp = vec4(1.0 / inPos.w);
	vec4 projCoord = inPos * tmp;

	// Scale and bias
	projCoord += vec4(1.0);
	projCoord *= vec4(0.5);

	float t = clamp(ubo.time / 6., 0., 1.);

	vec2 coords = projCoord.st;
	vec2 dir = coords - vec2(.5);
	
	float dist = distance(coords, vec2(.5));
	vec2 offset = dir * (sin(dist * 80. - ubo.time*15.) + .5) / 30.;

	vec4 normal = texture(samplerWaterNormalMap, inUV * 8.0 + ubo.time);
	normal = normalize(normal * 2.0 - 1.0);

	vec4 viewDir = normalize(vec4(inEyePos, 1.0));
	vec4 viewTanSpace = normalize(vec4(dot(viewDir, tangent), dot(viewDir, bitangent), dot(viewDir, viewNormal), 1.0));	
	vec4 viewReflection = normalize(reflect(-1.0 * viewTanSpace, normal));
	float fresnel = dot(normal, viewReflection);	

	vec4 dudv = normal * distortAmount;

	if (gl_FrontFacing) {
		float shadow = shadowMapping();
		vec4 refraction = texture(samplerRefraction, vec2(projCoord) + dudv.st);
		vec4 reflection = texture(samplerReflection, vec2(projCoord) + dudv.st);
		outFragColor = mix(refraction, reflection, fresnel) * shadow;
	} else{
		outFragColor = vec4(0.0, 0.0, 0.0, 1.0);
	}

	outFragColor.a = 1.0;
//	outFragColor.rgb = fresnel.rrr;
}