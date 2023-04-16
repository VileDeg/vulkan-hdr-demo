#pragma once

struct PipelineData {
    PipelineShaders shaders;
    std::vector<VkDescriptorSetLayout> setLayouts;
};

struct Pipeline {
    Pipeline(PipelineData pd);

    void Init();
    void Build(VkDevice device, VkRenderPass renderPass);

    PipelineData _pd;

    VertexInputDescription vertexDesc;
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo;
    VkPipelineInputAssemblyStateCreateInfo inputAssembly;
    VkPipelineViewportStateCreateInfo viewportState;
    VkPipelineRasterizationStateCreateInfo rasterizer;
    VkPipelineMultisampleStateCreateInfo multisampling;
    VkPipelineDepthStencilStateCreateInfo depthStencil;
    VkPipelineColorBlendAttachmentState colorBlendAttachment;
    VkPipelineColorBlendStateCreateInfo colorBlending;

    std::vector<VkDynamicState> dynamicStates;
    VkPipelineDynamicStateCreateInfo dynamicState;

    VkPipelineLayout layout;
    VkPushConstantRange pushConstantRange;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    VkPipelineLayoutCreateInfo pipelineLayoutInfo;

    VkPipeline pipeline;
};