#pragma once

#include "material.h"
#include "asset_system.h"

#include <string>
#include <unordered_map>

struct BaseEngine;

enum PipelineInfoFlags
{
	PIPELINE_INFO_DEPTH_TEST_DISABLE = 1,
	PIPELINE_INFO_DEPTH_WRITE_DISABLE = 2,
	PIPELINE_INFO_BLEND_ENABLE = 4,
	PIPELINE_INFO_NO_VERTICES = 8
};

struct ShaderInfo
{
	std::string filename;
	VkShaderStageFlagBits type;
	uint32_t uniform_buffer_count, storage_buffer_count, texture_count;
};

struct PipelineInfo
{
	std::vector<ShaderInfo> shaders;
	VkCompareOp depth_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;
	VkCullModeFlags cull_mode = VK_CULL_MODE_BACK_BIT;
	VkPipelineColorBlendAttachmentState color_blend_attachment_state;
	uint32_t render_pass_index;
	uint32_t framebuffer_count = 1;
	uint32_t flags = 0;
};

class MaterialSystem
{
public:
	void init(std::string filename, BaseEngine *engine, std::vector<VkRenderPass> render_passes);
	void destroy(BaseEngine *engine);
	const Material &get_material(size_t mat_id);
	std::vector<DescriptorInfo> get_descriptor_infos(size_t mat_id);
	size_t get_material_id(std::string name);

	bool read_pipelines(std::string filename, BaseEngine *engine, std::vector<VkRenderPass> render_passes);
	bool read_materials(std::string filename, BaseEngine *engine);

	std::unordered_map<std::string, Pipeline> _pipelines;
	std::unordered_map<std::string, size_t> _material_ids;
	std::vector<Material> _materials;
};
