#pragma once

#include "resource.h"
#include <vector>
#include <glm/glm.hpp>
#include "inc.h"

struct VertexInputDescription
{
	std::vector<VkVertexInputBindingDescription> bindings;
	std::vector<VkVertexInputAttributeDescription> attributes;

	VkPipelineVertexInputStateCreateFlags flags = 0;
};

struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 tangent;
	glm::vec2 uv;

	static VertexInputDescription get_vertex_description();

	bool operator==(const Vertex &other) const
	{
		return position == other.position && normal == other.normal && uv == other.uv;
	}
};

struct Mesh
{
	std::vector<Vertex> _vertices;
	std::vector<uint32_t> _indices;
	Buffer _vertex_buffer;
	Buffer _index_buffer;
};
