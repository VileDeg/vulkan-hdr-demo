#include "stdafx.h"
#include "engine.h"

static PipelineShaders loadShaders(VkDevice device, const std::string& vertName, const std::string& fragName)
{
    PipelineShaders shaders{};

    shaders.vert.code = vk_utils::readShaderBinary(SHADER_PATH + vertName);
    shaders.frag.code = vk_utils::readShaderBinary(SHADER_PATH + fragName);

    if (vk_utils::createShaderModule(device, shaders.vert.code, &shaders.vert.module)) {
        std::cout << "Vertex shader [" << vertName << "] successfully loaded." << std::endl;
    } else {
        PRWRN("Failed to load vertex shader [" << vertName << "]!");
    }

    if (vk_utils::createShaderModule(device, shaders.frag.code, &shaders.frag.module)) {
        std::cout << "Fragment shader [" << fragName << "] successfully loaded." << std::endl;
    } else {
        PRWRN("Failed to load fragment shader [" << fragName << "]!");
    }

    return shaders;
}


void Engine::createGraphicsPipeline(
    const std::string& name, const std::string vertBinName, const std::string fragBinName, 
    VkShaderStageFlags pushConstantsStages, uint32_t pushConstantsSize, 
    std::vector<VkDescriptorSetLayout> setLayouts, 
    VkFormat colorFormat, VkFormat depthFormat, int cullMode)
{
    PipelineShaders shaders = loadShaders(_device, vertBinName, fragBinName);

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

    VkPipelineLayout layout;
    VK_ASSERT(vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &layout));


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

    VkPipeline pipeline;
    VK_ASSERT(vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));
    
    setDebugName(VK_OBJECT_TYPE_PIPELINE, pipeline, name);

    int stages = pushConstantsStages;
    auto& mat = _materials[name] = {
       .tag = name,
       .pipeline = pipeline,
       .pipelineLayout = layout,
       .pushConstantsStages = stages
    };
    
    shaders.cleanup(_device);
    _deletionStack.push([&]() {
        vkDestroyPipeline(_device, mat.pipeline, nullptr);
        vkDestroyPipelineLayout(_device, mat.pipelineLayout, nullptr);
    });
}

void Engine::createPipelines()
{
    // Find a suitable depth format

    {
        createGraphicsPipeline(
            "general",
            "scene.vert.spv", "scene.frag.spv",
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(GPUScenePC),
            { _sceneSetLayout },
            _viewport.colorFormat, _viewport.depthFormat,
            VK_CULL_MODE_BACK_BIT
        );

        createGraphicsPipeline(
            "skybox",
            "skybox.vert.spv", "skybox.frag.spv",
            VK_SHADER_STAGE_VERTEX_BIT, sizeof(GPUScenePC),
            { _sceneSetLayout },
            _viewport.colorFormat, _viewport.depthFormat,
            VK_CULL_MODE_FRONT_BIT
        );

        createGraphicsPipeline(
            "shadow",
            "shadow_pass.vert.spv", "shadow_pass.frag.spv",
            VK_SHADER_STAGE_VERTEX_BIT, sizeof(GPUShadowPC),
            { _shadowSetLayout },
            _shadow.colorFormat, _shadow.depthFormat,
            VK_CULL_MODE_FRONT_BIT
        );
    }

    { // Compute 
        for (auto& stage : _postfx.stages) {
            stage.second.Create(_device, _linearSampler, stage.second.shaderName, stage.second.usesPushConstants);
        }

        _postfx.UpdateStagesDescriptorSets(MAX_FRAMES_IN_FLIGHT, _viewport.imageViews);
    }
    
    _deletionStack.push([=]() mutable {
        for (auto& stage : _postfx.stages) {
            stage.second.Destroy();
        }
    });
}
