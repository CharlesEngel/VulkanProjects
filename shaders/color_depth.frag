#version 450

layout (location = 0) out vec4 outFragColor;

layout (location = 0) in vec2 texCoord;

layout (set = 0, binding = 2) uniform sampler2D color;

void main()
{
	outFragColor = texture(color, texCoord);
}
