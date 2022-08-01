#version 450

layout (location = 0) out vec4 outFragColor;

layout (location = 0) in vec2 texCoord;

layout (binding = 0) uniform DrawMode
{
	int mode;
} drawMode;

layout (binding = 1) uniform sampler2D ao;
layout (binding = 2) uniform sampler2D blur;
layout (binding = 3) uniform sampler2D color;

void main()
{
	if (drawMode.mode == 0)
	{
		outFragColor = texture(color, texCoord) * texture(blur, texCoord).r;
	}
	else if (drawMode.mode == 1)
	{
		outFragColor = texture(color, texCoord);
	}
	else if (drawMode.mode == 2)
	{
		outFragColor = vec4(texture(ao, texCoord).r);
	}
	else
	{
		outFragColor = vec4(texture(blur, texCoord).r);
	}

}
