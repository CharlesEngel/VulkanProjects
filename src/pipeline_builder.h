#pragma once

#include "inc.h"

struct PipelineBuilder
{
	VkPipeline build_pipeline(VkDevice device, VkRenderPass render_pass);

	std::vector<VkPipelineShaderStageCreateInfo> _shader_stages;
	VkPipelineVertexInputStateCreateInfo _vertex_input_info;
	VkPipelineInputAssemblyStateCreateInfo _input_assembly;
	VkViewport _viewport;
	VkRect2D _scissor;
	VkPipelineRasterizationStateCreateInfo _rasterizer;
	std::vector<VkPipelineColorBlendAttachmentState> _color_blend_attachments;
	VkPipelineMultisampleStateCreateInfo _multisampling;
	VkPipelineLayout _pipeline_layout;
	VkPipelineDepthStencilStateCreateInfo _depth_stencil;
	VkPipelineDynamicStateCreateInfo _dynamic_state;
};
