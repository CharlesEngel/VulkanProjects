#include "deferred_engine.h"

#include <imgui/imgui.h>
#include <imgui/imgui_impl_vulkan.h>
#include <imgui/imgui_impl_sdl.h>

#include <random>

#include "../infos.h"
#include "../inc.h"
#include "glm/gtx/transform.hpp"
#include "../pipeline_builder.h"
#include "../asset_system.h"

void DeferredEngine::init()
{
	init_sdl("Deferred Rendering");
	init_vulkan("Deferred Rendering", true);
	init_swapchain();
	init_commands();
	init_sync_structures();
	init_descriptor_pool();
	init_render_passes();
	init_framebuffers();
	_material_system.init("../assets/deferred/material_system", this, {_g_pass, _lighting_pass, _forward_pass});
	
	_mat_ids[0] = _material_system.get_material_id("rust");
	_mat_ids[1] = _material_system.get_material_id("cheese");
	_mat_ids[2] = _material_system.get_material_id("dent");
	_mat_ids[3] = _material_system.get_material_id("rock");
	_mat_ids[4] = _material_system.get_material_id("alien");
	_mat_ids[5] = _material_system.get_material_id("conc");
	_mat_ids[6] = _material_system.get_material_id("lighting");
	_mat_ids[7] = _material_system.get_material_id("light_draw");

	init_descriptors();
	init_pipelines();
	//_asset_system.init("../assets/_asset_system_test", this);
	init_models();
	init_scene();
	write_descriptors();
	init_imgui(_lighting_pass);
}

void DeferredEngine::draw()
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
	ObjectData obj_data[NUM_MONKEYS+1];
	obj_data[0].model_matrix = glm::translate(glm::vec3(5, -10, 0));

	for (int i = 1; i < NUM_MONKEYS+1; i++)
	{
		glm::mat4 translate = glm::translate(_monkey_pos[i-1]);
		glm::mat4 rotate = glm::rotate(glm::mat4(1.0f), glm::radians(_frame_number * 0.2f), glm::vec3(std::sin(0.2 * i), std::cos(0.2 * i), 0.0f));
		glm::mat4 scale = glm::scale(glm::vec3(0.8f, 0.8f, 0.8f));
		obj_data[i].model_matrix = translate * rotate * scale;
	}

	// Initialize structures for light uniform buffers
	ObjectData light_uniform_front[NUM_LIGHTS];
	ObjectData light_uniform_back[NUM_LIGHTS];
	ObjectData light_draw_uniform_front[NUM_LIGHTS];
	ObjectData light_draw_uniform_back[NUM_LIGHTS];
	
	int front_index = 0;
	int back_index = 0;
	for (int i = 0; i < NUM_LIGHTS; i++)
	{
		// Get position of light into a vec3
		auto l_pos = glm::vec3(_lights_info[i].pos_r.x, _lights_info[i].pos_r.y, _lights_info[i].pos_r.z);
		// Radius is stored in w component of pos_r
		float rad = _lights_info[i].pos_r.w;

		// Set up transform matrices
		glm::mat4 l_t = glm::translate(l_pos);
		glm::mat4 l_s = glm::scale(glm::vec3(rad));

		// If the camera is inside the sphere
		if (std::abs(glm::length(l_pos + cam_pos)) <= rad)
		{
			// Set info for drawing the back faces of the object
			light_uniform_back[back_index].model_matrix = l_t * l_s;
			light_draw_uniform_back[back_index].model_matrix = l_t * glm::scale(glm::vec3(0.1f));
			_lights_info_back[back_index].color = _lights_info[i].color;
			
			// Transform position into view space for lighting calculations
			_lights_info_back[back_index].pos_r = view * glm::vec4(l_pos, 1.0f);
			_lights_info_back[back_index++].pos_r.w = rad;
		}
		else // Otherwise do the same for drawing the front faces
		{
			light_uniform_front[front_index].model_matrix = l_t * l_s;
			light_draw_uniform_front[front_index].model_matrix = l_t * glm::scale(glm::vec3(0.1f));
			_lights_info_front[front_index].color = _lights_info[i].color;
			_lights_info_front[front_index].pos_r = view * glm::vec4(l_pos, 1.0f);//_lights_info[i].pos;
			_lights_info_front[front_index++].pos_r.w = rad;
		}

	}

	// Write data to uniform buffers
	void *data;
	vmaMapMemory(_allocator, _uniform_buffers[frame_index]._allocation, &data);
	memcpy(data, &cam_data, sizeof(CameraData));
	vmaUnmapMemory(_allocator, _uniform_buffers[frame_index]._allocation);

	vmaMapMemory(_allocator, _storage_buffers[frame_index]._allocation, &data);
	memcpy(data, obj_data, sizeof(ObjectData) * NUM_MONKEYS+1);
	vmaUnmapMemory(_allocator, _storage_buffers[frame_index]._allocation);

	// Write the data for front facing light volumes to the beginning of the buffer
	// Then write the data for the back facing light volumes to the end
	vmaMapMemory(_allocator, _light_buffers[frame_index]._allocation, &data);
	memcpy(data, _lights_info_front, sizeof(LightData) * front_index);
	memcpy((LightData*)data + front_index, _lights_info_back, sizeof(LightData) * back_index);
	vmaUnmapMemory(_allocator, _light_buffers[frame_index]._allocation);

	vmaMapMemory(_allocator, _light_storage_buffers[frame_index]._allocation, &data);
	memcpy(data, light_uniform_front, sizeof(ObjectData) * front_index);
	memcpy((ObjectData*)data + front_index, light_uniform_back, sizeof(ObjectData) * back_index);
	vmaUnmapMemory(_allocator, _light_storage_buffers[frame_index]._allocation);

	vmaMapMemory(_allocator, _light_draw_storage_buffers[frame_index]._allocation, &data);
	memcpy(data, light_draw_uniform_front, sizeof(ObjectData) * front_index);
	memcpy((ObjectData*)data + front_index, light_draw_uniform_back, sizeof(ObjectData) * back_index);
	vmaUnmapMemory(_allocator, _light_draw_storage_buffers[frame_index]._allocation);

	VkClearValue clear_value;
	clear_value.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

	VkClearValue depth_clear;
	depth_clear.depthStencil.depth = 1.0f;

	// Setup gui to adjust positions/colors
	ImGui::Begin("Menu", NULL, ImGuiWindowFlags_MenuBar);
	
	if (ImGui::CollapsingHeader("Monkey Positions"))
	{
		for (int n = 0; n < NUM_MONKEYS; n++)
		{
			ImGui::SliderFloat3(("Position" + std::to_string(n)).c_str(), (float*)&(_monkey_pos[n]), -50.0, 50.0);
		}
	}

	if (ImGui::CollapsingHeader("Lights"))
	{
		for (int n = 0; n < NUM_LIGHTS; n++)
		{
			if(ImGui::CollapsingHeader((std::string("Light ") + std::to_string(n)).c_str()))
			{
				ImGui::ColorEdit3(("Color" + std::to_string(n)).c_str(), (float*)&_lights_info[n].color);
				ImGui::SliderFloat3(("Light Position" + std::to_string(n)).c_str(), (float*)&_lights_info[n].pos_r, -30.0, 30.0);
				ImGui::SliderFloat(("Radius" + std::to_string(n)).c_str(), (float*)&_lights_info[n].pos_r.w, 0.0f, MAX_RADIUS);
			}
		}
	}
	ImGui::End();

	// Begin deferred pass and draw the objects to the g-buffers
	VkRenderPassBeginInfo rp_info = {};
	rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rp_info.pNext = nullptr;
	rp_info.renderPass = _g_pass;
	rp_info.renderArea.offset.x = 0;
	rp_info.renderArea.offset.y = 0;
	rp_info.renderArea.extent = _window_extent;
	rp_info.framebuffer = _g_framebuffer;
	rp_info.clearValueCount = 5;
	VkClearValue g_clear_values[] = {clear_value, clear_value, clear_value, clear_value, depth_clear};
	VkClearValue clear_values[] = {clear_value};
	rp_info.pClearValues = g_clear_values;

	vkCmdBeginRenderPass(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _g_pipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _g_pipeline_layout, 0, 1, &_descriptor_sets[NUM_TEXTURES-1][frame_index], 0, nullptr);
	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(cmd, 0, 1, &_empire_mesh._vertex_buffer._buffer, &offset);
	vkCmdBindIndexBuffer(cmd, _empire_mesh._index_buffer._buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(cmd, _empire_mesh._indices.size(), 1, 0, 0, 0);

	// Draw monkeys with random textures
	vkCmdBindVertexBuffers(cmd, 0, 1, &_monkey_mesh._vertex_buffer._buffer, &offset);
	for (int i = 0; i < NUM_TEXTURES; i++)
	{
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _g_pipeline_layout, 0, 1, &_descriptor_sets[i % NUM_TEXTURES][frame_index], 0, nullptr);
		vkCmdBindIndexBuffer(cmd, _monkey_mesh._index_buffer._buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(cmd, _monkey_mesh._indices.size(), NUM_MONKEYS/NUM_TEXTURES, 0, 0, i*(NUM_MONKEYS/NUM_TEXTURES));
	}
	vkCmdEndRenderPass(cmd);

	// Begin lighting pass
	rp_info.renderPass = _lighting_pass;
	rp_info.framebuffer = _lighting_framebuffers[swapchain_image_index];
	rp_info.clearValueCount = 1;
	rp_info.pClearValues = clear_values;
	vkCmdBeginRenderPass(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);
	// Draw ambient light in a single full-screen quad
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _ambient_pipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _ambient_pipeline_layout, 0, 1, &_descriptor_sets[NUM_TEXTURES+1][frame_index], 0, nullptr);
	vkCmdDraw(cmd, 6, 1, 0, 0);

	//Draw front facing light volumes
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _lighting_front_pipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _lighting_pipeline_layout, 0, 1, &_descriptor_sets[NUM_TEXTURES+0][frame_index], 0, nullptr);
	vkCmdBindVertexBuffers(cmd, 0, 1, &_light_mesh._vertex_buffer._buffer, &offset);
	vkCmdBindIndexBuffer(cmd, _light_mesh._index_buffer._buffer, 0, VK_INDEX_TYPE_UINT32);

	// Draw back facing light volumes
	vkCmdDrawIndexed(cmd, _light_mesh._indices.size(), front_index, 0, 0, 0);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _lighting_back_pipeline);
	vkCmdDrawIndexed(cmd, _light_mesh._indices.size(), back_index, 0, 0, front_index);
	vkCmdEndRenderPass(cmd);

	// Begin pass to draw lights into the scene
	rp_info.renderPass = _forward_pass;
	rp_info.framebuffer = _forward_framebuffers[swapchain_image_index];
	rp_info.clearValueCount = 0;
	rp_info.pClearValues = nullptr;
	vkCmdBeginRenderPass(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _light_draw_pipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _light_draw_pipeline_layout, 0, 1, &_descriptor_sets[NUM_TEXTURES+2][frame_index], 0, nullptr);

	// Draw lights
	vkCmdDrawIndexed(cmd, _light_mesh._indices.size(), NUM_LIGHTS, 0, 0, 0);

	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
	vkCmdEndRenderPass(cmd);

	end_draw(frame_index, swapchain_image_index, cmd);
}

void DeferredEngine::init_render_passes()
{
	std::vector<VkAttachmentDescription> attachments = {};

	// Create lighting render pass
	attachments.push_back(infos::color_attachment_description(_swapchain_image_format, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
	attachments.push_back(infos::depth_attachment_description(_depth_image._format, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));
	
	// Lighting pass reuses depth from deferred pass
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	_lighting_pass = create_render_pass(attachments, true);

	// Create forward pass
	// Reuses color attachment from lighting pass
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	_forward_pass = create_render_pass(attachments, true);

	std::vector<VkAttachmentDescription> g_attachments = {};

	// Create deferred pass
	g_attachments.push_back(infos::color_attachment_description(VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
	g_attachments.push_back(infos::color_attachment_description(VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
	g_attachments.push_back(infos::color_attachment_description(VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
	g_attachments.push_back(infos::color_attachment_description(VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
	g_attachments.push_back(infos::depth_attachment_description(_depth_image._format, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));
	_g_pass = create_render_pass(g_attachments, true);
}

void DeferredEngine::init_framebuffers()
{
	// Create textures for deferred attachments
	_g_depth_image = create_texture(_window_extent.width, _window_extent.height, 4, _depth_image._format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_IMAGE_ASPECT_DEPTH_BIT, VK_FILTER_NEAREST);
	_g_position_image = create_texture(_window_extent.width, _window_extent.height, 4, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_NEAREST);
	_g_normal_image = create_texture(_window_extent.width, _window_extent.height, 4, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_NEAREST);
	_g_albedo_specular_image = create_texture(_window_extent.width, _window_extent.height, 4, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_NEAREST);
	_g_ao_image = create_texture(_window_extent.width, _window_extent.height, 4, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_NEAREST);

	_swapchain_deletion_queue.push_function([=]() {
		vkDestroyImageView(_device, _g_depth_image._image_view, nullptr);
		vkDestroyImageView(_device, _g_position_image._image_view, nullptr);
		vkDestroyImageView(_device, _g_normal_image._image_view, nullptr);
		vkDestroyImageView(_device, _g_albedo_specular_image._image_view, nullptr);
		vkDestroyImageView(_device, _g_ao_image._image_view, nullptr);
		vkDestroySampler(_device, _g_depth_image._image_info.sampler, nullptr);
		vkDestroySampler(_device, _g_position_image._image_info.sampler, nullptr);
		vkDestroySampler(_device, _g_normal_image._image_info.sampler, nullptr);
		vkDestroySampler(_device, _g_albedo_specular_image._image_info.sampler, nullptr);
		vkDestroySampler(_device, _g_ao_image._image_info.sampler, nullptr);
		vmaDestroyImage(_allocator, _g_depth_image._image, _g_depth_image._allocation);
		vmaDestroyImage(_allocator, _g_position_image._image, _g_position_image._allocation);
		vmaDestroyImage(_allocator, _g_normal_image._image, _g_normal_image._allocation);
		vmaDestroyImage(_allocator, _g_albedo_specular_image._image, _g_albedo_specular_image._allocation);
		vmaDestroyImage(_allocator, _g_ao_image._image, _g_ao_image._allocation);
	});

	// Create framebuffer objects
	std::vector<VkImageView> g_views = {_g_position_image._image_view, _g_normal_image._image_view, _g_albedo_specular_image._image_view, _g_ao_image._image_view, _depth_image._image_view};
	_g_framebuffer = create_framebuffer(_g_pass, g_views);
	_lighting_framebuffers = create_swapchain_framebuffers(_lighting_pass);
	_forward_framebuffers = create_swapchain_framebuffers(_forward_pass);
}

void DeferredEngine::init_descriptors()
{
	// Create deferred pass descriptor sets
	/*std::vector<VkDescriptorSetLayoutBinding> bindings = {
		infos::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
		infos::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1),
		infos::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
		infos::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),
		infos::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4),
		infos::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 5),
		infos::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 6)
	};

	_descriptor_layout = create_descriptor_layout(bindings);*/
	_descriptor_layout = _material_system._pipelines["g_pass"].descriptor_layout;
	_tex_descriptor_layout = _material_system._pipelines["light_front"].descriptor_layout;
	_light_draw_descriptor_layout = _material_system._pipelines["light_draw"].descriptor_layout;
	_ambient_descriptor_layout = _material_system._pipelines["ambient"].descriptor_layout;

	for (int i = 0; i < NUM_TEXTURES; i++)
	{
		_descriptor_sets[i] = allocate_descriptor_sets(_descriptor_layout, FRAME_OVERLAP);
	}
	_descriptor_sets[NUM_TEXTURES+0] = allocate_descriptor_sets(_tex_descriptor_layout, FRAME_OVERLAP);
	_descriptor_sets[NUM_TEXTURES+1] = allocate_descriptor_sets(_ambient_descriptor_layout, FRAME_OVERLAP);
	_descriptor_sets[NUM_TEXTURES+2] = allocate_descriptor_sets(_light_draw_descriptor_layout, FRAME_OVERLAP);

	// Create lighting pass descriptor sets
	/*bindings = {
		infos::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
		infos::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
		infos::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
		infos::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),
		infos::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 4),
		infos::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 5),
		infos::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 6),
		infos::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 7)
	};
	_tex_descriptor_layout = create_descriptor_layout(bindings);*/

	// Create forward pass descriptor sets
	/*bindings = {
		infos::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 5),
		infos::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 6),
		infos::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 7)
	};
	_light_draw_descriptor_layout = create_descriptor_layout(bindings);*/

	// Create descriptor sets for ambient lighting
	/*bindings = {
		infos::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
		infos::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
	};
	_ambient_descriptor_layout = create_descriptor_layout(bindings);*/
}

void DeferredEngine::init_pipelines()
{
	// Load shaders
	/*auto vertex_shader = load_shader("../shaders/lighting_pass.vert.spv");
	auto fragment_shader = load_shader("../shaders/lighting_pass.frag.spv");
	auto ambient_vertex_shader = load_shader("../shaders/ambient_pass.vert.spv");
	auto ambient_fragment_shader = load_shader("../shaders/ambient_pass.frag.spv");
	auto g_vertex_shader = load_shader("../shaders/g_pass.vert.spv");
	auto g_fragment_shader = load_shader("../shaders/g_pass.frag.spv");
	auto light_draw_fragment_shader = load_shader("../shaders/light_draw.frag.spv");
	
	// Create pipeline layouts
	VkPipelineLayoutCreateInfo pipeline_layout_info = infos::pipeline_layout_create_info();
	VkDescriptorSetLayout layouts[1] = {_tex_descriptor_layout};
	pipeline_layout_info.setLayoutCount = 1;
	pipeline_layout_info.pSetLayouts = layouts;
	VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &_lighting_pipeline_layout));

	pipeline_layout_info = infos::pipeline_layout_create_info();
	pipeline_layout_info.setLayoutCount = 1;
	pipeline_layout_info.pSetLayouts = &_ambient_descriptor_layout;
	VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &_ambient_pipeline_layout));

	pipeline_layout_info.setLayoutCount = 1;
	pipeline_layout_info.pSetLayouts = &_light_draw_descriptor_layout;
	VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &_light_draw_pipeline_layout));

	pipeline_layout_info.setLayoutCount = 1;
	pipeline_layout_info.pSetLayouts = &_descriptor_layout;
	VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &_g_pipeline_layout));

	// Set dynamic state for dynamic viewport/scissor
	VkDynamicState dynamic_states[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dynamic_state_info = {};
	dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state_info.pNext = nullptr;
	dynamic_state_info.dynamicStateCount = 2;
	dynamic_state_info.pDynamicStates = dynamic_states;

	PipelineBuilder pipeline_builder;
	pipeline_builder._shader_stages.push_back(infos::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertex_shader));
	pipeline_builder._shader_stages.push_back(infos::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragment_shader));

	VertexInputDescription vertex_description = Vertex::get_vertex_description();
	pipeline_builder._vertex_input_info = infos::vertex_input_state_create_info();
	pipeline_builder._input_assembly = infos::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipeline_builder._vertex_input_info.pVertexAttributeDescriptions = vertex_description.attributes.data();
	pipeline_builder._vertex_input_info.vertexAttributeDescriptionCount = vertex_description.attributes.size();
	pipeline_builder._vertex_input_info.pVertexBindingDescriptions = vertex_description.bindings.data();
	pipeline_builder._vertex_input_info.vertexBindingDescriptionCount = vertex_description.bindings.size();
	pipeline_builder._viewport.x = 0.0f;
	pipeline_builder._viewport.y = 0.0f;
	pipeline_builder._viewport.width = (float)_window_extent.width;
	pipeline_builder._viewport.height = (float)_window_extent.height;
	pipeline_builder._viewport.minDepth = 0.0f;
	pipeline_builder._viewport.maxDepth = 1.0f;
	pipeline_builder._scissor.offset = {0, 0};
	pipeline_builder._scissor.extent = _window_extent;
	pipeline_builder._rasterizer = infos::rasterization_state_create_info(VK_POLYGON_MODE_FILL);
	// Cull front faces
	pipeline_builder._rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
	pipeline_builder._rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	pipeline_builder._multisampling = infos::multisampling_state_create_info();
	pipeline_builder._color_blend_attachments = {infos::color_blend_attachment_state()};
	pipeline_builder._pipeline_layout = _lighting_pipeline_layout;
	pipeline_builder._depth_stencil = infos::depth_stencil_create_info(true, false, VK_COMPARE_OP_GREATER_OR_EQUAL);
	pipeline_builder._dynamic_state = dynamic_state_info;

	// Additive blending
	pipeline_builder._color_blend_attachments[0].blendEnable = true;
	pipeline_builder._color_blend_attachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	pipeline_builder._color_blend_attachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	pipeline_builder._color_blend_attachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	pipeline_builder._color_blend_attachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	pipeline_builder._color_blend_attachments[0].colorBlendOp = VK_BLEND_OP_ADD;
	pipeline_builder._color_blend_attachments[0].alphaBlendOp = VK_BLEND_OP_ADD;

	// Create lightng back pipeline
	_lighting_back_pipeline = pipeline_builder.build_pipeline(_device, _lighting_pass);

	// Now cull back faces
	pipeline_builder._rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	pipeline_builder._depth_stencil = infos::depth_stencil_create_info(true, false, VK_COMPARE_OP_LESS_OR_EQUAL);

	// Create lighting front pipeline
	_lighting_front_pipeline = pipeline_builder.build_pipeline(_device, _lighting_pass);*/

	_lighting_front_pipeline = _material_system._pipelines["light_front"].pipeline;
	_lighting_back_pipeline = _material_system._pipelines["light_back"].pipeline;
	_lighting_pipeline_layout = _material_system._pipelines["light_front"].layout;

	/*_main_deletion_queue.push_function([=]() {
		vkDestroyPipeline(_device, _lighting_front_pipeline, nullptr);
		vkDestroyPipeline(_device, _lighting_back_pipeline, nullptr);
		vkDestroyPipelineLayout(_device, _lighting_pipeline_layout, nullptr);
	});*/


	/*pipeline_builder._vertex_input_info = infos::vertex_input_state_create_info();
	pipeline_builder._input_assembly = infos::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipeline_builder._shader_stages.clear();
	pipeline_builder._shader_stages.push_back(infos::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, ambient_vertex_shader));
	pipeline_builder._shader_stages.push_back(infos::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, ambient_fragment_shader));
	pipeline_builder._pipeline_layout = _ambient_pipeline_layout;
	
	// Don't bother with culling for full-screen quad
	pipeline_builder._rasterizer.cullMode = VK_CULL_MODE_NONE;

	// Create ambient lighting pipeline
	_ambient_pipeline = pipeline_builder.build_pipeline(_device, _lighting_pass);*/

	_ambient_pipeline = _material_system._pipelines["ambient"].pipeline;
	_ambient_pipeline_layout = _material_system._pipelines["ambient"].layout;

	/*_main_deletion_queue.push_function([=]() {
		vkDestroyPipeline(_device, _ambient_pipeline, nullptr);
		vkDestroyPipelineLayout(_device, _ambient_pipeline_layout, nullptr);
	});*/

	/*vkDestroyShaderModule(_device, ambient_vertex_shader, nullptr);
	vkDestroyShaderModule(_device, ambient_fragment_shader, nullptr);

	pipeline_builder._shader_stages.clear();
	pipeline_builder._shader_stages.push_back(infos::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, g_vertex_shader));
	pipeline_builder._shader_stages.push_back(infos::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, g_fragment_shader));
	pipeline_builder._color_blend_attachments = {infos::color_blend_attachment_state(), infos::color_blend_attachment_state(), infos::color_blend_attachment_state(), infos::color_blend_attachment_state()};
	pipeline_builder._pipeline_layout = _g_pipeline_layout;
	pipeline_builder._vertex_input_info.pVertexAttributeDescriptions = vertex_description.attributes.data();
	pipeline_builder._vertex_input_info.vertexAttributeDescriptionCount = vertex_description.attributes.size();
	pipeline_builder._vertex_input_info.pVertexBindingDescriptions = vertex_description.bindings.data();
	pipeline_builder._vertex_input_info.vertexBindingDescriptionCount = vertex_description.bindings.size();
	pipeline_builder._depth_stencil = infos::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

	// Cull back faces for main geometry
	pipeline_builder._rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	pipeline_builder._rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	// Create deferred pass pipeline
	_g_pipeline = pipeline_builder.build_pipeline(_device, _g_pass);

	vkDestroyShaderModule(_device, g_vertex_shader, nullptr);
	vkDestroyShaderModule(_device, g_fragment_shader, nullptr);*/

	_g_pipeline = _material_system._pipelines["g_pass"].pipeline;
	_g_pipeline_layout = _material_system._pipelines["g_pass"].layout;

	/*_main_deletion_queue.push_function([=]() {
		vkDestroyPipeline(_device, _g_pipeline, nullptr);
		vkDestroyPipelineLayout(_device, _g_pipeline_layout, nullptr);
	});*/

	/*pipeline_builder._shader_stages.clear();
	pipeline_builder._shader_stages.push_back(infos::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertex_shader));
	pipeline_builder._shader_stages.push_back(infos::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, light_draw_fragment_shader));
	pipeline_builder._pipeline_layout = _light_draw_pipeline_layout;
	pipeline_builder._color_blend_attachments = {infos::color_blend_attachment_state()};

	// Create forward pass pipeline
	_light_draw_pipeline = pipeline_builder.build_pipeline(_device, _forward_pass);*/

	_light_draw_pipeline = _material_system._pipelines["light_draw"].pipeline;
	_light_draw_pipeline_layout = _material_system._pipelines["light_draw"].layout;

	/*_main_deletion_queue.push_function([=]() {
		vkDestroyPipeline(_device, _light_draw_pipeline, nullptr);
		vkDestroyPipelineLayout(_device, _light_draw_pipeline_layout, nullptr);
	});*/

	/*vkDestroyShaderModule(_device, vertex_shader, nullptr);
	vkDestroyShaderModule(_device, fragment_shader, nullptr);
	vkDestroyShaderModule(_device, light_draw_fragment_shader, nullptr);*/
}

void DeferredEngine::init_models()
{
	// Load models
	/*_monkey_mesh = load_mesh("../assets/monkey_smooth.m");
	upload_mesh(_monkey_mesh);
	_empire_mesh = load_mesh("../assets/lost_empire.m");
	upload_mesh(_empire_mesh);
	_light_mesh = load_mesh("../assets/sphere.m");
	upload_mesh(_light_mesh);*/
	_monkey_mesh = _asset_system.get_mesh(_asset_system.get_mesh_id("monkey"));
	_empire_mesh = _asset_system.get_mesh(_asset_system.get_mesh_id("empire"));
	_light_mesh = _asset_system.get_mesh(_asset_system.get_mesh_id("light"));

	// Load textures
	/*_albedo[0] = load_texture("../assets/iron/rustediron2_basecolor.t", VK_FORMAT_R8G8B8A8_SRGB);
	_specular[0] = load_texture("../assets/iron/rustediron2_roughness.t", VK_FORMAT_R8G8B8A8_UNORM);
	_normal[0] = load_texture("../assets/iron/rustediron2_normal.t", VK_FORMAT_R8G8B8A8_SRGB);
	_metal[0] = load_texture("../assets/iron/rustediron2_metallic.t", VK_FORMAT_R8G8B8A8_UNORM);
	_ao[0] = load_texture("../assets/iron/rustediron2_ao.t", VK_FORMAT_R8G8B8A8_UNORM);
	_albedo[1] = load_texture("../assets/dent/dented-metal_albedo.t", VK_FORMAT_R8G8B8A8_SRGB);
	_specular[1] = load_texture("../assets/dent/dented-metal_roughness.t", VK_FORMAT_R8G8B8A8_UNORM);
	_normal[1] = load_texture("../assets/dent/dented-metal_normal-dx.t", VK_FORMAT_R8G8B8A8_SRGB);
	_metal[1] = load_texture("../assets/dent/dented-metal_metallic.t", VK_FORMAT_R8G8B8A8_UNORM);
	_ao[1] = load_texture("../assets/dent/dented-metal_ao.t", VK_FORMAT_R8G8B8A8_UNORM);
	_albedo[2] = load_texture("../assets/stone/slimy-slippery-rock1_albedo.t", VK_FORMAT_R8G8B8A8_SRGB);
	_specular[2] = load_texture("../assets/stone/slimy-slippery-rock1_roughness.t", VK_FORMAT_R8G8B8A8_UNORM);
	_normal[2] = load_texture("../assets/stone/slimy-slippery-rock1_normal-dx.t", VK_FORMAT_R8G8B8A8_SRGB);
	_metal[2] = load_texture("../assets/stone/slimy-slippery-rock1_metallic.t", VK_FORMAT_R8G8B8A8_UNORM);
	_ao[2] = load_texture("../assets/stone/slimy-slippery-rock1_ao.t", VK_FORMAT_R8G8B8A8_UNORM);
	_albedo[3] = load_texture("../assets/concrete/degraded-concrete_albedo.t", VK_FORMAT_R8G8B8A8_SRGB);
	_specular[3] = load_texture("../assets/concrete/degraded-concrete_roughness.t", VK_FORMAT_R8G8B8A8_UNORM);
	_normal[3] = load_texture("../assets/concrete/degraded-concrete_normal-dx.t", VK_FORMAT_R8G8B8A8_SRGB);
	_metal[3] = load_texture("../assets/concrete/degraded-concrete_metallic.t", VK_FORMAT_R8G8B8A8_UNORM);
	_ao[3] = load_texture("../assets/concrete/degraded-concrete_ao.t", VK_FORMAT_R8G8B8A8_UNORM);*/

	_albedo[0] = _asset_system.get_texture(_asset_system.get_texture_id("rust_albedo"));
	_specular[0] = _asset_system.get_texture(_asset_system.get_texture_id("rust_specular"));
	_normal[0] = _asset_system.get_texture(_asset_system.get_texture_id("rust_normal"));
	_metal[0] = _asset_system.get_texture(_asset_system.get_texture_id("rust_metal"));
	_ao[0] = _asset_system.get_texture(_asset_system.get_texture_id("rust_ao"));

	_albedo[3] = _asset_system.get_texture(_asset_system.get_texture_id("cheese_albedo"));
	_specular[3] = _asset_system.get_texture(_asset_system.get_texture_id("cheese_specular"));
	_normal[3] = _asset_system.get_texture(_asset_system.get_texture_id("cheese_normal"));
	_metal[3] = _asset_system.get_texture(_asset_system.get_texture_id("cheese_metal"));
	_ao[3] = _asset_system.get_texture(_asset_system.get_texture_id("cheese_ao"));
	
	_albedo[1] = _asset_system.get_texture(_asset_system.get_texture_id("dent_albedo"));
	_specular[1] = _asset_system.get_texture(_asset_system.get_texture_id("dent_specular"));
	_normal[1] = _asset_system.get_texture(_asset_system.get_texture_id("dent_normal"));
	_metal[1] = _asset_system.get_texture(_asset_system.get_texture_id("dent_metal"));
	_ao[1] = _asset_system.get_texture(_asset_system.get_texture_id("dent_ao"));

	_albedo[2] = _asset_system.get_texture(_asset_system.get_texture_id("rock_albedo"));
	_specular[2] = _asset_system.get_texture(_asset_system.get_texture_id("rock_specular"));
	_normal[2] = _asset_system.get_texture(_asset_system.get_texture_id("rock_normal"));
	_metal[2] = _asset_system.get_texture(_asset_system.get_texture_id("rock_metal"));
	_ao[2] = _asset_system.get_texture(_asset_system.get_texture_id("rock_ao"));

	_albedo[4] = _asset_system.get_texture(_asset_system.get_texture_id("conc_albedo"));
	_specular[4] = _asset_system.get_texture(_asset_system.get_texture_id("conc_specular"));
	_normal[4] = _asset_system.get_texture(_asset_system.get_texture_id("conc_normal"));
	_metal[4] = _asset_system.get_texture(_asset_system.get_texture_id("conc_metal"));
	_ao[4] = _asset_system.get_texture(_asset_system.get_texture_id("conc_ao"));
}

void DeferredEngine::init_scene()
{
	// Create uniform/storage buffers
	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		_uniform_buffers[i] = create_buffer(sizeof(CameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
		_storage_buffers[i] = create_buffer(sizeof(ObjectData) * NUM_MONKEYS+1, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
		//_light_uniform_buffers[i] = create_buffer(sizeof(CameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
		_light_storage_buffers[i] = create_buffer(sizeof(ObjectData) * NUM_LIGHTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
		_light_buffers[i] = create_buffer(sizeof(LightData) * NUM_LIGHTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
		_light_draw_storage_buffers[i] = create_buffer(sizeof(ObjectData) * NUM_LIGHTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	}

	_main_deletion_queue.push_function([=]() {
		for (int i = 0; i < FRAME_OVERLAP; i++)
		{
			vmaDestroyBuffer(_allocator, _uniform_buffers[i]._buffer, _uniform_buffers[i]._allocation);
			vmaDestroyBuffer(_allocator, _storage_buffers[i]._buffer, _storage_buffers[i]._allocation);
			//vmaDestroyBuffer(_allocator, _light_uniform_buffers[i]._buffer, _light_uniform_buffers[i]._allocation);
			vmaDestroyBuffer(_allocator, _light_storage_buffers[i]._buffer, _light_storage_buffers[i]._allocation);
			vmaDestroyBuffer(_allocator, _light_buffers[i]._buffer, _light_buffers[i]._allocation);
			vmaDestroyBuffer(_allocator, _light_draw_storage_buffers[i]._buffer, _light_draw_storage_buffers[i]._allocation);
		}
	});

	// Initialize light to random position/color/radius
	for (int i = 0; i < NUM_LIGHTS; i++)
	{
		_lights_info[i].pos_r.x = 20 * std::pow((float)rand() / RAND_MAX, 0.5f) - 10;
		_lights_info[i].pos_r.y = 20 * std::pow((float)rand() / RAND_MAX, 0.5f) - 5;
		_lights_info[i].pos_r.z = -50 * std::pow((float)rand() / RAND_MAX, 1.0f);
		_lights_info[i].pos_r.w = std::min(-_lights_info[i].pos_r.z * std::pow((float)rand() / RAND_MAX, 7.0f), (float)MAX_RADIUS);
		_lights_info[i].color = glm::vec4(2.0f * glm::normalize(glm::vec3((float)rand() / RAND_MAX, (float)rand() / RAND_MAX, (float)rand() / RAND_MAX)), 1.0f);
	}

	// Place monkey heads in random positions
	for (int i = 0; i < NUM_MONKEYS; i++)
	{
		_monkey_pos[i] = glm::vec3(-30 * (float)rand() / RAND_MAX + 15, 20 * (float)rand() / RAND_MAX - 5, -50 * (float)rand() / RAND_MAX);;
	}

}

void DeferredEngine::write_descriptors()
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
			if (in == 1)
			{
				info_count--;
			}
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
						writes.push_back(infos::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, _descriptor_sets[info_count][i], buffer_info, j));
					}
					else if (name.size() > 3 && name.substr(0, 3) == "SB:")
					{
						VkDescriptorBufferInfo *buffer_info;
						auto label = name.substr(3, name.size()-3);
						if (label == "obj_data")
						{
							buffer_info = &_storage_buffers[i]._buffer_info;
						}
						else if (label == "light_obj_data")
						{
							buffer_info = &_light_storage_buffers[i]._buffer_info;
						}
						else if (label == "light_data")
						{
							buffer_info = &_light_buffers[i]._buffer_info;
						}
						else if (label == "light_draw_data")
						{
							buffer_info = &_light_draw_storage_buffers[i]._buffer_info;
						}
						writes.push_back(infos::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, _descriptor_sets[info_count][i], buffer_info, j));
					}
					else if (name.size() > 4 && name.substr(0, 4) == "TEX:")
					{
						VkDescriptorImageInfo *tex_info;
						auto label = name.substr(4, name.size()-4);
						tex_info = &_asset_system.get_texture(_asset_system.get_texture_id(label))._image_info;
						writes.push_back(infos::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, _descriptor_sets[info_count][i], tex_info, j));
					}
					else if (name.size() > 4 && name.substr(0, 4) == "ATT:")
					{
						VkDescriptorImageInfo *tex_info;
						auto label = name.substr(4, name.size()-4);

						if (label == "g_albedo")
						{
							tex_info = &_g_albedo_specular_image._image_info;
						}
						else if (label == "g_ao")
						{
							tex_info = &_g_ao_image._image_info;
						}
						else if (label == "g_pos")
						{
							tex_info = &_g_position_image._image_info;
						}
						else if (label == "g_norm")
						{
							tex_info = &_g_normal_image._image_info;
						}
						else if (label == "g_depth")
						{
							tex_info = &_g_depth_image._image_info;
						}

						writes.push_back(infos::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, _descriptor_sets[info_count][i], tex_info, j));
					}
				}
			}

			vkUpdateDescriptorSets(_device, writes.size(), writes.data(), 0, nullptr);

			info_count++;
		}
	}
}

void DeferredEngine::resize_window(uint32_t w, uint32_t h)
{
	// Resize swapchain
	resize_swapchain(w, h, _lighting_pass);

	// Recreate framebuffers
	init_framebuffers();
	// Rewrite descriptor sets to use new g-buffers
	write_descriptors();
}
