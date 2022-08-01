#include "asset_packer.h"

#include <iostream>

#include <lz4.h>

int main()
{
	AssetPacker::FileData data = AssetPacker::pack_mesh("../assets/sphere.obj");
	AssetPacker::save_file("../assets/sphere.m", data);
	AssetPacker::close_file(data);
	data = AssetPacker::pack_mesh("../assets/monkey_smooth.obj");
	AssetPacker::save_file("../assets/monkey_smooth.m", data);
	AssetPacker::close_file(data);
	data = AssetPacker::pack_mesh("../assets/lost_empire.obj");
	AssetPacker::save_file("../assets/lost_empire.m", data);
	AssetPacker::close_file(data);

	data = AssetPacker::pack_texture("../assets/lost_empire-RGBA.png", VK_FORMAT_R8G8B8A8_SRGB);
	AssetPacker::save_file("../assets/lost_empire.t", data);
	AssetPacker::close_file(data);

	data = AssetPacker::pack_texture("../assets/concrete/degraded-concrete_albedo.png", VK_FORMAT_R8G8B8A8_SRGB);
	AssetPacker::save_file("../assets/concrete/degraded-concrete_albedo.t", data);
	AssetPacker::close_file(data);
	data = AssetPacker::pack_texture("../assets/concrete/degraded-concrete_roughness.png", VK_FORMAT_R8G8B8A8_UNORM);
	AssetPacker::save_file("../assets/concrete/degraded-concrete_roughness.t", data);
	AssetPacker::close_file(data);
	data = AssetPacker::pack_texture("../assets/concrete/degraded-concrete_normal-dx.png", VK_FORMAT_R8G8B8A8_SRGB);
	AssetPacker::save_file("../assets/concrete/degraded-concrete_normal-dx.t", data);
	AssetPacker::close_file(data);
	data = AssetPacker::pack_texture("../assets/concrete/degraded-concrete_metallic.png", VK_FORMAT_R8G8B8A8_UNORM);
	AssetPacker::save_file("../assets/concrete/degraded-concrete_metallic.t", data);
	AssetPacker::close_file(data);
	data = AssetPacker::pack_texture("../assets/concrete/degraded-concrete_ao.png", VK_FORMAT_R8G8B8A8_UNORM);
	AssetPacker::save_file("../assets/concrete/degraded-concrete_ao.t", data);
	AssetPacker::close_file(data);

	data = AssetPacker::pack_texture("../assets/alien/alien-carniverous-plant_albedo.png", VK_FORMAT_R8G8B8A8_SRGB);
	AssetPacker::save_file("../assets/alien/alien_albedo.t", data);
	AssetPacker::close_file(data);
	data = AssetPacker::pack_texture("../assets/alien/alien-carniverous-plant_normal-dx.png", VK_FORMAT_R8G8B8A8_UNORM);
	AssetPacker::save_file("../assets/alien/alien_normal.t", data);
	AssetPacker::close_file(data);
	data = AssetPacker::pack_texture("../assets/alien/alien-carniverous-plant_metallic.png", VK_FORMAT_R8G8B8A8_SRGB);
	AssetPacker::save_file("../assets/alien/alien_metal.t", data);
	AssetPacker::close_file(data);
	data = AssetPacker::pack_texture("../assets/alien/alien-carniverous-plant_roughness.png", VK_FORMAT_R8G8B8A8_UNORM);
	AssetPacker::save_file("../assets/alien/alien_roughness.t", data);
	AssetPacker::close_file(data);
	data = AssetPacker::pack_texture("../assets/alien/alien-carniverous-plant_ao.png", VK_FORMAT_R8G8B8A8_UNORM);
	AssetPacker::save_file("../assets/alien/alien_ao.t", data);
	AssetPacker::close_file(data);

	data = AssetPacker::pack_texture("../assets/dent/dented-metal_albedo.png", VK_FORMAT_R8G8B8A8_SRGB);
	AssetPacker::save_file("../assets/dent/dented-metal_albedo.t", data);
	AssetPacker::close_file(data);
	data = AssetPacker::pack_texture("../assets/dent/dented-metal_roughness.png", VK_FORMAT_R8G8B8A8_UNORM);
	AssetPacker::save_file("../assets/dent/dented-metal_roughness.t", data);
	AssetPacker::close_file(data);
	data = AssetPacker::pack_texture("../assets/dent/dented-metal_normal-dx.png", VK_FORMAT_R8G8B8A8_SRGB);
	AssetPacker::save_file("../assets/dent/dented-metal_normal-dx.t", data);
	AssetPacker::close_file(data);
	data = AssetPacker::pack_texture("../assets/dent/dented-metal_metallic.png", VK_FORMAT_R8G8B8A8_UNORM);
	AssetPacker::save_file("../assets/dent/dented-metal_metallic.t", data);
	AssetPacker::close_file(data);
	data = AssetPacker::pack_texture("../assets/dent/dented-metal_ao.png", VK_FORMAT_R8G8B8A8_UNORM);
	AssetPacker::save_file("../assets/dent/dented-metal_ao.t", data);
	AssetPacker::close_file(data);

	data = AssetPacker::pack_texture("../assets/iron/rustediron2_basecolor.png", VK_FORMAT_R8G8B8A8_SRGB);
	AssetPacker::save_file("../assets/iron/rustediron2_basecolor.t", data);
	AssetPacker::close_file(data);
	data = AssetPacker::pack_texture("../assets/iron/rustediron2_roughness.png", VK_FORMAT_R8G8B8A8_UNORM);
	AssetPacker::save_file("../assets/iron/rustediron2_roughness.t", data);
	AssetPacker::close_file(data);
	data = AssetPacker::pack_texture("../assets/iron/rustediron2_normal.png", VK_FORMAT_R8G8B8A8_SRGB);
	AssetPacker::save_file("../assets/iron/rustediron2_normal.t", data);
	AssetPacker::close_file(data);
	data = AssetPacker::pack_texture("../assets/iron/rustediron2_metallic.png", VK_FORMAT_R8G8B8A8_UNORM);
	AssetPacker::save_file("../assets/iron/rustediron2_metallic.t", data);
	AssetPacker::close_file(data);
	data = AssetPacker::pack_texture("../assets/iron/rustediron2_ao.png", VK_FORMAT_R8G8B8A8_SRGB);
	AssetPacker::save_file("../assets/iron/rustediron2_ao.t", data);
	AssetPacker::close_file(data);

	data = AssetPacker::pack_texture("../assets/stone/slimy-slippery-rock1_albedo.png", VK_FORMAT_R8G8B8A8_SRGB);
	AssetPacker::save_file("../assets/stone/slimy-slippery-rock1_albedo.t", data);
	AssetPacker::close_file(data);
	data = AssetPacker::pack_texture("../assets/stone/slimy-slippery-rock1_roughness.png", VK_FORMAT_R8G8B8A8_UNORM);
	AssetPacker::save_file("../assets/stone/slimy-slippery-rock1_roughness.t", data);
	AssetPacker::close_file(data);
	data = AssetPacker::pack_texture("../assets/stone/slimy-slippery-rock1_normal-dx.png", VK_FORMAT_R8G8B8A8_SRGB);
	AssetPacker::save_file("../assets/stone/slimy-slippery-rock1_normal-dx.t", data);
	AssetPacker::close_file(data);
	data = AssetPacker::pack_texture("../assets/stone/slimy-slippery-rock1_metallic.psd", VK_FORMAT_R8G8B8A8_UNORM);
	AssetPacker::save_file("../assets/stone/slimy-slippery-rock1_metallic.t", data);
	AssetPacker::close_file(data);
	data = AssetPacker::pack_texture("../assets/stone/slimy-slippery-rock1_ao.png", VK_FORMAT_R8G8B8A8_UNORM);
	AssetPacker::save_file("../assets/stone/slimy-slippery-rock1_ao.t", data);
	AssetPacker::close_file(data);

	data = AssetPacker::pack_texture("../assets/cheese/cheese_albedo.png", VK_FORMAT_R8G8B8A8_SRGB);
	AssetPacker::save_file("../assets/cheese/cheese_albedo.t", data);
	AssetPacker::close_file(data);
	data = AssetPacker::pack_texture("../assets/cheese/cheese_roughness.png", VK_FORMAT_R8G8B8A8_UNORM);
	AssetPacker::save_file("../assets/cheese/cheese_roughness.t", data);
	AssetPacker::close_file(data);
	data = AssetPacker::pack_texture("../assets/cheese/cheese_normal.png", VK_FORMAT_R8G8B8A8_SRGB);
	AssetPacker::save_file("../assets/cheese/cheese_normal.t", data);
	AssetPacker::close_file(data);
	data = AssetPacker::pack_texture("../assets/cheese/cheese_metallic.png", VK_FORMAT_R8G8B8A8_UNORM);
	AssetPacker::save_file("../assets/cheese/cheese_metallic.t", data);
	AssetPacker::close_file(data);
	data = AssetPacker::pack_texture("../assets/cheese/cheese_ao.png", VK_FORMAT_R8G8B8A8_UNORM);
	AssetPacker::save_file("../assets/cheese/cheese_ao.t", data);
	AssetPacker::close_file(data);
}
