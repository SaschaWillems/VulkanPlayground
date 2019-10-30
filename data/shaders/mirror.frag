#version 450

layout (binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 model;
	vec4 cameraPos;
	float time;
} ubo;

layout (binding = 1) uniform sampler2D samplerRefraction;
layout (binding = 2) uniform sampler2D samplerReflection;
layout (binding = 3) uniform sampler2D samplerWaterNormalMap;

layout (location = 0) in vec2 inUV;
layout (location = 1) in vec4 inPos;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec3 inEyePos;

layout (location = 0) out vec4 outFragColor;

void main() 
{
	const vec4 tangent = vec4(1.0, 0.0, 0.0, 0.0);
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

	vec3 viewReflection = normalize(reflect(-1.0 * inEyePos, normal.xyz));
	float fresnel = dot(normal.xyz, viewReflection);

	vec4 dudv = normal * distortAmount;

	if (gl_FrontFacing) {
		vec4 refraction = texture(samplerRefraction, vec2(projCoord) + dudv.st);
		vec4 reflection = texture(samplerReflection, vec2(projCoord) + dudv.st);
		outFragColor = mix(refraction, reflection, /*fresnel*/ 0.5);
		outFragColor = reflection;
	} else{
		outFragColor = vec4(0.0, 0.0, 0.0, 1.0);
	}

	outFragColor.a = 1.0;
	//outFragColor.rgb = normal.rgb;
}