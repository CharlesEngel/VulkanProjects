#version 450

layout (location = 0) out vec4 outFragPos;
layout (location = 1) out vec4 outFragNorm;
layout (location = 2) out vec4 outFragAlbedoSpecular;
layout (location = 3) out vec4 outFragAmbientOcclusion;

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNorm;
layout (location = 2) in vec3 inTangent;
layout (location = 3) in vec2 texCoord;

layout (set = 0, binding = 2) uniform sampler2D albedo;
layout (set = 0, binding = 3) uniform sampler2D specular;
layout (set = 0, binding = 4) uniform sampler2D normals;
layout (set = 0, binding = 5) uniform sampler2D metal;
layout (set = 0, binding = 6) uniform sampler2D ao;

void main()
{
	vec3 Norm = normalize(inNorm);
	vec3 Tangent = normalize(inTangent);
	vec3 biTangent = normalize(cross(Norm, Tangent));
	Tangent = normalize(cross(biTangent, Norm));
	mat3 TBN = (mat3(Tangent, biTangent, Norm));
	vec3 norm = texture(normals, texCoord).xyz;
	norm = normalize(TBN * norm);
	outFragPos = vec4(inPos, 1.0);
	outFragNorm = vec4(norm, texture(metal, texCoord).r);
	outFragAlbedoSpecular = vec4(texture(albedo, texCoord).xyz, texture(specular, texCoord).r);
	outFragAmbientOcclusion = vec4(vec3(texture(ao, texCoord).r), 1.0f);
}
