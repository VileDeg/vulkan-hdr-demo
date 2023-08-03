#include "stdafx.h"
#include "engine.h"


static PipelineShaders loadShaders(VkDevice device, const std::string& vertName, const std::string& fragName)
{
    PipelineShaders shaders{};

    shaders.vert.code = utils::readShaderBinary(Engine::SHADER_PATH + vertName);
    shaders.frag.code = utils::readShaderBinary(Engine::SHADER_PATH + fragName);

    if (utils::createShaderModule(device, shaders.vert.code, &shaders.vert.module)) {
        std::cout << "Vertex shader successfully loaded." << std::endl;
    } else {
        PRWRN("Failed to load vertex shader");
    }

    if (utils::createShaderModule(device, shaders.frag.code, &shaders.frag.module)) {
        std::cout << "Fragment shader successfully loaded." << std::endl;
    } else {
        PRWRN("Failed to load fragment shader");
    }

    return shaders;
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

    { // Compute 
        _compute.histogram.Create(_device, _linearSampler, "histogram.comp.spv");
        _compute.averageLuminance.Create(_device, _linearSampler, "average_luminance.comp.spv");

        _compute.durand.stages[0].Create(_device, _linearSampler, "ltm_durand_lum_chrom.comp.spv");
        _compute.durand.stages[1].Create(_device, _linearSampler, "ltm_durand_bilateral_base.comp.spv");
        _compute.durand.stages[2].Create(_device, _linearSampler, "ltm_durand_reconstruct.comp.spv");

        for (auto& stage : _compute.fusion.stages) {
            stage.second.Create(_device, _linearSampler, stage.second.shaderName, stage.second.usesPushConstants);
        }

        /*_compute.fusion.stages["0"].Create(_device, _linearSampler, "ltm_fusion_0.comp.spv");
        _compute.fusion.stages["1"].Create(_device, _linearSampler, "ltm_fusion_1.comp.spv", true);
        _compute.fusion.stages["2"].Create(_device, _linearSampler, "ltm_fusion_2.comp.spv");
        _compute.fusion.stages["3"].Create(_device, _linearSampler, "ltm_fusion_3.comp.spv");
        _compute.fusion.stages["4"].Create(_device, _linearSampler, "ltm_fusion_4.comp.spv");

        _compute.fusion.stages["upsample0"].Create(_device, _linearSampler, "upsample0.comp.spv", true);
        _compute.fusion.stages["upsample1"].Create(_device, _linearSampler, "upsample1.comp.spv", true);
        _compute.fusion.stages["upsample2"].Create(_device, _linearSampler, "upsample2.comp.spv", true);

        _compute.fusion.stages["add"].Create(_device, _linearSampler, "mipmap_add.comp.spv", true);*/
        
        _compute.toneMapping.Create(_device, _linearSampler, "tonemap.comp.spv");
    }

    _deletionStack.push([=]() mutable {
        _compute.histogram.Destroy();
        _compute.averageLuminance.Destroy();

        for (auto& stage : _compute.durand.stages) {
            stage.Destroy();
        }

        for (auto& stage : _compute.fusion.stages) {
            stage.second.Destroy();
        }

        _compute.toneMapping.Destroy();

        /*_compute.fusion.upsample0.Destroy();
        _compute.fusion.upsample1.Destroy();
        _compute.fusion.upsample2.Destroy();*/
    });
}
