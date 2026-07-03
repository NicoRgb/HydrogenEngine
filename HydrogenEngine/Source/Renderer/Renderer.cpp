#include "Hydrogen/Renderer/Renderer.hpp"
#include "Hydrogen/Application.hpp"

#include <backends/imgui_impl_vulkan.h>

using namespace Hydrogen;

ImGuiTextureCache g_ImGuiTextureCache;
ImVec2 g_OldViewportContentRegion = ImVec2(1920, 1080);
ImVec2 g_ViewportContentRegion = ImVec2(1920, 1080);

std::unique_ptr<RenderBuffer> g_VertexBuffer;
std::unique_ptr<RenderBuffer> g_UniformBuffer;
std::unique_ptr<RenderBuffer> g_PassUniformBuffer;

Renderer::Renderer(const std::shared_ptr<Viewport>& viewport, RenderDevice* device, SwapChain* swapChain)
	: m_Viewport(viewport), m_Device(device), m_SwapChain(swapChain)
{
	m_RenderGraph = std::make_unique<RenderGraph>(device);

	CreateCommandBuffer();
	CreateSyncObjects();
	InitImGui();

	VkSamplerCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	createInfo.magFilter = VK_FILTER_LINEAR;
	createInfo.minFilter = VK_FILTER_LINEAR;

	createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	createInfo.anisotropyEnable = VK_FALSE;
	createInfo.maxAnisotropy = 1.0f;
	createInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	createInfo.unnormalizedCoordinates = VK_FALSE;

	createInfo.compareEnable = VK_FALSE;
	createInfo.compareOp = VK_COMPARE_OP_ALWAYS;

	createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	createInfo.mipLodBias = 0.0f;
	createInfo.minLod = 0.0f;
	createInfo.maxLod = 1.0f;

	VkResult result = vkCreateSampler(m_Device->GetVulkanDevice(), &createInfo, nullptr, &m_ImguiSampler);
	if (result != VK_SUCCESS)
	{
		HY_ENGINE_FATAL("Failed to create Vulkan sampler... vkCreateSampler returned {}", (uint16_t)result);
	}

	const std::vector<float> vertices = {
		0.0f, -0.5f, 1.0f, 1.0f, 1.0f,
		0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
		-0.5f, 0.5f, 0.0f, 0.0f, 1.0f
	};

	BufferDescription desc = { vertices.size() * sizeof(float), BufferType::Vertex, false };
	g_VertexBuffer = std::make_unique<RenderBuffer>(m_Device, desc);
	g_VertexBuffer->UploadDataStaging(vertices.data(), vertices.size() * sizeof(float));

	desc = { sizeof(glm::vec4), BufferType::Uniform, true, true };
	g_UniformBuffer = std::make_unique<RenderBuffer>(m_Device, desc);

	glm::vec4 color = glm::vec4(0, 255, 0, 255);
	g_UniformBuffer->UploadData(&color, sizeof(glm::vec4), 0);

	g_PassUniformBuffer = std::make_unique<RenderBuffer>(m_Device, desc);

	color = glm::vec4(255, 0, 0, 255);
	g_PassUniformBuffer->UploadData(&color, sizeof(glm::vec4), 0);
}

Renderer::~Renderer()
{
	m_Device->WaitForIdle();

	g_VertexBuffer.reset();
	g_UniformBuffer.reset();
	g_PassUniformBuffer.reset();

	vkDestroySampler(m_Device->GetVulkanDevice(), m_ImguiSampler, nullptr);
	g_ImGuiTextureCache.Clear();

	m_Viewport->ImGuiShutdown();
	ImGui_ImplVulkan_Shutdown();

	vkDestroyDescriptorPool(m_Device->GetVulkanDevice(), m_ImGuiDescriptorPool, nullptr);

	vkDestroySemaphore(m_Device->GetVulkanDevice(), m_ImageAvailableSemaphore, nullptr);
	vkDestroySemaphore(m_Device->GetVulkanDevice(), m_PresentFinishedSemaphore, nullptr);
	vkDestroyFence(m_Device->GetVulkanDevice(), m_WaitFence, nullptr);

	vkFreeCommandBuffers(m_Device->GetVulkanDevice(), m_Device->GetCommandPool(), 1, &m_CommandBuffer);
}

void Renderer::BeginImGuiFrame()
{
	ImGui_ImplVulkan_NewFrame();
	m_Viewport->ImGuiNewFrame();
	ImGui::NewFrame();
}

void Renderer::Render()
{
	vkWaitForFences(m_Device->GetVulkanDevice(), 1, &m_WaitFence, VK_TRUE, UINT64_MAX);
	vkResetFences(m_Device->GetVulkanDevice(), 1, &m_WaitFence);

	m_RenderGraph->Reset();

	if (g_ViewportContentRegion.x != g_OldViewportContentRegion.x || g_ViewportContentRegion.y != g_OldViewportContentRegion.y)
	{
		g_OldViewportContentRegion = g_ViewportContentRegion;
		ClearCache();
	}

	RgTextureDesc finalSceneDesc = {};
	finalSceneDesc.Width = (uint32_t)g_ViewportContentRegion.x;
	finalSceneDesc.Height = (uint32_t)g_ViewportContentRegion.y;
	finalSceneDesc.Format = TextureFormat::RGBA8_SRGB;
	auto finalTexture = m_RenderGraph->CreateTexture(finalSceneDesc);

	auto swapChainImage = m_SwapChain->AcquireNextImage(m_RenderGraph.get(), m_ImageAvailableSemaphore);

	auto vertexShader = Application::Get()->MainAssetManager.GetAsset<ShaderAsset>("TriangleVertexShader.glsl");
	auto fragmentShader = Application::Get()->MainAssetManager.GetAsset<ShaderAsset>("TriangleFragmentShader.glsl");

	VkSampler viewportSampler = m_ImguiSampler;

	m_RenderGraph->AddPass("Triangle", { { 0, DescriptorType::UniformBuffer, 1, ShaderStage::Fragment } },
		[finalTexture](RgPassBuilder& builder)
		{
			builder.WriteColor(finalTexture);
		},
		[vertexShader, fragmentShader](RgCommandList& cmd)
		{
			DescriptorBindingValue value;
			value.Type = DescriptorType::UniformBuffer;
			value.RenderBuffers.push_back(g_PassUniformBuffer.get());
			cmd.UpdateDescriptorSet({ value });

			PipelineSpec trianglePipeline = {};
			trianglePipeline.VertexBufferLayout = { { VertexElementType::Float2 }, { VertexElementType::Float3 } };
			trianglePipeline.ColorBlending = { BlendMode::None };

			cmd.BindPipeline(vertexShader, fragmentShader, trianglePipeline);
			cmd.BindVertexBuffer(g_VertexBuffer.get());
			cmd.Draw(3);
		});

	m_RenderGraph->AddPass("ImGui", {},
		[finalTexture, swapChainImage](RgPassBuilder& builder)
		{
			builder.WriteColor(swapChainImage);
			builder.ReadTexture(finalTexture);
		},
		[viewportSampler, finalSceneDesc, finalTexture, vertexShader, fragmentShader](RgCommandList& cmd)
		{
			ImGui::Begin("Viewport");
			g_ViewportContentRegion = ImGui::GetContentRegionAvail();
			ImGui::Image(g_ImGuiTextureCache.GetTextureID(cmd.GetTextureView(finalTexture).ImageView, viewportSampler), ImVec2((float)finalSceneDesc.Width, (float)finalSceneDesc.Height));
			ImGui::End();

			ImGui::Render();
			ImDrawData* drawData = ImGui::GetDrawData();

			ImGui_ImplVulkan_RenderDrawData(drawData, cmd.GetCommandBuffer());
		});

	m_RenderGraph->Compile({ { 0, DescriptorType::UniformBuffer, 1, ShaderStage::Fragment } });

	vkResetCommandBuffer(m_CommandBuffer, 0);
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0;
	beginInfo.pInheritanceInfo = nullptr;

	VkResult result = vkBeginCommandBuffer(m_CommandBuffer, &beginInfo);
	if (result != VK_SUCCESS)
	{
		HY_ENGINE_FATAL("Failed to begin Vulkan command buffer... vkBeginCommandBuffer returned {}", (uint16_t)result);
	}

	DescriptorBindingValue value;
	value.Type = DescriptorType::UniformBuffer;
	value.RenderBuffers.push_back(g_UniformBuffer.get());

	m_RenderGraph->Execute(m_CommandBuffer, { value });

	result = vkEndCommandBuffer(m_CommandBuffer);
	if (result != VK_SUCCESS)
	{
		HY_ENGINE_FATAL("Failed to end Vulkan command buffer... vkEndCommandBuffer returned {}", (uint16_t)result);
	}

	VkCommandBuffer commandBuffers[] = { m_CommandBuffer };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &m_ImageAvailableSemaphore;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.pCommandBuffers = commandBuffers;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &m_PresentFinishedSemaphore;

	vkQueueSubmit(m_Device->GetGraphicsQueue(), 1, &submitInfo, m_WaitFence);

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &m_PresentFinishedSemaphore;
	
	VkSwapchainKHR swapChains[] = { m_SwapChain->GetVulkanSwapchain() };
	uint32_t imageIndices[] = { m_SwapChain->GetCurrentImageIndex() };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = imageIndices;
	vkQueuePresentKHR(m_Device->GetPresentQueue(), &presentInfo);
}

void Renderer::UpdateSwapChain(SwapChain* swapChain)
{
	m_SwapChain = swapChain;
	ClearCache();
}

void Renderer::ClearCache()
{
	g_ImGuiTextureCache.Clear();
	m_RenderGraph->ClearCache();
}

void Renderer::CreateCommandBuffer()
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_Device->GetCommandPool();
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	VkResult result = vkAllocateCommandBuffers(m_Device->GetVulkanDevice(), &allocInfo, &m_CommandBuffer);
	if (result != VK_SUCCESS)
	{
		HY_ENGINE_FATAL("Failed to allocate Vulkan command buffer... vkAllocateCommandBuffers returned {}", (uint16_t)result);
	}
}

void Renderer::CreateSyncObjects()
{
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkResult result = vkCreateSemaphore(m_Device->GetVulkanDevice(), &semaphoreInfo, nullptr, &m_ImageAvailableSemaphore);
	if (result != VK_SUCCESS)
	{
		HY_ENGINE_FATAL("Failed to create Vulkan semaphore... vkCreateSemaphore returned {}", (uint16_t)result);
	}

	result = vkCreateSemaphore(m_Device->GetVulkanDevice(), &semaphoreInfo, nullptr, &m_PresentFinishedSemaphore);
	if (result != VK_SUCCESS)
	{
		HY_ENGINE_FATAL("Failed to create Vulkan semaphore... vkCreateSemaphore returned {}", (uint16_t)result);
	}

	result = vkCreateFence(m_Device->GetVulkanDevice(), &fenceInfo, nullptr, &m_WaitFence);
	if (result != VK_SUCCESS)
	{
		HY_ENGINE_FATAL("Failed to create Vulkan fence... vkCreateFence returned {}", (uint16_t)result);
	}
}

void Renderer::InitImGui()
{
	VkDescriptorPoolSize poolSizes[] = {
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

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolInfo.maxSets = 1000 * IM_ARRAYSIZE(poolSizes);
	poolInfo.poolSizeCount = (uint32_t)IM_ARRAYSIZE(poolSizes);
	poolInfo.pPoolSizes = poolSizes;

	VkResult result = vkCreateDescriptorPool(m_Device->GetVulkanDevice(), &poolInfo, nullptr, &m_ImGuiDescriptorPool);
	if (result != VK_SUCCESS)
	{
		HY_ENGINE_FATAL("Failed to create Vulkan descriptor pool... vkCreateDescriptorPool returned {}", (uint16_t)result);
	}

	VkAttachmentDescription attachment{};
	attachment.format = m_SwapChain->GetVulkanImageFormat();
	attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &attachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	VkRenderPass bootstrapRenderPass;
	result = vkCreateRenderPass(m_Device->GetVulkanDevice(), &renderPassInfo, nullptr, &bootstrapRenderPass);
	if (result != VK_SUCCESS)
	{
		HY_ENGINE_FATAL("Failed to create Vulkan render pass... vkCreateRenderPass returned {}", (uint16_t)result);
	}

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	m_Viewport->InitImGui();

	ImGui_ImplVulkan_InitInfo initInfo = {};
	initInfo.Instance = RenderInstance::Get()->GetVulkanInstance();
	initInfo.PhysicalDevice = m_Device->GetVulkanPhysicalDevice();
	initInfo.Device = m_Device->GetVulkanDevice();
	initInfo.QueueFamily = m_Device->GetGraphicsFamilyIndex();
	initInfo.Queue = m_Device->GetGraphicsQueue();
	initInfo.PipelineCache = VK_NULL_HANDLE;
	initInfo.DescriptorPool = m_ImGuiDescriptorPool;
	initInfo.Subpass = 0;
	initInfo.MinImageCount = 2;
	initInfo.ImageCount = 3;
	initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	initInfo.Allocator = nullptr;
	initInfo.RenderPass = bootstrapRenderPass;
	initInfo.Subpass = 0;
	initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	initInfo.CheckVkResultFn = [](VkResult result) {
		if (result != VK_SUCCESS)
		{
			HY_ENGINE_FATAL("Failed to validate Vulkan command.", (uint16_t)result);
		}
		};

	if (!ImGui_ImplVulkan_Init(&initInfo))
	{
		HY_ENGINE_FATAL("Failed to initialize ImGui Vulkan implementation!");
	}

	vkDestroyRenderPass(m_Device->GetVulkanDevice(), bootstrapRenderPass, nullptr);
}
