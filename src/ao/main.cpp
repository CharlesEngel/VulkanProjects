#include "imgui/imgui.h"
#include "ao_engine.h"

#include <imgui/imgui.h>
#include <imgui/imgui_impl_vulkan.h>
#include <imgui/imgui_impl_sdl.h>

#include <glm/gtx/transform.hpp>

int main()
{
	AOEngine engine;
	engine.init();

	bool quit = false;

	while (!quit)
	{
		engine.run(quit);
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame(engine._window);
		ImGui::NewFrame();
		engine.draw();
	}


	engine.cleanup();

	return 0;
}

