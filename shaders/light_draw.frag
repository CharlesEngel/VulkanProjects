#version 450

layout (location = 0) out vec4 outFragColor;

layout (location = 0) flat in int index;

struct LightData
{
	vec4 pos_r;
	vec4 color;
};

layout(std140, set = 0, binding = 2) readonly buffer LightBuffer
{
	LightData lights[];
} lightBuffer;

void main()
{
	vec3 color = lightBuffer.lights[index].color.rgb;
	color = color / (color + vec3(1.0));
	//color = pow(color, vec3(1.0f/2.2f));
	outFragColor = vec4(color, 1.0f);
}
