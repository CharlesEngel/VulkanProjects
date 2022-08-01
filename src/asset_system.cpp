#include "asset_system.h"

#include "asset_packer/asset_packer.h"

#include "base_engine.h"

#include <iostream>
#include <fstream>
#include <lz4.h>

void AssetSystem::init(std::string asset_list_name, BaseEngine *engine)
{
	std::ifstream read_f;
	read_f.open(asset_list_name);

	if (!read_f.is_open())
	{
		std::cout << "Failed to open asset system file!\n";
	}

	std::string line;
	while (std::getline(read_f, line))
	{
		std::string type = "";
		std::string name = "";
		std::string file = "";

		int i;
		for (i = 0; i < line.size() && line[i] != ':'; i++)
		{
			type += line[i];
		}

		for (++i; i < line.size() && line[i] != ':'; i++)
		{
			file += line[i];
		}

		for (++i; i < line.size(); i++)
		{
			name += line[i];
		}

		if (type.size() == 0 || name.size() == 0 || file.size() == 0)
		{
			continue;
		}

		if (type == "m")
		{
			if (m_id_map.count(name) != 0)
			{
				std::cout << "Name conflict with mesh: " << name << "\nUsing first instance encountered.\n";
				continue;
			}

			Mesh m = load_mesh(file.c_str());

			if (m._indices.size() == 0)
			{
				continue;
			}

			engine->upload_mesh(m);

			m_id_map[name] = meshes.size();
			meshes.push_back(m);
		}

		else if (type == "t")
		{
			if (t_id_map.count(name) != 0)
			{
				std::cout << "Name conflict with texture: " << name << "\nUsing first instance encountered.\n";
				continue;
			}

			VkFormat f;
			int w, h;
			auto pixels = load_texture(file.c_str(), f, w, h);
			
			if (pixels == nullptr)
			{
				continue;
			}

			Texture t;
			t.width = w;
			t.height = h;
			engine->upload_texture(t, pixels, f);

			t_id_map[name] = textures.size();
			textures.push_back(t);
		}	
	}

	read_f.close();
}

void AssetSystem::destroy()
{
	
}

void AssetSystem::update_assets(std::string filename)
{

}

size_t AssetSystem::get_mesh_id(std::string mesh_name)
{
	if (m_id_map.count(mesh_name) != 0)
	{
		return m_id_map[mesh_name];
	}
	else
	{
		std::cout << "Attempted to get id for mesh " << mesh_name << " which does not exist\n";
		return (size_t)-1;
	}
}

size_t AssetSystem::get_texture_id(std::string texture_name)
{
	if (t_id_map.count(texture_name) != 0)
	{
		return t_id_map[texture_name];
	}
	else
	{
		std::cout << "Attempted to get id for texture " << texture_name << " which does not exist\n";
		return (size_t)-1;
	}
}

Mesh &AssetSystem::get_mesh(size_t mesh_id)
{
	return meshes[mesh_id];
}

Texture &AssetSystem::get_texture(size_t texture_id)
{
	return textures[texture_id];
}

Mesh AssetSystem::load_mesh(const char *filename)
{
	Mesh m;
	AssetPacker::FileData m_data;

	if (!AssetPacker::load_file(filename, m_data))
	{
		std::cout << "Failed to load mesh: " << filename << "\n";
		return m;
	}

	uint32_t num_verts = reinterpret_cast<uint32_t*>(m_data.metadata)[0];
	uint32_t num_inds = reinterpret_cast<uint32_t*>(m_data.metadata)[1];

	m._vertices.resize(num_verts);
	m._indices.resize(num_inds);

	auto buf_size = sizeof(Vertex) * num_verts + sizeof(uint32_t) * num_inds;
	char *buf = new char[buf_size];
	LZ4_decompress_safe(m_data.data, buf, m_data.size, buf_size);

	memcpy(m._vertices.data(), buf, sizeof(Vertex) * num_verts);
	memcpy(m._indices.data(), buf + sizeof(Vertex) * num_verts, 4 * num_inds);

	AssetPacker::close_file(m_data);
	delete[] buf;

	return m;
}

void *AssetSystem::load_texture(const char *filename, VkFormat &format, int &width, int &height)
{
	AssetPacker::FileData tex_data;

	if (!AssetPacker::load_file(filename, tex_data))
	{
		std::cout << "Failed to load texture: " << filename << "\n";
		return nullptr;
	}

	width = reinterpret_cast<uint32_t*>(tex_data.metadata)[0];
	height = reinterpret_cast<uint32_t*>(tex_data.metadata)[1];
	int channels = reinterpret_cast<uint32_t*>(tex_data.metadata)[2];
	format = (VkFormat)reinterpret_cast<uint32_t*>(tex_data.metadata)[3];

	auto pixels_size = width * height * 4;
	void *pixel_ptr = (void*)(new char[pixels_size]);

	LZ4_decompress_safe(tex_data.data, (char*)pixel_ptr, tex_data.size, pixels_size);

	AssetPacker::close_file(tex_data);

	return pixel_ptr;
}
