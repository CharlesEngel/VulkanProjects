#include "ao_engine.h"

#include <imgui/imgui.h>
#include <imgui/imgui_impl_vulkan.h>
#include <imgui/imgui_impl_sdl.h>

#include <random>

#include "../infos.h"
#include "../inc.h"
#include "glm/gtx/transform.hpp"
#include "../pipeline_builder.h"
#include "../asset_system.h"

void AOEngine::init()
{
	init_sdl("AO Rendering");
	init_vulkan("AO Rendering", true);
	init_swapchain();
	init_commands();
	init_sync_structures();
	init_descriptor_pool();
	init_render_passes();
	init_framebuffers();
	_material_system.init("../assets/ao/material_system", this, {_depth_pass, _ao_pass, _blur_pass, _draw_pass});
	
	_mat_ids[0] = _material_system.get_material_id("draw_depth");
	_mat_ids[1] = _material_system.get_material_id("ao");
	_mat_ids[2] = _material_system.get_material_id("blur");
	_mat_ids[3] = _material_system.get_material_id("draw_screen");

	_empire_mesh_id = _asset_system.get_mesh_id("empire");

	init_descriptors();
	init_scene();
	write_descriptors();
	init_imgui(_draw_pass);

	for (int i = 0; i < 64; i++)
	{
		auto k_vec = glm::normalize(glm::vec3((float)rand() - RAND_MAX / 2, (float)rand() - RAND_MAX / 2, (float)rand()));
		k_vec *= (float)rand() / RAND_MAX;
		float scale = (float)i / 64;
		scale = 0.1f + (scale * scale) * 1.0f;
		k_vec *= scale;
		ao_data.kernel[i] = glm::vec4(k_vec.x, k_vec.y, k_vec.z, 0.0f);
	}

	for (int i = 0; i < 16; i++)
	{
		auto rand_rot = glm::normalize(glm::vec2((float)rand() - RAND_MAX / 2, (float)rand() - RAND_MAX / 2));
		ao_data.rotation[i] = glm::vec4(rand_rot.x, rand_rot.y, 0, 0);
	}

	ao_data.radBiasContrastAspect = glm::vec4(0.65f, 0.05f, 2.9f, (float) _window_extent.width / _window_extent.height);
	_draw_mode = 0;
}

void AOEngine::draw()
{
	uint32_t frame_index;
	uint32_t swapchain_image_index;
	VkCommandBuffer cmd;

	if (!start_draw(&frame_index, &swapchain_image_index, &cmd))
	{
		return;
	}

	// Set up viewport and scissor based on window dimensions
	VkViewport viewport = {};
	viewport.height = (float)_window_extent.height;
	viewport.width = (float)_window_extent.width;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	viewport.x = 0;
	viewport.y = 0;

	VkRect2D scissor = {};
	scissor.extent = _window_extent;
	scissor.offset = {0, 0};

	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	// Initialize structures for camera uniform buffers
	glm::vec3 cam_pos = {0.0f, -6.0f, -10.0f};
	glm::mat4 view = glm::translate(glm::mat4(1.0f), cam_pos);
	glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)_window_extent.width / _window_extent.height, 0.1f, 200.0f);
	projection[1][1] *= -1;

	CameraData cam_data;
	cam_data.proj = projection;
	cam_data.view = view;
	cam_data.proj_view = projection * view;

	// Initialize structures for object uniform buffers
	ObjectData obj_data;
	obj_data.model_matrix = glm::translate(glm::vec3(5, -10, 0));

	// Initialize structures for SSAO uniform buffer
	ao_data.proj = projection;
	ao_data.radBiasContrastAspect.w = (float) _window_extent.width / _window_extent.height;

	// Write data to uniform buffers
	void *data;
	vmaMapMemory(_allocator, _uniform_buffers[frame_index]._allocation, &data);
	memcpy(data, &cam_data, sizeof(CameraData));
	vmaUnmapMemory(_allocator, _uniform_buffers[frame_index]._allocation);

	vmaMapMemory(_allocator, _storage_buffers[frame_index]._allocation, &data);
	memcpy(data, &obj_data, sizeof(ObjectData));
	vmaUnmapMemory(_allocator, _storage_buffers[frame_index]._allocation);

	vmaMapMemory(_allocator, _ao_uniform_buffers[frame_index]._allocation, &data);
	memcpy(data, &ao_data, sizeof(AOData));
	vmaUnmapMemory(_allocator, _ao_uniform_buffers[frame_index]._allocation);

	vmaMapMemory(_allocator, _draw_mode_uniform_buffers[frame_index]._allocation, &data);
	memcpy(data, &_draw_mode, sizeof(int));
	vmaUnmapMemory(_allocator, _draw_mode_uniform_buffers[frame_index]._allocation);

	VkClearValue clear_value;
	clear_value.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

	VkClearValue depth_clear;
	depth_clear.depthStencil.depth = 1.0f;

	ImGui::Begin("Menu", NULL, ImGuiWindowFlags_MenuBar);
	ImGui::SliderFloat("AO Radius", (float*)&ao_data.radBiasContrastAspect.x, 0.0f, 5.0f);
	ImGui::SliderFloat("AO Bias", (float*)&ao_data.radBiasContrastAspect.y, 0.0f, 5.0f);
	ImGui::SliderFloat("AO Contrast", (float*)&ao_data.radBiasContrastAspect.z, 0.0f, 5.0f);

	if (ImGui::Button("Full"))
	{
		_draw_mode = 0;
	}
	ImGui::SameLine();
	if (ImGui::Button("Color"))
	{
		_draw_mode = 1;
	}
	ImGui::SameLine();
	if (ImGui::Button("AO"))
	{
		_draw_mode = 2;
	}
	ImGui::SameLine();
	if (ImGui::Button("Blur"))
	{
		_draw_mode = 3;
	}

	ImGui::End();

	VkRenderPassBeginInfo rp_info = {};
	rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rp_info.pNext = nullptr;
	rp_info.renderPass = _depth_pass;
	rp_info.renderArea.offset.x = 0;
	rp_info.renderArea.offset.y = 0;
	rp_info.renderArea.extent = _window_extent;
	rp_info.framebuffer = _depth_framebuffer;

	VkClearValue d_clear_values[] = {clear_value, depth_clear};
	rp_info.clearValueCount = 2;
	rp_info.pClearValues = d_clear_values;

	auto material = _material_system.get_material(_mat_ids[0]);

	vkCmdBeginRenderPass(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);

	for (int i = 0; i < material.draws.size(); i++)
	{
		auto draw = material.draws[i];
		if (draw->render_pass_id == 0)
		{
			auto mesh = _asset_system.get_mesh(_empire_mesh_id);
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw->pipeline);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw->layout, 0, 1, &_descriptor_sets[0][i][frame_index], 0, nullptr);
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(cmd, 0, 1, &(mesh._vertex_buffer._buffer), &offset);
			vkCmdBindIndexBuffer(cmd, (mesh._index_buffer._buffer), 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(cmd, mesh._indices.size(), 1, 0, 0, 0);
		}
	}
	vkCmdEndRenderPass(cmd);

	rp_info.renderPass = _ao_pass;
	rp_info.framebuffer = _ao_framebuffer;
	rp_info.clearValueCount = 1;
	rp_info.pClearValues = &clear_value;
	vkCmdBeginRenderPass(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);

	material = _material_system.get_material(_mat_ids[1]);

	for (int i = 0; i < material.draws.size(); i++)
	{
		auto draw = material.draws[i];
		
		if (draw->render_pass_id == 1)
		{
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw->pipeline);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw->layout, 0, 1, &_descriptor_sets[1][i][frame_index], 0, nullptr);
			vkCmdDraw(cmd, 6, 1, 0, 0);
		}
	}

	vkCmdEndRenderPass(cmd);

	rp_info.renderPass = _blur_pass;
	rp_info.framebuffer = _blur_framebuffer;
	vkCmdBeginRenderPass(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);

	material = _material_system.get_material(_mat_ids[2]);

	for (int i = 0; i < material.draws.size(); i++)
	{
		auto draw = material.draws[i];
		
		if (draw->render_pass_id == 2)
		{
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw->pipeline);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw->layout, 0, 1, &_descriptor_sets[2][i][frame_index], 0, nullptr);
			vkCmdDraw(cmd, 6, 1, 0, 0);
		}
	}

	vkCmdEndRenderPass(cmd);

	rp_info.renderPass = _draw_pass;
	rp_info.framebuffer = _draw_framebuffers[swapchain_image_index];
	rp_info.clearValueCount = 2;
	VkClearValue values[] = {clear_value, depth_clear};
	rp_info.pClearValues = values;
	vkCmdBeginRenderPass(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);

	material = _material_system.get_material(_mat_ids[3]);

	for (int i = 0; i < material.draws.size(); i++)
	{
		auto draw = material.draws[i];
		
		if (draw->render_pass_id == 3)
		{
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw->pipeline);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw->layout, 0, 1, &_descriptor_sets[3][i][frame_index], 0, nullptr);
			vkCmdDraw(cmd, 6, 1, 0, 0);
		}
	}

	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
	vkCmdEndRenderPass(cmd);

	end_draw(frame_index, swapchain_image_index, cmd);
}

void AOEngine::init_render_passes()
{
	std::vector<VkAttachmentDescription> attachments = {};

	// Create depth render pass
	attachments.push_back(infos::color_attachment_description(VK_FORMAT_R8G8B8A8_SRGB, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
	attachments.push_back(infos::depth_attachment_description(_depth_image._format, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
	_depth_pass = create_render_pass(attachments, true);

	attachments.clear();
	attachments.push_back(infos::color_attachment_description(VK_FORMAT_R32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
	_ao_pass = create_render_pass(attachments, false);
	_blur_pass = create_render_pass(attachments, false);

	attachments.clear();
	attachments.push_back(infos::color_attachment_description(_swapchain_image_format, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR));
	attachments.push_back(infos::depth_attachment_description(_depth_image._format, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));
	_draw_pass = create_render_pass(attachments, true);
}

void AOEngine::init_framebuffers()
{
	// Create textures for deferred attachments
	_ao_depth_image = create_texture(_window_extent.width, _window_extent.height, 4, _depth_image._format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_IMAGE_ASPECT_DEPTH_BIT, VK_FILTER_NEAREST);
	_ao_image = create_texture(_window_extent.width, _window_extent.height, 4, VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_NEAREST);
	_blur_image = create_texture(_window_extent.width, _window_extent.height, 4, VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_NEAREST);
	_color_image = create_texture(_window_extent.width, _window_extent.height, 4, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_NEAREST);

	_swapchain_deletion_queue.push_function([=]() {
		vkDestroyImageView(_device, _ao_depth_image._image_view, nullptr);
		vkDestroyImageView(_device, _ao_image._image_view, nullptr);
		vkDestroyImageView(_device, _blur_image._image_view, nullptr);
		vkDestroyImageView(_device, _color_image._image_view, nullptr);
		vkDestroySampler(_device, _ao_depth_image._image_info.sampler, nullptr);
		vkDestroySampler(_device, _ao_image._image_info.sampler, nullptr);
		vkDestroySampler(_device, _blur_image._image_info.sampler, nullptr);
		vkDestroySampler(_device, _color_image._image_info.sampler, nullptr);
		vmaDestroyImage(_allocator, _ao_depth_image._image, _ao_depth_image._allocation);
		vmaDestroyImage(_allocator, _ao_image._image, _ao_image._allocation);
		vmaDestroyImage(_allocator, _blur_image._image, _blur_image._allocation);
		vmaDestroyImage(_allocator, _color_image._image, _color_image._allocation);
	});

	// Create framebuffer objects
	_depth_framebuffer = create_framebuffer(_depth_pass, {_color_image._image_view, _ao_depth_image._image_view});
	_ao_framebuffer = create_framebuffer(_ao_pass, {_ao_image._image_view});
	_blur_framebuffer = create_framebuffer(_blur_pass, {_blur_image._image_view});
	_draw_framebuffers = create_swapchain_framebuffers(_draw_pass);
}

void AOEngine::init_descriptors()
{
	for (int i = 0; i < NUM_MATS; i++)
	{
		auto mat = _material_system.get_material(_mat_ids[i]);

		for (size_t j = 0; j < mat.descriptors.size(); j++)
		{
			_descriptor_sets[i].push_back(allocate_descriptor_sets(mat.descriptors[j].layout, FRAME_OVERLAP));
		}
	}
}

void AOEngine::init_scene()
{
	// Create uniform/storage buffers
	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		_uniform_buffers[i] = create_buffer(sizeof(CameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
		_storage_buffers[i] = create_buffer(sizeof(ObjectData), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
		_ao_uniform_buffers[i] = create_buffer(sizeof(AOData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
		_draw_mode_uniform_buffers[i] = create_buffer(sizeof(int), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	}

	_main_deletion_queue.push_function([=]() {
		for (int i = 0; i < FRAME_OVERLAP; i++)
		{
			vmaDestroyBuffer(_allocator, _uniform_buffers[i]._buffer, _uniform_buffers[i]._allocation);
			vmaDestroyBuffer(_allocator, _storage_buffers[i]._buffer, _storage_buffers[i]._allocation);
			vmaDestroyBuffer(_allocator, _ao_uniform_buffers[i]._buffer, _ao_uniform_buffers[i]._allocation);
			vmaDestroyBuffer(_allocator, _draw_mode_uniform_buffers[i]._buffer, _draw_mode_uniform_buffers[i]._allocation);
		}
	});
}

void AOEngine::write_descriptors()
{
	std::vector<std::vector<DescriptorInfo>> infos = {};

	for (int n = 0; n < NUM_MATS; n++)
	{
		infos.push_back(_material_system.get_descriptor_infos(_mat_ids[n]));
	}

	int info_count = 0;

	for (int n = 0; n < NUM_MATS; n++)
	{
		for (int in = 0; in < infos[n].size(); in++)
		{
			std::vector<VkWriteDescriptorSet> writes = {};
			auto info = infos[n][in];
			for (int i = 0; i < FRAME_OVERLAP; i++)
			{
				for (size_t j = 0; j < info.descriptor_names.size(); j++)
				{
					auto name = info.descriptor_names[j];
					if (name.size() > 3 && name.substr(0, 3) == "UB:")
					{
						VkDescriptorBufferInfo *buffer_info;
						auto label = name.substr(3, name.size()-3);
						if (label == "cam_data")
						{
							buffer_info = &_uniform_buffers[i]._buffer_info;
						}
						else if (label == "ao_data")
						{
							buffer_info = &_ao_uniform_buffers[i]._buffer_info;
						}
						else if (label == "draw_mode")
						{
							buffer_info = &_draw_mode_uniform_buffers[i]._buffer_info;
						}
						writes.push_back(infos::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, _descriptor_sets[n][in][i], buffer_info, j));
					}
					else if (name.size() > 3 && name.substr(0, 3) == "SB:")
					{
						VkDescriptorBufferInfo *buffer_info;
						auto label = name.substr(3, name.size()-3);
						if (label == "obj_data")
						{
							buffer_info = &_storage_buffers[i]._buffer_info;
						}
						writes.push_back(infos::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, _descriptor_sets[n][in][i], buffer_info, j));
					}
					else if (name.size() > 4 && name.substr(0, 4) == "TEX:")
					{
						VkDescriptorImageInfo *tex_info;
						auto label = name.substr(4, name.size()-4);
						tex_info = &_asset_system.get_texture(_asset_system.get_texture_id(label))._image_info;
						writes.push_back(infos::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, _descriptor_sets[n][in][i], tex_info, j));
					}
					else if (name.size() > 4 && name.substr(0, 4) == "ATT:")
					{
						VkDescriptorImageInfo *tex_info;
						auto label = name.substr(4, name.size()-4);

						if (label == "depth")
						{
							tex_info = &_ao_depth_image._image_info;
						}
						else if (label == "ao")
						{
							tex_info = &_ao_image._image_info;
						}
						else if (label == "blur")
						{
							tex_info = &_blur_image._image_info;
						}
						else if (label == "color")
						{
							tex_info = &_color_image._image_info;
						}
						writes.push_back(infos::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, _descriptor_sets[n][in][i], tex_info, j));
					}
				}
			}

			vkUpdateDescriptorSets(_device, writes.size(), writes.data(), 0, nullptr);

			info_count++;
		}
	}
}

void AOEngine::resize_window(uint32_t w, uint32_t h)
{
	// Resize swapchain
	resize_swapchain(w, h, _draw_pass);

	// Recreate framebuffers
	init_framebuffers();
	// Rewrite descriptor sets to use new g-buffers
	write_descriptors();
}
