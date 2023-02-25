#include "stdafx.h"
#include "Enigne.h"

#include "vk_pipeline_builder.h"

static constexpr const char* SHADER_PATH = "../assets/shaders/bin/";

PipelineShaders Engine::loadShaders(const std::string& vertName, const std::string& fragName)
{
    PipelineShaders shaders{};

    shaders.vert.code = readShaderBinary(SHADER_PATH + vertName);
    shaders.frag.code = readShaderBinary(SHADER_PATH + fragName);

    if (createShaderModule(shaders.vert.code, &shaders.vert.module)) {
        std::cout << "Vertex shader successfully loaded." << std::endl;
    } else {
        PRWRN("Failed to load vertex shader");
    }
    
    if (createShaderModule(shaders.frag.code, &shaders.frag.module)) {
        std::cout << "Fragment shader successfully loaded." << std::endl;
    }
    else {
        PRWRN("Failed to load fragment shader");
    }

    return shaders;
}

//void Engine::cleanupShaders(PipelineShaders& shaders)
//{
//    vkDestroyShaderModule(_device, shaders.vert.module, nullptr);
//    vkDestroyShaderModule(_device, shaders.frag.module, nullptr);
//}

void Engine::createGraphicsPipeline()
{
    auto shaders = loadShaders("tri_mesh.vert.spv", "shader.frag.spv");

    VertexInputDescription description = Vertex::getDescription();

    /*PipelineBuilder builder{
        ._shaderStages = {
            vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT,
                shaders.vert.module),
            vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT,
                shaders.frag.module)
        },
        ._vertexInputInfo = vkinit::vertex_input_state_create_info(
            description.bindings.size(), description.bindings.data(), 
            description.attributes.size(), description.attributes.data()),
        ._inputAssembly = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST),
        ._viewport = 

    };*/

    VkPipelineShaderStageCreateInfo shaderStages[] = { 
        vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, 
            shaders.vert.module), 
        vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT,
            shaders.frag.module) 
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = vkinit::vertex_input_state_create_info(
        uint32_t(description.bindings.size()), description.bindings.data(),
        uint32_t(description.attributes.size()), description.attributes.data()
    );

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    VkPipelineViewportStateCreateInfo viewportState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);

    VkPipelineMultisampleStateCreateInfo multisampling = vkinit::multisampling_state_create_info();

    VkPipelineColorBlendAttachmentState colorBlendAttachment = vkinit::color_blend_attachment_state();

    VkPipelineColorBlendStateCreateInfo colorBlending{
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

    VkPipelineDynamicStateCreateInfo dynamicState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()
    };  

    {
        VkPushConstantRange pushConstantRange{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = sizeof(MeshPushConstants)
        };

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkinit::pipeline_layout_create_info();
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        VKASSERT(vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &_pipelineLayout));
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = _pipelineLayout,
        .renderPass = _renderPass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE
    };

    VKASSERT(vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_graphicsPipeline));

    //cleanupShaders(shaders);
    shaders.cleanup(_device);
}

void Engine::cleanupPipeline()
{
    vkDestroyPipeline(_device, _graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);
}
