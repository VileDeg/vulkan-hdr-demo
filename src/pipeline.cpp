#include "stdafx.h"
#include "engine.h"

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

static void s_createComputePipeline(VkDevice device, const std::string& shaderBinName, ComputeStage& cp)
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

static void s_createGraphicsPipeline(
    VkDevice device, 
    const std::string vertBinName, const std::string fragBinName,
    VkShaderStageFlags pushConstantsStages, uint32_t pushConstantsSize,
    std::vector<VkDescriptorSetLayout> setLayouts,
    VkFormat colorFormat, VkFormat depthFormat,
    int cullMode,
    VkPipeline& pipeline, VkPipelineLayout& layout)
{
    PipelineShaders shaders = loadShaders(device, vertBinName, fragBinName);

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages {
        vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, shaders.vert.module),
        vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, shaders.frag.module)
    };

    VertexInputDescription vertexDesc = Vertex::getDescription();

    VkPipelineVertexInputStateCreateInfo vertexInputState = vkinit::vertex_input_state_create_info(
        uint32_t(vertexDesc.bindings.size()), vertexDesc.bindings.data(),
        uint32_t(vertexDesc.attributes.size()), vertexDesc.attributes.data()
    );

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    VkPipelineViewportStateCreateInfo viewportState = vkinit::viewport_state_create_info();

    VkPipelineRasterizationStateCreateInfo rasterizationState = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL, cullMode);

    VkPipelineMultisampleStateCreateInfo multisamplingState = vkinit::multisampling_state_create_info();

    VkPipelineDepthStencilStateCreateInfo depthStencilState = vkinit::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

    VkPipelineColorBlendAttachmentState colorBlendAttachment = vkinit::color_blend_attachment_state();

    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
        .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}
    };

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()
    };

    VkPushConstantRange pushConstantRange = {
        .stageFlags = pushConstantsStages,
        .offset = 0,
        .size = pushConstantsSize
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<uint32_t>(setLayouts.size()),
        .pSetLayouts = setLayouts.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };

    VKASSERT(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &layout));


    VkPipelineRenderingCreateInfo renderingInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &colorFormat,
        .depthAttachmentFormat = depthFormat
    };

    VkGraphicsPipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderingInfo,
        .stageCount = static_cast<uint32_t>(shaderStages.size()),
        .pStages = shaderStages.data(),
        .pVertexInputState = &vertexInputState,
        .pInputAssemblyState = &inputAssemblyState,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizationState,
        .pMultisampleState = &multisamplingState,
        .pDepthStencilState = &depthStencilState,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = layout,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE
    };

    VKASSERT(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));

    shaders.cleanup(device);
}

void Engine::createPipelines()
{
    // Find a suitable depth format
    VkBool32 validDepthFormat = utils::getSupportedDepthFormat(_physicalDevice, &_shadow.depthFormat);
    assert(validDepthFormat);

    {
        VkPipelineLayout layout;
        VkPipeline pipeline;

        s_createGraphicsPipeline(_device,
            "shader.vert.spv", "shader.frag.spv",
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(GPUScenePC),
            { _globalSetLayout, _diffuseTextureSetLayout },
            _viewport.colorFormat, _viewport.depthFormat, 
            VK_CULL_MODE_BACK_BIT,
            pipeline, layout
        );
        createMaterial(pipeline, layout, "general");

        s_createGraphicsPipeline(_device,
            "shader.vert.spv", "shader.frag.spv",
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(GPUScenePC),
            { _globalSetLayout, _diffuseTextureSetLayout },
            _viewport.colorFormat, _viewport.depthFormat,
            VK_CULL_MODE_FRONT_BIT,
            pipeline, layout
        );
        createMaterial(pipeline, layout, "skybox");

        s_createGraphicsPipeline(_device,
            "shadow_pass.vert.spv", "shadow_pass.frag.spv",
            VK_SHADER_STAGE_VERTEX_BIT, sizeof(GPUShadowPC),
            { _shadowSetLayout },
            _shadow.colorFormat, _shadow.depthFormat,
            VK_CULL_MODE_FRONT_BIT,
            pipeline, layout
        );
        createMaterial(pipeline, layout, "shadow");
    }

    { // Compute luminance histogram
        s_createComputePipeline(_device, "histogram.comp.spv"        , _compute.histogram);
        s_createComputePipeline(_device, "average_luminance.comp.spv", _compute.averageLuminance);
        s_createComputePipeline(_device, "bilateral.comp.spv"        , _compute.blur);
        s_createComputePipeline(_device, "tonemap.comp.spv"          , _compute.toneMapping);
    }

    _deletionStack.push([=]() mutable {
        vkDestroyPipelineLayout(_device, _compute.histogram.pipelineLayout, nullptr);
        vkDestroyPipeline(_device, _compute.histogram.pipeline, nullptr);

        vkDestroyPipelineLayout(_device, _compute.averageLuminance.pipelineLayout, nullptr);
        vkDestroyPipeline(_device, _compute.averageLuminance.pipeline, nullptr);

        vkDestroyPipelineLayout(_device, _compute.blur.pipelineLayout, nullptr);
        vkDestroyPipeline(_device, _compute.blur.pipeline, nullptr);

        vkDestroyPipelineLayout(_device, _compute.toneMapping.pipelineLayout, nullptr);
        vkDestroyPipeline(_device, _compute.toneMapping.pipeline, nullptr);
    });
}
