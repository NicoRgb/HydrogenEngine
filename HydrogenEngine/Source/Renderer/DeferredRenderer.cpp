#include "Hydrogen/Renderer/DeferredRenderer.hpp"
#include "Hydrogen/Application.hpp"

using namespace Hydrogen;

#define MAX_TEXTURES 128
#define MAX_SPOTLIGHTS 16

struct ScreenVertex
{
	glm::vec2 pos;
	glm::vec2 uv;
};

static ScreenVertex vertices[] = {
	{{-1.0f, -1.0f}, {0.0f, 0.0f}},
	{{ 1.0f, -1.0f}, {1.0f, 0.0f}},
	{{ 1.0f,  1.0f}, {1.0f, 1.0f}},
	{{-1.0f,  1.0f}, {0.0f, 1.0f}}
};

static std::vector<uint32_t> indices = { 0, 1, 2, 2, 3, 0 };

DeferredRenderer::DeferredRenderer(const std::shared_ptr<RenderContext>& renderContext, uint32_t width, uint32_t height)
	: m_RenderContext(renderContext)
{
	auto& assetManager = Application::Get()->MainAssetManager;

	m_DefaultTexture = Texture::Create(renderContext, TextureFormat::FormatR8G8B8A8, 1, 1);
	uint32_t data = 0xFFFFFFFF;
	m_DefaultTexture->UploadData(&data);

	m_FullscreenVertexBuffer = VertexBuffer::Create(m_RenderContext, { { VertexElementType::Float2 }, { VertexElementType::Float2 } }, (void*)vertices, sizeof(vertices) / sizeof(ScreenVertex));
	m_FullscreenIndexBuffer = IndexBuffer::Create(m_RenderContext, indices);

	auto sphereData = GenerateUVSphere(16, 16);
	m_SphereVertexBuffer = VertexBuffer::Create(m_RenderContext, { { VertexElementType::Float3 }, { VertexElementType::Float2 }, { VertexElementType::Float3 } }, (void*)sphereData.Vertices.data(), sphereData.Vertices.size() / 8);
	m_SphereIndexBuffer = IndexBuffer::Create(m_RenderContext, sphereData.Indices);

	m_CommandBuffer = CommandBuffer::Create(renderContext);

	m_GBufferRenderGraph = RenderGraph::Create(renderContext,
		{
			.Width = width,
			.Height = height,
			.Attachments = {
				{ AttachmentType::Color, 1, TextureFormat::FormatR16G16B16A16, true, true, false }, // position
				{ AttachmentType::Color, 1, TextureFormat::FormatR16G16B16A16, true, true, false }, // normal
				{ AttachmentType::Color, 1, TextureFormat::FormatR8G8B8A8, true, true, false }, // rgb = albedo, a = roughness
				{ AttachmentType::Color, 1, TextureFormat::FormatR8G8B8A8, true, true, false }, // r = metallic, b = emissive

				{ AttachmentType::Depth, 1, TextureFormat::FormatD32Float, false, true, false },
			}
		});

	const auto& gBufferVertexShader = assetManager.GetAsset<ShaderAsset>("GBufferVertexShader.glsl");
	const auto& gBufferFragmentShader = assetManager.GetAsset<ShaderAsset>("GBufferFragmentShader.glsl");
	m_GBufferPipeline = Pipeline::Create(renderContext, m_GBufferRenderGraph, gBufferVertexShader, gBufferFragmentShader,
		{ {VertexElementType::Float3}, {VertexElementType::Float2}, {VertexElementType::Float3} },
		{ { 0, DescriptorType::UniformBuffer, ShaderStage::Vertex, sizeof(CameraInfoUniformBuffer), 1 },
		  { 1, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, MAX_TEXTURES } },
		{ { sizeof(GeometryPassPushConstants), ShaderStage::Vertex | ShaderStage::Fragment } }, Primitive::Triangles, CullMode::Back, BlendMode::None, { true, true });

	for (uint32_t i = 0; i < MAX_TEXTURES; i++)
	{
		m_GBufferPipeline->UploadTextureSampler(1, i, m_DefaultTexture);
	}

	m_LightingRenderGraph = RenderGraph::Create(renderContext,
		{
			.Width = width,
			.Height = height,
			.Attachments = {
				{ AttachmentType::Color, 1, TextureFormat::FormatR16G16B16A16, true, true, false },
				{ AttachmentType::Depth, 1, TextureFormat::FormatD32Float, false, true, false }
			}
		});

	const auto& directionalLightsVertexShader = assetManager.GetAsset<ShaderAsset>("DirectionalLightsVertexShader.glsl");
	const auto& directionalLightsFragmentShader = assetManager.GetAsset<ShaderAsset>("DirectionalLightsFragmentShader.glsl");
	m_DirectionalLightsPipeline = Pipeline::Create(renderContext, m_LightingRenderGraph, directionalLightsVertexShader, directionalLightsFragmentShader,
		{ {VertexElementType::Float2}, {VertexElementType::Float2} },
		{ { 0, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, 1 },
		  { 1, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, 1 },
		  { 2, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, 1 },
		  { 3, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, 1 },
		  { 4, DescriptorType::StorageBuffer, ShaderStage::Fragment, sizeof(DirectionalLightsBuffer) + MAX_LIGHTS * sizeof(DirectionalLight), 1 },
		  { 5, DescriptorType::UniformBuffer, ShaderStage::Fragment, sizeof(CameraInfoUniformBuffer), 1 } },
		{ { sizeof(LightingPassPushConstants), ShaderStage::Vertex}}, Primitive::Triangles, CullMode::None, BlendMode::None, { false, false });

	const auto& pointLightVertexShader = assetManager.GetAsset<ShaderAsset>("PointLightVertexShader.glsl");
	const auto& pointLightFragmentShader = assetManager.GetAsset<ShaderAsset>("PointLightFragmentShader.glsl");
	m_PointLightPipeline = Pipeline::Create(renderContext, m_LightingRenderGraph, pointLightVertexShader, pointLightFragmentShader,
		{ {VertexElementType::Float3}, {VertexElementType::Float2}, {VertexElementType::Float3} },
		{ { 0, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, 1 },
		  { 1, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, 1 },
		  { 2, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, 1 },
		  { 3, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, 1 },
		  { 4, DescriptorType::UniformBuffer, ShaderStage::Vertex | ShaderStage::Fragment, sizeof(CameraInfoUniformBuffer), 1 } },
		{ { sizeof(LightingPassPushConstants), ShaderStage::Vertex | ShaderStage::Fragment } }, Primitive::Triangles, CullMode::Front, BlendMode::Additive, { true, false });
}

DeferredRenderer::~DeferredRenderer()
{
}

void DeferredRenderer::Resize(uint32_t width, uint32_t height)
{
	m_GBufferRenderGraph->OnResize(width, height);
	m_LightingRenderGraph->OnResize(width, height);
}

void DeferredRenderer::Render(const std::shared_ptr<Scene>& scene, CameraComponent& cameraComponent, glm::vec3 cameraPos)
{
	RenderGeometryPass(scene, cameraComponent, cameraPos);
	RenderLightingPass(scene, cameraComponent, cameraPos);
}

void DeferredRenderer::RenderGeometryPass(const std::shared_ptr<Scene>& scene, const CameraComponent& cameraComponent, glm::vec3 cameraPos)
{
	auto textureMap = UploadAlbedoTextures(scene);

	CameraInfoUniformBuffer uniformBuffer{};
	uniformBuffer.ViewProj = cameraComponent.Proj * cameraComponent.View;
	uniformBuffer.CameraPos = cameraPos;

	m_GBufferPipeline->UploadUniformBufferData(0, &uniformBuffer, sizeof(CameraInfoUniformBuffer));

	m_CommandBuffer->BeginFrame(m_GBufferRenderGraph);
	m_CommandBuffer->StartRecording(m_GBufferRenderGraph);

	m_CommandBuffer->SetViewport(m_GBufferRenderGraph);
	m_CommandBuffer->SetScissor(m_GBufferRenderGraph);

	Application::Get()->CurrentScene->GetScene()->IterateComponents<MeshRendererComponent>(
		[&](Entity e, const MeshRendererComponent& m)
		{
			auto albedo = m.Material->GetAlbedoMap();
			HY_ASSERT(textureMap.count(albedo ? albedo->GetTexture().get() : nullptr) > 0, "Texture not found in texture map");

			GeometryPassPushConstants pushConstants{};
			pushConstants.Model = e.GetComponent<TransformComponent>().Transform;
			pushConstants.AlbedoIndex = textureMap[albedo ? albedo->GetTexture().get() : nullptr];
			pushConstants.Tint = m.Material->GetTint();
			pushConstants.Roughness = m.Material->GetRoughnessFactor();
			pushConstants.Metallic = m.Material->GetMetallicFactor();
			pushConstants.Emissive = m.Material->GetEmissiveStrength();

			m_CommandBuffer->BindPipeline(m_GBufferPipeline);
			m_CommandBuffer->BindVertexBuffer(m.Mesh->GetVertexBuffer());
			m_CommandBuffer->BindIndexBuffer(m.Mesh->GetIndexBuffer());

			m_CommandBuffer->UploadPushConstants(m_GBufferPipeline, 0, (void*)&pushConstants);
			m_CommandBuffer->DrawIndexed(m.Mesh->GetIndexBuffer());
		});

	m_CommandBuffer->EndRecording();
	m_CommandBuffer->EndFrame();
}

void DeferredRenderer::RenderLightingPass(const std::shared_ptr<Scene>& scene, const CameraComponent& cameraComponent, glm::vec3 cameraPos)
{
	CameraInfoUniformBuffer uniformBuffer{};
	uniformBuffer.ViewProj = cameraComponent.Proj * cameraComponent.View;
	uniformBuffer.CameraPos = cameraPos;

	m_DirectionalLightsPipeline->UploadUniformBufferData(5, &uniformBuffer, sizeof(CameraInfoUniformBuffer));
	m_DirectionalLightsPipeline->UploadTextureSampler(0, 0, m_GBufferRenderGraph->GetColorTexture(0));
	m_DirectionalLightsPipeline->UploadTextureSampler(1, 0, m_GBufferRenderGraph->GetColorTexture(1));
	m_DirectionalLightsPipeline->UploadTextureSampler(2, 0, m_GBufferRenderGraph->GetColorTexture(2));
	m_DirectionalLightsPipeline->UploadTextureSampler(3, 0, m_GBufferRenderGraph->GetColorTexture(3));

	m_PointLightPipeline->UploadUniformBufferData(4, &uniformBuffer, sizeof(CameraInfoUniformBuffer));
	m_PointLightPipeline->UploadTextureSampler(0, 0, m_GBufferRenderGraph->GetColorTexture(0));
	m_PointLightPipeline->UploadTextureSampler(1, 0, m_GBufferRenderGraph->GetColorTexture(1));
	m_PointLightPipeline->UploadTextureSampler(2, 0, m_GBufferRenderGraph->GetColorTexture(2));
	m_PointLightPipeline->UploadTextureSampler(3, 0, m_GBufferRenderGraph->GetColorTexture(3));

	std::vector<DirectionalLight> directionalLights;
	Application::Get()->CurrentScene->GetScene()->IterateComponents<DirectionalLightComponent>(
		[&](Entity e, const DirectionalLightComponent& l)
		{
			const auto& transform = e.GetComponent<TransformComponent>().Transform;
			directionalLights.push_back({ l.Color, l.Intensity, glm::vec3(transform[2]), 0.0f });
		});

	size_t lightDataSize = sizeof(DirectionalLightsBuffer) + directionalLights.size() * sizeof(DirectionalLight);
	uint32_t* lightData = new uint32_t[lightDataSize / sizeof(uint32_t)]();

	DirectionalLightsBuffer* header = reinterpret_cast<DirectionalLightsBuffer*>(lightData);
	header->LightCount = directionalLights.size();
	DirectionalLight* gpuLights = reinterpret_cast<DirectionalLight*>(lightData + sizeof(DirectionalLightsBuffer) / sizeof(uint32_t));
	memcpy(gpuLights, directionalLights.data(), directionalLights.size() * sizeof(DirectionalLight));

	m_DirectionalLightsPipeline->UploadStorageBufferData(4, lightData, lightDataSize);

	m_CommandBuffer->BeginFrame(m_LightingRenderGraph);
	m_CommandBuffer->StartRecording(m_LightingRenderGraph);

	m_CommandBuffer->SetViewport(m_LightingRenderGraph);
	m_CommandBuffer->SetScissor(m_LightingRenderGraph);

	m_CommandBuffer->BindPipeline(m_DirectionalLightsPipeline);

	m_CommandBuffer->BindVertexBuffer(m_FullscreenVertexBuffer);
	m_CommandBuffer->BindIndexBuffer(m_FullscreenIndexBuffer);

	m_CommandBuffer->DrawIndexed(m_FullscreenIndexBuffer);

	RenderPointLights(scene);

	m_CommandBuffer->EndRecording();
	m_CommandBuffer->EndFrame();

	delete[] lightData;
}

std::unordered_map<Texture*, uint32_t> DeferredRenderer::UploadAlbedoTextures(const std::shared_ptr<Scene>& scene)
{
	std::unordered_map<Texture*, uint32_t> result;
	uint32_t numUploadedTextures = 1;

	Application::Get()->CurrentScene->GetScene()->IterateComponents<MeshRendererComponent>(
		[&](Entity e, const MeshRendererComponent& m)
		{
			auto albedo = m.Material->GetAlbedoMap();
			if (!albedo)
			{
				result[nullptr] = 0;
				return;
			}

			if (result.count(albedo->GetTexture().get()) > 0)
			{
				return;
			}

			if (numUploadedTextures >= MAX_TEXTURES)
			{
				HY_ENGINE_ERROR("Exceeded maximum texture limit of {}", MAX_TEXTURES);
				result[albedo->GetTexture().get()] = 0;
				return;
			}

			result[albedo->GetTexture().get()] = numUploadedTextures;
			m_GBufferPipeline->UploadTextureSampler(1, numUploadedTextures++, albedo->GetTexture());
		});

	return result;
}

void DeferredRenderer::RenderPointLights(const std::shared_ptr<Scene>& scene)
{
	Application::Get()->CurrentScene->GetScene()->IterateComponents<PointLightComponent>(
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

			m_CommandBuffer->BindPipeline(m_PointLightPipeline);
			m_CommandBuffer->BindVertexBuffer(m_SphereVertexBuffer);
			m_CommandBuffer->BindIndexBuffer(m_SphereIndexBuffer);
			m_CommandBuffer->UploadPushConstants(m_PointLightPipeline, 0, (void*)&pushConstants);
			m_CommandBuffer->DrawIndexed(m_SphereIndexBuffer);
		});
}
