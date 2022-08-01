#pragma once

#include "inc.h"

#include <vector>
#include <string>

struct Pipeline
{
	VkPipeline pipeline;
	VkPipelineLayout layout;
	VkDescriptorSetLayout descriptor_layout;
	uint32_t render_pass_id;
};

struct DescriptorInfo
{
	VkDescriptorSetLayout layout;
	std::vector<std::string> descriptor_names;
};

struct Material
{
	std::vector<Pipeline*> draws;
	std::vector<DescriptorInfo> descriptors;
};
