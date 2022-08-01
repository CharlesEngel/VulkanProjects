#pragma once

#include "inc.h"
#include <vma/vk_mem_alloc.h>

struct Texture
{
	VkImage _image;
	VkImageView _image_view;
	VkFormat _format;
	VmaAllocation _allocation;
	VkDescriptorImageInfo _image_info;
	uint32_t _mip_levels = 1;
	uint32_t width, height;
};

struct Buffer
{
	VkBuffer _buffer;
	VmaAllocation _allocation;
	VkDescriptorBufferInfo _buffer_info;
};
