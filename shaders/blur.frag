#version 450

layout (location = 0) out vec4 outFragColor;

layout (location = 0) in vec2 texCoord;

layout (binding = 0) uniform sampler2D ao;

void main()
{
	vec2 size = 1.0f / vec2(textureSize(ao, 0));
	float result = 0.0f;

	const ivec2 offset00 = ivec2(-2, -2);
	const ivec2 offset01 = ivec2(-2, -1);
	const ivec2 offset02 = ivec2(-2, 0);
	const ivec2 offset03 = ivec2(-2, 1);
	const ivec2 offset04 = ivec2(-1, -2);
	const ivec2 offset05 = ivec2(-1, -1);
	const ivec2 offset06 = ivec2(-1, 0);
	const ivec2 offset07 = ivec2(-1, 1);
	const ivec2 offset08 = ivec2(0, -2);
	const ivec2 offset09 = ivec2(0, -1);
	const ivec2 offset10 = ivec2(0, 0);
	const ivec2 offset11 = ivec2(0, 1);
	const ivec2 offset12 = ivec2(1, -2);
	const ivec2 offset13 = ivec2(1, -1);
	const ivec2 offset14 = ivec2(1, 0);
	const ivec2 offset15 = ivec2(1, 1);

	result += textureOffset(ao, texCoord, offset00).r;
	result += textureOffset(ao, texCoord, offset01).r;
	result += textureOffset(ao, texCoord, offset02).r;
	result += textureOffset(ao, texCoord, offset03).r;
	result += textureOffset(ao, texCoord, offset04).r;
	result += textureOffset(ao, texCoord, offset05).r;
	result += textureOffset(ao, texCoord, offset06).r;
	result += textureOffset(ao, texCoord, offset07).r;
	result += textureOffset(ao, texCoord, offset08).r;
	result += textureOffset(ao, texCoord, offset09).r;
	result += textureOffset(ao, texCoord, offset10).r;
	result += textureOffset(ao, texCoord, offset11).r;
	result += textureOffset(ao, texCoord, offset12).r;
	result += textureOffset(ao, texCoord, offset13).r;
	result += textureOffset(ao, texCoord, offset14).r;
	result += textureOffset(ao, texCoord, offset15).r;

	outFragColor = vec4(vec3(result / 16), 1.0f);
}
