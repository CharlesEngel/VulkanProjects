#include "asset_packer.h"

#include <fstream>

#include <lz4.h>

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif
#include <stb_image/stb_image.h>

#include <tinyobjloader/tiny_obj_loader.h>

#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include "../inc.h"

bool AssetPacker::load_file(std::string filename, AssetPacker::FileData &data)
{
	std::ifstream s;
	s.open(filename, std::ios::binary);

	if (!s.is_open())
	{
		return false;
	}

	s.seekg(0, s.end);
	data.size = (size_t)(s.tellg()) - (size_t)36;
	s.seekg(0, s.beg);
	data.data = new char[data.size];

	s.read(data.type, 4);
	s.read(data.metadata, 32);
	s.read(data.data, data.size);

	s.close();

	return true;
}

bool AssetPacker::save_file(std::string filename, const AssetPacker::FileData &data)
{
	std::ofstream s;
	s.open(filename, std::ios::binary | std::ios::out);

	if (!s.is_open())
	{
		return false;
	}

	s.write(data.type, 4);
	s.write(data.metadata, 32);
	s.write(data.data, data.size);

	s.close();

	return true;
}

void AssetPacker::close_file(AssetPacker::FileData &data)
{
	delete[] data.data;
}

AssetPacker::FileData AssetPacker::pack_texture(std::string filename, VkFormat format)
{
	AssetPacker::FileData compressed_data;
	compressed_data.type[0] = 'T';
	compressed_data.type[1] = 'E';
	compressed_data.type[2] = 'X';
	compressed_data.type[3] = 'I';

	int width, height, channels;
	stbi_uc *pixels = stbi_load(filename.c_str(), &width, &height, &channels, STBI_rgb_alpha);

	char *pixel_ptr = (char*)pixels;
	reinterpret_cast<uint32_t*>(compressed_data.metadata)[0] = width;
	reinterpret_cast<uint32_t*>(compressed_data.metadata)[1] = height;
	reinterpret_cast<uint32_t*>(compressed_data.metadata)[2] = channels;
	reinterpret_cast<uint32_t*>(compressed_data.metadata)[3] = format;

	int texture_size = width * height * 4;
	int compress_bound = LZ4_compressBound(texture_size);
	compressed_data.data = new char[compress_bound];
	int compressed_size = LZ4_compress_default(pixel_ptr, compressed_data.data, texture_size, compress_bound);
	char *final_buffer = new char[compressed_size];
	memcpy(final_buffer, compressed_data.data, compressed_size);
	delete[] compressed_data.data;
	compressed_data.data = final_buffer;
	compressed_data.size = compressed_size;

	return compressed_data;
}

struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 tangent;
	glm::vec2 uv;

	bool operator==(const Vertex &other) const
	{
		return position == other.position && normal == other.normal && uv == other.uv;
	}
};

namespace std
{
    template<> struct hash<Vertex>
    {
	size_t operator()(Vertex const& vertex) const
	{
	    return ((hash<glm::vec3>()(vertex.position) ^
		   (hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^
		   (hash<glm::vec2>()(vertex.uv) << 1);
	}
    };
}

AssetPacker::FileData AssetPacker::pack_mesh(std::string filename)
{
	AssetPacker::FileData packed_mesh;

	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string warn;
	std::string err;

	tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.c_str(), nullptr);

	std::unordered_map<Vertex, uint32_t> unique_vertices{};
	std::vector<Vertex> vertices{};
	std::vector<uint32_t> indices{};

	for (size_t s = 0; s < shapes.size(); s++)
	{
		for (auto idx : shapes[s].mesh.indices)
		{
			tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
			tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
			tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];

			tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
			tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
			tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];

			tinyobj::real_t ux = attrib.texcoords[2 * idx.texcoord_index + 0];
			tinyobj::real_t uy = attrib.texcoords[2 * idx.texcoord_index + 1];

			Vertex vert;
			vert.position.x = vx;
			vert.position.y = vy;
			vert.position.z = vz;
			vert.normal.x = nx;
			vert.normal.y = ny;
			vert.normal.z = nz;
			vert.uv.x = ux;
			vert.uv.y = 1-uy;

			if (unique_vertices.count(vert) == 0)
			{
				unique_vertices[vert] = (uint32_t)vertices.size();
				vertices.push_back(vert);
			}

			indices.push_back(unique_vertices[vert]);
		}
	}

	for (int i = 0; i < indices.size(); i += 3)
	{
		glm::vec3 e1 = vertices[indices[i+1]].position - vertices[indices[i]].position;
		glm::vec3 e2 = vertices[indices[i+2]].position - vertices[indices[i]].position;
		glm::vec2 duv1 = vertices[indices[i+1]].uv - vertices[indices[i]].uv;
		glm::vec2 duv2 = vertices[indices[i+2]].uv - vertices[indices[i]].uv;

		float denom = 1.0f / (duv1.x * duv2.y - duv2.x * duv1.y);

		glm::vec3 tan;
		tan.x = denom * (duv2.y * e1.x - duv1.y * e2.x);
		tan.y = denom * (duv2.y * e1.y - duv1.y * e2.y);
		tan.z = denom * (duv2.y * e1.z - duv1.y * e2.z);

		vertices[indices[i+2]].tangent = tan;
		vertices[indices[i+1]].tangent = tan;
		vertices[indices[i]].tangent = tan;
	}

	reinterpret_cast<uint32_t*>(packed_mesh.metadata)[0] = vertices.size();
	reinterpret_cast<uint32_t*>(packed_mesh.metadata)[1] = indices.size();

	packed_mesh.type[0] = 'M';
	packed_mesh.type[1] = 'E';
	packed_mesh.type[2] = 'S';
	packed_mesh.type[3] = 'H';

	packed_mesh.size = sizeof(Vertex) * vertices.size() + sizeof(uint32_t) * indices.size();
	packed_mesh.data = new char[packed_mesh.size];

	memcpy(packed_mesh.data, vertices.data(), sizeof(Vertex) * vertices.size());
	memcpy(packed_mesh.data + sizeof(Vertex) * vertices.size(), indices.data(), sizeof(uint32_t) * indices.size());

	AssetPacker::FileData compressed_data;
	int compress_bound = LZ4_compressBound(packed_mesh.size);
	compressed_data.data = new char[compress_bound];
	int compressed_size = LZ4_compress_default(packed_mesh.data, compressed_data.data, packed_mesh.size, compress_bound);

	AssetPacker::close_file(packed_mesh);
	packed_mesh.data = new char[compressed_size];
	memcpy(packed_mesh.data, compressed_data.data, compressed_size);
	packed_mesh.size = compressed_size;
	AssetPacker::close_file(compressed_data);

	return packed_mesh;
}
