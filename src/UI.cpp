#include "stdafx.h"

#include "Engine.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

#define USE_IMGUI 1

#if USE_IMGUI

void Engine::initImgui()
{
	// 1: create descriptor pool for IMGUI
	// the size of the pool is very oversize, but it's copied from imgui demo itself.
	VkDescriptorPoolSize pool_sizes[] = {
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo pool_info{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		.maxSets = 1000,
		.poolSizeCount = std::size(pool_sizes),
		.pPoolSizes = pool_sizes
	};

	VkDescriptorPool imguiPool;
	VKASSERT(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool));

	// 2: initialize imgui library

	//this initializes the core structures of imgui
	ImGui::CreateContext();

	//this initializes imgui for SDL
	ImGui_ImplGlfw_InitForVulkan(_window, true);

	//this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo init_info{
		.Instance = _instance,
		.PhysicalDevice = _physicalDevice,
		.Device = _device,
		.Queue = _graphicsQueue,
		.DescriptorPool = imguiPool,
		.MinImageCount = 3,
		.ImageCount = 3,
		.MSAASamples = VK_SAMPLE_COUNT_1_BIT
	};

	ImGui_ImplVulkan_Init(&init_info, _renderPass);

	//execute a gpu command to upload imgui font textures
	immediate_submit([&](VkCommandBuffer cmd) {
		ImGui_ImplVulkan_CreateFontsTexture(cmd);
		});

	//clear font textures from cpu data
	ImGui_ImplVulkan_DestroyFontUploadObjects();

	//add the destroy the imgui created structures
	_deletionStack.push([=]() {
		vkDestroyDescriptorPool(_device, imguiPool, nullptr);

		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	});
}

void Engine::uiUpdateHDR()
{
	ImGui::SetNextItemOpen(true, ImGuiCond_Once);

	if (ImGui::TreeNode("HDR")) {
		ImGui::SeparatorText("Tone mapping");
		
		{
			const char* items[] = { 
				"Reinhard Extended", "Reinhard", "Uncharted2", "ACES Narkowicz", "ACES Hill" };
			static int item_current = 0;
			if (ImGui::Combo("Operator", &item_current, items, IM_ARRAYSIZE(items))) {
				_renderContext.ssboData.toneMappingMode = item_current;
			}
		}

		ImGui::Checkbox("Enable tone mapping", &_inp.toneMappingEnabled);
		ImGui::SeparatorText("Exposure");

		{
			const char* items[] = { "Log", "Linear" };
			static int curr = 0;
			if (ImGui::Combo("Mode", &curr, items, IM_ARRAYSIZE(items))) {
				_renderContext.ssboData.exposureMode = curr;
			}
		}


		ImGui::SliderFloat("Exposure", &_renderContext.ssboData.exposure, 1.f, 10.f);

		ImGui::Checkbox("Enable exposure", &_inp.exposureEnabled);


		ImGui::TreePop();
	}
}

void Engine::uiUpdateRenderContext()
{
	if (ImGui::TreeNode("Scene lighting")) {
		for (int i = 0; i < MAX_LIGHTS; ++i) {

			// Use SetNextItemOpen() so set the default state of a node to be open. We could
			// also use TreeNodeEx() with the ImGuiTreeNodeFlags_DefaultOpen flag to achieve the same thing!
			if (i == 0) {
				ImGui::SetNextItemOpen(true, ImGuiCond_Once);
			}

			if (ImGui::TreeNode((void*)(intptr_t)i, "Light source %d", i)) {
				Light& l = _renderContext.sceneData.lights[i];

				glm::vec3 tmpPos = l.position;
				if (ImGui::DragFloat3("Position", glm::value_ptr(tmpPos), 0.1f, -100.f, 100.f)) {
					_renderContext.UpdateLightPosition(i, tmpPos);
				}

				if (ImGui::TreeNode((void*)(intptr_t)(i + 1), "Color")) {
					ImGui::PushItemWidth(100.f);
					ImGui::ColorPicker3("Color", glm::value_ptr(l.color));
					ImGui::PopItemWidth();
					ImGui::TreePop();
				}

				ImGui::Text("Radius %f", l.radius);
				ImGui::SameLine();
				if (ImGui::Button("+")) {
					_renderContext.UpdateLightAttenuation(i, 1);
				}
				ImGui::SameLine();
				if (ImGui::Button("-")) {
					_renderContext.UpdateLightAttenuation(i, 2);
				}

				if (ImGui::Button("Reset Intensity")) {
					l.intensity = 1.f;
				}

				ImGui::DragFloat("Intensity", &l.intensity, 1.f, 1.f, 10000.f);
				bool lon = l.enabled;
				if (ImGui::Checkbox("Enabled", &lon)) {
					l.enabled = lon;
				}


				ImGui::TreePop();
			}
		}

		ImGui::TreePop();
	}
}

void Engine::imguiCommands()
{
	if (!_inp.uiEnabled) {
		return;
	}
	// Start the Dear ImGui frame
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGuiIO& io = ImGui::GetIO(); (void)io;
	
	{
		ImGui::Begin("Config");

		uiUpdateHDR();
		uiUpdateRenderContext();
		
		/*static std::vector<const char*> models_cstr;
		if (ImGui::Button("Browse models")) {
			for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(modelPath)) {
				models_cstr.push_back(dirEntry.path().string().c_str());
				std::cout << dirEntry << std::endl;
			}
		}

		static int i = 0;
		if (ImGui::Combo("Models", &i, models_cstr.data(), models_cstr.size())) {
			std::cout << "Model " << models_cstr[i] << " selected for loading" << std::endl;
		}*/

		ImGui::Text("Current MAX: %f", *reinterpret_cast<float*>(&_renderContext.ssboData.oldMax));

		ImGui::Separator();
		ImGui::SliderFloat("Filed of view", &_fovY, 45.f, 90.f);
		ImGui::Separator();

		static bool show_demo_window = false;
		ImGui::Checkbox("Show demo window", &show_demo_window);
		if (show_demo_window) {
			ImGui::ShowDemoWindow(&show_demo_window);
		}
		ImGui::Separator();
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
		ImGui::End();
	}
}

void Engine::imguiOnDrawStart()
{
	if (!_inp.uiEnabled) {
		return;
	}
	ImGui::Render();
}

void Engine::imguiOnRenderPassEnd(VkCommandBuffer cmdBuffer)
{
	if (!_inp.uiEnabled) {
		return;
	}
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);
}

#else

void Engine::initImgui() {}

void Engine::imguiCommands() {}

void Engine::imguiOnDrawStart() {}

void Engine::imguiOnRenderPassEnd(VkCommandBuffer cmdBuffer) {}

#endif