#pragma once

#include "../base_engine.h"

#define NUM_MATS 4

struct aligned_vec3
{
	alignas(16) glm::vec3 vec;
};

struct AOData
{
	glm::mat4 proj;
	glm::vec4 kernel[64];
	glm::vec4 rotation[16];
	glm::vec4 radBiasContrastAspect;
};

struct CameraData
{
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 proj_view;
};

struct ObjectData
{
	glm::mat4 model_matrix;
};

class AOEngine : public BaseEngine
{
public:
	void init();
	void draw();

	void init_render_passes();
	void init_framebuffers();
	void init_descriptors();
	void init_scene();
	void write_descriptors();

	virtual void resize_window(uint32_t w, uint32_t h);

	VkRenderPass _depth_pass;
	VkRenderPass _ao_pass;
	VkRenderPass _blur_pass;
	VkRenderPass _draw_pass;

	size_t _mat_ids[NUM_MATS];
	size_t _empire_mesh_id;

	VkFramebuffer _depth_framebuffer;
	VkFramebuffer _ao_framebuffer;
	VkFramebuffer _blur_framebuffer;
	std::vector<VkFramebuffer> _draw_framebuffers;

	Texture _ao_depth_image, _ao_image, _blur_image, _color_image;

	std::vector<std::vector<VkDescriptorSet>> _descriptor_sets[NUM_MATS];

	Buffer _uniform_buffers[FRAME_OVERLAP];
	Buffer _storage_buffers[FRAME_OVERLAP];

	Buffer _ao_uniform_buffers[FRAME_OVERLAP];
	Buffer _draw_mode_uniform_buffers[FRAME_OVERLAP];

	AOData ao_data;
	int _draw_mode;
};
