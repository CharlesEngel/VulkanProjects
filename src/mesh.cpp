#include "mesh.h"

//#include <tinyobjloader/tiny_obj_loader.h>
#include <iostream>
#include <unordered_map>

#include <lz4.h>

#include "asset_packer/asset_packer.h"

VertexInputDescription Vertex::get_vertex_description()
{
	VertexInputDescription description;

	VkVertexInputBindingDescription main_binding = {};
	main_binding.binding = 0;
	main_binding.stride = sizeof(Vertex);
	main_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	description.bindings.push_back(main_binding);

	VkVertexInputAttributeDescription position_attribute = {};
	position_attribute.binding = 0;
	position_attribute.location = 0;
	position_attribute.format = VK_FORMAT_R32G32B32_SFLOAT;
	position_attribute.offset = offsetof(Vertex, position);

	VkVertexInputAttributeDescription normal_attribute = {};
	normal_attribute.binding = 0;
	normal_attribute.location = 1;
	normal_attribute.format = VK_FORMAT_R32G32B32_SFLOAT;
	normal_attribute.offset = offsetof(Vertex, normal);

	VkVertexInputAttributeDescription tangent_attribute = {};
	tangent_attribute.binding = 0;
	tangent_attribute.location = 2;
	tangent_attribute.format = VK_FORMAT_R32G32B32_SFLOAT;
	tangent_attribute.offset = offsetof(Vertex, tangent);

	VkVertexInputAttributeDescription uv_attribute;
	uv_attribute.binding = 0;
	uv_attribute.location = 3;
	uv_attribute.format = VK_FORMAT_R32G32_SFLOAT;
	uv_attribute.offset = offsetof(Vertex, uv);

	description.attributes.push_back(position_attribute);
	description.attributes.push_back(normal_attribute);
	description.attributes.push_back(tangent_attribute);
	description.attributes.push_back(uv_attribute);

	return description;
}
