#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "Hydrogen/Renderer/Renderer.hpp"
#include "Hydrogen/Application.hpp"

#include <tracy/Tracy.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace Hydrogen;

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

Renderer::Renderer(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<Viewport>& viewport)
{
	InitComponents(renderContext, viewport->GetWidth(), viewport->GetHeight());
	m_PostProcessingRenderGraph = RenderGraph::Create(renderContext, RenderGraphSpec{
		.Width = static_cast<uint32_t>(viewport->GetWidth()),
		.Height = static_cast<uint32_t>(viewport->GetHeight()),
		.Attachments = {
			{ AttachmentType::Color, 1, TextureFormat::ViewportDefault, false, true, true }
		}
		});

	m_PostProcessingPipeline = Pipeline::Create(m_RenderContext, m_PostProcessingRenderGraph,
		Application::Get()->MainAssetManager.GetAsset<ShaderAsset>("PostProcessingVertexShader.glsl"),
		Application::Get()->MainAssetManager.GetAsset<ShaderAsset>("PostProcessingFragmentShader.glsl"),
		{ { VertexElementType::Float2 }, { VertexElementType::Float2 } },
		{ { 0, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, 1 },
		  { 1, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, 1 } },
		{ }, Primitive::Triangles, CullMode::None, BlendMode::None, { false, false });

	m_FullscreenVertexBuffer = VertexBuffer::Create(m_RenderContext, { { VertexElementType::Float2 }, { VertexElementType::Float2 } }, (void*)vertices, sizeof(vertices) / sizeof(ScreenVertex));
	m_FullscreenIndexBuffer = IndexBuffer::Create(m_RenderContext, indices);
}

Renderer::Renderer(const std::shared_ptr<RenderContext>& renderContext, uint32_t width, uint32_t height)
{
	InitComponents(renderContext, width, height);
	m_PostProcessingRenderGraph = RenderGraph::Create(renderContext, RenderGraphSpec{
		.Width = static_cast<uint32_t>(width),
		.Height = static_cast<uint32_t>(height),
		.Attachments = {
			{ AttachmentType::Color, 1, TextureFormat::FormatB8G8R8A8_SRGB, true, true, false }
		}
		});

	m_PostProcessingPipeline = Pipeline::Create(m_RenderContext, m_PostProcessingRenderGraph,
		Application::Get()->MainAssetManager.GetAsset<ShaderAsset>("PostProcessingVertexShader.glsl"),
		Application::Get()->MainAssetManager.GetAsset<ShaderAsset>("PostProcessingFragmentShader.glsl"),
		{ { VertexElementType::Float2 }, { VertexElementType::Float2 } },
		{ { 0, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, 1 },
		  { 1, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, 1 } },
		{ }, Primitive::Triangles, CullMode::None, BlendMode::None, { false, false });

	m_FullscreenVertexBuffer = VertexBuffer::Create(m_RenderContext, { { VertexElementType::Float2 }, { VertexElementType::Float2 } }, (void*)vertices, sizeof(vertices) / sizeof(ScreenVertex));
	m_FullscreenIndexBuffer = IndexBuffer::Create(m_RenderContext, indices);

	m_SampledTexture = m_PostProcessingRenderGraph->GetColorTexture(0);
}

Renderer::~Renderer()
{
	m_CommandBuffer = nullptr;
	m_DefaultTexture = nullptr;
	m_SceneColor = nullptr;
	m_SceneBright = nullptr;
	m_SampledTexture = nullptr;
}

void Renderer::Render(const std::shared_ptr<Scene>& scene, CameraComponent& cameraComponent, glm::vec3 cameraPos)
{
	BeginFrame(cameraComponent, cameraPos);

	Application::Get()->CurrentScene->GetScene()->IterateComponents<MeshRendererComponent>(
		[&](Entity e, const MeshRendererComponent& m)
		{
			SubmitMesh(m, e.GetComponent<TransformComponent>().Transform);
		});

	Application::Get()->CurrentScene->GetScene()->IterateComponents<DirectionalLightComponent>(
		[&](Entity e, const DirectionalLightComponent& l)
		{ 
			SubmitDirectionalLight(l, e.GetComponent<TransformComponent>().Transform);
		});

	Application::Get()->CurrentScene->GetScene()->IterateComponents<PointLightComponent>(
		[&](Entity e, const PointLightComponent& l)
		{
			SubmitPointLight(l, e.GetComponent<TransformComponent>().Transform);
		});

	RenderFrame(m_RenderGraph);

	RenderPostProcessing();
}

void Renderer::Resize(uint32_t width, uint32_t height)
{
	for (uint32_t i = 0; i < MAX_LIGHTS; i++)
	{
		m_ShadowRenderGraphs[i]->OnResize(width, height);
	}

	m_RenderGraph->OnResize(width, height);
	m_PostProcessingRenderGraph->OnResize(width, height);

	m_BlurRenderGraphs[0]->OnResize(width, height);
	m_BlurRenderGraphs[1]->OnResize(width, height);

	uint32_t maxMsaaSamples = m_RenderContext->GetCapabilities().MaxMSAASamples;
	if (maxMsaaSamples > 1)
	{
		m_SceneColor = m_RenderGraph->GetResolveTexture(0);
		m_SceneBright = m_RenderGraph->GetResolveTexture(1);
	}
	else
	{
		m_SceneColor = m_RenderGraph->GetColorTexture(0);
		m_SceneBright = m_RenderGraph->GetColorTexture(1);
	}

	m_SampledTexture = m_PostProcessingRenderGraph->GetColorTexture(0);
}

void Renderer::InitComponents(const std::shared_ptr<RenderContext>& renderContext, uint32_t width, uint32_t height)
{
	m_RenderContext = renderContext;
	m_CommandBuffer = CommandBuffer::Create(m_RenderContext);

	m_DefaultTexture = Texture::Create(renderContext, TextureFormat::FormatR8G8B8A8, 1, 1);
	uint32_t data = 0xFFFFFFFF;
	m_DefaultTexture->UploadData(&data);

	uint32_t maxMsaaSamples = m_RenderContext->GetCapabilities().MaxMSAASamples;

	RenderGraphSpec spec;
	spec.Width = width;
	spec.Height = height;

	if (maxMsaaSamples > 1)
	{
		spec.Attachments = {
			{ AttachmentType::Color, maxMsaaSamples, TextureFormat::FormatR16G16B16A16, false, true, false },
			{ AttachmentType::Color, maxMsaaSamples, TextureFormat::FormatR16G16B16A16, false, true, false },
			{ AttachmentType::Depth, maxMsaaSamples, TextureFormat::FormatD32Float, false, true, false },
			{ AttachmentType::Resolve, 1, TextureFormat::FormatR16G16B16A16, true, true, false },
			{ AttachmentType::Resolve, 1, TextureFormat::FormatR16G16B16A16, true, true, false }
		};
	}
	else
	{
		spec.Attachments = {
			{ AttachmentType::Color, 1, TextureFormat::FormatR16G16B16A16, true, true, false },
			{ AttachmentType::Color, 1, TextureFormat::FormatR16G16B16A16, true, true, false },
			{ AttachmentType::Depth, 1, TextureFormat::FormatD32Float, false, true, false }
		};
	}

	m_RenderGraph = RenderGraph::Create(m_RenderContext, spec);
	if (maxMsaaSamples > 1)
	{
		m_SceneColor = m_RenderGraph->GetResolveTexture(0);
		m_SceneBright = m_RenderGraph->GetResolveTexture(1);
	}
	else
	{
		m_SceneColor = m_RenderGraph->GetColorTexture(0);
		m_SceneBright = m_RenderGraph->GetColorTexture(1);
	}

	for (uint32_t i = 0; i < MAX_LIGHTS; i++)
	{
		RenderGraphSpec lightSpec;
		lightSpec.Width = width;
		lightSpec.Height = height;
		lightSpec.Attachments = {
			{ AttachmentType::Depth, 1, TextureFormat::FormatD32Float, true, true, false }
		};
		m_ShadowRenderGraphs[i] = RenderGraph::Create(m_RenderContext, lightSpec);
	}

	auto& assetManager = Application::Get()->MainAssetManager;
	const auto& shadowVertexShader = assetManager.GetAsset<ShaderAsset>("ShadowVertexShader.glsl");
	const auto& shadowFragmentShader = assetManager.GetAsset<ShaderAsset>("ShadowFragmentShader.glsl");

	m_ShadowPipeline = Pipeline::Create(m_RenderContext, m_ShadowRenderGraphs[0], shadowVertexShader, shadowFragmentShader,
		{ {VertexElementType::Float3}, {VertexElementType::Float2}, {VertexElementType::Float3} },
		{ { 0, DescriptorType::UniformBuffer, ShaderStage::Vertex, sizeof(ShadowUniformBuffer), 1 }, },
		{ { sizeof(ShadowPushConstants), ShaderStage::Vertex } }, Primitive::Triangles, CullMode::Back, BlendMode::None, { true, true });

	RenderGraphSpec blurSpec;
	blurSpec.Width = width;
	blurSpec.Height = height;
	blurSpec.Attachments = { { AttachmentType::Color, 1, TextureFormat::FormatR16G16B16A16, true, true, false } };

	m_BlurRenderGraphs[0] = RenderGraph::Create(m_RenderContext, blurSpec); // horizontal
	m_BlurRenderGraphs[1] = RenderGraph::Create(m_RenderContext, blurSpec); // vertical

	auto blurVS = assetManager.GetAsset<ShaderAsset>("BlurVertexShader.glsl");
	auto blurFS = assetManager.GetAsset<ShaderAsset>("BlurFragmentShader.glsl");

	m_BlurPipeline = Pipeline::Create(m_RenderContext, m_BlurRenderGraphs[0], blurVS, blurFS,
		{ { VertexElementType::Float2 }, { VertexElementType::Float2 } },
		{ { 0, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, 1 } },
		{ { sizeof(BlurPushConstants), ShaderStage::Fragment } }, Primitive::Triangles, CullMode::None, BlendMode::None, { false, false });
}

void Renderer::BeginFrame(CameraComponent& cameraComponent, glm::vec3 cameraPos)
{
	ZoneScopedN("Renderer::BeginFrame");

	m_FrameInfo.Pipelines.clear();
	m_FrameInfo.Textures.clear();
	m_FrameInfo.ShadowMaps.clear();
	m_FrameInfo.Objects.clear();

	m_FrameInfo.Textures.push_back(m_DefaultTexture);

	m_FrameInfo.CameraInfo.View = cameraComponent.View;
	m_FrameInfo.CameraInfo.Proj = cameraComponent.Proj;
	m_FrameInfo.CameraInfo.ViewPos = cameraPos;

	m_FrameInfo.NumLights = 0;
}

void Renderer::RenderFrame(const std::shared_ptr<RenderGraph>& target)
{
	m_CommandBuffer->BeginFrame(m_RenderGraph);
	m_CommandBuffer->StartRecording(m_RenderGraph);

	m_CommandBuffer->SetViewport(m_RenderGraph);
	m_CommandBuffer->SetScissor(m_RenderGraph);

	size_t lightDataSize = sizeof(SceneLightsBuffer) + m_FrameInfo.NumLights * sizeof(GPULight);
	uint32_t* lightData = new uint32_t[lightDataSize / sizeof(uint32_t)]();

	SceneLightsBuffer* header = reinterpret_cast<SceneLightsBuffer*>(lightData);
	header->lightCount = m_FrameInfo.NumLights;
	GPULight* gpuLights = reinterpret_cast<GPULight*>(lightData + sizeof(SceneLightsBuffer) / sizeof(uint32_t));
	memcpy(gpuLights, m_FrameInfo.Lights, m_FrameInfo.NumLights * sizeof(GPULight));

	for (const auto& pipeline : m_FrameInfo.Pipelines)
	{
		pipeline->UploadUniformBufferData(0, &m_FrameInfo.CameraInfo, sizeof(UniformBuffer));
		pipeline->UploadStorageBufferData(2, lightData, lightDataSize);

		for (uint32_t i = 0; i < MAX_TEXTURES; i++)
		{
			if (i < m_FrameInfo.Textures.size())
			{
				pipeline->UploadTextureSampler(1, i, m_FrameInfo.Textures[i]);
				continue;
			}
			pipeline->UploadTextureSampler(1, i, m_DefaultTexture);
		}

		for (uint32_t i = 0; i < MAX_LIGHTS; i++)
		{
			if (i < m_FrameInfo.ShadowMaps.size())
			{
				pipeline->UploadTextureSampler(3, i, m_FrameInfo.ShadowMaps[i]);
				continue;
			}
			pipeline->UploadTextureSampler(3, i, m_DefaultTexture);
		}
	}

	for (const auto& object : m_FrameInfo.Objects)
	{
		m_CommandBuffer->BindPipeline(object.Shader);
		m_CommandBuffer->BindVertexBuffer(object.VertexBuf);
		m_CommandBuffer->BindIndexBuffer(object.IndexBuf);

		PushConstants pc{};
		pc.Model = object.Transform;
		pc.Color = object.Color;
		pc.TextureIndex = object.TextureIndex;

		m_CommandBuffer->UploadPushConstants(object.Shader, 0, (void*)&pc);
		m_CommandBuffer->DrawIndexed(object.IndexBuf);
	}

	m_CommandBuffer->EndRecording();
	m_CommandBuffer->EndFrame();

	delete[] lightData;
}

void Renderer::RenderShadowPass(const glm::mat4& lightTransform, const glm::mat4& lightSpaceMatrix, const std::shared_ptr<RenderGraph>& lightRenderGraph)
{
	m_CommandBuffer->BeginFrame(lightRenderGraph);
	m_CommandBuffer->StartRecording(lightRenderGraph);

	m_CommandBuffer->SetViewport(lightRenderGraph);
	m_CommandBuffer->SetScissor(lightRenderGraph);

	ShadowUniformBuffer shadowUBO = {};
	shadowUBO.LightSpaceMatrix = lightSpaceMatrix;

	m_ShadowPipeline->UploadUniformBufferData(0, &shadowUBO, sizeof(ShadowUniformBuffer));

	for (const auto& object : m_FrameInfo.Objects)
	{
		m_CommandBuffer->BindPipeline(m_ShadowPipeline);
		m_CommandBuffer->BindVertexBuffer(object.VertexBuf);
		m_CommandBuffer->BindIndexBuffer(object.IndexBuf);

		ShadowPushConstants pc{};
		pc.Model = object.Transform;

		m_CommandBuffer->UploadPushConstants(m_ShadowPipeline, 0, (void*)&pc);
		m_CommandBuffer->DrawIndexed(object.IndexBuf);
	}

	m_CommandBuffer->EndRecording();
	m_CommandBuffer->EndFrame();
}

void Renderer::SubmitMesh(const MeshRendererComponent& meshRenderer, const glm::mat4& transform)
{
	if (!meshRenderer.Mesh || !meshRenderer.VertexShader || !meshRenderer.FragmentShader)
		return;

	const auto& pipeline = GetOrCreatePipeline(meshRenderer.VertexShader, meshRenderer.FragmentShader);

	if (std::find(m_FrameInfo.Pipelines.begin(), m_FrameInfo.Pipelines.end(), pipeline) == m_FrameInfo.Pipelines.end())
	{
		m_FrameInfo.Pipelines.push_back(pipeline);
	}

	uint32_t texIndex = 0;

	auto albedo = meshRenderer.Material->GetAlbedo();
	if (albedo)
	{
		const auto& texture = albedo->GetTexture();
		auto it = std::find(m_FrameInfo.Textures.begin(), m_FrameInfo.Textures.end(), texture);
		if (it != m_FrameInfo.Textures.end())
		{
			texIndex = std::distance(m_FrameInfo.Textures.begin(), it);
		}
		else
		{
			texIndex = m_FrameInfo.Textures.size();
			m_FrameInfo.Textures.push_back(texture);
		}
	}

	m_FrameInfo.Objects.push_back({ meshRenderer.Mesh->GetVertexBuffer(), meshRenderer.Mesh->GetIndexBuffer(), pipeline, transform, meshRenderer.Color, texIndex });
}

void Renderer::SubmitDirectionalLight(const DirectionalLightComponent& light, const glm::mat4& transform)
{
	uint32_t idx = m_FrameInfo.NumLights++;

	glm::vec3 lightDir = glm::normalize(glm::vec3(transform[2]));
	glm::vec3 lightPos = glm::vec3(transform[3]);

	m_FrameInfo.Lights[idx].Position = glm::vec4(lightPos, 0.0f);
	m_FrameInfo.Lights[idx].Color = glm::vec4(light.Color, 0.0f);
	m_FrameInfo.Lights[idx].Color.a = light.Intensity;
	m_FrameInfo.Lights[idx].Direction = glm::vec4(lightDir, 0.0f);
	m_FrameInfo.Lights[idx].Direction.w = 1.0f;

	glm::mat4 lightView = glm::lookAt(lightPos, lightPos + lightDir, glm::vec3(0.0f, 1.0f, 0.0f));

	constexpr float size = 20.0f;
	glm::mat4 lightProj = glm::ortho(-size, size, -size, size, 0.1f, 100.0f);
	lightProj[1][1] *= -1.0f;

	m_FrameInfo.Lights[idx].LightSpaceMatrix = lightProj * lightView;

	RenderShadowPass(transform, m_FrameInfo.Lights[idx].LightSpaceMatrix, m_ShadowRenderGraphs[idx]);
	m_FrameInfo.ShadowMaps.push_back(m_ShadowRenderGraphs[idx]->GetDepthTexture());

	m_FrameInfo.Lights[idx].Position.w = idx;
}

void Renderer::SubmitPointLight(const PointLightComponent& light, const glm::mat4& transform)
{
	uint32_t idx = m_FrameInfo.NumLights++;

	glm::vec3 lightDir = glm::normalize(glm::vec3(transform[2]));
	glm::vec3 lightPos = glm::vec3(transform[3]);

	m_FrameInfo.Lights[idx].Position = glm::vec4(lightPos, 0.0f);
	m_FrameInfo.Lights[idx].Color = glm::vec4(light.Color, 0.0f);
	m_FrameInfo.Lights[idx].Color.a = light.Intensity;
	m_FrameInfo.Lights[idx].Direction = glm::vec4(lightDir, 0.0f);

	m_FrameInfo.Lights[idx].Direction.w = 0.0f;

	glm::mat4 lightView = glm::lookAt(lightPos, lightPos + lightDir, glm::vec3(0.0f, 1.0f, 0.0f));

	constexpr float size = 20.0f;
	glm::mat4 lightProj = glm::ortho(-size, size, -size, size, 0.1f, 100.0f);
	lightProj[1][1] *= -1.0f;

	m_FrameInfo.Lights[idx].LightSpaceMatrix = lightProj * lightView;

	RenderShadowPass(transform, m_FrameInfo.Lights[idx].LightSpaceMatrix, m_ShadowRenderGraphs[idx]);
	m_FrameInfo.ShadowMaps.push_back(m_ShadowRenderGraphs[idx]->GetDepthTexture());

	m_FrameInfo.Lights[idx].Position.w = idx;
}

void Renderer::RenderPostProcessing()
{
	std::shared_ptr<Texture> BlurredBrightTexture = nullptr;
	{
		bool horizontal = true, first_iteration = true;
		int amount = 10;

		for (int i = 0; i < amount; i++)
		{
			m_BlurPipeline->UploadTextureSampler(0, 0, first_iteration ? m_SceneBright : m_BlurRenderGraphs[!horizontal]->GetColorTexture(0));

			m_CommandBuffer->BeginFrame(m_BlurRenderGraphs[horizontal]);
			m_CommandBuffer->StartRecording(m_BlurRenderGraphs[horizontal]);

			m_CommandBuffer->SetViewport(m_PostProcessingRenderGraph);
			m_CommandBuffer->SetScissor(m_PostProcessingRenderGraph);

			BlurPushConstants pc{ horizontal ? 1 : 0 };
			m_CommandBuffer->UploadPushConstants(m_BlurPipeline, 0, &pc);

			m_CommandBuffer->BindPipeline(m_BlurPipeline);
			m_CommandBuffer->BindVertexBuffer(m_FullscreenVertexBuffer);
			m_CommandBuffer->BindIndexBuffer(m_FullscreenIndexBuffer);

			m_CommandBuffer->DrawIndexed(m_FullscreenIndexBuffer);

			m_CommandBuffer->EndRecording();
			m_CommandBuffer->EndFrame();

			horizontal = !horizontal;
			if (first_iteration)
				first_iteration = false;
		}

		BlurredBrightTexture = m_BlurRenderGraphs[!horizontal]->GetColorTexture(0);
	}
	{
		m_CommandBuffer->BeginFrame(m_PostProcessingRenderGraph);
		m_CommandBuffer->StartRecording(m_PostProcessingRenderGraph);

		m_CommandBuffer->SetViewport(m_PostProcessingRenderGraph);
		m_CommandBuffer->SetScissor(m_PostProcessingRenderGraph);

		m_PostProcessingPipeline->UploadTextureSampler(0, 0, m_SceneColor);
		m_PostProcessingPipeline->UploadTextureSampler(1, 0, BlurredBrightTexture);

		m_CommandBuffer->BindPipeline(m_PostProcessingPipeline);
		m_CommandBuffer->BindVertexBuffer(m_FullscreenVertexBuffer);
		m_CommandBuffer->BindIndexBuffer(m_FullscreenIndexBuffer);

		m_CommandBuffer->DrawIndexed(m_FullscreenIndexBuffer);

		m_CommandBuffer->EndRecording();
		m_CommandBuffer->EndFrame();
	}
}

const std::shared_ptr<Pipeline>& Renderer::GetOrCreatePipeline(const std::shared_ptr<ShaderAsset>& vertexShader, const std::shared_ptr<ShaderAsset>& fragmentShader)
{
	PipelineKey key = PipelineKey{ std::filesystem::path(vertexShader->GetPath()).filename().string(),
						 std::filesystem::path(fragmentShader->GetPath()).filename().string() };

	if (m_Pipelines.find(key) != m_Pipelines.end())
	{
		return m_Pipelines[key];
	}

	m_Pipelines[key] =
		Pipeline::Create(m_RenderContext, m_RenderGraph, vertexShader, fragmentShader,
			{ {VertexElementType::Float3}, {VertexElementType::Float2}, {VertexElementType::Float3} },
			{ { 0, DescriptorType::UniformBuffer, ShaderStage::Vertex | ShaderStage::Fragment, sizeof(UniformBuffer), 1 },
			  { 1, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, MAX_TEXTURES },
			  { 2, DescriptorType::StorageBuffer, ShaderStage::Fragment, sizeof(SceneLightsBuffer) + MAX_LIGHTS * sizeof(GPULight), 1 },
			  { 3, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, MAX_LIGHTS } },
			{ { sizeof(PushConstants), ShaderStage::Vertex | ShaderStage::Fragment } }, Primitive::Triangles, CullMode::Back, BlendMode::Alpha, { true, true });

	return m_Pipelines[key];
}

DebugGUIRenderer::DebugGUIRenderer(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<Viewport>& viewport)
{
	m_RenderContext = renderContext;
	m_CommandBuffer = CommandBuffer::Create(m_RenderContext);

	RenderGraphSpec spec;
	spec.Width = (uint32_t)viewport->GetWidth();
	spec.Height = (uint32_t)viewport->GetHeight();
	spec.Attachments = {
		{ AttachmentType::Color, 1, TextureFormat::ViewportDefault, false, true, true },
		{ AttachmentType::Depth, 1, TextureFormat::FormatD32Float, false, true, false }
	};

	m_RenderGraph = RenderGraph::Create(m_RenderContext, spec);
	m_DebugGUI = DebugGUI::Create(m_RenderContext, m_RenderGraph);
}

DebugGUIRenderer::~DebugGUIRenderer()
{
	m_CommandBuffer = nullptr;
}

void DebugGUIRenderer::Render()
{
	m_CommandBuffer->BeginFrame(m_RenderGraph);
	m_CommandBuffer->StartRecording(m_RenderGraph);
	m_DebugGUI->Render(m_CommandBuffer);
	m_CommandBuffer->EndRecording();
	m_CommandBuffer->EndFrame();
}

void DebugGUIRenderer::Resize(uint32_t width, uint32_t height)
{
	m_RenderGraph->OnResize(width, height);
}
