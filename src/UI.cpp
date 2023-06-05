#include "stdafx.h"

#include "Engine.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

#define USE_IMGUI 1

#if USE_IMGUI 1

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
		const char* items[] = { "Reinhard", "ACES Narkowicz", "ACES Hill" };
		static int item_current = 0;
		if (ImGui::Combo("TM", &item_current, items, IM_ARRAYSIZE(items))) {
			_toneMappingOp = item_current;
			pr("ToneMappingOp: " << _toneMappingOp);
		}

		ImGui::Checkbox("Enable tone mapping", &_inp.toneMappingEnabled);
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

				ImGui::DragFloat3("Position", glm::value_ptr(l.position), 1.f, -100.f, 100.f);

				if (ImGui::TreeNode((void*)(intptr_t)(i + 1), "Color")) {
					ImGui::ColorPicker3("Color", glm::value_ptr(l.color));
					ImGui::TreePop();
				}

				if (ImGui::DragFloat("Radius", &l.radius, 1.f, 1.f, 50.f)) {
					_renderContext.UpdateLightAttenuation(i);
				}

				ImGui::DragFloat("Intensity", &l.intensity, 1.f, 1.f, 50.f);
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
	// Start the Dear ImGui frame
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	
	{
		ImGui::Begin("Config");

		uiUpdateHDR();

		uiUpdateRenderContext();
		
		ImGui::DragFloat("Filed of view", &_fovY, 1.f, 45.f, 90.f);
		
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
	ImGui::Render();
}

void Engine::imguiOnRenderPassEnd(VkCommandBuffer cmdBuffer)
{
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);
}

#else

void Engine::initImgui() {}

void Engine::imguiCommands() {}

void Engine::imguiOnDrawStart() {}

void Engine::imguiOnRenderPassEnd(VkCommandBuffer cmdBuffer) {}

#endif