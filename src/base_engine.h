#pragma once

#include <inc.h>

#include <functional>
#include <deque>

#include "resource.h"
#include "mesh.h"
#include "deletion_queue.h"
#include "asset_system.h"
#include "material_system.h"

#include <vma/vk_mem_alloc.h>

#define VK_CHECK(x)								\
	do 									\
	{									\
		VkResult err = x;						\
		if (err)							\
		{								\
			std::cout << "Detected Vulkan Error: " << err << "\n";	\
			abort();						\
		}								\
	} while (0)

//Number of frames in flight at once
const uint32_t FRAME_OVERLAP = 2;

// Includes a set of helper functions to make setup
// easier when starting a new project
class BaseEngine
{
public:
	void cleanup();

	void run(bool &quit);

	// Sets up command buffer and acquires image
	bool start_draw(uint32_t *frame_index, uint32_t *swapchain_image_index, VkCommandBuffer *cmd);
	// Submits command buffer and displays image
	void end_draw(uint32_t frame_index, uint32_t swapchain_image_index, VkCommandBuffer cmd);

	void init_sdl(std::string window_name);
	void init_vulkan(std::string app_name, bool validation_layers);
	void init_swapchain();
	void init_commands();
	void init_sync_structures();
	void init_descriptor_pool();
	void init_imgui(VkRenderPass render_pass);

	// Must be implemented when inheriting.
	// Handles recreating framebuffers and other things
	// not created by BaseEngine.
	virtual void resize_window(uint32_t w, uint32_t h) = 0;

	VkRenderPass create_render_pass(std::vector<VkAttachmentDescription> attachments, bool depth_attachment);
	VkPipeline create_pipeline(const PipelineInfo &info, VkPipelineLayout layout, VkRenderPass render_pass);
	VkFramebuffer create_framebuffer(VkRenderPass render_pass, std::vector<VkImageView> attachments);
	std::vector<VkFramebuffer> create_swapchain_framebuffers(VkRenderPass render_pass);
	VkDescriptorSetLayout create_descriptor_layout(std::vector<VkDescriptorSetLayoutBinding> bindings);
	std::vector<VkDescriptorSet> allocate_descriptor_sets(VkDescriptorSetLayout layout, int count);
	VkShaderModule load_shader(std::string filename);
	Buffer create_buffer(size_t alloc_size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage);
	Texture create_texture(size_t width, size_t height, size_t pixel_size, VkFormat format, VkImageUsageFlags usage, VmaMemoryUsage memory_usage, VkImageAspectFlags aspect, VkFilter filter = VK_FILTER_NEAREST, uint32_t mip_levels = 1);
	void upload_texture(Texture &tex, void *pixel_ptr, VkFormat format);
	void upload_mesh(Mesh &mesh);
	Mesh load_mesh(std::string filename);
	void immediate_submit(std::function<void(VkCommandBuffer cmd)> &&function);
	void resize_swapchain(uint32_t w, uint32_t h, VkRenderPass render_pass);

	int _frame_number;
	VkExtent2D _window_extent{1920, 1080};
	struct SDL_Window *_window{nullptr};

	// DeletionQueue is a reverse order queue (Is that just a stack?)
	// of functions for deleting Vulkan components.
	// _swapchain_deletion_queue separates items that
	// need to be deleted and recreated when resizing the window.
	DeletionQueue _main_deletion_queue;
	DeletionQueue _swapchain_deletion_queue;

	VkInstance _instance;
	VkDebugUtilsMessengerEXT _debug_messenger;
	VkPhysicalDevice _chosen_gpu;
	VkPhysicalDeviceProperties _gpu_properties;
	VkDevice _device;
	VkSurfaceKHR _surface;
	VmaAllocator _allocator;

	VkQueue _graphics_queue;
	uint32_t _graphics_queue_family;

	VkSwapchainKHR _swapchain;
	VkFormat _swapchain_image_format;
	std::vector<VkImage> _swapchain_images;
	std::vector<VkImageView> _swapchain_image_views;
	Texture _depth_image;

	VkCommandPool _upload_command_pool;
	VkCommandBuffer _upload_command_buffer;
	VkCommandPool _draw_command_pools[FRAME_OVERLAP];
	VkCommandBuffer _draw_command_buffers[FRAME_OVERLAP];

	VkFence _render_fences[FRAME_OVERLAP];
	VkSemaphore _render_semaphores[FRAME_OVERLAP];
	VkSemaphore _present_semaphores[FRAME_OVERLAP];
	VkFence _upload_fence;

	MaterialSystem _material_system;
	AssetSystem _asset_system;

	VkDescriptorPool _descriptor_pool;
};
