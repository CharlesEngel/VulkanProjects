#include "pipeline_builder.h"

#include <iostream>

VkPipeline PipelineBuilder::build_pipeline(VkDevice device, VkRenderPass render_pass)
{
	VkPipelineViewportStateCreateInfo viewport_state = {};
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.pNext = nullptr;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &_viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &_scissor;

	VkPipelineColorBlendStateCreateInfo color_blend = {};
	color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blend.pNext = nullptr;
	color_blend.logicOpEnable = VK_FALSE;
	color_blend.logicOp = VK_LOGIC_OP_COPY;
	color_blend.attachmentCount = _color_blend_attachments.size();
	color_blend.pAttachments = _color_blend_attachments.data();

	VkGraphicsPipelineCreateInfo pipeline_info = {};
	pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_info.pNext = nullptr;
	pipeline_info.stageCount = _shader_stages.size();
	pipeline_info.pStages = _shader_stages.data();
	pipeline_info.pVertexInputState = &_vertex_input_info;
	pipeline_info.pInputAssemblyState = &_input_assembly;
	pipeline_info.pViewportState = &viewport_state;
	pipeline_info.pRasterizationState = &_rasterizer;
	pipeline_info.pMultisampleState = &_multisampling;
	pipeline_info.pColorBlendState = &color_blend;
	pipeline_info.pDepthStencilState = &_depth_stencil;
	pipeline_info.pDynamicState = &_dynamic_state;
	pipeline_info.layout = _pipeline_layout;
	pipeline_info.renderPass = render_pass;
	pipeline_info.subpass = 0;
	pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

	VkPipeline pipeline;

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline) != VK_SUCCESS)
	{
		std::cout << "failed to create pipeline\n";
		return VK_NULL_HANDLE;
	}
	else
	{
		return pipeline;
	}
}
