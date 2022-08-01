#include "material_system.h"

#include "base_engine.h"
#include "infos.h"

#include <fstream>
#include <iostream>

void MaterialSystem::init(std::string filename, BaseEngine *engine, std::vector<VkRenderPass> render_passes)
{
	std::ifstream system_file;
	system_file.open(filename);

	if (!system_file.is_open())
	{
		std::cout << "Failed to open material system file!\n";
		return;
	}

	std::string pipelines_file_name;
	std::string asset_file_name;
	std::string materials_file_name;

	if (!std::getline(system_file, pipelines_file_name))
	{
		std::cout << "Error: material system file not formatted correctly!\n" << "Could not get pipelines file name from material system file!\n";
		return;
	}

	if (!std::getline(system_file, asset_file_name))
	{
		std::cout << "Error: material system file not formatted correctly!\n" << "Could not get asset file name from material system file!\n";
		return;
	}

	if (!std::getline(system_file, materials_file_name))
	{
		std::cout << "Error: material system file not formatted correctly!\n" << "Could not get materials file name from material system file!\n";
		return;
	}

	system_file.close();

	if (!read_pipelines(pipelines_file_name, engine, render_passes))
	{
		std::cout << "Failed to read pipelines file!\n";
	}

	if (!read_materials(materials_file_name, engine))
	{
		std::cout << "Failed to read materials file!\n";
	}

	engine->_asset_system.init(asset_file_name, engine);
}

void MaterialSystem::destroy(BaseEngine *engine)
{
	for (auto pipeline : _pipelines)
	{
		vkDestroyPipeline(engine->_device, pipeline.second.pipeline, nullptr);
		vkDestroyPipelineLayout(engine->_device, pipeline.second.layout, nullptr);
		//vkDestroyDescriptorSetLayout(engine->_device, pipeline.second.descriptor_layout, nullptr);
	}
}

const Material &MaterialSystem::get_material(size_t mat_id)
{
	if (mat_id >= _materials.size())
	{
		std::cout << "Tried to access material out of bounds!\n";
		return _materials[0];
	}

	return _materials[mat_id];
};

std::vector<DescriptorInfo> MaterialSystem::get_descriptor_infos(size_t mat_id)
{
	if (mat_id >= _materials.size())
	{
		std::cout << "Tried to access descriptor info for material out of bounds!\n";
		return _materials[0].descriptors;
	}

	return _materials[mat_id].descriptors;
}

size_t MaterialSystem::get_material_id(std::string name)
{
	if (_material_ids.count(name) == 0)
	{
		return (size_t)-1;
	}

	return _material_ids[name];
}

bool MaterialSystem::read_pipelines(std::string filename, BaseEngine *engine, std::vector<VkRenderPass> render_passes)
{
	std::ifstream file;
	file.open(filename);

	if (!file.is_open())
	{
		return false;
	}

	std::string line;
	while (std::getline(file, line))
	{
		if (line != "PIPELINE")
		{
			continue;
		}
		PipelineInfo info = {};
		info.color_blend_attachment_state = infos::color_blend_attachment_state();
		std::string name;
		std::getline(file, name);

		while (std::getline(file, line) && line != "!PIPELINE")
		{
			if (line == "SHADER")
			{
				ShaderInfo s_info = {};
				while (std::getline(file, line) && line != "!SHADER")
				{
					if (line == "VERTEX")
					{
						s_info.type = VK_SHADER_STAGE_VERTEX_BIT;
					}
					else if (line == "FRAGMENT")
					{
						s_info.type = VK_SHADER_STAGE_FRAGMENT_BIT;
					}
					else if (line.size() >= 5 && line.substr(0, 5) == "FILE:")
					{
						s_info.filename = line.substr(5, line.size() - 5);
					}
					else if (line.size() >= 3 && line.substr(0, 3) == "UB:")
					{
						s_info.uniform_buffer_count = std::stoi(line.substr(3, line.size()-3));
					}
					else if (line.size() >= 3 && line.substr(0, 3) == "SB:")
					{
						s_info.storage_buffer_count = std::stoi(line.substr(3, line.size()-3));
					}
					else if (line.size() >= 4 && line.substr(0, 4) == "TEX:")
					{
						s_info.texture_count = std::stoi(line.substr(4, line.size()-4));
					}
				}
				info.shaders.push_back(s_info);
			}
			else if (line == "NO_DEPTH_TEST")
			{
				info.flags |= PIPELINE_INFO_DEPTH_TEST_DISABLE;
			}
			else if (line == "NO_DEPTH_WRITE")
			{
				info.flags |= PIPELINE_INFO_DEPTH_WRITE_DISABLE;
			}
			else if (line == "DEPTH_GREATER")
			{
				info.depth_compare_op = VK_COMPARE_OP_GREATER_OR_EQUAL;
			}
			else if (line == "DEPTH_EQUAL")
			{
				info.depth_compare_op = VK_COMPARE_OP_EQUAL;
			}
			else if (line == "NO_CULL")
			{
				info.cull_mode = VK_CULL_MODE_NONE;
			}
			else if (line == "CULL_FRONT")
			{
				info.cull_mode = VK_CULL_MODE_FRONT_BIT;
			}
			else if (line == "NO_VERTS")
			{
				info.flags |= PIPELINE_INFO_NO_VERTICES;
			}
			else if (line == "BLEND_ADD")
			{
				info.color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
				info.color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
				info.color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
				info.color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
				info.color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
				info.color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;
				info.color_blend_attachment_state.blendEnable = VK_TRUE;
			}
			else if (line == "BLEND_LERP")
			{
				info.color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
				info.color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				info.color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
				info.color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
				info.color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
				info.color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;
				info.color_blend_attachment_state.blendEnable = VK_TRUE;
			}
			else if (line.size() > 3 && line.substr(0, 3) == "RP:")
			{
				info.render_pass_index = std::stoi(line.substr(3, line.size()-3));
			}
			else if (line.size() > 3 && line.substr(0, 3) == "FB:")
			{
				info.framebuffer_count = std::stoi(line.substr(3, line.size()-3));
			}
		}

		std::vector<VkDescriptorSetLayoutBinding> bindings = {};
		uint32_t i = 0;
		for (auto shader : info.shaders)
		{
			for (uint32_t j = 0; j < shader.uniform_buffer_count; j++)
			{
				auto binding = infos::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, shader.type, i);
				bindings.push_back(binding);
				i++;
			}

			for (uint32_t j = 0; j < shader.storage_buffer_count; j++)
			{
				auto binding = infos::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, shader.type, i);
				bindings.push_back(binding);
				i++;
			}

			for (uint32_t j = 0; j < shader.texture_count; j++)
			{
				auto binding = infos::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, shader.type, i);
				bindings.push_back(binding);
				i++;
			}
		}

		auto descriptor_layout = engine->create_descriptor_layout(bindings);

		auto pipeline_layout_info = infos::pipeline_layout_create_info();
		pipeline_layout_info.setLayoutCount = 1;
		pipeline_layout_info.pSetLayouts = &descriptor_layout;
		VkPipelineLayout layout;
		vkCreatePipelineLayout(engine->_device, &pipeline_layout_info, nullptr, &layout);

		auto v_pipeline = engine->create_pipeline(info, layout, render_passes[info.render_pass_index]);
		Pipeline pipeline;
		pipeline.descriptor_layout = descriptor_layout;
		pipeline.layout = layout;
		pipeline.pipeline = v_pipeline;
		pipeline.render_pass_id = info.render_pass_index;
		_pipelines[name] = pipeline;
	}

	file.close();
	return true;
}

bool MaterialSystem::read_materials(std::string filename, BaseEngine *engine)
{
	std::ifstream file;
	file.open(filename);

	if (!file.is_open())
	{
		return false;
	}

	std::string line;
	while (std::getline(file, line))
	{
		if (line != "MAT")
		{
			continue;
		}

		Material mat;
		std::string name;
		std::getline(file, name);

		while (std::getline(file, line) && line != "!MAT")
		{
			if (line != "PIPE_MAT")
			{
				continue;
			}

			std::string p_name;
			std::getline(file, p_name);
			mat.draws.push_back(&_pipelines[p_name]);
			DescriptorInfo d_info;
			d_info.layout = _pipelines[p_name].descriptor_layout;

			while (std::getline(file, line) && line != "!PIPE_MAT")
			{
				if (line == "")
				{
					continue;
				}

				d_info.descriptor_names.push_back(line);
			}

			mat.descriptors.push_back(d_info);
		}

		_material_ids[name] = _materials.size();
		_materials.push_back(mat);
	}

	file.close();
	return true;
}
