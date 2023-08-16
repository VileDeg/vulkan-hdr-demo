#include "stdafx.h"
#include "engine.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

#include "imgui/imgui_internal.h"

#include "imgui/implot.h"

#define SHOW_IMGUI_METRICS 0

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


static const char* get_longest_str(std::vector<const char*>& strs)
{
	if (strs.size() == 0) {
		return nullptr;
	}
	const char* ptr = strs[strs.size() - 1];
	for (size_t i = 0; i < strs.size(); ++i) {
		if (strlen(strs[i]) > strlen(ptr)) {
			ptr = strs[i];
		}
	}
	return ptr;
}

static std::vector<const char*> browse_path(std::string path, std::vector<std::string>& strs, std::function<bool(std::filesystem::directory_entry)> entryCondition)
{
	strs.clear();

	for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(path)) {
		if (entryCondition(dirEntry)) {
			strs.push_back(dirEntry.path().string());
		}
	}

	std::vector<const char*> cstrs(strs.size());
	for (size_t i = 0; i < strs.size(); ++i) {
		cstrs[i] = strs[i].c_str();
	}

	return cstrs;
}

static std::vector<const char*> browse_path_for_files_with_format(std::string path, std::string format, std::vector<std::string>& strs)
{
	return browse_path(path, strs, [format](std::filesystem::directory_entry dirEntry) {
		std::string pstr = dirEntry.path().string();
		return pstr.find_last_of(".") != std::string::npos &&
			pstr.substr(pstr.find_last_of(".")) == format;
	});
}

static std::vector<const char*> browse_path_for_folders(std::string path, std::vector<std::string>& strs)
{
	return browse_path(path, strs, [](std::filesystem::directory_entry dirEntry) {
		std::string pstr = dirEntry.path().string();
		return dirEntry.is_directory();
	});
}


static ImVec2 ui_ViewportTexWindowFit(float viewportAspect, bool scrollbar) {
	ImVec2 avail = ImGui::GetContentRegionAvail();

	if (scrollbar) {
		ImGuiStyle& style = ImGui::GetStyle();
		float scrollbarWidth = style.ScrollbarSize;
		avail.x -= scrollbarWidth + 15;
	}

	float min = std::min(avail.x, avail.y);
	ImVec2 dim = ImVec2(min, min);

	if (avail.y > min) {
		dim.y /= viewportAspect;
	} else {
		dim.x *= viewportAspect;
	}

	return dim;
}

static void ui_AttachmentImageButton(VkDescriptorSet tex_id, const char* att_name, ImVec2 dim, int button_i, bool& selected) {
	ImGui::SeparatorText(att_name);

	ImGui::PushID(button_i);
	{
		float my_tex_w = (float)dim.x;
		float my_tex_h = (float)dim.y;

		ImVec2 size = ImVec2(my_tex_w, my_tex_h);                   // Size of the image we want to make visible
		ImVec2 uv0 = ImVec2(0.0f, 0.0f);                            // UV coordinates for lower-left
		ImVec2 uv1 = ImVec2(size.x / my_tex_w, size.y / my_tex_h);  // UV coordinates in our texture
		ImVec4 bg_col = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);             // Black background
		ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);           // No tint


		if (ImGui::ImageButton("", tex_id, size, uv0, uv1, bg_col, tint_col)) {
			selected = true;
		}
	}
	ImGui::PopID();
}

static void ui_PostFXPipelineButton(bool& flag, std::string name,
	bool* toDisable = nullptr, bool center = true, bool single_button = true)
{
	ImVec2 button_size = ImVec2(ImGui::CalcTextSize(name.c_str()).x, 0);

	if (center) {
		if (single_button) {
			// obtain width of window
			float width = ImGui::GetWindowSize().x;

			// figure out where we need to move the button to. It's good if you understand why this formula works!
			float centre_position_for_button = (width - button_size.x) / 2;

			// tell Dear ImGui to render the button at the current y pos, but with the new x pos
			ImGui::SetCursorPosX(centre_position_for_button);
		} else {
			// obtain size of window
			ImVec2 avail = ImGui::GetWindowSize();

			// calculate centre of window for button. I recommend trying to figure out why this works!
			float centre_position_for_button{
				// we have two buttons, so twice the size - and we need to account for the spacing in the middle
				(avail.x - button_size.x * 2 - ImGui::GetStyle().ItemSpacing.x) / 2
				//(avail.y - button_size.y) / 2
			};

			// tell Dear ImGui to render the button at the new pos
			ImGui::SetCursorPosX(centre_position_for_button);
		}
	}

	bool flagPrev = flag;
	if (!flagPrev) {
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().DisabledAlpha);
	}

	if (ImGui::Button(name.c_str())) {
		flag = !flag;
		if (flag && toDisable) {
			*toDisable = false;
		}
	}

	if (!flagPrev) {
		ImGui::PopStyleVar();
	}
}



void Engine::ui_Update()
{
	// Start the Dear ImGui frame
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGuiIO& io = ImGui::GetIO();

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
		ui_MenuBar();

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

		ui_Window("Config", [this]() {
			// Leave a fixed amount of width for labels (by passing a negative value), the rest goes to widgets.
			ImGui::PushItemWidth(ImGui::GetFontSize() * -13);
			ui_Scene();
			ui_HDR();
			ui_RenderContext();
			ui_DebugDisplay();
			ImGui::PopItemWidth();
		});

		ui_Window("Viewport", [this]() {
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
			ui_Viewport();
			ImGui::PopStyleVar();
			ImGui::PopStyleVar();
		});

		ui_Window("PostFX Pipeline", [this]() {
			ui_PostFXPipeline();
		});

		ui_Window("Attachment Viewer", [this]() {
			ui_AttachmentViewer();
		});

		ui_StatusBar();

#if SHOW_IMGUI_METRICS == 1
		ImGui::ShowMetricsWindow();
#endif
	}
	ImGui::End();
}

void Engine::ui_InitImGui()
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
	VK_ASSERT(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool));

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

	ui_RegisterTextures();

	//add the destroy the imgui created structures
	_deletionStack.push([=]() {
		ui_UnregisterTextures();

		vkDestroyDescriptorPool(_device, imguiPool, nullptr);

		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();

		ImPlot::DestroyContext();
		ImGui::DestroyContext();
		});
}

void Engine::ui_Init()
{
	ui_InitImGui();

	uiWindows["Config"] = true;
	uiWindows["Viewport"] = true;
	uiWindows["PostFX Pipeline"] = true;
	uiWindows["Attachment Viewer"] = true;
}

void Engine::ui_RegisterTextures()
{
	ASSERT(_viewport.ui_texids.size() == 0);
	_viewport.ui_texids.resize(_viewport.imageViews.size());

	for (size_t i = 0; i < _viewport.imageViews.size(); ++i) {
		_viewport.ui_texids[i] = ImGui_ImplVulkan_AddTexture(_nearestSampler, _viewport.imageViews[i], ViewportPass::UI_IMAGE_LAYOUT);
	}

	for (auto& att : _postfx.att) {
		ASSERT(att.second.ui_texid == VK_NULL_HANDLE);
		att.second.ui_texid = ImGui_ImplVulkan_AddTexture(_nearestSampler, att.second.view, Attachment::UI_IMAGE_LAYOUT);
	}

	for (auto& pyr : _postfx.pyr) {
		ASSERT(pyr.second.ui_texids.size() == 0);
		
		for (auto& view : pyr.second.views) {
			pyr.second.ui_texids.push_back(
				ImGui_ImplVulkan_AddTexture(_nearestSampler, view, AttachmentPyramid::UI_IMAGE_LAYOUT));
		}
	}
}

void Engine::ui_UnregisterTextures()
{
	for (auto& texid : _viewport.ui_texids) {
		ImGui_ImplVulkan_RemoveTexture(texid);
		texid = VK_NULL_HANDLE;
	}

	_viewport.ui_texids.clear();

	for (auto& att : _postfx.att) {
		ImGui_ImplVulkan_RemoveTexture(att.second.ui_texid);
		att.second.ui_texid = VK_NULL_HANDLE;
	}

	for (auto& pyr : _postfx.pyr) {
		for (auto& texid : pyr.second.ui_texids) {
			ImGui_ImplVulkan_RemoveTexture(texid);
			texid = VK_NULL_HANDLE;
		}
		pyr.second.ui_texids.clear();
	}
}


void Engine::ui_Scene()
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

void Engine::ui_HDR()
{
	if (ImGui::TreeNodeEx("HDR", ImGuiTreeNodeFlags_DefaultOpen)) {

		if (ImGui::TreeNodeEx("Bloom", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Checkbox("Enable bloom", &_postfx.enableBloom);

			//ImGui::SliderFloat("Bloom Threshold", &_postfx.ub.bloomThreshold, 0.1, 10);
			ImGui::SliderFloat("Bloom Weight", &_postfx.ub.bloomWeight, 0.001, 0.5f);

			ImGui::TreePop();
		}

		if (ImGui::TreeNodeEx("Global tone mapping", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (ImGui::Checkbox("Enable global tone mapping", &_postfx.enableGlobalToneMapping)) {
				if (_postfx.enableGlobalToneMapping) {
					_postfx.enableLocalToneMapping = false;
				}
			}

			const char* items[] = {
				"Reinhard Extended", "Reinhard", "Uncharted2", "ACES Narkowicz", "ACES Hill" };
			static int item_current = _postfx.ub.toneMappingMode;
			if (ImGui::Combo("ToneMapping", &item_current, items, IM_ARRAYSIZE(items))) {
				_postfx.ub.toneMappingMode = item_current;
			}

			ImGui::TreePop();
		}

		if (ImGui::TreeNodeEx("Local tone mapping", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (ImGui::Checkbox("Enable local tone mapping", &_postfx.enableLocalToneMapping)) {
				if (_postfx.enableLocalToneMapping) {
					_postfx.enableGlobalToneMapping = false;
				}
			}

			const char* items[] = { "Durand 2002", "Exposure fusion" };
			static int item_current = (int)_postfx.localToneMappingMode;
			if (ImGui::Combo("LTM Mode", &item_current, items, IM_ARRAYSIZE(items))) {
				_postfx.localToneMappingMode = (PostFX::LTM)item_current;
			}

			if (ImGui::TreeNodeEx("Durand 2002", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::SliderFloat("Base Scale", &_postfx.ub.baseScale, 0.001f, 1.f);
				ImGui::SliderFloat("Base Offset", &_postfx.ub.baseOffset, -0.999f, 0.999f);

				//ImGui::SliderFloat("Spacial sigma", &_postfx.ub.sigmaS, 3.f, 50.0f);
				ImGui::SliderInt("Bilateral Radius", &_postfx.ub.durandBilateralRadius, 1, 25);

				ImGui::Text("Spacial sigma(2%% of viewport size) %f", _postfx.ub.sigmaS);
				ImGui::SliderFloat("Range sigma", &_postfx.ub.sigmaR, 0.1f, 2.0f);

				ImGui::TreePop();
			}

			if (ImGui::TreeNodeEx("Exposure fusion", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::SliderFloat("Shadows Exposure", &_postfx.ub.shadowsExposure, 0, 10);
				//ImGui::SliderFloat("Midtones Exposure", &_postfx.ub.midtonesExposure, -10, 5);
				ImGui::SliderFloat("Highlights Exposure", &_postfx.ub.highlightsExposure, -20, 0);

				ImGui::SliderFloat("Exposedness Weight Sigma", &_postfx.ub.exposednessWeightSigma, 0.01, 10);

				ImGui::TreePop();
			}

			ImGui::TreePop();
		}

		if (ImGui::TreeNodeEx("Gamma correction", ImGuiTreeNodeFlags_DefaultOpen)) {

			{ // Gamma correction
				ImGui::Checkbox("Enable Gamma Correction", &_postfx.enableGammaCorrection);

				ImGui::SliderFloat("Gamma", &_postfx.ub.gamma, 0.5f, 3.f);
			}

			ImGui::TreePop();
		}

		if (ImGui::TreeNodeEx("Temporal eye adaptation", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Checkbox("Enable eye adaptation", &_postfx.enableAdaptation);

			if (ImGui::TreeNodeEx("Average luminance computation", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::Text("Current average luminance: %f", _gpu.compSSBO->averageLuminance);
				ImGui::Text("Target average luminance: %f", _gpu.compSSBO->targetAverageLuminance);

				ImGui::Separator();

				ImGui::SliderFloat("Min log luminance", &_postfx.ub.minLogLum, -10.f, 0.f);
				ImGui::SliderFloat("Max log luminance", &_postfx.maxLogLuminance, 1.f, 20.f);

				ImGui::SliderFloat("Histogram index weight", &_postfx.ub.weights.x, 0.f, 2.f);
				//ImGui::SliderFloat("Weight Y", &_renderContext.cmp.weights.y, 0.f, 255.f);
				ImGui::SliderFloat("Awaited luminance (bin)", &_postfx.ub.weights.z, 0.f, 100.f);
				ImGui::SliderFloat("Awaited luminance weight", &_postfx.ub.weights.w, 0.f, 5.f);

				ImGui::TreePop();
			}

			if (ImGui::TreeNodeEx("Histogram bounds", ImGuiTreeNodeFlags_DefaultOpen)) {

				ImGui::SliderFloat("Lower", &_postfx.lumPixelLowerBound, 0.f, 0.45f);
				ImGui::SliderFloat("Upper", &_postfx.lumPixelUpperBound, 0.55f, 1.f);

				ImGui::Separator();
				ImGui::Text("Total pixels: %u", _postfx.ub.totalPixelNum);
				ImGui::Text("Histogram bounds: %f %f", _postfx.lumPixelLowerBound, _postfx.lumPixelUpperBound);
				ImGui::Text("Histogram bounds indices: %u %u", _postfx.ub.lumLowerIndex, _postfx.ub.lumUpperIndex);

				ImGui::TreePop();
			}

			ImGui::TreePop();
		}
	
		if (ImGui::TreeNodeEx("Plots", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (_postfx.enableAdaptation) {
				if (ImGui::TreeNodeEx("Adaptation Window")) {
				
					ImGui::Separator();

					static RollingBuffer rdata, rdata1;

					ImGui::SliderFloat("Adaptation time coefficient", &_postfx.eyeAdaptationTimeCoefficient, 1.f, 10.f);

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

						uint32_t start_i = _postfx.ub.lumLowerIndex;
						uint32_t end_i   = _postfx.ub.lumUpperIndex;

						std::vector<uint32_t> xs1(end_i - start_i);
						std::iota(xs1.begin(), xs1.end(), start_i);

						std::vector<uint32_t> vals1(_gpu.compSSBO->luminance + start_i, _gpu.compSSBO->luminance + end_i);

						ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle);
						ImPlot::PlotStems("Luminance bounded", xs1.data(), vals1.data(), vals1.size());


						ImPlot::EndPlot();
					}
					ImGui::TreePop();
				}
			}
			
			ImGui::TreePop();
		}
		
		ImGui::TreePop();
	}
}

void Engine::ui_RenderContext()
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
				if (ImGui::DragFloat3("Position", glm::value_ptr(tmpPos), 0.01f, -100.f, 100.f)) {
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

void Engine::ui_DebugDisplay()
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

void Engine::ui_MenuBar()
{
	if (ImGui::BeginMenuBar()) {
		ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;
		
		if (ImGui::BeginMenu("Save Scene")) {
			ui_SaveScene();

			ImGui::EndMenu();
		} else if (_saveShortcutPressed) {
			ImGui::Begin("Save Scene", 0, flags); {
				_saveShortcutPressed = !ui_SaveScene();
			} ImGui::End();
		}

		if (ImGui::BeginMenu("Load Scene")) {
			ui_LoadScene();

			ImGui::EndMenu();
		} else if (_loadShortcutPressed) {
			ImGui::Begin("Load Scene", 0, flags); {
				_loadShortcutPressed = !ui_LoadScene();
			} ImGui::End();
		}

		if (ImGui::BeginMenu("Load Skybox")) {
			ui_LoadSkybox();

			ImGui::EndMenu();
		}

		for (auto& wnd : uiWindows) {
			if (wnd.second) {
				continue;
			}
			if (ImGui::BeginMenu(wnd.first.c_str())) {
				wnd.second = true;

				ImGui::EndMenu();
			}
		}

		ImGui::EndMenuBar();
	}
}

void Engine::ui_Viewport()
{
	_isViewportHovered = ImGui::IsWindowHovered();

	ImVec2 vSize = ImGui::GetContentRegionAvail();

	uint32_t usX = (uint32_t)vSize.x;
	uint32_t usY = (uint32_t)vSize.y;

	// We round up our viewport size to multiple of 4 to make mipmapping of it a lot more accurate
	// rounding to multiple of 8, 16 etc. would be even better but that would be visible when resizing
	/*static int step = 32;

	usX = math_utils::roundUpPw2(usX, step);
	usY = math_utils::roundUpPw2(usY, step);*/

	if (_viewport.width != usX || _viewport.height != usY) {
		// Viewport will be resized later after all rendering is finished
		newViewportSizeX = usX;
		newViewportSizeY = usY;
		_wasViewportResized = true;
	}

	ImGui::Image(_viewport.ui_texids[_frameInFlightNum], ImVec2(_viewport.width, _viewport.height));
}

void Engine::ui_PostFXPipeline()
{
	ui_PostFXPipelineButton(_postfx.enableAdaptation, "Exposure Adaptation");

	ui_PostFXPipelineButton(_postfx.enableBloom, "Bloom");

	ui_PostFXPipelineButton(_postfx.enableGlobalToneMapping, "Global Tone Mapping", &_postfx.enableLocalToneMapping, true, false);

	ImGui::SameLine();

	ui_PostFXPipelineButton(_postfx.enableLocalToneMapping, "Local Tone Mapping", &_postfx.enableGlobalToneMapping, false);

	ui_PostFXPipelineButton(_postfx.enableGammaCorrection, "Gamma Correction");
}

void Engine::ui_AttachmentViewer()
{
	ImVec2 defaultPopupSize = { 512, 512 };
	bool popup_docking = true;
	ImGuiWindowFlags flags = 0;
	flags |= popup_docking ? 0 : ImGuiWindowFlags_NoDocking;

	ImVec2 dim = ui_ViewportTexWindowFit(_viewport.aspectRatio, true);

	if (ImGui::TreeNodeEx("Images", ImGuiTreeNodeFlags_DefaultOpen)) {
		int i = 0;
		for (auto& att : _postfx.att) {

			size_t undersc = att.first.find_first_of("_") + 1;
			ASSERT(undersc != std::string::npos);

			std::string pref = att.first.substr(0, undersc);
			if (!_postfx.isEffectEnabled(_postfx.getEffectFromPrefix(pref))) {
				continue;
			}

			ui_AttachmentImageButton(att.second.ui_texid, att.first.c_str(), dim, i, att.second.ui_selected);

			if (att.second.ui_selected) {
				ImGui::SetNextWindowSize(defaultPopupSize, ImGuiCond_Once);

				ImGui::Begin(att.first.c_str(), &att.second.ui_selected, flags);
				{
					ImVec2 dim = ui_ViewportTexWindowFit(_viewport.aspectRatio, false);

					ImGui::Image(att.second.ui_texid, dim);
				}
				ImGui::End();
			}
			++i;
		}



		ImGui::TreePop();
	}

	if (ImGui::TreeNodeEx("Pyramids", ImGuiTreeNodeFlags_DefaultOpen)) {
		int i = 0;
		for (auto& pyr : _postfx.pyr) {
			size_t undersc = pyr.first.find_first_of("_") + 1;
			ASSERT(undersc != std::string::npos);

			std::string pref = pyr.first.substr(0, undersc);
			if (!_postfx.isEffectEnabled(_postfx.getEffectFromPrefix(pref))) {
				continue;
			}

			const char* att_name = pyr.first.c_str();
			int button_i = i;

			ui_AttachmentImageButton(pyr.second.ui_texids[0], att_name, dim, i, pyr.second.ui_selected);

			if (pyr.second.ui_selected) {
				ImGui::SetNextWindowSize(defaultPopupSize, ImGuiCond_Once);

				ImGui::Begin(att_name, &pyr.second.ui_selected, flags);
				{
					ImVec2 dim = ui_ViewportTexWindowFit(_viewport.aspectRatio, false);

					int i = 0;
					for (auto& view : pyr.second.views) {
						ImGui::SeparatorText(("Mip " + std::to_string(i)).c_str());

						VkDescriptorSet tex_id = pyr.second.ui_texids[i];
						ImGui::Image(tex_id, dim);

						++i;
					}
				}
				ImGui::End();
			}
			++i;
		}
		ImGui::TreePop();
	}
}

void Engine::ui_StatusBar()
{
	ImGuiIO& io = ImGui::GetIO();
	_frameRate = io.Framerate;
	_deltaTime = io.DeltaTime;

	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
	float height = ImGui::GetFrameHeight();

	// Function from imgui_internal.h. Issue: https://github.com/ocornut/imgui/issues/3518
	if (ImGui::BeginViewportSideBar("##SecondaryMenuBar", NULL, ImGuiDir_Down, height, window_flags)) {
		if (ImGui::BeginMenuBar()) {
			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
			ImGui::EndMenuBar();
		}
	}
	ImGui::End();
}

bool Engine::ui_LoadSkybox()
{
	bool loaded = false;

	std::vector<std::string> entries;
	std::vector<const char*> entries_cstr;

	entries_cstr = browse_path_for_folders(SKYBOX_PATH, entries);

	float width = 0;
	width = ImGui::CalcTextSize(get_longest_str(entries_cstr)).x + ImGui::GetStyle().FramePadding.x * 2.0f;
	ImGui::PushItemWidth(width);

	static int i = 0;
	if (ImGui::ListBox("##entries", &i, entries_cstr.data(), entries_cstr.size())) {
		std::string skyboxDir = entries[i].substr(entries[i].find_last_of("/")+1);
		loadSkybox(skyboxDir);
		loaded = true;
	}

	ImGui::PopItemWidth();

	return loaded;
}

bool Engine::ui_LoadScene()
{
	bool loaded = false;

	std::vector<std::string> scenes;
	std::vector<const char*> scenes_cstr;

	scenes_cstr = browse_path_for_files_with_format(SCENE_PATH, ".json", scenes);

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

bool Engine::ui_SaveScene()
{
	bool saved = false;

	std::vector<std::string> scenes;
	std::vector<const char*> scenes_cstr;

	scenes_cstr = browse_path_for_files_with_format(SCENE_PATH, ".json", scenes);
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
			saveScene(SCENE_PATH + buf);
			saved = true;
		}
	}

	ImGui::PopItemWidth();

	return saved;
}



bool& Engine::ui_GetWindowFlag(std::string name) {
	ASSERT(uiWindows.contains(name));
	return uiWindows[name];
}


void Engine::ui_Window(std::string name, std::function<void()> func, int flags/* = 0*/)
{
	if (ui_GetWindowFlag(name)) {
		ImGui::Begin(name.c_str(), &ui_GetWindowFlag(name), (ImGuiWindowFlags)flags);
		{
			func();
		}
		ImGui::End();
	}
}



void Engine::ui_OnDrawStart()
{
	ImGui::Render();
}

void Engine::ui_OnRenderPassEnd(VkCommandBuffer cmdBuffer)
{
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer); //, _mainPipeline

	ImGuiIO& io = ImGui::GetIO();
	
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}

	
}
