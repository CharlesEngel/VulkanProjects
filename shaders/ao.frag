#version 450

layout (location = 0) out vec4 outFragColor;

layout (location = 0) in vec2 texCoord;

layout (set = 0, binding = 0) uniform AOData
{
	mat4 proj;
	vec4[64] kernel;
	vec4[16] rotation;
	vec4 radBiasContrastAspect;
} aoData;

layout (binding = 1) uniform sampler2D depthMap;

float m22 = aoData.proj[2][2];
float m32 = aoData.proj[3][2];

float far = m32 / (m22 + 1);
float near = m32 / (m22 - 1);
const float tanFOV = 0.414213562;

float linDepth(float depth)
{
	return 2.0f * near * far / (far + near - depth * (far - near));
}

vec3 getNormal(vec2 pixelSize)
{
	const ivec2 xOffset = ivec2(1, 0);
	const ivec2 yOffset = ivec2(0, 1);

	float zC = texture(depthMap, texCoord).r * 2.0f - 1.0f;
	float zH = textureOffset(depthMap, texCoord, xOffset).r * 2.0f - 1.0f;
	float zV = textureOffset(depthMap, texCoord, yOffset).r * 2.0f - 1.0f;

	zC = linDepth(zC);
	zH = linDepth(zH);
	zV = linDepth(zV);

	vec3 dirH = vec3(pixelSize.x, 0.0f, zH - zC);
	vec3 dirV = vec3(0.0f, pixelSize.y, zV - zC);

	vec3 norm = normalize(cross(dirH, dirV));
	norm.x = -norm.x;
	return norm;
}

void main()
{
	const int samples = 64;

	float radius = aoData.radBiasContrastAspect.x;
	float bias = aoData.radBiasContrastAspect.y;
	float contrast = aoData.radBiasContrastAspect.z;
	float aspect = aoData.radBiasContrastAspect.w;

	vec2 texSize = textureSize(depthMap, 0);
	vec2 pixelSize = vec2(1.0f) / texSize;
	vec3 normal = getNormal(pixelSize);

	int random_ind = int(gl_FragCoord.x) % 4 + 4 * (int(gl_FragCoord.y) % 4);
	vec3 random = normalize(aoData.rotation[random_ind].xyz);

	vec3 ssPos = vec3(texCoord.x * 2.0f - 1.0f, texCoord.y * 2.0f - 1.0f, texture(depthMap, texCoord).r);
	vec2 viewRay = vec2(-ssPos.x * tanFOV * aspect, ssPos.y * tanFOV);
	float viewZ = -linDepth(ssPos.z);
	vec3 position = vec3(viewRay.x * viewZ, viewRay.y * viewZ, viewZ);

	vec3 tangent = normalize(random - normal * dot(random, normal));
	vec3 biTangent = cross(normal, tangent);
	mat3 TBN = mat3(tangent, biTangent, normal);

	float occlusion = 0.0f;

	for (int i = 0; i < samples; i++)
	{
		vec3 samplePos = TBN * (aoData.kernel[i].xyz);
		samplePos = position + samplePos * radius;

		vec4 offset = vec4(samplePos, 1.0f);
		offset = aoData.proj * offset;
		offset = vec4(offset.xyz / offset.w, 1.0f);
		offset.xyz = offset.xyz * 0.5f + 0.5f;

		float sampleDepth = -linDepth(texture(depthMap, offset.xy).r);

		float rangeCheck = smoothstep(0.0f, 1.0f, radius / abs(position.z - sampleDepth));
		occlusion += step(samplePos.z + bias, sampleDepth) * rangeCheck;
	}

	float ao = 1.0 - contrast * occlusion * (1.0f / samples);

	outFragColor = vec4(vec3(clamp(ao, 0.0f, 1.0f)), 1.0f);
}
