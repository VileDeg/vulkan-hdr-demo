#include "stdafx.h"
#include "engine.h"

#include "pipeline.h"

static std::vector<char> readShaderBinary(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    ASSERTMSG(file.is_open(), "Failed to open file: " << filename);

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}

static bool createShaderModule(VkDevice device, const std::vector<char>& code, VkShaderModule* module)
{
    VkShaderModuleCreateInfo moduleInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code.size(),
        .pCode = (uint32_t*)code.data()
    };

    return vkCreateShaderModule(device, &moduleInfo, nullptr, module) == VK_SUCCESS;
}


static PipelineShaders loadShaders(VkDevice device, const std::string& vertName, const std::string& fragName)
{
    PipelineShaders shaders{};

    shaders.vert.code = readShaderBinary(Engine::SHADER_PATH + vertName);
    shaders.frag.code = readShaderBinary(Engine::SHADER_PATH + fragName);

    if (createShaderModule(device, shaders.vert.code, &shaders.vert.module)) {
        std::cout << "Vertex shader successfully loaded." << std::endl;
    } else {
        PRWRN("Failed to load vertex shader");
    }

    if (createShaderModule(device, shaders.frag.code, &shaders.frag.module)) {
        std::cout << "Fragment shader successfully loaded." << std::endl;
    } else {
        PRWRN("Failed to load fragment shader");
    }

    return shaders;
}

static void s_createComputePipeline(VkDevice device, const std::string& shaderBinName, ComputeParts& cp)
{
    ShaderData comp;
    comp.code = readShaderBinary(Engine::SHADER_PATH + shaderBinName);

    if (createShaderModule(device, comp.code, &comp.module)) {
        std::cout << "Compute shader successfully loaded." << std::endl;
    } else {
        PRWRN("Failed to load compute shader");
    }

    VkPipelineShaderStageCreateInfo stageInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = comp.module,
        .pName = "main"
    };

    VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &cp.setLayout,
    };

    VKASSERT(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &cp.pipelineLayout));

    VkComputePipelineCreateInfo computePipelineInfo{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = stageInfo,
        .layout = cp.pipelineLayout,
    };

    VKASSERT(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineInfo, nullptr, &cp.pipeline));

    vkDestroyShaderModule(device, comp.module, nullptr);
}

void Engine::createPipelines()
{
    PipelineData pd_general{
        .shaders = loadShaders(_device, "shader.vert.spv", "shader.frag.spv"),
        .pushConstantsStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .pushConstantsSize = sizeof(GPUScenePC),
        .setLayouts = { _globalSetLayout, _diffuseTextureSetLayout }
    };

    PipelineData pd_shadow{
        .shaders = loadShaders(_device, "shadow_pass.vert.spv", "shadow_pass.frag.spv"),
        .pushConstantsStages = VK_SHADER_STAGE_VERTEX_BIT,
        .pushConstantsSize = sizeof(GPUShadowPC),
        .setLayouts = { _shadowSetLayout }
    };


    /*auto newMaterial = [&](std::string matName, PipelineData pd, VkRenderPass renderpass) {
        Pipeline pipeline(pd);
        pipeline.Init();
        pipeline.Build(_device, renderpass);

        createMaterial(pipeline.pipeline, pipeline.layout, matName);
    };*/


    VkPipelineRenderingCreateInfo renderingInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &_viewport.colorFormat,
        .depthAttachmentFormat = _viewport.depthFormat
    };

    { // General
        Pipeline pipeline(pd_general);
        pipeline.Build(_device, renderingInfo);

        createMaterial(pipeline.pipeline, pipeline.layout, "general");
    }
    { // Skybox
        Pipeline pipeline_skybox(pd_general);
        pipeline_skybox.rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;

        pipeline_skybox.Build(_device, renderingInfo);

        createMaterial(pipeline_skybox.pipeline, pipeline_skybox.layout, "skybox");
    }

    // Find a suitable depth format
    VkBool32 validDepthFormat = utils::getSupportedDepthFormat(_physicalDevice, &_shadow.depthFormat);
    assert(validDepthFormat);

    renderingInfo.pColorAttachmentFormats = &_shadow.colorFormat;
    renderingInfo.depthAttachmentFormat = _shadow.depthFormat;

    { // Shadow
        Pipeline pipeline_shadow(pd_shadow);

        // Changing culling mode to front mostly fixes Peter Panning 
        // and also for some reason fixes the bug when some objects get drawn on top although they shouldn't
        pipeline_shadow.rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
        pipeline_shadow.Build(_device, renderingInfo);

        createMaterial(pipeline_shadow.pipeline, pipeline_shadow.layout, "shadow");
    }

    pd_general.shaders.cleanup(_device);
    pd_shadow.shaders.cleanup(_device);

    { // Compute luminance histogram
        s_createComputePipeline(_device, "histogram.comp.spv"        , _compute.histogram);
        s_createComputePipeline(_device, "average_luminance.comp.spv", _compute.averageLuminance);
        s_createComputePipeline(_device, "tonemap.comp.spv"          , _compute.toneMapping);
    }

    _deletionStack.push([=]() mutable {
        vkDestroyPipelineLayout(_device, _compute.histogram.pipelineLayout, nullptr);
        vkDestroyPipeline(_device, _compute.histogram.pipeline, nullptr);

        vkDestroyPipelineLayout(_device, _compute.averageLuminance.pipelineLayout, nullptr);
        vkDestroyPipeline(_device, _compute.averageLuminance.pipeline, nullptr);

        vkDestroyPipelineLayout(_device, _compute.toneMapping.pipelineLayout, nullptr);
        vkDestroyPipeline(_device, _compute.toneMapping.pipeline, nullptr);
    });
}

Pipeline::Pipeline(PipelineData pd)
    : _pd(pd) 
{
    Init();
}

void Pipeline::Init()
{
    shaderStages.push_back(
        vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, _pd.shaders.vert.module));
    shaderStages.push_back(
        vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, _pd.shaders.frag.module));

    vertexDesc = Vertex::getDescription();
    vertexInputInfo = vkinit::vertex_input_state_create_info(
        uint32_t(vertexDesc.bindings.size()), vertexDesc.bindings.data(),
        uint32_t(vertexDesc.attributes.size()), vertexDesc.attributes.data()
    );

    inputAssembly = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    rasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);

    multisampling = vkinit::multisampling_state_create_info();

    depthStencil = vkinit::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

    colorBlendAttachment = vkinit::color_blend_attachment_state();

    colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
        .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}
    };

    dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    dynamicState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()
    };

    pushConstantRange = {
        .stageFlags = _pd.pushConstantsStages,
        .offset = 0,
        .size = _pd.pushConstantsSize
    };

    pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<uint32_t>(_pd.setLayouts.size()),
        .pSetLayouts = _pd.setLayouts.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };
}

void Pipeline::Build(VkDevice device, VkPipelineRenderingCreateInfoKHR renderingInfo)
{
    VKASSERT(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &layout));


    VkGraphicsPipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderingInfo,
        .stageCount = static_cast<uint32_t>(shaderStages.size()),
        .pStages = shaderStages.data(),
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = layout,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE
    };

    VKASSERT(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));
}

