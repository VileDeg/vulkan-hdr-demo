#include "stdafx.h"
#include "Engine.h"

#include "shader.h"
#include "pipeline.h"

static PipelineShaders loadShaders(VkDevice device, const std::string& vertName, const std::string& fragName)
{
    PipelineShaders shaders{};

    shaders.vert.code = readShaderBinary(Engine::shaderPath + vertName);
    shaders.frag.code = readShaderBinary(Engine::shaderPath + fragName);

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

void Engine::createPipelines()
{
    PipelineData pd_general{
        .shaders = loadShaders(_device, "shader.vert.spv", "shader.frag.spv"),
        .setLayouts = { _globalSetLayout, _objectSetLayout, _singleTextureSetLayout }
    };

    /*PipelineData pd_color_light{
        .shaders = loadShaders(_device, "shader.vert.spv", "color_light.frag.spv"),
        .setLayouts = { _globalSetLayout, _objectSetLayout }
    };

    PipelineData pd_color_no_light{
        .shaders = loadShaders(_device, "shader.vert.spv", "color_no_light.frag.spv"),
        .setLayouts = { _globalSetLayout, _objectSetLayout }
    };*/

    auto newMaterial = [&](std::string matName, PipelineData pd) {
        Pipeline pipeline(pd);
        pipeline.Init();
        pipeline.Build(_device, _mainRenderpass);

        createMaterial(pipeline.pipeline, pipeline.layout, matName);

        pd.shaders.cleanup(_device);
    };

    newMaterial("general", pd_general);

    _mainPipeline = _materials["general"].pipeline;
    /*newMaterial("color_light", pd_color_light);
    newMaterial("color_no_light", pd_color_no_light);*/
}

Pipeline::Pipeline(PipelineData pd)
    : _pd(pd) {}

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
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(GPUPushConstantData)
    };

    pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<uint32_t>(_pd.setLayouts.size()),
        .pSetLayouts = _pd.setLayouts.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };
}

void Pipeline::Build(VkDevice device, VkRenderPass renderPass)
{
    VKASSERT(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &layout));

    VkGraphicsPipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
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
        .renderPass = renderPass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE
    };

    VKASSERT(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));
}

