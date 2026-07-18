#include "Hydrogen/Renderer/Renderer.hpp"
#include "Hydrogen/Application.hpp"
#include "Hydrogen/ProceduralMesh.hpp"

#include <backends/imgui_impl_vulkan.h>

using namespace Hydrogen;

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
}

Renderer::Renderer(RenderDevice* device)
	: m_Viewport(nullptr), m_Device(device), m_SwapChain(nullptr)
{
	m_RenderGraph = std::make_unique<RenderGraph>(device);

	CreateCommandBuffer();
	CreateSyncObjects();

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
}

Renderer::~Renderer()
{
	m_Device->WaitForIdle();

	vkDestroySampler(m_Device->GetVulkanDevice(), m_ImguiSampler, nullptr);

	if (m_Viewport)
	{
		m_Viewport->ImGuiShutdown();
		ImGui_ImplVulkan_Shutdown();
	}

	vkDestroyDescriptorPool(m_Device->GetVulkanDevice(), m_ImGuiDescriptorPool, nullptr);

	vkDestroySemaphore(m_Device->GetVulkanDevice(), m_ImageAvailableSemaphore, nullptr);
	vkDestroySemaphore(m_Device->GetVulkanDevice(), m_PresentFinishedSemaphore, nullptr);
	vkDestroyFence(m_Device->GetVulkanDevice(), m_WaitFence, nullptr);

	vkFreeCommandBuffers(m_Device->GetVulkanDevice(), m_Device->GetCommandPool(), 1, &m_CommandBuffer);
}

void Renderer::BeginImGuiFrame()
{
	HY_ASSERT(m_Viewport, "Renderer initialized without viewport does not have imgui capabilities");

	ImGui_ImplVulkan_NewFrame();
	m_Viewport->ImGuiNewFrame();
	ImGui::NewFrame();
}

std::vector<RgTextureView> Renderer::Render(const std::function<const std::vector<DescriptorBindingValue>(RenderGraph* graph)>& setupPasses, bool present)
{
	if (present)
	{
		HY_ASSERT(m_SwapChain, "Renderer initialized without swapchain does not have present capabilities");
	}

	vkWaitForFences(m_Device->GetVulkanDevice(), 1, &m_WaitFence, VK_TRUE, UINT64_MAX);
	vkResetFences(m_Device->GetVulkanDevice(), 1, &m_WaitFence);

	m_RenderGraph->Reset();

	const auto descriptorBindingValues = setupPasses(m_RenderGraph.get());

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

	m_RenderGraph->Execute(m_CommandBuffer, descriptorBindingValues);

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
	if (present)
	{
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &m_ImageAvailableSemaphore;
	}
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.pCommandBuffers = commandBuffers;

	if (present)
	{
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &m_PresentFinishedSemaphore;
	}

	vkQueueSubmit(m_Device->GetGraphicsQueue(), 1, &submitInfo, m_WaitFence);

	if (!present)
	{
		return m_RenderGraph->GetOutputs();
	}

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

	return m_RenderGraph->GetOutputs();
}

void Renderer::UpdateSwapChain(SwapChain* swapChain)
{
	m_SwapChain = swapChain;
	ClearCache();
}

void Renderer::ClearCache()
{
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

std::unique_ptr<RenderBuffer> DefaultRenderer::s_SphereVertexBuffer;
std::unique_ptr<RenderBuffer> DefaultRenderer::s_SphereIndexBuffer;

struct UniformBuffer
{
	glm::mat4 View;
	glm::mat4 Proj;
	glm::vec3 ViewPos;
	float Padding;
};

struct GeometryPassPushConstants
{
	glm::mat4 Model;

	int32_t AlbedoIndex;
	int32_t NormalIndex;
	int32_t ORMIndex;
	int32_t EmissiveIndex;

	glm::vec4 Tint;

	float Roughness;
	float Metallic;
	float Padding0;
	float Padding1;

	glm::vec4 Emissive;
};

struct LightingPassPushConstants
{
	alignas(16) glm::mat4 Model;
	alignas(16) glm::vec3 Color;
	float Intensity;
	alignas(16) glm::vec3 Position;
	float Radius;
};

RgTextureView DefaultRenderer::RenderSceneDeferred(Renderer* renderer, RenderSettings settings, const CameraComponent& camera, glm::vec3 cameraPos, Scene* scene)
{
	if (!s_SphereVertexBuffer)
	{
		auto sphereData = GenerateUVSphere(16, 16);

		BufferDescription vertexBufferDesc;
		vertexBufferDesc.cpuVisible = false;
		vertexBufferDesc.size = sphereData.Vertices.size() * sizeof(float);
		vertexBufferDesc.type = BufferType::Vertex;

		s_SphereVertexBuffer = std::make_unique<RenderBuffer>(Application::Get()->GetRenderDevice(), vertexBufferDesc);
		s_SphereVertexBuffer->UploadDataStaging((void*)sphereData.Vertices.data(), vertexBufferDesc.size);

		BufferDescription indexBufferDesc;
		indexBufferDesc.cpuVisible = false;
		indexBufferDesc.size = sphereData.Indices.size() * sizeof(uint32_t);
		indexBufferDesc.type = BufferType::Index;

		s_SphereIndexBuffer = std::make_unique<RenderBuffer>(Application::Get()->GetRenderDevice(), indexBufferDesc);
		s_SphereIndexBuffer->UploadDataStaging((void*)sphereData.Indices.data(), indexBufferDesc.size);
	}

	std::vector<const Texture*> AlbedoTextures;
	std::vector<const Texture*> NormalTextures;
	std::vector<const Texture*> ORMTextures;
	std::vector<const Texture*> EmissiveTextures;

	UploadMaterialTextures(scene, AlbedoTextures, NormalTextures, ORMTextures, EmissiveTextures);

	std::vector<const Texture*> Textures;
	Textures.reserve(AlbedoTextures.size() + NormalTextures.size() + ORMTextures.size() + EmissiveTextures.size());
	Textures.insert(Textures.end(), AlbedoTextures.begin(), AlbedoTextures.end());
	Textures.insert(Textures.end(), NormalTextures.begin(), NormalTextures.end());
	Textures.insert(Textures.end(), ORMTextures.begin(), ORMTextures.end());
	Textures.insert(Textures.end(), EmissiveTextures.begin(), EmissiveTextures.end());

	UniformBuffer cameraInfo = {};
	cameraInfo.View = camera.View;
	cameraInfo.Proj = camera.Proj;
	cameraInfo.ViewPos = cameraPos;

	const auto& outputs = renderer->Render(
		[&](RenderGraph* graph) -> const std::vector<DescriptorBindingValue>
		{
			uint32_t textureWidth = settings.Display.Width;
			uint32_t textureHeight = settings.Display.Height;

			auto gBufferPosition = graph->CreateTexture({ .Width = textureWidth, .Height = textureHeight, .Format = TextureFormat::RGBA16_SFLOAT });
			auto gBufferNormal = graph->CreateTexture({ .Width = textureWidth, .Height = textureHeight, .Format = TextureFormat::RGBA16_SFLOAT });
			auto gBufferAlbedoRoughness = graph->CreateTexture({ .Width = textureWidth, .Height = textureHeight, .Format = TextureFormat::RGBA8_SRGB });
			auto gBufferMetallicAO = graph->CreateTexture({ .Width = textureWidth, .Height = textureHeight, .Format = TextureFormat::RGBA8_SRGB });
			auto gBufferEmissive = graph->CreateTexture({ .Width = textureWidth, .Height = textureHeight, .Format = TextureFormat::RGBA16_SFLOAT });
			auto gBufferDepth = graph->CreateTexture({ .Width = textureWidth, .Height = textureHeight, .Format = TextureFormat::D32_SFLOAT });
			
			graph->AddPass("GBuffer",
				{
					{ 0, DescriptorType::CombinedImageSampler, 1000, ShaderStage::Fragment, DescriptorBindingFlags::VariableDescriptorCount }
				},

				{
					{ .Textures = Textures },
				},

				[&](RgPassBuilder& builder)
				{
					builder.WriteColor(gBufferPosition);
					builder.WriteColor(gBufferNormal);
					builder.WriteColor(gBufferAlbedoRoughness);
					builder.WriteColor(gBufferMetallicAO);
					builder.WriteColor(gBufferEmissive);
					builder.WriteDepth(gBufferDepth);
				},
				[&](RgCommandList& cmd)
				{
					auto vertexShader = Application::Get()->MainAssetManager.GetAsset<ShaderAsset>("GBufferVertexShader.glsl");
					auto fragmentShader = Application::Get()->MainAssetManager.GetAsset<ShaderAsset>("GBufferFragmentShader.glsl");

					PipelineSpec gBufferPipeline = {};
					gBufferPipeline.VertexBufferLayout = { {VertexElementType::Float3}, {VertexElementType::Float2}, {VertexElementType::Float3}, {VertexElementType::Float3} };
					gBufferPipeline.PushConstants = { { sizeof(GeometryPassPushConstants), (ShaderStage)((uint32_t)ShaderStage::Fragment | (uint32_t)ShaderStage::Vertex) } };
					gBufferPipeline.CullMode = ShaderCullMode::Back;
					gBufferPipeline.ColorBlending = { BlendMode::None, BlendMode::None, BlendMode::None, BlendMode::None, BlendMode::None };
					gBufferPipeline.DepthSpec = { .DepthTest = true, .DepthWrite = true, .Operator = DepthTestOp::Less };
					if (settings.Debug.WireframeMode)
					{
						gBufferPipeline.PolygonMode = PolygonModeStyle::Line;
					}

					cmd.BindPipeline(vertexShader, fragmentShader, gBufferPipeline);

					uint32_t albedoIndex = 0;
					uint32_t normalIndex = 0;
					uint32_t ORMIndex = 0;
					uint32_t emissiveIndex = 0;

					uint32_t albedoOffset = 0;
					uint32_t normalOffset = (uint32_t)AlbedoTextures.size();
					uint32_t ORMOffset = (uint32_t)AlbedoTextures.size() + (uint32_t)NormalTextures.size();
					uint32_t emissiveOffset = (uint32_t)AlbedoTextures.size() + (uint32_t)NormalTextures.size() + (uint32_t)ORMTextures.size();

					scene->IterateComponents<MeshRendererComponent>([&cmd, &albedoIndex, &normalIndex, &ORMIndex, &emissiveIndex, albedoOffset, normalOffset, ORMOffset, emissiveOffset]
					(Entity e, MeshRendererComponent mesh)
						{
							GeometryPassPushConstants pushConstants{};
							pushConstants.Model = e.GetComponent<TransformComponent>().Transform;

							pushConstants.AlbedoIndex = -1;
							pushConstants.NormalIndex = -1;
							pushConstants.ORMIndex = -1;
							pushConstants.EmissiveIndex = -1;

							if (mesh.Material->GetAlbedoMap())
							{
								pushConstants.AlbedoIndex = albedoIndex + albedoOffset;
								albedoIndex++;
							}
							if (mesh.Material->GetNormalMap())
							{
								pushConstants.NormalIndex = normalIndex + normalOffset;
								normalIndex++;
							}
							if (mesh.Material->GetORMMap())
							{
								pushConstants.ORMIndex = ORMIndex + ORMOffset;
								ORMIndex++;
							}
							if (mesh.Material->GetEmissiveMap())
							{
								pushConstants.EmissiveIndex = emissiveIndex + emissiveOffset;
								emissiveIndex++;
							}

							pushConstants.Tint = glm::vec4(mesh.Material->GetTint(), 1.0);
							pushConstants.Roughness = mesh.Material->GetRoughnessFactor();
							pushConstants.Metallic = mesh.Material->GetMetallicFactor();
							pushConstants.Emissive = mesh.Material->GetEmissive();

							cmd.PushConstants(&pushConstants, sizeof(GeometryPassPushConstants), 0, (ShaderStage)((uint32_t)ShaderStage::Fragment | (uint32_t)ShaderStage::Vertex));

							cmd.BindVertexBuffer(mesh.Mesh->GetVertexBuffer(Application::Get()->GetRenderDevice()));
							cmd.BindIndexBuffer(mesh.Mesh->GetIndexBuffer(Application::Get()->GetRenderDevice()));
							cmd.DrawIndexed(mesh.Mesh->GetIndexCount());
						});
				});

			auto sceneColor = graph->CreateTexture({ .Width = textureWidth, .Height = textureHeight, .Format = TextureFormat::RGBA16_SFLOAT });
			auto sceneBright = graph->CreateTexture({ .Width = textureWidth, .Height = textureHeight, .Format = TextureFormat::RGBA16_SFLOAT });

			const auto directionalLights = GetDirectionalLights(scene);

			graph->AddPass("Lighting",
				{
					{ 0, DescriptorType::CombinedImageSampler, 1, ShaderStage::Fragment },
					{ 1, DescriptorType::CombinedImageSampler, 1, ShaderStage::Fragment },
					{ 2, DescriptorType::CombinedImageSampler, 1, ShaderStage::Fragment },
					{ 3, DescriptorType::CombinedImageSampler, 1, ShaderStage::Fragment },
					{ 4, DescriptorType::CombinedImageSampler, 1, ShaderStage::Fragment },
					{ 5, DescriptorType::StorageBuffer, 1, ShaderStage::Fragment }
				},

				{
					{ .Resources = {gBufferPosition} },
					{ .Resources = {gBufferNormal} },
					{ .Resources = {gBufferAlbedoRoughness} },
					{ .Resources = {gBufferMetallicAO} },
					{ .Resources = {gBufferEmissive} },
					{ .Size = directionalLights.size() * sizeof(DirectionalLight), .Data = (uint32_t*)directionalLights.data()},
				},

				[&](RgPassBuilder& builder)
				{
					builder.WriteColor(sceneColor);
					builder.WriteColor(sceneBright);

					builder.ReadTexture(gBufferPosition);
					builder.ReadTexture(gBufferNormal);
					builder.ReadTexture(gBufferAlbedoRoughness);
					builder.ReadTexture(gBufferMetallicAO);
					builder.ReadTexture(gBufferEmissive);
				},
				[&](RgCommandList& cmd)
				{
					// directional lights
					auto vertexShader = Application::Get()->MainAssetManager.GetAsset<ShaderAsset>("DirectionalLightsVertexShader.glsl");
					auto fragmentShader = Application::Get()->MainAssetManager.GetAsset<ShaderAsset>("DirectionalLightsPBRFragmentShader.glsl");

					PipelineSpec directionalLightsPipeline = {};
					directionalLightsPipeline.VertexBufferLayout = {};
					directionalLightsPipeline.PushConstants = {};
					directionalLightsPipeline.CullMode = ShaderCullMode::None;
					directionalLightsPipeline.ColorBlending = { BlendMode::None, BlendMode::None };
					directionalLightsPipeline.DepthSpec = { .DepthTest = false, .DepthWrite = false };

					cmd.BindPipeline(vertexShader, fragmentShader, directionalLightsPipeline);
					cmd.Draw(3);

					// point lights
					vertexShader = Application::Get()->MainAssetManager.GetAsset<ShaderAsset>("PointLightVertexShader.glsl");
					fragmentShader = Application::Get()->MainAssetManager.GetAsset<ShaderAsset>("PointLightPBRFragmentShader.glsl");

					PipelineSpec pointLightPipeline = {};
					pointLightPipeline.VertexBufferLayout = { {VertexElementType::Float3}, {VertexElementType::Float2}, {VertexElementType::Float3} };
					pointLightPipeline.PushConstants = { { sizeof(LightingPassPushConstants), (ShaderStage)((uint32_t)ShaderStage::Fragment | (uint32_t)ShaderStage::Vertex) } };
					pointLightPipeline.CullMode = ShaderCullMode::Front;
					pointLightPipeline.ColorBlending = { BlendMode::Additive, BlendMode::Additive };
					pointLightPipeline.DepthSpec = { .DepthTest = false, .DepthWrite = false };

					cmd.BindPipeline(vertexShader, fragmentShader, pointLightPipeline);

					scene->IterateComponents<PointLightComponent>(
						[&](Entity e, const PointLightComponent& l)
						{
							const auto& transform = e.GetComponent<TransformComponent>().Transform;
							glm::vec3 position = glm::vec3(transform[3]);
							glm::mat4 model = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), glm::vec3(l.Radius));

							LightingPassPushConstants pushConstants{};
							pushConstants.Model = model;
							pushConstants.Color = l.Color;
							pushConstants.Intensity = l.Intensity;
							pushConstants.Position = position;
							pushConstants.Radius = l.Radius;

							cmd.PushConstants(&pushConstants, sizeof(LightingPassPushConstants), 0, (ShaderStage)((uint32_t)ShaderStage::Fragment | (uint32_t)ShaderStage::Vertex));

							cmd.BindVertexBuffer(s_SphereVertexBuffer.get());
							cmd.BindIndexBuffer(s_SphereIndexBuffer.get());
							cmd.DrawIndexed(s_SphereIndexBuffer->GetSize() / sizeof(uint32_t));
						});
				});

			graph->AddOutput(sceneColor);

			graph->Compile({ { 0, DescriptorType::UniformBuffer, 1, (ShaderStage)((uint32_t)ShaderStage::Vertex | (uint32_t)ShaderStage::Fragment) } });

			return { { sizeof(UniformBuffer), (uint32_t*)&cameraInfo } };
		}, false);

	return outputs[0];
}

void DefaultRenderer::RenderImGui(Renderer* renderer, SwapChain* swapChain)
{
	renderer->Render(
		[renderer, swapChain](RenderGraph* graph) -> const std::vector<DescriptorBindingValue>
		{
			auto swapChainImage = swapChain->AcquireNextImage(graph, renderer->GetImageAvailableSemaphore());

			graph->AddPass("ImGui", {}, {},
				[swapChainImage](RgPassBuilder& builder)
				{
					builder.WriteColor(swapChainImage);
				},
				[](RgCommandList& cmd)
				{
					ImGui::Render();
					ImDrawData* drawData = ImGui::GetDrawData();

					ImGui_ImplVulkan_RenderDrawData(drawData, cmd.GetCommandBuffer());
				});

			graph->Compile({});
			return {};
		}, true);
}

void DefaultRenderer::UploadMaterialTextures(Scene* scene, std::vector<const Texture*>& albedoTextures, std::vector<const Texture*>& normalTextures, std::vector<const Texture*>& ORMTextures, std::vector<const Texture*>& emissiveTextures)
{
	scene->IterateComponents<MeshRendererComponent>(
		[&](Entity e, const MeshRendererComponent& m)
		{
			if (!m.Mesh)
				return;

			auto albedo = m.Material->GetAlbedoMap();
			if (albedo)
			{
				albedoTextures.push_back(albedo->GetTexture(Application::Get()->GetRenderDevice()));
			}

			auto normal = m.Material->GetNormalMap();
			if (normal)
			{
				normalTextures.push_back(normal->GetTexture(Application::Get()->GetRenderDevice()));
			}
			
			auto orm = m.Material->GetORMMap();
			if (orm)
			{
				ORMTextures.push_back(orm->GetTexture(Application::Get()->GetRenderDevice()));
			}

			auto emissive = m.Material->GetEmissiveMap();
			if (emissive)
			{
				emissiveTextures.push_back(emissive->GetTexture(Application::Get()->GetRenderDevice()));
			}
		});
}

std::vector<DirectionalLight> DefaultRenderer::GetDirectionalLights(Scene* scene)
{
	std::vector<DirectionalLight> directionalLights;
	scene->IterateComponents<DirectionalLightComponent>(
		[&](Entity e, const DirectionalLightComponent& l)
		{
			const auto& transform = e.GetComponent<TransformComponent>().Transform;
			directionalLights.push_back({ l.Color, l.Intensity, glm::vec3(transform[2]), 0.0f });
		});

	return directionalLights;
}
