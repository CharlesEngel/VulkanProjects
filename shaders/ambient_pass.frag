#version 450

layout (location = 0) out vec4 outFragColor;

layout (location = 0) in vec2 texCoord;

layout (set = 0, binding = 0) uniform sampler2D albedo_specular;
layout (set = 0, binding = 1) uniform sampler2D ao;

void main()
{
	vec3 color = texture(ao, texCoord).r * 0.03 * texture(albedo_specular, texCoord).xyz;
	color = color / (color + vec3(1.0));
	//color = pow(color, vec3(1.0f/2.2f));
	outFragColor = vec4(color, 1.0f);
}
