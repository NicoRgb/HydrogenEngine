#include "Hydrogen/Platform/Vulkan/VulkanPipeline.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderPass.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanTexture.hpp"
#include "Hydrogen/Core.hpp"

using namespace Hydrogen;

VulkanPipeline::VulkanPipeline(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<RenderPass>& renderPass, const std::shared_ptr<ShaderAsset>& vertexShaderAsset, const std::shared_ptr<ShaderAsset>& fragmentShaderAsset, VertexLayout vertexLayout, const std::vector<DescriptorBinding> descriptorBindings, const std::vector<PushConstantsRange> pushConstantsRanges)
	: m_RenderContext(RenderContext::Get<VulkanRenderContext>(renderContext))
{
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions(vertexLayout.size());

	uint32_t currentOffset = 0;
	for (size_t i = 0; i < attributeDescriptions.size(); i++)
	{
		attributeDescriptions[i].binding = 0;
		attributeDescriptions[i].location = (uint32_t)i;
		attributeDescriptions[i].offset = currentOffset;

		switch (vertexLayout[i].type)
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

	std::vector<VkDescriptorSetLayoutBinding> layoutBindings(descriptorBindings.size());
	for (size_t i = 0; i < layoutBindings.size(); i++)
	{
		layoutBindings[i].binding = descriptorBindings[i].binding;
		switch (descriptorBindings[i].type)
		{
		case DescriptorType::UniformBuffer:
			layoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			break;
		case DescriptorType::CombinedImageSampler:
			layoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			break;
		default:
			HY_ASSERT(false, "Unsupported descriptor type");
		}
		layoutBindings[i].descriptorCount = descriptorBindings[i].num_elements;
		layoutBindings[i].stageFlags = 0;

		if ((descriptorBindings[i].stageFlags & ShaderStage::Vertex) == ShaderStage::Vertex)
		{
			layoutBindings[i].stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
		}
		if ((descriptorBindings[i].stageFlags & ShaderStage::Fragment) == ShaderStage::Fragment)
		{
			layoutBindings[i].stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
		}

		if (descriptorBindings[i].type == DescriptorType::UniformBuffer)
		{
			VkDeviceSize bufferSize = (VkDeviceSize)descriptorBindings[i].size;

			m_UniformBuffersMapped[descriptorBindings[i].binding].resize(m_RenderContext->GetMaxFramesInFlight());

			for (size_t j = 0; j < m_RenderContext->GetMaxFramesInFlight(); j++)
			{
				m_UniformBuffers[descriptorBindings[i].binding].emplace_back(m_RenderContext, bufferSize,
					VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
				m_UniformBuffers[descriptorBindings[i].binding][j].AllocateMemory();

				vkMapMemory(m_RenderContext->GetDevice(), m_UniformBuffers[descriptorBindings[i].binding][j].GetBufferMemory(), 0, bufferSize, 0, &m_UniformBuffersMapped[descriptorBindings[i].binding][j]);
			}
		}
	}

	VkShaderModule vertexShaderModule = CreateShaderModule(vertexShaderAsset->GetByteCode());
	VkShaderModule fragmentShaderModule = CreateShaderModule(fragmentShaderAsset->GetByteCode());

	VkPipelineShaderStageCreateInfo vertexShaderStageInfo{};
	vertexShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderStageInfo.module = vertexShaderModule;
	vertexShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragmentShaderStageInfo{};
	fragmentShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderStageInfo.module = fragmentShaderModule;
	fragmentShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertexShaderStageInfo, fragmentShaderStageInfo };

	const std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)m_RenderContext->GetSwapChainExtent().width;
	viewport.height = (float)m_RenderContext->GetSwapChainExtent().height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = m_RenderContext->GetSwapChainExtent();

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
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
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
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
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

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = 0.0f;
	depthStencil.maxDepthBounds = 1.0f;
	depthStencil.stencilTestEnable = VK_FALSE;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
	layoutInfo.pBindings = layoutBindings.data();

	if (vkCreateDescriptorSetLayout(m_RenderContext->GetDevice(), &layoutInfo, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}

	uint32_t uniformBufferCount = 0;
	uint32_t combinedImageSamplerCount = 0;
	for (const auto& binding : descriptorBindings)
	{
		switch (binding.type)
		{
		case DescriptorType::UniformBuffer:
			uniformBufferCount += binding.num_elements;
			break;
		case DescriptorType::CombinedImageSampler:
			combinedImageSamplerCount += binding.num_elements;
			break;
		default:
			HY_ASSERT(false, "Unsupported descriptor type");
		}
	}

	std::vector<VkDescriptorPoolSize> poolSizes;
	if (uniformBufferCount > 0)
	{
		VkDescriptorPoolSize uboPool{};
		uboPool.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboPool.descriptorCount = uniformBufferCount * m_RenderContext->GetMaxFramesInFlight();
		poolSizes.push_back(uboPool);
	}
	if (combinedImageSamplerCount > 0)
	{
		VkDescriptorPoolSize samplerPool{};
		samplerPool.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerPool.descriptorCount = combinedImageSamplerCount * m_RenderContext->GetMaxFramesInFlight();
		poolSizes.push_back(samplerPool);
	}

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = static_cast<uint32_t>(m_RenderContext->GetMaxFramesInFlight());

	HY_ASSERT(vkCreateDescriptorPool(m_RenderContext->GetDevice(), &poolInfo, nullptr, &m_DescriptorPool) == VK_SUCCESS, "Failed to create vulkan descriptor pool");

	std::vector<VkDescriptorSetLayout> layouts(m_RenderContext->GetMaxFramesInFlight(), m_DescriptorSetLayout);
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_DescriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(m_RenderContext->GetMaxFramesInFlight());
	allocInfo.pSetLayouts = layouts.data();

	m_DescriptorSets.resize(m_RenderContext->GetMaxFramesInFlight());
	HY_ASSERT(vkAllocateDescriptorSets(m_RenderContext->GetDevice(), &allocInfo, m_DescriptorSets.data()) == VK_SUCCESS, "Failed to allocate vulkan descriptor sets");

	for (size_t i = 0; i < m_RenderContext->GetMaxFramesInFlight(); i++)
	{
		size_t offset = 0;
		for (size_t j = 0; j < layoutBindings.size(); j++)
		{
			switch (descriptorBindings[j].type)
			{
			case DescriptorType::UniformBuffer:
			{
				VkDescriptorBufferInfo bufferInfo{};
				bufferInfo.buffer = m_UniformBuffers[layoutBindings[j].binding][i].GetBuffer();
				bufferInfo.offset = offset;
				bufferInfo.range = m_UniformBuffers[layoutBindings[j].binding][i].GetSize();
				offset += m_UniformBuffers[layoutBindings[j].binding][i].GetSize();

				VkWriteDescriptorSet descriptorWrite{};
				descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrite.dstSet = m_DescriptorSets[i];
				descriptorWrite.dstBinding = layoutBindings[j].binding;
				descriptorWrite.dstArrayElement = 0;
				descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				descriptorWrite.descriptorCount = 1;
				descriptorWrite.pBufferInfo = &bufferInfo;

				vkUpdateDescriptorSets(m_RenderContext->GetDevice(), 1, &descriptorWrite, 0, nullptr);
				break;
			}

			default:
				break;
			}
		}
	}

	m_VkPushConstantsRanges.resize(pushConstantsRanges.size());
	uint32_t pushConstantRangeOffset = 0;
	for (size_t i = 0; i < pushConstantsRanges.size(); i++)
	{
		m_VkPushConstantsRanges[i].offset = pushConstantRangeOffset;
		m_VkPushConstantsRanges[i].size = pushConstantsRanges[i].size;
		m_VkPushConstantsRanges[i].stageFlags = 0;

		if ((pushConstantsRanges[i].stageFlags & ShaderStage::Vertex) == ShaderStage::Vertex)
		{
			m_VkPushConstantsRanges[i].stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
		}
		if ((pushConstantsRanges[i].stageFlags & ShaderStage::Fragment) == ShaderStage::Fragment)
		{
			m_VkPushConstantsRanges[i].stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
		}

		pushConstantRangeOffset += pushConstantsRanges[i].size;
	}

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &m_DescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantsRanges.size());
	pipelineLayoutInfo.pPushConstantRanges = m_VkPushConstantsRanges.data();

	HY_ASSERT(vkCreatePipelineLayout(m_RenderContext->GetDevice(), &pipelineLayoutInfo, nullptr, &m_PipelineLayout) == VK_SUCCESS, "Failed to create vulkan pipeline layout");

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = m_PipelineLayout;
	pipelineInfo.renderPass = RenderPass::Get<VulkanRenderPass>(renderPass)->GetRenderPass();
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	HY_ASSERT(vkCreateGraphicsPipelines(m_RenderContext->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline) == VK_SUCCESS, "Failed to create vulkan graphics pipeline");

	vkDestroyShaderModule(m_RenderContext->GetDevice(), vertexShaderModule, nullptr);
	vkDestroyShaderModule(m_RenderContext->GetDevice(), fragmentShaderModule, nullptr);
}

VulkanPipeline::~VulkanPipeline()
{
	vkDestroyDescriptorPool(m_RenderContext->GetDevice(), m_DescriptorPool, nullptr);

	vkDestroyDescriptorSetLayout(m_RenderContext->GetDevice(), m_DescriptorSetLayout, nullptr);
	vkDestroyPipeline(m_RenderContext->GetDevice(), m_Pipeline, nullptr);
	vkDestroyPipelineLayout(m_RenderContext->GetDevice(), m_PipelineLayout, nullptr);
}

void VulkanPipeline::UploadUniformBufferData(uint32_t binding, void* data, size_t size)
{
	memcpy(m_UniformBuffersMapped[binding][m_RenderContext->GetCurrentFrame()], data, size);
}

void VulkanPipeline::UploadTextureSampler(uint32_t binding, uint32_t index, const std::shared_ptr<Texture>& texture)
{
	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = Texture::Get<VulkanTexture>(texture)->GetImageView();
	imageInfo.sampler = Texture::Get<VulkanTexture>(texture)->GetSampler();

	for (size_t i = 0; i < m_RenderContext->GetMaxFramesInFlight(); i++)
	{
		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = m_DescriptorSets[i];
		descriptorWrite.dstBinding = binding;
		descriptorWrite.dstArrayElement = index;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(m_RenderContext->GetDevice(), 1, &descriptorWrite, 0, nullptr);
	}
}

VkShaderModule VulkanPipeline::CreateShaderModule(const std::vector<uint32_t>& code) const
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size() * 4;
	createInfo.pCode = code.data();

	VkShaderModule shaderModule = VK_NULL_HANDLE;
	HY_ASSERT(vkCreateShaderModule(m_RenderContext->GetDevice(), &createInfo, nullptr, &shaderModule) == VK_SUCCESS, "Failed to create vulkan shader module");
	return shaderModule;
}
