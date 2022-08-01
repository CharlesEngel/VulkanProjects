#pragma once

#include "inc.h"

#include "mesh.h"
#include "resource.h"

#include <unordered_map>
#include <vector>
#include <string>

struct BaseEngine;

class AssetSystem
{
public:
	void init(std::string asset_list_name, BaseEngine *engine);
	void destroy();

	void update_assets(std::string filename);

	size_t get_mesh_id(std::string mesh_name);
	size_t get_texture_id(std::string texture_name);

	Mesh &get_mesh(size_t mesh_id);
	Texture &get_texture(size_t texture_id);

private:
	std::unordered_map<std::string, size_t> m_id_map;
	std::unordered_map<std::string, size_t> t_id_map;
	std::vector<Mesh> meshes;
	std::vector<Texture> textures;

	Mesh load_mesh(const char *filename);
	void *load_texture(const char *filename, VkFormat &format, int &width, int &height);
};
