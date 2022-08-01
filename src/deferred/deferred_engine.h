#pragma once

#include "../base_engine.h"

#define NUM_LIGHTS 300
#define NUM_MONKEYS 1000
#define NUM_TEXTURES 6
#define MAX_RADIUS 100
#define NUM_MATS 8

struct LightData
{
	glm::vec4 pos_r;
	glm::vec4 color;
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

class DeferredEngine : public BaseEngine
{
public:
	void init();
	void draw();

	void init_render_passes();
	void init_framebuffers();
	void init_descriptors();
	void init_pipelines();
	void init_models();
	void init_scene();
	void write_descriptors();

	virtual void resize_window(uint32_t w, uint32_t h);

	// Render passes for filling g-buffers, calculating lighting
	// and drawing lights in a forward pass
	VkRenderPass _g_pass;
	VkRenderPass _lighting_pass;
	VkRenderPass _forward_pass;

	size_t _mat_ids[NUM_MATS];

	// Framebuffers for the render passes
	std::vector<VkFramebuffer> _lighting_framebuffers;
	VkFramebuffer _g_framebuffer;
	std::vector<VkFramebuffer> _forward_framebuffers;

	// Textures to draw to for  deferred pass
	Texture _g_depth_image, _g_position_image, _g_normal_image, _g_albedo_specular_image, _g_ao_image;

	// Descriptor sets and layouts for the different materials
	VkDescriptorSetLayout _descriptor_layout;
	VkDescriptorSetLayout _tex_descriptor_layout;
	VkDescriptorSetLayout _light_draw_descriptor_layout;
	VkDescriptorSetLayout _ambient_descriptor_layout;
	// NOTE: NUM_TEXTURES+0 is tex, NUM_TEXTURES+1 is ambient, NUM_TEXTURES+2 is light_draw
	std::vector<VkDescriptorSet> _descriptor_sets[NUM_TEXTURES + 3];

	// Pipelines to draw light_volumes. One draws front faces, the other draws back faces
	VkPipeline _lighting_front_pipeline;
	VkPipeline _lighting_back_pipeline;
	VkPipelineLayout _lighting_pipeline_layout;

	// Pipeline to draw lights into scene
	VkPipeline _light_draw_pipeline;
	VkPipelineLayout _light_draw_pipeline_layout;

	// Pipeline for ambient lighting
	VkPipeline _ambient_pipeline;
	VkPipelineLayout _ambient_pipeline_layout;

	// Pipeline to draw to g-buffers
	VkPipeline _g_pipeline;
	VkPipelineLayout _g_pipeline_layout;

	// Meshes are hardcoded because I didn't have an asset system by the time I made this,
	// but it's easy enough to add more
	Mesh _monkey_mesh;
	Mesh _empire_mesh;
	Mesh _light_mesh;

	// Textures are also hardcoded
	Texture _albedo[NUM_TEXTURES];
	Texture _specular[NUM_TEXTURES];
	Texture _normal[NUM_TEXTURES];
	Texture _metal[NUM_TEXTURES];
	Texture _ao[NUM_TEXTURES];

	// Buffers for drawing objects into the scene
	Buffer _uniform_buffers[FRAME_OVERLAP];
	Buffer _storage_buffers[FRAME_OVERLAP];

	// Buffers for drawing light volumes
	Buffer _light_buffers[FRAME_OVERLAP];
	//Buffer _light_uniform_buffers[FRAME_OVERLAP];
	Buffer _light_storage_buffers[FRAME_OVERLAP];

	// Buffer for drawing lights into scene
	Buffer _light_draw_storage_buffers[FRAME_OVERLAP];

	// Keeps information about the lights
	LightData _lights_info[NUM_LIGHTS];

	// Structures for writing to uniform buffers.
	// These could probably be made local to the draw
	// function, but having them here works too.
	LightData _lights_info_front[NUM_LIGHTS];
	LightData _lights_info_back[NUM_LIGHTS];

	// Positions for monkey heads
	glm::vec3 _monkey_pos[NUM_MONKEYS];
};
