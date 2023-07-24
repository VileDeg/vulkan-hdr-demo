#include "stdafx.h"
#include "engine.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

#include "imgui/implot.h"

/* utility structure for realtime plot
 * From implot.cpp */ 
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
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows


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
		.MSAASamples = VK_SAMPLE_COUNT_1_BIT,
		// Enable dynamic rendering extension usage
		.UseDynamicRendering = true,
		.ColorAttachmentFormat = _swapchain.colorFormat

	};

	ImGui_ImplVulkan_Init(&init_info, VK_NULL_HANDLE);

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

void Engine::uiUpdateScene()
{
	if (ImGui::TreeNodeEx("Scene configs", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SeparatorText("Shadow"); {
			ImGui::Checkbox("Enable Shadows", &_renderContext.sceneData.enableShadows);
			ImGui::Checkbox("Enable PCF", &_renderContext.sceneData.enablePCF);

			ImGui::SliderFloat("Shadow Bias", &_renderContext.sceneData.shadowBias, 0.f, 1.0f);
		}
		ImGui::Separator();
		ImGui::Checkbox("Enable bump mapping", &_renderContext.sceneData.enableBumpMapping);
		ImGui::SliderFloat("Bump Strength", &_renderContext.sceneData.bumpStrength, 0.f, 5.f);
		ImGui::SliderFloat("Bump Step", &_renderContext.sceneData.bumpStep, 0.0001f, 0.005f, "%.4f");
		ImGui::SliderFloat("Bump UV Factor", &_renderContext.sceneData.bumpUVFactor, 0.0001f, 0.005f, "%.4f");

		ImGui::Separator();
		ImGui::Checkbox("Enable skybox", &_renderContext.enableSkybox);
		if (ImGui::Checkbox("Display light sources", &_renderContext.displayLightSourceObjects)) {
			setDisplayLightSourceObjects(_renderContext.displayLightSourceObjects);
		}

		ImGui::Separator();
		ImGui::SliderFloat("Field of view", &_fovY, 45.f, 120.f);

		ImGui::TreePop();
	}
}

void Engine::uiUpdateHDR()
{
	if (ImGui::TreeNodeEx("HDR", ImGuiTreeNodeFlags_DefaultOpen)) {

		if (ImGui::TreeNodeEx("Tone mapping", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Checkbox("Enable tone mapping", &_renderContext.comp.enableToneMapping);

			const char* items[] = {
				"Reinhard Extended", "Reinhard", "Uncharted2", "ACES Narkowicz", "ACES Hill" };
			static int item_current = _renderContext.comp.toneMappingMode;
			if (ImGui::Combo("ToneMapping", &item_current, items, IM_ARRAYSIZE(items))) {
				_renderContext.comp.toneMappingMode = item_current;
			}

			{ // Scene EV
				ImGui::Text("Adjust scene EV"); ImGui::SameLine();
				if (ImGui::Button("-")) {
					_renderContext.sceneData.exposure -= 1;
				} ImGui::SameLine();
				ImGui::Text("%f", _renderContext.sceneData.exposure); ImGui::SameLine();
				if (ImGui::Button("+")) {
					_renderContext.sceneData.exposure += 1;
				}
			}

			{ // Gamma correction
				const char* items[] = {
					"No gamma correction", "Gamma correction", "Inverse gamma correction" };
				static int item_current = _renderContext.comp.gammaMode;
				if (ImGui::Combo("Gamma", &item_current, items, IM_ARRAYSIZE(items))) {
					_renderContext.comp.gammaMode = item_current;
				}
			}

			ImGui::TreePop();
		}

		if (ImGui::TreeNodeEx("Temporal eye adaptation", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Checkbox("Enable eye adaptation", &_renderContext.comp.enableAdaptation);

			if (ImGui::TreeNodeEx("Average luminance computation", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::Text("Current average luminance: %f", _gpu.compSSBO->averageLuminance);
				ImGui::Text("Target average luminance: %f", _gpu.compSSBO->targetAverageLuminance);

				ImGui::Separator();

				ImGui::SliderFloat("Min log luminance", &_renderContext.comp.minLogLum, -10.f, 0.f);
				ImGui::SliderFloat("Max log luminance", &_renderContext.maxLogLuminance, 1.f, 20.f);

				ImGui::SliderFloat("Histogram index weight", &_renderContext.comp.weights.x, 0.f, 2.f);
				//ImGui::SliderFloat("Weight Y", &_renderContext.cmp.weights.y, 0.f, 255.f);
				ImGui::SliderFloat("Awaited luminance (bin)", &_renderContext.comp.weights.z, 0.f, 100.f);
				ImGui::SliderFloat("Awaited luminance weight", &_renderContext.comp.weights.w, 0.f, 5.f);

				ImGui::TreePop();
			}

			if (ImGui::TreeNodeEx("Histogram bounds", ImGuiTreeNodeFlags_DefaultOpen)) {

				ImGui::SliderFloat("Lower", &_renderContext.lumPixelLowerBound, 0.f, 0.45f);
				ImGui::SliderFloat("Upper", &_renderContext.lumPixelUpperBound, 0.55f, 1.f);

				ImGui::Separator();
				ImGui::Text("Total pixels: %u", _renderContext.comp.totalPixelNum);
				ImGui::Text("Histogram bounds: %f %f", _renderContext.lumPixelLowerBound, _renderContext.lumPixelUpperBound);
				ImGui::Text("Histogram bounds indices: %u %u", _renderContext.comp.lumLowerIndex, _renderContext.comp.lumUpperIndex);

				ImGui::TreePop();
			}

			ImGui::TreePop();
		}
	
		if (ImGui::TreeNodeEx("Plots", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (ImGui::TreeNodeEx("Adaptation Window")) {
				
				ImGui::Separator();

				static RollingBuffer rdata, rdata1;

				ImGui::SliderFloat("Adaptation time coefficient", &_renderContext.eyeAdaptationTimeCoefficient, 1.f, 10.f);

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
			if (ImGui::TreeNodeEx("Histogram", ImGuiTreeNodeFlags_DefaultOpen)) {
				if (ImPlot::BeginPlot("Luminance", ImVec2(-1, 200))) {
					// We have to wait for GPU to finish execution because compute shader 
					// might have not finished operating on current _gpu.compSSBO buffer
					// even though the command was already recorded to queue.
					// This is probably a temporary solution as it is very inefficient.
					// If we don't use this, histogram will be very laggy most of the time.
					vkDeviceWaitIdle(_device);

					uint32_t bins = MAX_LUMINANCE_BINS;

					std::array<uint32_t, MAX_LUMINANCE_BINS> xs;
					std::iota(xs.begin(), xs.end(), 0);
					

					for (uint32_t i = 0; i < MAX_LUMINANCE_BINS; ++i) {
						xs[i] = i;
					}

					uint32_t maxBin = 0;
					for (uint32_t i = 0; i < MAX_LUMINANCE_BINS; ++i) {
						if (_gpu.compSSBO->luminance[i] > maxBin) {
							maxBin = _gpu.compSSBO->luminance[i];
						}
					}

					ImPlot::SetupLegend(ImPlotLocation_North, ImPlotLegendFlags_Outside);
					ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);

					ImPlot::SetupAxisLimits(ImAxis_X1, 0, bins);
					ImPlot::SetupAxisLimits(ImAxis_Y1, 0, maxBin, ImPlotCond_Always);

					ImPlot::SetNextMarkerStyle(ImPlotMarker_Cross);


					ImPlot::PlotStems("Luminance", xs.data(), _gpu.compSSBO->luminance, bins);

					uint32_t start_i = _renderContext.comp.lumLowerIndex;
					uint32_t end_i   = _renderContext.comp.lumUpperIndex;

					std::vector<uint32_t> xs1(end_i - start_i);
					std::iota(xs1.begin(), xs1.end(), start_i);

					std::vector<uint32_t> vals1(_gpu.compSSBO->luminance + start_i, _gpu.compSSBO->luminance + end_i);

					ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle);
					ImPlot::PlotStems("Luminance bounded", xs1.data(), vals1.data(), vals1.size());


					ImPlot::EndPlot();
				}
				ImGui::TreePop();
			}
			
			ImGui::TreePop();
		}
		
		ImGui::TreePop();
	}
}

void Engine::uiUpdateRenderContext()
{
	if (ImGui::TreeNodeEx("Scene lighting")) {
		float amb = _renderContext.sceneData.ambientColor.x;
		if (ImGui::SliderFloat("Ambient factor", &amb, 0.f, 1.f)) {
			_renderContext.sceneData.ambientColor = { amb, amb, amb };
		}

		if (ImGui::SliderFloat("Radius brightness treshold", &_renderContext.lightRadiusTreshold, 0.f, 0.25f)) {
			for (int i = 0; i < MAX_LIGHTS; ++i) {
				_renderContext.UpdateLightRadius(i);
			}
		}

		ImGui::Spacing();
		for (int i = 0; i < MAX_LIGHTS; ++i) {

			if (ImGui::TreeNodeEx((void*)(intptr_t)i, ImGuiTreeNodeFlags_DefaultOpen, "Light source %d", i)) {
				GPULight& l = _renderContext.sceneData.lights[i];

				ImGui::Checkbox("Enabled", &l.enabled);

				glm::vec3 tmpPos = l.position;
				if (ImGui::DragFloat3("Position", glm::value_ptr(tmpPos), 0.1f, -100.f, 100.f)) {
					_renderContext.UpdateLightPosition(i, tmpPos);
				}

				if (ImGui::DragFloat("Intensity", &l.intensity, 1.f, 1.f, 10000.f)) {
					_renderContext.UpdateLightRadius(i);
				}

				if (ImGui::Button("Reset Intensity")) {
					l.intensity = 1.f;
				}

				static float atten = 0.1f;
				if (ImGui::DragFloat("Attenuation", &atten, 0.01f, 0.f, 1.f, "%.5f")) {
					l.linear = (0.7 - 0.0014) * atten;
					l.quadratic = (0.44 - 0.000007) * atten;

					_renderContext.UpdateLightRadius(i);
				}

				ImGui::Text("Radius %f", l.radius);

				if (ImGui::TreeNode((void*)(intptr_t)(i + 1), "Color")) {
					ImGui::PushItemWidth(100.f);
					ImGui::ColorPicker3("Color", glm::value_ptr(l.color));
					ImGui::PopItemWidth();
					ImGui::TreePop();
				}

				ImGui::TreePop();
			}
		}

		ImGui::TreePop();
	}
}

void Engine::uiUpdateDebugDisplay()
{
	if (ImGui::TreeNodeEx("Debug display")) {
		if (ImGui::TreeNodeEx("Shadow cubemap")) {
			ImGui::Checkbox("Display shadow map", &_renderContext.sceneData.showShadowMap);

			int& light_i = _renderContext.sceneData.shadowMapDisplayIndex;
			if (ImGui::Button("<")) {
				light_i = std::clamp(--light_i, 0, MAX_LIGHTS - 1);
			}
			ImGui::SameLine();
			ImGui::Text("Shadow map %d", light_i);
			ImGui::SameLine();
			if (ImGui::Button(">")) {
				light_i = std::clamp(++light_i, 0, MAX_LIGHTS - 1);
			}
			ImGui::TreePop();

			ImGui::SliderFloat("Shadow Display Brightness", &_renderContext.sceneData.shadowMapDisplayBrightness, 1.f, 10.f);
		}

		ImGui::Checkbox("Show normals", &_renderContext.sceneData.showNormals);


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

		ImGui::TreePop();
	}
}


static const char* get_longest_str(std::vector<const char*>& strs) 
{
	if (strs.size() == 0) {
		return nullptr;
	}
	const char* ptr = strs[strs.size()-1];
	for (size_t i = 0; i < strs.size(); ++i) {
		if (strlen(strs[i]) > strlen(ptr)) {
			ptr = strs[i];
		}
	}
	return ptr;
}

static std::vector<const char*> browse_path(std::string path, std::string format, std::vector<std::string>& strs) 
{
	strs.clear();

	for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(path)) {
		std::string pstr = dirEntry.path().string();
		if (pstr.find_last_of(".") != std::string::npos &&
			pstr.substr(pstr.find_last_of(".")) == format)
		{
			strs.push_back(pstr);
		}
	}

	std::vector<const char*> cstrs(strs.size());
	for (size_t i = 0; i < strs.size(); ++i) {
		cstrs[i] = strs[i].c_str();
	}

	return cstrs;
}

bool Engine::uiSaveScene()
{
	bool saved = false;

	std::vector<std::string> scenes;
	std::vector<const char*> scenes_cstr;

	scenes_cstr = browse_path(Engine::SCENE_PATH, ".json", scenes);
	scenes_cstr.push_back("* New");

	float width = 0;
	width = ImGui::CalcTextSize(get_longest_str(scenes_cstr)).x + ImGui::GetStyle().FramePadding.x * 2.0f;
	ImGui::PushItemWidth(width);

	static int i = 0;
	if (ImGui::ListBox("##Scenes", &i, scenes_cstr.data(), scenes_cstr.size())) {
		if (i != scenes_cstr.size() - 1) {
			saveScene(scenes_cstr[i]);
			saved = true;
		}
	}

	// Corresponds to "* New" entry. Let user enter the saved file name
	if (i == scenes_cstr.size() - 1) {
		char buf[256] = { 0 };
		if (ImGui::InputText("##Scene name", buf, 256, ImGuiInputTextFlags_EnterReturnsTrue)) {
			saveScene(Engine::SCENE_PATH + buf);
			saved = true;
		}
	}

	ImGui::PopItemWidth();

	return saved;
}

bool Engine::uiLoadScene()
{
	bool loaded = false;

	std::vector<std::string> scenes;
	std::vector<const char*> scenes_cstr;

	scenes_cstr = browse_path(Engine::SCENE_PATH, ".json", scenes);

	float width = 0;
	width = ImGui::CalcTextSize(get_longest_str(scenes_cstr)).x + ImGui::GetStyle().FramePadding.x * 2.0f;
	ImGui::PushItemWidth(width);

	static int i = 0;
	if (ImGui::ListBox("##Scenes", &i, scenes_cstr.data(), scenes_cstr.size())) {
		loadScene(scenes_cstr[i]);
		loaded = true;
	}

	ImGui::PopItemWidth();

	return loaded;
}

void Engine::uiUpdateMenuBar()
{
	if (ImGui::BeginMenuBar()) {
		ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;
		
		if (ImGui::BeginMenu("Save")) {
			uiSaveScene();

			ImGui::EndMenu();
		} else if (_saveShortcutPressed) {
			ImGui::Begin("Save", 0, flags); {
				_saveShortcutPressed = !uiSaveScene();
			} ImGui::End();
		}

		if (ImGui::BeginMenu("Load")) {
			uiLoadScene();

			ImGui::EndMenu();
		} else if (_loadShortcutPressed) {
			ImGui::Begin("Load", 0, flags); {
				_loadShortcutPressed = !uiLoadScene();
			} ImGui::End();
		}

		ImGui::EndMenuBar();
	}
}

void Engine::uiUpdateViewport()
{
	_isViewportHovered = ImGui::IsWindowHovered();

	ImVec2 vSize = ImGui::GetContentRegionAvail();
	uint32_t usX = (uint32_t)vSize.x;
	uint32_t usY = (uint32_t)vSize.y;

	if (_viewport.imageExtent.width != usX || _viewport.imageExtent.height != usY) {
		// Need to wait for all commands to finish so that we can safely recreate and reregister all viewport images
		vkDeviceWaitIdle(_device);

		imgui_UnregisterViewportImageViews();

		// Create viewport images and etc. with new extent
		recreateViewport(usX, usY);

		imgui_RegisterViewportImageViews();
	}

	ImGui::Image(_imguiViewportImageViewDescriptorSets[_frameInFlightNum], ImVec2(usX, usY));
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
	if (opt_fullscreen) {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	} else {
		dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
	}

	if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode) {
		window_flags |= ImGuiWindowFlags_NoBackground;
	}

	if (!opt_padding) {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	}
	static bool p_open = true;

	ImGui::Begin("DockSpace", &p_open, window_flags);
	{
		uiUpdateMenuBar();

		if (!opt_padding) {
			ImGui::PopStyleVar();
		}

		if (opt_fullscreen) {
			ImGui::PopStyleVar(2);
		}

		ImGuiIO& io = ImGui::GetIO();

		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
			ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
		}

		ImGui::Begin("Config");
		{
			uiUpdateScene();
			uiUpdateHDR();
			uiUpdateRenderContext();
			uiUpdateDebugDisplay();

			ImGui::Separator();
			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

			_frameRate = io.Framerate;
			_deltaTime = io.DeltaTime;
		}
		ImGui::End();
	
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
		ImGui::Begin("Viewport", (bool*)0, ImGuiWindowFlags_None);
		{
			uiUpdateViewport();		
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
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
}
