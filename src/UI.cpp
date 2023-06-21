#include "stdafx.h"

#include "Engine.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

#define USE_IMGUI 1

#if USE_IMGUI

static void EmbraceTheDarkness();

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

	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
	//io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoTaskBarIcons;
	//io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoMerge;


	float fontScale = 0.75f;

	io.Fonts->AddFontFromFileTTF("assets/fonts/CascadiaCode/static/CascadiaMono-Bold.ttf", 18.0f * fontScale);
	io.FontDefault = io.Fonts->AddFontFromFileTTF("assets/fonts/CascadiaCode/CascadiaMono.ttf", 18.0f * fontScale);
	//bold->Scale = font->Scale = fontScale;
	//io.FontGlobalScale = fontScale;


	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	//Set style
	//EmbraceTheDarkness();

	

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

	ImGui_ImplVulkan_Init(&init_info, _mainRenderpass);

	//execute a gpu command to upload imgui font textures
	immediate_submit([&](VkCommandBuffer cmd) {
		ImGui_ImplVulkan_CreateFontsTexture(cmd);
		});

	//clear font textures from cpu data
	ImGui_ImplVulkan_DestroyFontUploadObjects();

	imgui_RegisterViewportImageViews();


	//add the destroy the imgui created structures
	_deletionStack.push([=]() {
		imgui_UnregisterViewportImageViews();

		vkDestroyDescriptorPool(_device, imguiPool, nullptr);

		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	});
}

void Engine::imgui_RegisterViewportImageViews()
{
	ASSERT(_imguiViewportImageViewDescriptorSets.size() == 0);
	_imguiViewportImageViewDescriptorSets.resize(_viewport.imageViews.size());

	for (size_t i = 0; i < _viewport.imageViews.size(); ++i) {
		_imguiViewportImageViewDescriptorSets[i] =
			ImGui_ImplVulkan_AddTexture(_linearSampler, _viewport.imageViews[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}
}

void Engine::imgui_UnregisterViewportImageViews()
{
	for (auto& dset : _imguiViewportImageViewDescriptorSets) {
		ImGui_ImplVulkan_RemoveTexture(dset);
	}

	_imguiViewportImageViewDescriptorSets.clear();
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


		ImGui::SliderFloat("Exposure", &_renderContext.ssboData.exposure, 0.f, 1.f);

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

	ImGuiIO& io = ImGui::GetIO();
	//io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	static bool opt_fullscreen = true;
	static bool opt_padding = false;
	static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

	ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
	if (opt_fullscreen)
	{
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	} else
	{
		dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
	}

	if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
		window_flags |= ImGuiWindowFlags_NoBackground;

	if (!opt_padding)
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	static bool p_open = true;

	ImGui::Begin("DockSpace", &p_open, window_flags);
	{

		if (!opt_padding)
			ImGui::PopStyleVar();

		if (opt_fullscreen)
			ImGui::PopStyleVar(2);


		ImGuiIO& io = ImGui::GetIO();
		//ImGuiStyle& style = ImGui::GetStyle();
		////Set min panel width
		//float minWinSizeX = style.WindowMinSize.x;

		//const float DOCKSPACE_MIN_PANEL_WIDTH = 340.f;
		//style.WindowMinSize.x = DOCKSPACE_MIN_PANEL_WIDTH;

		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
		}

		//style.WindowMinSize.x = minWinSizeX;

		//Prevent camera movement if viewport is not hovered.
		//Window::SetViewportHovered(m_ViewportHovered);

		//UIDrawViewport();

		/*if (m_DisplayControls)
			Input::UIDisplayControlsConfig(&m_DisplayControls, m_PanelFlags);*/

		ImGui::Begin("Config");
		{

			uiUpdateHDR();
			uiUpdateRenderContext();


			static std::vector<std::string> models;
			static std::vector<const char*> models_cstr;

			if (ImGui::Button("Browse models")) {
				models.clear();
				models_cstr.clear();

				for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(modelPath)) {
					std::string pstr = dirEntry.path().string();
					if (pstr.find_last_of(".") != std::string::npos &&
						pstr.substr(pstr.find_last_of(".")) == ".obj")
					{ // If file is of .obj format
						models.push_back(pstr);
						std::cout << dirEntry << std::endl;
					}
				}

				for (auto& s : models) {
					models_cstr.push_back(s.c_str());
				}
			}

			static int i = 0;
			if (ImGui::Combo("Models", &i, models_cstr.data(), models_cstr.size())) {
				std::cout << "Model " << models_cstr[i] << " selected for loading" << std::endl;
				createScene(models_cstr[i]);
			}

			ImGui::Text("Current MAX: %f", *reinterpret_cast<float*>(&_renderContext.ssboData.oldMax));

			ImGui::Separator();
			ImGui::SliderFloat("Filed of view", &_fovY, 45.f, 90.f);
			ImGui::Separator();

			bool showNormals = _renderContext.ssboData.showNormals;
			if (ImGui::Checkbox("Show normals", &showNormals)) {
				_renderContext.ssboData.showNormals = showNormals;
			}

			static bool show_demo_window = false;
			ImGui::Checkbox("Show demo window", &show_demo_window);
			if (show_demo_window) {
				ImGui::ShowDemoWindow(&show_demo_window);
			}
			ImGui::Separator();
			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

		}
		ImGui::End();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });


		ImGui::Begin("Viewport", (bool*)0, ImGuiWindowFlags_None);
		{

			/*auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
			auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
			auto viewportOffset = ImGui::GetWindowPos();
			m_ViewportBounds[0] = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y };
			m_ViewportBounds[1] = { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y };
			glm::vec2 viewportSize = m_ViewportBounds[1] - m_ViewportBounds[0];
			m_ViewportFocused = ImGui::IsWindowFocused();
			m_ViewportHovered = ImGui::IsWindowHovered();*/


			ImVec2 vSize = ImGui::GetContentRegionAvail();
			uint32_t usX = (uint32_t)vSize.x;
			uint32_t usY = (uint32_t)vSize.y;

			if (_viewport.imageExtent.width != usX || _viewport.imageExtent.height != usY) {
				vkDeviceWaitIdle(_device);

				imgui_UnregisterViewportImageViews();
				vkDeviceWaitIdle(_device);

				// Create viewport images and etc. with new extent
				recreateViewport(usX, usY);

				imgui_RegisterViewportImageViews();
			}


			ImGui::Image(_imguiViewportImageViewDescriptorSets[_frameInFlightNum], ImVec2(usX, usY));
		}
		ImGui::End();
		ImGui::PopStyleVar();
	}
	ImGui::End();
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
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer); //, _mainPipeline

	ImGuiIO& io = ImGui::GetIO();
	if(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		//GLFWwindow* backup_current_context = glfwGetCurrentContext();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		//fwMakeContextCurrent(backup_current_context);
	}
}


static void EmbraceTheDarkness()
{
	//This style is freely available from ImGui github issue
	//https://github.com/ocornut/imgui/issues/707
	ImVec4* colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.19f, 0.19f, 0.19f, 0.92f);
	colors[ImGuiCol_Border] = ImVec4(0.19f, 0.19f, 0.19f, 0.29f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.24f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.54f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
	colors[ImGuiCol_Button] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.00f, 0.00f, 0.36f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.22f, 0.23f, 0.33f);
	colors[ImGuiCol_Separator] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
	colors[ImGuiCol_Tab] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 0.36f);
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_DockingPreview] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
	colors[ImGuiCol_DockingEmptyBg] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_TableHeaderBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
	colors[ImGuiCol_TableBorderStrong] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
	colors[ImGuiCol_TableBorderLight] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
	colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
	colors[ImGuiCol_NavHighlight] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 0.00f, 0.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.35f);

	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowPadding = ImVec2(8.00f, 8.00f);
	style.FramePadding = ImVec2(5.00f, 2.00f);
	style.CellPadding = ImVec2(6.00f, 6.00f);
	style.ItemSpacing = ImVec2(6.00f, 6.00f);
	style.ItemInnerSpacing = ImVec2(6.00f, 6.00f);
	style.TouchExtraPadding = ImVec2(0.00f, 0.00f);
	style.IndentSpacing = 25;
	style.ScrollbarSize = 15;
	style.GrabMinSize = 10;
	style.WindowBorderSize = 1;
	style.ChildBorderSize = 1;
	style.PopupBorderSize = 1;
	style.FrameBorderSize = 1;
	style.TabBorderSize = 1;
	style.WindowRounding = 7;
	style.ChildRounding = 4;
	style.FrameRounding = 3;
	style.PopupRounding = 4;
	style.ScrollbarRounding = 9;
	style.GrabRounding = 3;
	style.LogSliderDeadzone = 4;
	style.TabRounding = 4;
}

#else

void Engine::initImgui() {}

void Engine::imguiCommands() {}

void Engine::imguiOnDrawStart() {}

void Engine::imguiOnRenderPassEnd(VkCommandBuffer cmdBuffer) {}

#endif