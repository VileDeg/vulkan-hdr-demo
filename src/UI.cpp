#include "stdafx.h"
#include "engine.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

#include "imgui/implot.h"

// utility structure for realtime plot
// From implot.cpp
struct RollingBuffer {
	float Span;
	ImVector<ImVec2> Data;
	RollingBuffer() {
		Span = 10.0f;
		Data.reserve(2000);
	}
	void AddPoint(float x, float y) {
		float xmod = fmodf(x, Span);
		if (!Data.empty() && xmod < Data.back().x)
			Data.shrink(0);
		Data.push_back(ImVec2(xmod, y));
	}
};

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
	ImPlot::CreateContext();

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
	


	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}


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

	ImGui_ImplVulkan_Init(&init_info, _swapchain.renderpass);

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

		ImPlot::DestroyContext();
		ImGui::DestroyContext();
	});
}

void Engine::imgui_RegisterViewportImageViews()
{
	ASSERT(_imguiViewportImageViewDescriptorSets.size() == 0);
	_imguiViewportImageViewDescriptorSets.resize(_viewport.imageViews.size());

	for (size_t i = 0; i < _viewport.imageViews.size(); ++i) {
		_imguiViewportImageViewDescriptorSets[i] =
			ImGui_ImplVulkan_AddTexture(_linearSampler, _viewport.imageViews[i], VK_IMAGE_LAYOUT_GENERAL);
	}
}

void Engine::imgui_UnregisterViewportImageViews()
{
	for (auto& dset : _imguiViewportImageViewDescriptorSets) {
		ImGui_ImplVulkan_RemoveTexture(dset);
	}

	_imguiViewportImageViewDescriptorSets.clear();
}

void Engine::uiUpdateScene()
{
	ImGui::SetNextItemOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNode("Scene configs")) {

		float amb = _renderContext.sceneData.ambientColor.x;
		if (ImGui::SliderFloat("Ambient factor", &amb, 0.f, 1.f)) {
			_renderContext.sceneData.ambientColor = { amb, amb, amb };
		}
		

		ImGui::SeparatorText("Shadow");
		ImGui::Checkbox("Display shadow map", &_renderContext.sceneData.showShadowMap);
		ImGui::SliderFloat("Shadow Bias", &_renderContext.sceneData.shadowBias, 0.f, 1.0f);
		ImGui::SliderFloat("Shadow Opacity", &_renderContext.sceneData.shadowOpacity, 0.f, 1.0f);
		ImGui::SliderFloat("Shadow Display Brightness", &_renderContext.sceneData.shadowMapDisplayBrightness, 1.f, 10.f);
		ImGui::Checkbox("Enable PCF", &_renderContext.sceneData.enablePCF);

		ImGui::Separator();
		ImGui::Checkbox("Enable skybox", &_renderContext.enableSkybox);

		ImGui::TreePop();
	}
}

void Engine::uiUpdateHDR()
{
	ImGui::SetNextItemOpen(true, ImGuiCond_Once);

	if (ImGui::TreeNode("HDR")) {
		ImGui::SeparatorText("Tone mapping"); {

			const char* items[] = {
				"Reinhard Extended", "Reinhard", "Uncharted2", "ACES Narkowicz", "ACES Hill" };
			static int item_current = _state.cmp.toneMappingMode;
			if (ImGui::Combo("ToneMapping", &item_current, items, IM_ARRAYSIZE(items))) {
				_state.cmp.toneMappingMode = item_current;
			}
		}
		ImGui::Checkbox("Enable tone mapping", &_state.cmp.enableToneMapping);

		ImGui::SeparatorText("Adjust scene EV"); {
			if (ImGui::Button("-")) {
				_state.exposure -= 1;
			}
			ImGui::SameLine();
			ImGui::Text("%f", _state.exposure);
			ImGui::SameLine();
			if (ImGui::Button("+")) {
				_state.exposure += 1;
			}
		}

		ImGui::Checkbox("Enable eye adaptation", &_state.cmp.enableAdaptation);

		{
			const char* items[] = {
				"No gamma correction", "Gamma correction", "Inverse gamma correction" };
			static int item_current = _state.cmp.gammaMode;
			if (ImGui::Combo("Gamma", &item_current, items, IM_ARRAYSIZE(items))) {
				_state.cmp.gammaMode = item_current;
			}
		}

		ImGui::SeparatorText("Average luminance computation"); {
			ImGui::SliderFloat("Min log luminance", &_state.cmp.minLogLum, -10.f, 0.f);

			ImGui::SliderFloat("Max log luminance", &_state.maxLogLuminance, 1.f, 20.f);

			ImGui::SliderFloat("Weight X", &_state.cmp.weights.x, 0.f, 2.f);
			ImGui::SliderFloat("Weight Y", &_state.cmp.weights.y, 0.f, 255.f);
			ImGui::SliderFloat("Weight Z", &_state.cmp.weights.z, 0.f, 100.f);
			ImGui::SliderFloat("Weight W", &_state.cmp.weights.w, 0.f, 5.f);
		}

		ImGui::Separator(); {

			ImGui::Text("Current average luminance: %f", _gpu.compSSBO->averageLuminance);
			ImGui::Text("Target average luminance: %f", _gpu.compSSBO->targetAverageLuminance);

		} ImGui::SeparatorText("Histogram bounds"); {

			ImGui::SliderFloat("Lower", &_state.lumPixelLowerBound, 0.f, 0.45f);
			ImGui::SliderFloat("Upper", &_state.lumPixelUpperBound, 0.55f, 1.f);
			
			ImGui::Text("Histogram bounds: %f %f", _state.lumPixelLowerBound, _state.lumPixelUpperBound);
			ImGui::Separator();
			ImGui::Text("Total pixels: %u", _state.cmp.totalPixelNum);
			ImGui::Text("Histogram bounds indices: %u %u", _state.cmp.lumLowerIndex, _state.cmp.lumUpperIndex);

		} ImGui::Separator();

	ImGui::SeparatorText("Plots"); {

		//ImGui::SetNextItemOpen(true, ImGuiCond_Once);
		if (ImGui::TreeNode("Exposure Window")) {
				
			ImGui::Separator();

			static RollingBuffer rdata, rdata1;

			ImGui::SliderFloat("Adaptation time coefficient", &_state.eyeAdaptationTimeCoefficient, 1.f, 10.f);

			static float t = 0;
			t += ImGui::GetIO().DeltaTime;

			static float history = 30.0f;
			ImGui::SliderFloat("History", &history, 1, 30, "%.1f s");

			rdata.AddPoint(t, _gpu.compSSBO->averageLuminance);
			rdata.Span = history;

			rdata1.AddPoint(t, _gpu.compSSBO->targetAverageLuminance);
			rdata1.Span = history;

			static ImPlotAxisFlags flags = ImPlotAxisFlags_NoTickLabels;

			if (ImPlot::BeginPlot("##Rolling", ImVec2(-1, 200))) { //, ImPlotFlags_CanvasOnly)
				static float maxY = 0;
				maxY = _gpu.compSSBO->targetAverageLuminance > maxY ? _gpu.compSSBO->targetAverageLuminance : maxY;

				ImPlot::SetupLegend(ImPlotLocation_North, ImPlotLegendFlags_Outside);

				ImPlot::SetupAxisLimits(ImAxis_X1, 0, history, ImGuiCond_Always);
				ImPlot::SetupAxisLimits(ImAxis_Y1, 0 - 0.1f * maxY, maxY + maxY * 0.1f, ImGuiCond_Always);

				std::string label = "Adaptation (actual)";
				ImPlot::PlotLine(label.c_str(), &rdata.Data[0].x, &rdata.Data[0].y, rdata.Data.size(), 0, 0, 2 * sizeof(float));

				label = "Adaptation (target)";
				ImPlot::PlotLine(label.c_str(), &rdata1.Data[0].x, &rdata1.Data[0].y, rdata1.Data.size(), 0, 0, 2 * sizeof(float));

				ImPlot::EndPlot();
			}

			ImGui::TreePop();
		}

		//ImGui::SetNextItemOpen(true, ImGuiCond_Once);
		if (ImGui::TreeNode("Histogram")) {

				
			if (ImPlot::BeginPlot("Luminance", ImVec2(-1, 200))) {
				constexpr int arr_size = ARRAY_SIZE(_gpu.compSSBO->luminance);

				int bins = arr_size;
				int xs[arr_size];

				for (int i = 0; i < arr_size; ++i) {
					xs[i] = i;
				}

				// Skip values that don't lie withing bounds
				/*int start_i = (arr_size-1) * _renderContext.luminanceHistogramBounds.x;
				int end_i = (arr_size-1) * _renderContext.luminanceHistogramBounds.y;*/

				std::vector<int> vals(_gpu.compSSBO->luminance, _gpu.compSSBO->luminance + arr_size);

				auto it = std::max_element(vals.begin(), vals.end());
				int maxBin = *it;

				ImPlot::SetupLegend(ImPlotLocation_North, ImPlotLegendFlags_Outside);
				ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);

				ImPlot::SetupAxisLimits(ImAxis_X1, 0, bins);
				ImPlot::SetupAxisLimits(ImAxis_Y1, 0, maxBin, ImPlotCond_Always); //dim.x * dim.y

				ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle);


				ImPlot::PlotStems("Luminance", xs, vals.data(), bins);

				int start_i = _state.cmp.lumLowerIndex;
				int end_i   = _state.cmp.lumUpperIndex;

				int xs1[2] = { start_i, end_i };
				int vals1[2] = { vals[start_i], vals[end_i] };
				ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle);

				ImPlot::PlotStems("Bounds", xs1, vals1, 2);


				ImPlot::EndPlot();
			}
			ImGui::TreePop();
		}
	}
	ImGui::TreePop();
	}
}

void Engine::uiUpdateRenderContext()
{
	ImGui::SetNextItemOpen(true, ImGuiCond_Once);
	if (ImGui::TreeNode("Scene lighting")) {
		for (int i = 0; i < MAX_LIGHTS; ++i) {

			if (i == 0) {
				ImGui::SetNextItemOpen(true, ImGuiCond_Once);
			}

			if (ImGui::TreeNode((void*)(intptr_t)i, "Light source %d", i)) {
				GPULight& l = _renderContext.sceneData.lights[i];

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

void Engine::imguiUpdate()
{
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
			for (int i = 0; i < _imguiFlags.size(); ++i) {
				auto s = std::string("Flag ") + std::to_string(i);
				ImGui::Checkbox(s.c_str(), _imguiFlags[i]);
			}

			uiUpdateScene();
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

			

			ImGui::Separator();
			ImGui::SliderFloat("Filed of view", &_fovY, 45.f, 90.f);
			ImGui::Separator();

			bool showNormals = _gpu.ssbo->showNormals;
			if (ImGui::Checkbox("Show normals", &showNormals)) {
				_gpu.ssbo->showNormals = showNormals;
			}

			


			static bool imgui_demo = false;
			ImGui::Checkbox("Show ImGui demo window", &imgui_demo);
			if (imgui_demo) {
				ImGui::ShowDemoWindow(&imgui_demo);
			}

			static bool implot_demo = false;
			ImGui::Checkbox("Show ImPlot demo window", &implot_demo);
			if (implot_demo) {
				ImPlot::ShowDemoWindow(&implot_demo);
			}

			ImGui::Separator();
			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

			_frameRate = io.Framerate;
			_deltaTime = io.DeltaTime;
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
	ImGui::Render();
}

void Engine::imguiOnRenderPassEnd(VkCommandBuffer cmdBuffer)
{
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
