#version 460

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec3 vTangent;
layout (location = 3) in vec2 vTexCoord;

layout (location = 0) out vec3 outPos;
layout (location = 1) out vec3 outNorm;
layout (location = 2) out vec3 outTangent;
layout (location = 3) out vec2 texCoord;

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

layout(std140, set = 0, binding = 1) readonly buffer ObjectBuffer
{
	ObjectData objects[];
} objectBuffer;

void main()
{
	mat4 modelMatrix = objectBuffer.objects[gl_InstanceIndex].model;
	mat4 transformMatrix = cameraData.viewproj * modelMatrix;
	mat4 modelView = cameraData.view * modelMatrix;
	mat4 modelViewInvTrans = transpose(inverse(modelView));
	gl_Position = transformMatrix * vec4(vPosition, 1.0f);
	outPos = (modelView * vec4(vPosition, 1.0f)).xyz;
	outNorm = (modelViewInvTrans * vec4(vNormal, 0.0f)).xyz;
	outTangent = (modelView * vec4(vTangent, 0.0f)).xyz;
	texCoord = vTexCoord;
}
