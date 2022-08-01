#version 460

layout (location = 0) out int outIndex;

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec3 vTangent;
layout (location = 3) in vec3 vTexCoord;

layout (set = 0, binding = 0) uniform CameraBuffer
{
	mat4 view;
	mat4 proj;
	mat4 viewproj;
} cameraData;

struct ObjectData
{
	mat4 model;
};

layout (std140, set = 0, binding = 1) readonly buffer ObjectBuffer
{
	ObjectData objects[];
} objectBuffer;

void main()
{
	outIndex = gl_InstanceIndex;
	gl_Position = cameraData.viewproj * objectBuffer.objects[gl_InstanceIndex].model * vec4(vPosition, 1.0f);
}
