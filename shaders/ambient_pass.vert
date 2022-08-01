#version 460

layout (location = 0) out vec2 texCoord;

void main()
{
	const vec3 positions[6] = vec3[6](
		vec3(1.0, 1.0, 0.0),
		vec3(-1.0, 1.0, 0.0),
		vec3(-1.0, -1.0, 0.0),
		vec3(1.0, 1.0, 0.0),
		vec3(-1.0, -1.0, 0.0),
		vec3(1.0, -1.0, 0.0)
	);

	const vec2 coords[6] = vec2[6](
		vec2(1.0, 1.0),
		vec2(0.0, 1.0),
		vec2(0.0, 0.0),
		vec2(1.0, 1.0),
		vec2(0.0, 0.0),
		vec2(1.0, 0.0)
	);

	gl_Position = vec4(positions[gl_VertexIndex], 1.0f);
	texCoord = coords[gl_VertexIndex];
}
