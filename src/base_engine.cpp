#include "base_engine.h"

#include <SDL2/SDL_vulkan.h>
#include "vkbootstrap/VkBootstrap.h"

#include "infos.h"

#include <imgui/imgui.h>
#include <imgui/imgui_impl_vulkan.h>
#include <imgui/imgui_impl_sdl.h>

#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

#include <fstream>

#include "asset_packer/asset_packer.h"
#include <lz4.h>

#include "pipeline_builder.h"

void BaseEngine::cleanup()
{
	// Wait for current frame to finish
	vkWaitForFences(_device, 1, &_render_fences[(_frame_number-1) % FRAME_OVERLAP], true, 1000000000);

	// Delete vulkan objects
	_swapchain_deletion_queue.flush();
	_material_system.destroy(this);
	_main_deletion_queue.flush();

	// Finish cleaning up vulkan/SDL
	vkDestroySurfaceKHR(_instance, _surface, nullptr);
	vkDestroyDevice(_device, nullptr);
	vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
	vkDestroyInstance(_instance, nullptr);

	SDL_DestroyWindow(_window);
}

void BaseEngine::run(bool &quit)
{
	SDL_Event e;

	while (SDL_PollEvent(&e) != 0)
	{
		ImGui_ImplSDL2_ProcessEvent(&e);
		if (e.type == SDL_QUIT)
		{
			quit = true;
		}
		else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED)
		{
			// Call virtual function to recreate swapchain/framebuffers and resize the window
			this->resize_window(e.window.data1, e.window.data2);
		}
	}

}

bool BaseEngine::start_draw(uint32_t *frame_index, uint32_t *swapchain_image_index, VkCommandBuffer *cmd)
{
	// Set the index to current frame
	*frame_index = _frame_number % FRAME_OVERLAP;

	VK_CHECK(vkWaitForFences(_device, 1, &_render_fences[*frame_index], true, UINT64_MAX));
	VK_CHECK(vkResetFences(_device, 1, &_render_fences[*frame_index]));

	VkResult result = vkAcquireNextImageKHR(_device, _swapchain, UINT64_MAX, _present_semaphores[*frame_index], nullptr, swapchain_image_index);

	// If swapchain is out of date, resize window
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		int w, h;
    		SDL_GetWindowSize(_window, &w, &h);
		this->resize_window(w, h);
	}

	// Ignore VK_SUBOPTIMAL_KHR
	if (result != VK_SUBOPTIMAL_KHR)
	{
		VK_CHECK(result);
	}

	VK_CHECK(vkResetCommandBuffer(_draw_command_buffers[*frame_index], 0));

	*cmd = _draw_command_buffers[*frame_index];

	VkCommandBufferBeginInfo cmd_begin_info = {};
	cmd_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmd_begin_info.pNext = nullptr;
	cmd_begin_info.pInheritanceInfo = nullptr;
	cmd_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CHECK(vkBeginCommandBuffer(*cmd, &cmd_begin_info));

	return true;
}

void BaseEngine::end_draw(uint32_t frame_index, uint32_t swapchain_image_index, VkCommandBuffer cmd)
{
	VK_CHECK(vkEndCommandBuffer(cmd));

	VkSubmitInfo submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.pNext = nullptr;

	VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	submit.pWaitDstStageMask = &wait_stage;
	submit.waitSemaphoreCount = 1;
	submit.pWaitSemaphores = &_present_semaphores[frame_index];
	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores = &_render_semaphores[frame_index];
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &cmd;

	VK_CHECK(vkQueueSubmit(_graphics_queue, 1, &submit, _render_fences[frame_index]));

	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pNext = nullptr;
	present_info.pSwapchains = &_swapchain;
	present_info.swapchainCount = 1;
	present_info.pWaitSemaphores = &_render_semaphores[frame_index];
	present_info.waitSemaphoreCount = 1;
	present_info.pImageIndices = &swapchain_image_index;

	VkResult result = vkQueuePresentKHR(_graphics_queue, &present_info);

	// Resize window if swapchain is out of date
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		int w, h;
		SDL_GetWindowSize(_window, &w, &h);
		this->resize_window(w, h);
	}

	if (result != VK_SUBOPTIMAL_KHR)
	{
		VK_CHECK(result);
	}

	_frame_number++;
}

void BaseEngine::init_sdl(std::string window_name)
{
	SDL_Init(SDL_INIT_VIDEO);

	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

	_window = SDL_CreateWindow(window_name.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, _window_extent.width, _window_extent.height, window_flags);
}

void BaseEngine::init_vulkan(std::string app_name, bool validation_layers)
{
	// Create Instance
	vkb::InstanceBuilder builder;

	auto inst_ret = builder.set_app_name(app_name.c_str())
		.request_validation_layers(validation_layers)
		.require_api_version(1, 2, 0)
		.use_default_debug_messenger()
		.build();

	vkb::Instance vkb_inst = inst_ret.value();
	_instance = vkb_inst.instance;
	_debug_messenger = vkb_inst.debug_messenger;

	SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

	// Select physical device
	vkb::PhysicalDeviceSelector selector {vkb_inst};
	vkb::PhysicalDevice physical_device = selector
		.set_minimum_version(1, 2)
		.set_surface(_surface)
		.select()
		.value();

	// Create device
	vkb::DeviceBuilder device_builder {physical_device};
	vkb::Device vkb_device = device_builder.build().value();

	_device = vkb_device.device;
	_chosen_gpu = physical_device.physical_device;

	// Select queues
	_graphics_queue = vkb_device.get_queue(vkb::QueueType::graphics).value();
	_graphics_queue_family = vkb_device.get_queue_index(vkb::QueueType::graphics).value();

	_gpu_properties = vkb_device.physical_device.properties;

	// Create memory allocator
	VmaAllocatorCreateInfo allocator_info = {};
	allocator_info.physicalDevice = _chosen_gpu;
	allocator_info.device = _device;
	allocator_info.instance = _instance;
	vmaCreateAllocator(&allocator_info, &_allocator);

	_main_deletion_queue.push_function([=]() {
		vmaDestroyAllocator(_allocator);
	});
}

void BaseEngine::init_swapchain()
{
	// Initialize swapchain
	vkb::SwapchainBuilder swapchain_builder{_chosen_gpu, _device, _surface};

	vkb::Swapchain vkb_swapchain = swapchain_builder
		//.set_desired_format({VK_FORMAT_R16G16B16A16_SFLOAT,VK_COLOR_SPACE_HDR10_ST2084_EXT})
		.use_default_format_selection()
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(_window_extent.width, _window_extent.height)
		.build()
		.value();

	_swapchain = vkb_swapchain.swapchain;
	_swapchain_images = vkb_swapchain.get_images().value();
	_swapchain_image_views = vkb_swapchain.get_image_views().value();
	_swapchain_image_format = vkb_swapchain.image_format;

	_swapchain_deletion_queue.push_function([=]() {
		vkDestroySwapchainKHR(_device, _swapchain, nullptr);

		for (int i = 0; i < _swapchain_image_views.size(); i++)
		{
			vkDestroyImageView(_device, _swapchain_image_views[i], nullptr);
		}
	});

	// Create depth image
	VkExtent3D depth_image_extent = { _window_extent.width, _window_extent.height, 1};

	// Hardcoding format. Hopefully most gpu's can handle a 32-bit float for depth.
	_depth_image._format = VK_FORMAT_D32_SFLOAT;
	VkImageCreateInfo dimg_info = infos::image_create_info(_depth_image._format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depth_image_extent);

	VmaAllocationCreateInfo dimg_alloc_info = {};
	dimg_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	dimg_alloc_info.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vmaCreateImage(_allocator, &dimg_info, &dimg_alloc_info, &_depth_image._image, &_depth_image._allocation, nullptr);

	// Create depth image view
	VkImageViewCreateInfo dview_info = infos::image_view_create_info(_depth_image._format, _depth_image._image, VK_IMAGE_ASPECT_DEPTH_BIT);
	VK_CHECK(vkCreateImageView(_device, &dview_info, nullptr, &_depth_image._image_view));

	_swapchain_deletion_queue.push_function([=]() {
		vkDestroyImageView(_device, _depth_image._image_view, nullptr);
		vmaDestroyImage(_allocator, _depth_image._image, _depth_image._allocation);
	});
}

void BaseEngine::init_commands()
{
	// Create main command pool and allocate buffers
	VkCommandPoolCreateInfo command_pool_info = infos::command_pool_create_info(_graphics_queue_family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		VK_CHECK(vkCreateCommandPool(_device, &command_pool_info, nullptr, &_draw_command_pools[i]));
		VkCommandBufferAllocateInfo cmd_alloc_info = infos::command_buffer_allocate_info(_draw_command_pools[i], 1);
		VK_CHECK(vkAllocateCommandBuffers(_device, &cmd_alloc_info, &_draw_command_buffers[i]));

		_main_deletion_queue.push_function([=]() {
			vkDestroyCommandPool(_device, _draw_command_pools[i], nullptr);
		});
	}

	// Create command pool for uploading and allocate buffers
	VkCommandPoolCreateInfo upload_command_pool_info = infos::command_pool_create_info(_graphics_queue_family);
	VK_CHECK(vkCreateCommandPool(_device, &upload_command_pool_info, nullptr, &_upload_command_pool));

	_main_deletion_queue.push_function([=]() {
		vkDestroyCommandPool(_device, _upload_command_pool, nullptr);
	});

	VkCommandBufferAllocateInfo cmd_alloc_info = infos::command_buffer_allocate_info(_upload_command_pool, 1);
	VK_CHECK(vkAllocateCommandBuffers(_device, &cmd_alloc_info, &_upload_command_buffer));
}

void BaseEngine::init_sync_structures()
{
	// Create fences and semaphores
	VkFenceCreateInfo fence_info = infos::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo semaphore_info = infos::semaphore_create_info();

	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		VK_CHECK(vkCreateFence(_device, &fence_info, nullptr, &_render_fences[i]));

		_main_deletion_queue.push_function([=]() {
			vkDestroyFence(_device, _render_fences[i], nullptr);
		});

		VK_CHECK(vkCreateSemaphore(_device, &semaphore_info, nullptr, &_present_semaphores[i]));
		VK_CHECK(vkCreateSemaphore(_device, &semaphore_info, nullptr, &_render_semaphores[i]));
		
		_main_deletion_queue.push_function([=]() {
			vkDestroySemaphore(_device, _present_semaphores[i], nullptr);
			vkDestroySemaphore(_device, _render_semaphores[i], nullptr);
		});
	}

	VkFenceCreateInfo upload_fence_create_info = infos::fence_create_info();
	VK_CHECK(vkCreateFence(_device, &upload_fence_create_info, nullptr, &_upload_fence));

	_main_deletion_queue.push_function([=]() {
		vkDestroyFence(_device, _upload_fence, nullptr);
	});
}

void BaseEngine::init_descriptor_pool()
{
	// Create main descriptor pool
	std::vector<VkDescriptorPoolSize> sizes = {
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000}
	};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = 0;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = (uint32_t)sizes.size();
	pool_info.pPoolSizes = sizes.data();

	vkCreateDescriptorPool(_device, &pool_info, nullptr, &_descriptor_pool);

	_main_deletion_queue.push_function([=]() {
		vkDestroyDescriptorPool(_device, _descriptor_pool, nullptr);
	});
}

void BaseEngine::init_imgui(VkRenderPass render_pass)
{
	// Setup imgui for use with vulkan
	VkDescriptorPoolSize pool_sizes[] = {
		{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
		{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
		{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000},
	};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.pNext = nullptr;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;

	VkDescriptorPool imgui_pool;
	VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imgui_pool));

	ImGui::CreateContext();
	ImGui_ImplSDL2_InitForVulkan(_window);

	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = _instance;
	init_info.PhysicalDevice = _chosen_gpu;
	init_info.Device = _device;
	init_info.Queue = _graphics_queue;
	init_info.DescriptorPool = imgui_pool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&init_info, render_pass);

	immediate_submit([&](VkCommandBuffer cmd) {
		ImGui_ImplVulkan_CreateFontsTexture(cmd);
	});

	ImGui_ImplVulkan_DestroyFontUploadObjects();

	_main_deletion_queue.push_function([=]() {
		vkDestroyDescriptorPool(_device, imgui_pool, nullptr);
		ImGui_ImplVulkan_Shutdown();
	});
}

VkRenderPass BaseEngine::create_render_pass(std::vector<VkAttachmentDescription> attachments, bool depth_attachment)
{
	VkRenderPass render_pass;
	int num_color = depth_attachment ? attachments.size() - 1 : attachments.size();
	std::vector<VkAttachmentReference> color_references = {};
	VkAttachmentReference depth_ref;

	// Setup color references
	for (int i = 0; i < num_color; i++)
	{
		VkAttachmentReference ref = {};
		ref.attachment = i;
		ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		color_references.push_back(ref);
	}

	// If a depth attachment is given, setup a reference
	if (depth_attachment)
	{
		depth_ref.attachment = attachments.size() - 1;
		depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	}

	// Create render pass with one subpass
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = color_references.size();
	subpass.pColorAttachments = color_references.data();
	subpass.pDepthStencilAttachment = depth_attachment ? &depth_ref : nullptr;

	VkRenderPassCreateInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_info.attachmentCount = attachments.size();
	render_pass_info.pAttachments = attachments.data();
	render_pass_info.subpassCount = 1;
	render_pass_info.pSubpasses = &subpass;

	VK_CHECK(vkCreateRenderPass(_device, &render_pass_info, nullptr, &render_pass));

	_main_deletion_queue.push_function([=]() {
		vkDestroyRenderPass(_device, render_pass, nullptr);
	});

	return render_pass;
}

VkPipeline BaseEngine::create_pipeline(const PipelineInfo &info, VkPipelineLayout layout, VkRenderPass render_pass)
{
	std::vector<VkShaderModule> shaders;

	for (auto shader : info.shaders)
	{
		shaders.push_back(load_shader(shader.filename));
	}

	std::vector<VkPipelineColorBlendAttachmentState> blend_attachments = {};
	for (int i = 0; i < info.framebuffer_count; i++)
	{
		blend_attachments.push_back(info.color_blend_attachment_state);
	}

	VkDynamicState dynamic_states[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dynamic_state_info = {};
	dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state_info.pNext = nullptr;
	dynamic_state_info.dynamicStateCount = 2;
	dynamic_state_info.pDynamicStates = dynamic_states;

	PipelineBuilder pipeline_builder;
	for (size_t i = 0; i < info.shaders.size(); i++)
	{
		pipeline_builder._shader_stages.push_back(infos::pipeline_shader_stage_create_info(info.shaders[i].type, shaders[i]));
	}

	pipeline_builder._vertex_input_info = infos::vertex_input_state_create_info();
	pipeline_builder._input_assembly = infos::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	VertexInputDescription vertex_description = Vertex::get_vertex_description();

	if (!(info.flags & PIPELINE_INFO_NO_VERTICES))
	{
		pipeline_builder._vertex_input_info.pVertexAttributeDescriptions = vertex_description.attributes.data();
		pipeline_builder._vertex_input_info.vertexAttributeDescriptionCount = vertex_description.attributes.size();
		pipeline_builder._vertex_input_info.pVertexBindingDescriptions = vertex_description.bindings.data();
		pipeline_builder._vertex_input_info.vertexBindingDescriptionCount = vertex_description.bindings.size();
	}

	pipeline_builder._viewport.x = 0.0f;
	pipeline_builder._viewport.y = 0.0f;
	pipeline_builder._viewport.width = (float)_window_extent.width;
	pipeline_builder._viewport.height = (float)_window_extent.height;
	pipeline_builder._viewport.minDepth = 0.0f;
	pipeline_builder._viewport.maxDepth = 1.0f;
	pipeline_builder._scissor.offset = {0, 0};
	pipeline_builder._scissor.extent = _window_extent;
	pipeline_builder._rasterizer = infos::rasterization_state_create_info(VK_POLYGON_MODE_FILL);
	pipeline_builder._rasterizer.cullMode = info.cull_mode;
	pipeline_builder._rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	pipeline_builder._multisampling = infos::multisampling_state_create_info();
	pipeline_builder._color_blend_attachments = blend_attachments;
	pipeline_builder._pipeline_layout = layout;
	pipeline_builder._depth_stencil = infos::depth_stencil_create_info(!(info.flags & PIPELINE_INFO_DEPTH_TEST_DISABLE), !(info.flags & PIPELINE_INFO_DEPTH_WRITE_DISABLE), info.depth_compare_op);
	pipeline_builder._dynamic_state = dynamic_state_info;

	auto p = pipeline_builder.build_pipeline(_device, render_pass);

	for (auto shader : shaders)
	{
		vkDestroyShaderModule(_device, shader, nullptr);
	}

	return p;
}

VkFramebuffer BaseEngine::create_framebuffer(VkRenderPass render_pass, std::vector<VkImageView> attachments)
{
	VkFramebuffer framebuffer;
	VkFramebufferCreateInfo fb_info = {};
	fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fb_info.pNext = nullptr;
	fb_info.renderPass = render_pass;
	fb_info.width = _window_extent.width;
	fb_info.height = _window_extent.height;
	fb_info.layers = 1;
	fb_info.attachmentCount = attachments.size();
	fb_info.pAttachments = attachments.data();
	VK_CHECK(vkCreateFramebuffer(_device, &fb_info, nullptr, &framebuffer));

	_swapchain_deletion_queue.push_function([=]() {
		vkDestroyFramebuffer(_device, framebuffer, nullptr);
	});

	return framebuffer;
	
}

std::vector<VkFramebuffer> BaseEngine::create_swapchain_framebuffers(VkRenderPass render_pass)
{
	VkFramebufferCreateInfo fb_info = {};
	fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fb_info.pNext = nullptr;
	fb_info.renderPass = render_pass;
	fb_info.width = _window_extent.width;
	fb_info.height = _window_extent.height;
	fb_info.layers = 1;

	const uint32_t swapchain_count = _swapchain_images.size();
	std::vector<VkFramebuffer> framebuffers = std::vector<VkFramebuffer>(swapchain_count);

	// Create a framebuffer for each swapchain image. All using the same depth buffer.
	for (int i = 0; i < swapchain_count; i++)
	{
		VkImageView attachments[2];
		attachments[0] = _swapchain_image_views[i];
		attachments[1] = _depth_image._image_view;
		fb_info.attachmentCount = 2;
		fb_info.pAttachments = attachments;
		VK_CHECK(vkCreateFramebuffer(_device, &fb_info, nullptr, &framebuffers[i]));

		_swapchain_deletion_queue.push_function([=]() {
			vkDestroyFramebuffer(_device, framebuffers[i], nullptr);
		});
	}

	return framebuffers;
}

VkDescriptorSetLayout BaseEngine::create_descriptor_layout(std::vector<VkDescriptorSetLayoutBinding> bindings)
{
	VkDescriptorSetLayout layout;
	VkDescriptorSetLayoutCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	info.pNext = nullptr;
	info.bindingCount = bindings.size();
	info.pBindings = bindings.data();
	info.flags = 0;
	vkCreateDescriptorSetLayout(_device, &info, nullptr, &layout);

	_main_deletion_queue.push_function([=]() {
		vkDestroyDescriptorSetLayout(_device, layout, nullptr);
	});

	return layout;
}

std::vector<VkDescriptorSet> BaseEngine::allocate_descriptor_sets(VkDescriptorSetLayout layout, int count)
{
	std::vector<VkDescriptorSet> descriptor_sets(count);
	std::vector<VkDescriptorSetLayout> layouts(count);

	for (int i = 0; i < count; i++)
	{
		layouts[i] = layout;
	}

	VkDescriptorSetAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.pNext = nullptr;
	alloc_info.descriptorPool = _descriptor_pool;
	alloc_info.descriptorSetCount = count;
	alloc_info.pSetLayouts = layouts.data();

	vkAllocateDescriptorSets(_device, &alloc_info, descriptor_sets.data());

	return descriptor_sets;
}

VkShaderModule BaseEngine::load_shader(std::string filename)
{
	// Load file
	VkShaderModule shader;
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		std::cout << "Error when building shader: " << filename << "\n";
	}

	// Get file size, and read into buffer
	size_t file_size = (size_t)file.tellg();
	std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));
	file.seekg(0);
	file.read((char*)buffer.data(), file_size);
	file.close();

	// Create module from shader
	VkShaderModuleCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.pNext = nullptr;
	create_info.codeSize = buffer.size() * sizeof(uint32_t);
	create_info.pCode = buffer.data();

	if (vkCreateShaderModule(_device, &create_info, nullptr, &shader) != VK_SUCCESS)
	{
		std::cout << "Error when building shader: " << filename << "\n";
	}
	
	return shader;
}

Buffer BaseEngine::create_buffer(size_t alloc_size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage)
{
	// Allocate buffer with vma
	Buffer buffer;
	VkBufferCreateInfo buffer_info = {};
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.pNext = nullptr;
	buffer_info.size = alloc_size;
	buffer_info.usage = usage;

	VmaAllocationCreateInfo vma_alloc_info = {};
	vma_alloc_info.usage = memory_usage;

	VK_CHECK(vmaCreateBuffer(_allocator, &buffer_info, &vma_alloc_info, &buffer._buffer, &buffer._allocation, nullptr));

	// Initialize info about buffer
	buffer._buffer_info = {};
	buffer._buffer_info.buffer = buffer._buffer;
	buffer._buffer_info.offset = 0;
	buffer._buffer_info.range = alloc_size;

	return buffer;
}

Texture BaseEngine::create_texture(size_t width, size_t height, size_t pixel_size, VkFormat format, VkImageUsageFlags usage, VmaMemoryUsage memory_usage, VkImageAspectFlags aspect, VkFilter filter, uint32_t mip_levels)
{
	// Size is the number of pixels * the size of each pixel (in bytes)
	size_t size = width * height * pixel_size;
	Texture texture;

	// Allocate image with vma
	VkExtent3D image_extent;
	image_extent.width = width;
	image_extent.height = height;
	image_extent.depth = 1;

	VkImageCreateInfo img_info = infos::image_create_info(format, usage, image_extent);
	img_info.mipLevels = mip_levels;
	VmaAllocationCreateInfo img_alloc_info = {};
	img_alloc_info.usage = memory_usage;

	vmaCreateImage(_allocator, &img_info, &img_alloc_info, &texture._image, &texture._allocation, nullptr);

	// Create image view
	VkImageViewCreateInfo image_view_info = infos::image_view_create_info(format, texture._image, aspect);
	image_view_info.subresourceRange.levelCount = mip_levels;
	VK_CHECK(vkCreateImageView(_device, &image_view_info, nullptr, &texture._image_view));

	// Create sampler
	VkSamplerCreateInfo sampler_info = infos::sampler_create_info(filter);
	sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_info.minLod = 0.0f;
	sampler_info.maxLod = (float)mip_levels;
	vkCreateSampler(_device, &sampler_info, nullptr, &texture._image_info.sampler);

	// Initialize info about texture
	texture._format = format;
	texture._image_info.imageView = texture._image_view;
	texture._image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	texture._mip_levels = mip_levels;
	texture.width = width;
	texture.height = height;

	return texture;
}

void BaseEngine::upload_texture(Texture &tex, void *pixel_ptr, VkFormat format)
{
	/*// Load in texture
	AssetPacker::FileData tex_data;
	AssetPacker::load_file(file, tex_data);
	int width = reinterpret_cast<uint32_t*>(tex_data.metadata)[0];
	int height = reinterpret_cast<uint32_t*>(tex_data.metadata)[1];
	int channels = reinterpret_cast<uint32_t*>(tex_data.metadata)[2];

	auto pixels_size = width * height * 4;
	void *pixel_ptr = (void*)(new char[pixels_size]);

	LZ4_decompress_safe(tex_data.data, (char*)pixel_ptr, tex_data.size, pixels_size);

	AssetPacker::close_file(tex_data);*/

	int width = tex.width;
	int height = tex.height;

	// Create staging buffer
	VkDeviceSize image_size = width * height * 4;
	VkFormat image_format = format;
	Buffer staging_buffer = create_buffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	// Copy data to staging buffer
	void *data;
	vmaMapMemory(_allocator, staging_buffer._allocation, &data);
	memcpy(data, pixel_ptr, static_cast<size_t>(image_size));
	vmaUnmapMemory(_allocator, staging_buffer._allocation);

	delete[] (char*)pixel_ptr;

	VkExtent3D image_extent;
	image_extent.width = width;
	image_extent.height = height;
	image_extent.depth = 1;

	// Calculate mip levels
	uint32_t mip_levels = (uint32_t)std::floor(std::log2(std::max(width, height))) + 1;

	// Create texture
	tex = create_texture(width, height, 4, image_format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_GPU_ONLY, VK_IMAGE_ASPECT_COLOR_BIT, VK_FILTER_LINEAR, mip_levels);

	immediate_submit([&](VkCommandBuffer cmd) {
		// Transition layout into DST_OPTIMAL and copy from staging buffer
		// to texture
		VkImageSubresourceRange range;
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseMipLevel = 0;
		range.levelCount = tex._mip_levels;
		range.baseArrayLayer = 0;
		range.layerCount = 1;

		VkImageMemoryBarrier image_barrier_to_transfer = {};
		image_barrier_to_transfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		image_barrier_to_transfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image_barrier_to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		image_barrier_to_transfer.image = tex._image;
		image_barrier_to_transfer.subresourceRange = range;
		image_barrier_to_transfer.srcAccessMask = 0;
		image_barrier_to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &image_barrier_to_transfer);

		VkBufferImageCopy copy_region = {};
		copy_region.bufferOffset = 0;
		copy_region.bufferRowLength = 0;
		copy_region.bufferImageHeight = 0;
		copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copy_region.imageSubresource.mipLevel = 0;
		copy_region.imageSubresource.baseArrayLayer = 0;
		copy_region.imageSubresource.layerCount = 1;
		copy_region.imageExtent = image_extent;

		vkCmdCopyBufferToImage(cmd, staging_buffer._buffer, tex._image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

		// Setup barrier for generating mipmaps
		VkImageMemoryBarrier mip_barrier = {};
		mip_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		mip_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		mip_barrier.image = tex._image;
		mip_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		mip_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		mip_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		mip_barrier.subresourceRange.baseArrayLayer = 0;
		mip_barrier.subresourceRange.layerCount = 1;
		mip_barrier.subresourceRange.levelCount = 1;

		uint32_t mip_width = width;
		uint32_t mip_height = height;

		for (int i = 1; i < tex._mip_levels; i++)
		{
			// Transfer one level into SRC_OPTIMAL
			mip_barrier.subresourceRange.baseMipLevel = i-1;
			mip_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			mip_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			mip_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			mip_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &mip_barrier);

			// Copy from one layer to the next, with linear filtering
			VkImageBlit blit = {};
			blit.srcOffsets[0] = {0, 0, 0};
			blit.srcOffsets[1] = {(int32_t)mip_width, (int32_t)mip_height, 1};
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.dstOffsets[0] = {0, 0, 0};
			blit.dstOffsets[1] = {(int)mip_width > 1 ? (int)mip_width / 2 : 1, (int)mip_height > 1 ? (int)mip_height / 2 : 1, 1};
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;

			vkCmdBlitImage(cmd, tex._image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, tex._image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

			// Transition level to SHADER_READ_ONLY_OPTIMAL
			mip_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			mip_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			mip_barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			mip_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &mip_barrier);

			mip_width = std::max(1, (int)mip_width / 2);
			mip_height = std::max(1, (int)mip_height / 2);
		}

		// Transfer lowest mip level to SHADER_READ_ONLY_OPTIMAL
		mip_barrier.subresourceRange.baseMipLevel = tex._mip_levels - 1;
		mip_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		mip_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		mip_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		mip_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &mip_barrier);
	});

	_main_deletion_queue.push_function([=]() {
		vkDestroySampler(_device, tex._image_info.sampler, nullptr);
		vkDestroyImageView(_device, tex._image_view, nullptr);
		vmaDestroyImage(_allocator, tex._image, tex._allocation);
	});

	vmaDestroyBuffer(_allocator, staging_buffer._buffer, staging_buffer._allocation);

	//return tex;
}

void BaseEngine::upload_mesh(Mesh &mesh)
{
	// Create staging buffers for vertex and index buffers
	const size_t buffer_size = mesh._vertices.size() * sizeof(Vertex);
	const size_t i_buffer_size = mesh._indices.size() * sizeof(uint32_t);
	Buffer staging_buffer = create_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	Buffer i_staging_buffer = create_buffer(i_buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	// Copy data to staging buffers
	void *data;
	vmaMapMemory(_allocator, staging_buffer._allocation, &data);
	memcpy(data, mesh._vertices.data(), buffer_size);
	vmaUnmapMemory(_allocator, staging_buffer._allocation);

	vmaMapMemory(_allocator, i_staging_buffer._allocation, &data);
	memcpy(data, mesh._indices.data(), i_buffer_size);
	vmaUnmapMemory(_allocator, i_staging_buffer._allocation);

	// Create buffers
	mesh._vertex_buffer = create_buffer(buffer_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
	mesh._index_buffer = create_buffer(i_buffer_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

	// Copy staging buffers to vertex and index buffers
	immediate_submit([=](VkCommandBuffer cmd) {
		VkBufferCopy copy;
		copy.dstOffset = 0;
		copy.srcOffset = 0;
		copy.size = buffer_size;
		vkCmdCopyBuffer(cmd, staging_buffer._buffer, mesh._vertex_buffer._buffer, 1, &copy);
		copy.size = i_buffer_size;
		vkCmdCopyBuffer(cmd, i_staging_buffer._buffer, mesh._index_buffer._buffer, 1, &copy);
	});

	_main_deletion_queue.push_function([=]() {
		vmaDestroyBuffer(_allocator, mesh._vertex_buffer._buffer, mesh._vertex_buffer._allocation);
		vmaDestroyBuffer(_allocator, mesh._index_buffer._buffer, mesh._index_buffer._allocation);
	});

	vmaDestroyBuffer(_allocator, staging_buffer._buffer, staging_buffer._allocation);
	vmaDestroyBuffer(_allocator, i_staging_buffer._buffer, i_staging_buffer._allocation);
}

Mesh BaseEngine::load_mesh(std::string filename)
{
	Mesh m;

	//m.load_from_obj(filename.c_str());

	return m;
}

void BaseEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)> &&function)
{
	// Get and begin command buffer
	VkCommandBuffer cmd = _upload_command_buffer;
	VkCommandBufferBeginInfo cmd_begin_info = infos::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));

	// Submit command using cmd
	function(cmd);

	// End and submit command buffer
	VK_CHECK(vkEndCommandBuffer(cmd));
	VkSubmitInfo submit = infos::submit_info(&cmd);
	VK_CHECK(vkQueueSubmit(_graphics_queue, 1, &submit, _upload_fence));

	vkWaitForFences(_device, 1, &_upload_fence, true, 9999999999);
	vkResetFences(_device, 1, &_upload_fence);
	vkResetCommandPool(_device, _upload_command_pool, 0);
}

void BaseEngine::resize_swapchain(uint32_t w, uint32_t h, VkRenderPass render_pass)
{
	// Wait until drawing is done
	vkDeviceWaitIdle(_device);

	// Delete swapchain specific vulkan objects
	_swapchain_deletion_queue.flush();

	// Set new window size
	// TODO: Make it so aspect ratio is preserved
	_window_extent = {w, h};

	// Create new swapchain
	init_swapchain();

	// Create framebuffers for new swapchain
	create_swapchain_framebuffers(render_pass);
}
