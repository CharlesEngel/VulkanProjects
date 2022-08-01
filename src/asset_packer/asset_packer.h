#pragma once

#include <string>

#include <vulkan/vulkan.h>

namespace AssetPacker
{
	struct FileData
	{
		char type[4];
		char metadata[32];
		char *data;
		size_t size;
	};

	bool load_file(std::string filename, FileData &data);
	bool save_file(std::string filename, const FileData &data);
	void close_file(FileData &data);

	FileData pack_texture(std::string filename, VkFormat format);
	FileData pack_mesh(std::string filename);
};
