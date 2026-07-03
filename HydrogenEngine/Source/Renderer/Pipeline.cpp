#include "Hydrogen/Renderer/Pipeline.hpp"

using namespace Hydrogen;

Pipeline::Pipeline(RenderDevice* device, VkRenderPass renderPass, const std::shared_ptr<ShaderAsset>& vertexShader, const std::shared_ptr<ShaderAsset>& fragmentShader, PipelineSpec spec)
	: m_Device(device), m_Spec(spec)
{
	VkShaderModule vertexShaderModule = CreateShaderModule(vertexShader->GetByteCode());
	VkShaderModule fragmentShaderModule = CreateShaderModule(fragmentShader->GetByteCode());

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertexShaderModule;
	vertShaderStageInfo.pName = spec.VertexMain.c_str();

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragmentShaderModule;
	fragShaderStageInfo.pName = spec.FragmentMain.c_str();

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = CreateVertexInputStateCreateInfo();

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	switch (m_Spec.Primitive)
	{
	case PrimitiveStyle::Lines:
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		break;
	case PrimitiveStyle::Triangles:
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		break;
	default:
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		break;
	}

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)m_Spec.ViewportExtent.width;
	viewport.height = (float)m_Spec.ViewportExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = m_Spec.ViewportExtent;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;

	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;

	switch (spec.CullMode)
	{
	case ShaderCullMode::None:
		rasterizer.cullMode = VK_CULL_MODE_NONE;
		break;
	case ShaderCullMode::Front:
		rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
		break;
	case ShaderCullMode::Back:
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		break;
	default:
		HY_ENGINE_ERROR("Invalid cull mode specified, defaulting to back face culling");
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	}

	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	m_VkPushConstantsRanges.resize(spec.PushConstants.size());
	uint32_t pushConstantRangeOffset = 0;
	for (size_t i = 0; i < spec.PushConstants.size(); i++)
	{
		m_VkPushConstantsRanges[i].offset = pushConstantRangeOffset;
		m_VkPushConstantsRanges[i].size = (uint32_t)spec.PushConstants[i].size;
		m_VkPushConstantsRanges[i].stageFlags = 0;

		if (((uint32_t)spec.PushConstants[i].stageFlags & (uint32_t)ShaderStage::Vertex) == (uint32_t)ShaderStage::Vertex)
		{
			m_VkPushConstantsRanges[i].stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
		}
		if (((uint32_t)spec.PushConstants[i].stageFlags & (uint32_t)ShaderStage::Fragment) == (uint32_t)ShaderStage::Fragment)
		{
			m_VkPushConstantsRanges[i].stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
		}

		pushConstantRangeOffset += (uint32_t)spec.PushConstants[i].size;
	}

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	//pipelineLayoutInfo.setLayoutCount = 1;
	//pipelineLayoutInfo.pSetLayouts = &m_DescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(spec.PushConstants.size());
	pipelineLayoutInfo.pPushConstantRanges = m_VkPushConstantsRanges.data();

	VkResult result = vkCreatePipelineLayout(m_Device->GetVulkanDevice(), &pipelineLayoutInfo, nullptr, &m_Layout);
	if (result != VK_SUCCESS)
	{
		HY_ENGINE_FATAL("Failed to create Vulkan pipeline layout... vkCreatePipelineLayout returned {}", (uint16_t)result);
	}

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = nullptr;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = m_Layout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	result = vkCreateGraphicsPipelines(m_Device->GetVulkanDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline);
	if (result != VK_SUCCESS)
	{
		HY_ENGINE_FATAL("Failed to create Vulkan pipeline... vkCreateGraphicsPipelines returned {}", (uint16_t)result);
	}

	vkDestroyShaderModule(m_Device->GetVulkanDevice(), vertexShaderModule, nullptr);
	vkDestroyShaderModule(m_Device->GetVulkanDevice(), fragmentShaderModule, nullptr);
}

Pipeline::~Pipeline()
{
	vkDestroyPipeline(m_Device->GetVulkanDevice(), m_Pipeline, nullptr);
	vkDestroyPipelineLayout(m_Device->GetVulkanDevice(), m_Layout, nullptr);
}

VkShaderModule Pipeline::CreateShaderModule(const std::vector<uint32_t>& byteCode)
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = byteCode.size() * 4;
	createInfo.pCode = byteCode.data();

	VkShaderModule shaderModule;
	VkResult result = vkCreateShaderModule(m_Device->GetVulkanDevice(), &createInfo, nullptr, &shaderModule);
	if (result != VK_SUCCESS)
	{
		HY_ENGINE_FATAL("Failed to create Vulkan shader module... vkCreateShaderModule returned {}", (uint16_t)result);
	}

	return shaderModule;
}

VkPipelineVertexInputStateCreateInfo Pipeline::CreateVertexInputStateCreateInfo()
{
	if (m_Spec.VertexBufferLayout.size() == 0)
	{
		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		return vertexInputInfo;
	}

	std::vector<VkVertexInputAttributeDescription> attributeDescriptions(m_Spec.VertexBufferLayout.size());

	uint32_t currentOffset = 0;
	for (size_t i = 0; i < attributeDescriptions.size(); i++)
	{
		attributeDescriptions[i].binding = 0;
		attributeDescriptions[i].location = (uint32_t)i;
		attributeDescriptions[i].offset = currentOffset;

		switch (m_Spec.VertexBufferLayout[i].Type)
		{
		case VertexElementType::Float:
			attributeDescriptions[i].format = VK_FORMAT_R32_SFLOAT;
			currentOffset += 4;
			break;
		case VertexElementType::Float2:
			attributeDescriptions[i].format = VK_FORMAT_R32G32_SFLOAT;
			currentOffset += 8;
			break;
		case VertexElementType::Float3:
			attributeDescriptions[i].format = VK_FORMAT_R32G32B32_SFLOAT;
			currentOffset += 12;
			break;
		case VertexElementType::Float4:
			attributeDescriptions[i].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			currentOffset += 16;
			break;
		case VertexElementType::Int:
			attributeDescriptions[i].format = VK_FORMAT_R32_SINT;
			currentOffset += 4;
			break;
		case VertexElementType::Int2:
			attributeDescriptions[i].format = VK_FORMAT_R32G32_SINT;
			currentOffset += 8;
			break;
		case VertexElementType::Int3:
			attributeDescriptions[i].format = VK_FORMAT_R32G32B32_SINT;
			currentOffset += 12;
			break;
		case VertexElementType::Int4:
			attributeDescriptions[i].format = VK_FORMAT_R32G32B32A32_SINT;
			currentOffset += 16;
			break;
		default:
			HY_ASSERT(false, "Invalid Vertex Attribute Type");
		}
	}

	VkVertexInputBindingDescription bindingDescription{};
	bindingDescription.binding = 0;
	bindingDescription.stride = currentOffset;
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	return vertexInputInfo;
}
