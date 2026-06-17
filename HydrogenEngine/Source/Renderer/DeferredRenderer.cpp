#include "Hydrogen/Renderer/DeferredRenderer.hpp"
#include "Hydrogen/Application.hpp"

using namespace Hydrogen;

#define MAX_TEXTURES 128
#define MAX_DIRECTIONAL_LIGHTS 16

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

static float skyboxVertices[] = {
	-1.0f,  1.0f, -1.0f,
	-1.0f, -1.0f, -1.0f,
	 1.0f, -1.0f, -1.0f,
	 1.0f, -1.0f, -1.0f,
	 1.0f,  1.0f, -1.0f,
	-1.0f,  1.0f, -1.0f,

	-1.0f, -1.0f,  1.0f,
	-1.0f, -1.0f, -1.0f,
	-1.0f,  1.0f, -1.0f,
	-1.0f,  1.0f, -1.0f,
	-1.0f,  1.0f,  1.0f,
	-1.0f, -1.0f,  1.0f,

	 1.0f, -1.0f, -1.0f,
	 1.0f, -1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f, -1.0f,
	 1.0f, -1.0f, -1.0f,

	-1.0f, -1.0f,  1.0f,
	-1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,
	 1.0f, -1.0f,  1.0f,
	-1.0f, -1.0f,  1.0f,

	-1.0f,  1.0f, -1.0f,
	 1.0f,  1.0f, -1.0f,
	 1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,
	-1.0f,  1.0f,  1.0f,
	-1.0f,  1.0f, -1.0f,

	-1.0f, -1.0f, -1.0f,
	-1.0f, -1.0f,  1.0f,
	 1.0f, -1.0f, -1.0f,
	 1.0f, -1.0f, -1.0f,
	-1.0f, -1.0f,  1.0f,
	 1.0f, -1.0f,  1.0f
};

#define SHADER_ASSETS(name) Application::Get()->MainAssetManager.GetAsset<ShaderAsset>(name ".glsl")

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

	m_SkyboxVertexBuffer = VertexBuffer::Create(m_RenderContext, { { VertexElementType::Float3 } }, (void*)skyboxVertices, sizeof(skyboxVertices) / (3 * sizeof(float)));

	m_CommandBuffer = CommandBuffer::Create(renderContext);

	m_FrameGraph = FrameGraph::Create(renderContext, width, height);

	FramePass GBufferPass;
	GBufferPass.AddResource("GBufferPosition",
		{
			.Type = FrameAttachmentType::Color,
			.SampleCount = 1,
			.Format = TextureFormat::FormatR16G16B16A16,
			.Sampled = true,
			.Cleared = true
		});
	GBufferPass.AddResource("GBufferNormal",
		{
			.Type = FrameAttachmentType::Color,
			.SampleCount = 1,
			.Format = TextureFormat::FormatR16G16B16A16,
			.Sampled = true,
			.Cleared = true
		});
	GBufferPass.AddResource("GBufferAlbedoRoughness",
		{
			.Type = FrameAttachmentType::Color,
			.SampleCount = 1,
			.Format = TextureFormat::FormatR8G8B8A8,
			.Sampled = true,
			.Cleared = true
		});
	GBufferPass.AddResource("GBufferMetallicAO",
		{
			.Type = FrameAttachmentType::Color,
			.SampleCount = 1,
			.Format = TextureFormat::FormatR8G8B8A8,
			.Sampled = true,
			.Cleared = true
		});
	GBufferPass.AddResource("GBufferEmissive",
		{
			.Type = FrameAttachmentType::Color,
			.SampleCount = 1,
			.Format = TextureFormat::FormatR16G16B16A16,
			.Sampled = true,
			.Cleared = true
		});
	GBufferPass.AddResource("GBufferObjectID",
		{
			.Type = FrameAttachmentType::Color,
			.SampleCount = 1,
			.Format = TextureFormat::FormatR32Uint,
			.Sampled = true,
			.Cleared = true
		});
	GBufferPass.AddResource("GBufferDepth",
		{
			.Type = FrameAttachmentType::Depth,
			.SampleCount = 1,
			.Format = TextureFormat::FormatD32Float,
			.Sampled = true,
			.Cleared = true
		});

	GBufferPass.AddPipeline("GBuffer", {
			.vertexShaderAsset = SHADER_ASSETS("GBufferVertexShader"),
			.fragmentShaderAsset = SHADER_ASSETS("GBufferFragmentShader"),
			.vertexLayout = { {VertexElementType::Float3}, {VertexElementType::Float2}, {VertexElementType::Float3}, {VertexElementType::Float3} },
			.descriptorBindings = {
				{ 0, DescriptorType::UniformBuffer, ShaderStage::Vertex, sizeof(CameraInfoUniformBuffer), 1 },
				{ 1, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, MAX_TEXTURES },
				{ 2, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, MAX_TEXTURES },
				{ 3, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, MAX_TEXTURES },
				{ 4, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, MAX_TEXTURES } },
			.pushConstantsRanges = { { sizeof(GeometryPassPushConstants), ShaderStage::Vertex | ShaderStage::Fragment } },
			.primitive = Primitive::Triangles,
			.cullMode = CullMode::Back,
			.blendMode = BlendMode::None,
			.depthSpec = {.DepthTest = true, .DepthWrite = true }
		});

	GBufferPass.SetRender([this](const std::shared_ptr<FrameGraph>& graph)
		{
			RenderGeometryPass(graph);
		});

	m_FrameGraph->AddPass("GBufferPass", std::make_unique<FramePass>(GBufferPass));

	FramePass LightingPass;
	LightingPass.AddResource("SceneColor",
		{
			.Type = FrameAttachmentType::Color,
			.SampleCount = 1,
			.Format = TextureFormat::FormatR16G16B16A16,
			.Sampled = true,
			.Cleared = true
		});
	LightingPass.AddResource("SceneBright",
		{
			.Type = FrameAttachmentType::Color,
			.SampleCount = 1,
			.Format = TextureFormat::FormatR16G16B16A16,
			.Sampled = true,
			.Cleared = true
		});
	LightingPass.AddResource("GBufferDepth", GBufferPass.GetResource("GBufferDepth"));

	LightingPass.AddPipeline("DirectionalLights", {
			.vertexShaderAsset = SHADER_ASSETS("DirectionalLightsVertexShader"),
			.fragmentShaderAsset = SHADER_ASSETS("DirectionalLightsPBRFragmentShader"),
			.vertexLayout = { {VertexElementType::Float2}, {VertexElementType::Float2} },
			.descriptorBindings = {
				{ 0, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, 1 },
				{ 1, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, 1 },
				{ 2, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, 1 },
				{ 3, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, 1 },
				{ 4, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, 1 },
				{ 5, DescriptorType::StorageBuffer, ShaderStage::Fragment, sizeof(DirectionalLightsBuffer) + MAX_DIRECTIONAL_LIGHTS * sizeof(DirectionalLight), 1 },
				{ 6, DescriptorType::UniformBuffer, ShaderStage::Fragment, sizeof(CameraInfoUniformBuffer), 1 } },
			.pushConstantsRanges = { },
			.primitive = Primitive::Triangles,
			.cullMode = CullMode::None,
			.blendMode = BlendMode::None,
			.depthSpec = {.DepthTest = false, .DepthWrite = false }
		});

	LightingPass.AddPipeline("PointLight", {
		.vertexShaderAsset = SHADER_ASSETS("PointLightVertexShader"),
		.fragmentShaderAsset = SHADER_ASSETS("PointLightPBRFragmentShader"),
		.vertexLayout = { {VertexElementType::Float3}, {VertexElementType::Float2}, {VertexElementType::Float3} },
		.descriptorBindings = {
			{ 0, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, 1 },
			{ 1, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, 1 },
			{ 2, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, 1 },
			{ 3, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, 1 },
			{ 4, DescriptorType::UniformBuffer, ShaderStage::Vertex | ShaderStage::Fragment, sizeof(CameraInfoUniformBuffer), 1 } },
		.pushConstantsRanges = { { sizeof(LightingPassPushConstants), ShaderStage::Vertex | ShaderStage::Fragment } },
		.primitive = Primitive::Triangles,
		.cullMode = CullMode::Front,
		.blendMode = BlendMode::Additive,
		.depthSpec = {.DepthTest = false, .DepthWrite = false }
		});

	LightingPass.AddPipeline("Skybox", {
		.vertexShaderAsset = SHADER_ASSETS("SkyboxVertexShader"),
		.fragmentShaderAsset = SHADER_ASSETS("SkyboxFragmentShader"),
		.vertexLayout = { {VertexElementType::Float3} },
		.descriptorBindings = {
			{ 0, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, 1 },
			{ 1, DescriptorType::UniformBuffer, ShaderStage::Vertex, sizeof(CameraInfoUniformBufferSplit), 1 } },
		.pushConstantsRanges = { },
		.primitive = Primitive::Triangles,
		.cullMode = CullMode::Back,
		.blendMode = BlendMode::Alpha,
		.depthSpec = {.DepthTest = true, .DepthWrite = false }
		});

	LightingPass.SetRender([this](const std::shared_ptr<FrameGraph>& graph)
		{
			RenderLightingPass(graph);
		});

	m_FrameGraph->AddPass("LightingPass", std::make_unique<FramePass>(LightingPass));

	FramePass GizmoPass;
	GizmoPass.AddResource("SceneColor", LightingPass.GetResource("SceneColor"));

	GizmoPass.AddPipeline("Billboard", {
		.vertexShaderAsset = SHADER_ASSETS("BillboardVertexShader"),
		.fragmentShaderAsset = SHADER_ASSETS("BillboardFragmentShader"),
		.vertexLayout = { {VertexElementType::Float2}, {VertexElementType::Float2} },
		.descriptorBindings = {
			{ 0, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, MAX_TEXTURES },
			{ 1, DescriptorType::UniformBuffer, ShaderStage::Vertex | ShaderStage::Fragment, sizeof(CameraInfoUniformBuffer), 1 } },
		.pushConstantsRanges = { { sizeof(BillboardPushConstants), ShaderStage::Vertex | ShaderStage::Fragment } },
		.primitive = Primitive::Triangles,
		.cullMode = CullMode::None,
		.blendMode = BlendMode::Alpha,
		.depthSpec = {.DepthTest = false, .DepthWrite = false }
		});

	GizmoPass.AddPipeline("Grid", {
		.vertexShaderAsset = SHADER_ASSETS("GridVertexShader"),
		.fragmentShaderAsset = SHADER_ASSETS("GridFragmentShader"),
		.vertexLayout = { {VertexElementType::Float2}, {VertexElementType::Float2} },
		.descriptorBindings = {
			{ 0, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, 1 },
			{ 1, DescriptorType::UniformBuffer, ShaderStage::Vertex | ShaderStage::Fragment, sizeof(CameraInfoUniformBuffer), 1 } },
		.pushConstantsRanges = { },
		.primitive = Primitive::Triangles,
		.cullMode = CullMode::None,
		.blendMode = BlendMode::Alpha,
		.depthSpec = {.DepthTest = false, .DepthWrite = false }
		});

	GizmoPass.SetRender([this](const std::shared_ptr<FrameGraph>& graph)
		{
			RenderGizmoPass(graph);
		});

	m_FrameGraph->AddPass("GizmoPass", std::make_unique<FramePass>(GizmoPass));

	constexpr uint8_t numPingPong = 2;
	for (uint8_t i = 0; i < numPingPong; i++)
	{
		std::string name = "BloomPingPong" + std::to_string(i);

		FramePass BloomPass;

		if ((i % 2) == 0)
		{
			BloomPass.AddResource("SceneBrightBlurredHorizontal",
				{
					.Type = FrameAttachmentType::Color,
					.SampleCount = 1,
					.Format = TextureFormat::FormatR16G16B16A16,
					.Sampled = true,
					.Cleared = true
				});

			BloomPass.SetRender([this, name, i](const std::shared_ptr<FrameGraph>& graph)
				{
					RenderBloomPass(graph, name, (i == 0) ? "SceneBright" : "SceneBrightBlurredVertical", true);
				});
		}
		if ((i % 2) != 0)
		{
			BloomPass.AddResource("SceneBrightBlurredVertical",
				{
					.Type = FrameAttachmentType::Color,
					.SampleCount = 1,
					.Format = TextureFormat::FormatR16G16B16A16,
					.Sampled = true,
					.Cleared = true
				});

			BloomPass.SetRender([this, name](const std::shared_ptr<FrameGraph>& graph)
				{
					RenderBloomPass(graph, name, "SceneBrightBlurredHorizontal", false);
				});
		}

		BloomPass.AddPipeline("Blur", {
			.vertexShaderAsset = SHADER_ASSETS("BlurVertexShader"),
			.fragmentShaderAsset = SHADER_ASSETS("BlurFragmentShader"),
			.vertexLayout = { {VertexElementType::Float2}, {VertexElementType::Float2} },
			.descriptorBindings = { { 0, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, 1 } },
			.pushConstantsRanges = { { sizeof(BlurPushConstants), ShaderStage::Fragment } },
			.primitive = Primitive::Triangles,
			.cullMode = CullMode::None,
			.blendMode = BlendMode::None,
			.depthSpec = {.DepthTest = false, .DepthWrite = false }
			});

		m_FrameGraph->AddPass(name, std::make_unique<FramePass>(BloomPass));
	}

	FramePass CompositePass;
	CompositePass.AddResource("FinalScene", {
		.Type = FrameAttachmentType::Color,
		.SampleCount = 1,
		.Format = TextureFormat::FormatB8G8R8A8_SRGB,
		.Sampled = true,
		.Cleared = true
		});

	CompositePass.AddPipeline("PostProcessing", {
		.vertexShaderAsset = SHADER_ASSETS("PostProcessingVertexShader"),
		.fragmentShaderAsset = SHADER_ASSETS("PostProcessingFragmentShader"),
		.vertexLayout = { {VertexElementType::Float2}, {VertexElementType::Float2} },
		.descriptorBindings = {
			{ 0, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, 1 },
			{ 1, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, 1 } },
		.pushConstantsRanges = { },
		.primitive = Primitive::Triangles,
		.cullMode = CullMode::None,
		.blendMode = BlendMode::None,
		.depthSpec = {.DepthTest = false, .DepthWrite = false }
		});

	CompositePass.SetRender([this](const std::shared_ptr<FrameGraph>& graph)
		{
			RenderCompositePass(graph);
		});

	m_FrameGraph->AddPass("CompositePass", std::make_unique<FramePass>(CompositePass));

	m_FrameGraph->Compose();

	auto gBufferPipeline = m_FrameGraph->GetPass("GBufferPass")->GetPipeline("GBuffer");
	for (uint32_t i = 0; i < MAX_TEXTURES; i++)
	{
		gBufferPipeline->UploadTextureSampler(1, i, m_DefaultTexture);
		gBufferPipeline->UploadTextureSampler(2, i, m_DefaultTexture);
		gBufferPipeline->UploadTextureSampler(3, i, m_DefaultTexture);
		gBufferPipeline->UploadTextureSampler(4, i, m_DefaultTexture);
	}

	auto billboardPipeline = m_FrameGraph->GetPass("GizmoPass")->GetPipeline("Billboard");
	for (uint32_t i = 0; i < MAX_TEXTURES; i++)
	{
		billboardPipeline->UploadTextureSampler(0, i, m_DefaultTexture);
	}
}

DeferredRenderer::~DeferredRenderer()
{
}

void DeferredRenderer::Resize(uint32_t width, uint32_t height)
{
	m_FrameGraph->Resize(width, height);
}

void DeferredRenderer::Render(const std::shared_ptr<Scene>& scene, CameraComponent& cameraComponent, glm::vec3 cameraPos, const std::vector<Gizmo>& gizmos, const std::shared_ptr<CubeMap>& skybox)
{
	m_Scene = scene;
	m_CameraComponent = cameraComponent;
	m_CameraPos = cameraPos;
	m_Gizmos = gizmos;
	m_Skybox = skybox;

	m_FrameGraph->Render();
}

uint32_t DeferredRenderer::ReadEntityIDFromGPU(uint32_t x, uint32_t y)
{
	return m_FrameGraph->GetTexture("GBufferObjectID")->ReadPixel(x, y);
}

void DeferredRenderer::RenderGeometryPass(const std::shared_ptr<FrameGraph>& graph)
{
	auto pass = graph->GetPass("GBufferPass");

	UploadMaterialTextures();

	CameraInfoUniformBuffer uniformBuffer{};
	uniformBuffer.ViewProj = m_CameraComponent.Proj * m_CameraComponent.View;
	uniformBuffer.CameraPos = m_CameraPos;

	pass->GetPipeline("GBuffer")->UploadUniformBufferData(0, &uniformBuffer, sizeof(CameraInfoUniformBuffer));

	m_CommandBuffer->BeginFrame(graph, "GBufferPass");
	m_CommandBuffer->StartRecording();

	m_CommandBuffer->SetViewport(graph->GetWidth(), graph->GetHeight());
	m_CommandBuffer->SetScissor(graph->GetWidth(), graph->GetHeight());

	m_Scene->IterateComponents<MeshRendererComponent>(
		[&](Entity e, const MeshRendererComponent& m)
		{
			if (!m.Mesh)
				return;

			auto albedo = m.Material->GetAlbedoMap();
			auto normal = m.Material->GetNormalMap();
			auto orm = m.Material->GetORMMap();
			auto emissive = m.Material->GetEmissiveMap();
			HY_ASSERT(m_AlbedoTextures.count(albedo ? albedo->GetTexture().get() : nullptr) > 0, "Texture not found in texture map");
			HY_ASSERT(m_NormalTextures.count(normal ? normal->GetTexture().get() : nullptr) > 0, "Texture not found in texture map");
			HY_ASSERT(m_ORMTextures.count(orm ? orm->GetTexture().get() : nullptr) > 0, "Texture not found in texture map");
			HY_ASSERT(m_EmissiveTextures.count(emissive ? emissive->GetTexture().get() : nullptr) > 0, "Texture not found in texture map");

			GeometryPassPushConstants pushConstants{};
			pushConstants.Model = e.GetComponent<TransformComponent>().Transform;
			pushConstants.AlbedoIndex = m_AlbedoTextures[albedo ? albedo->GetTexture().get() : nullptr];
			pushConstants.NormalIndex = m_NormalTextures[normal ? normal->GetTexture().get() : nullptr];
			pushConstants.ORMIndex = m_ORMTextures[orm ? orm->GetTexture().get() : nullptr];
			pushConstants.EmissiveIndex = m_EmissiveTextures[emissive ? emissive->GetTexture().get() : nullptr];
			pushConstants.Tint = glm::vec4(m.Material->GetTint(), 1.0);
			pushConstants.Roughness = m.Material->GetRoughnessFactor();
			pushConstants.Metallic = m.Material->GetMetallicFactor();
			pushConstants.Emissive = m.Material->GetEmissive();
			pushConstants.ObjectID = e.GetID();

			m_CommandBuffer->BindPipeline(pass->GetPipeline("GBuffer"));
			m_CommandBuffer->BindVertexBuffer(m.Mesh->GetVertexBuffer());
			m_CommandBuffer->BindIndexBuffer(m.Mesh->GetIndexBuffer());

			m_CommandBuffer->UploadPushConstants(pass->GetPipeline("GBuffer"), 0, (void*)&pushConstants);
			m_CommandBuffer->DrawIndexed(m.Mesh->GetIndexBuffer());
		});

	m_CommandBuffer->EndRecording();
	m_CommandBuffer->EndFrame();
}

void DeferredRenderer::RenderLightingPass(const std::shared_ptr<FrameGraph>& graph)
{
	auto pass = graph->GetPass("LightingPass");
	auto directionalLightsPipeline = pass->GetPipeline("DirectionalLights");
	auto pointLightPipeline = pass->GetPipeline("PointLight");
	auto skyboxPipeline = pass->GetPipeline("Skybox");

	CameraInfoUniformBuffer uniformBuffer{};
	uniformBuffer.ViewProj = m_CameraComponent.Proj * m_CameraComponent.View;
	uniformBuffer.CameraPos = m_CameraPos;

	CameraInfoUniformBufferSplit uniformBufferSplit{};
	uniformBufferSplit.View = m_CameraComponent.View;
	uniformBufferSplit.Proj = m_CameraComponent.Proj;
	uniformBufferSplit.CameraPos = m_CameraPos;

	directionalLightsPipeline->UploadUniformBufferData(6, &uniformBuffer, sizeof(CameraInfoUniformBuffer));
	directionalLightsPipeline->UploadTextureSampler(0, 0, m_FrameGraph->GetTexture("GBufferPosition"));
	directionalLightsPipeline->UploadTextureSampler(1, 0, m_FrameGraph->GetTexture("GBufferNormal"));
	directionalLightsPipeline->UploadTextureSampler(2, 0, m_FrameGraph->GetTexture("GBufferAlbedoRoughness"));
	directionalLightsPipeline->UploadTextureSampler(3, 0, m_FrameGraph->GetTexture("GBufferMetallicAO"));
	directionalLightsPipeline->UploadTextureSampler(4, 0, m_FrameGraph->GetTexture("GBufferEmissive"));

	pointLightPipeline->UploadUniformBufferData(4, &uniformBuffer, sizeof(CameraInfoUniformBuffer));
	pointLightPipeline->UploadTextureSampler(0, 0, m_FrameGraph->GetTexture("GBufferPosition"));
	pointLightPipeline->UploadTextureSampler(1, 0, m_FrameGraph->GetTexture("GBufferNormal"));
	pointLightPipeline->UploadTextureSampler(2, 0, m_FrameGraph->GetTexture("GBufferAlbedoRoughness"));
	pointLightPipeline->UploadTextureSampler(3, 0, m_FrameGraph->GetTexture("GBufferMetallicAO"));

	if (m_Skybox)
	{
		skyboxPipeline->UploadTextureSampler(0, 0, m_Skybox);
		skyboxPipeline->UploadUniformBufferData(1, & uniformBufferSplit, sizeof(CameraInfoUniformBufferSplit));
	}

	std::vector<DirectionalLight> directionalLights;
	m_Scene->IterateComponents<DirectionalLightComponent>(
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

	directionalLightsPipeline->UploadStorageBufferData(5, lightData, lightDataSize);

	m_CommandBuffer->BeginFrame(graph, "LightingPass");
	m_CommandBuffer->StartRecording();

	m_CommandBuffer->SetViewport(graph->GetWidth(), graph->GetHeight());
	m_CommandBuffer->SetScissor(graph->GetWidth(), graph->GetHeight());

	m_CommandBuffer->BindPipeline(directionalLightsPipeline);

	m_CommandBuffer->BindVertexBuffer(m_FullscreenVertexBuffer);
	m_CommandBuffer->BindIndexBuffer(m_FullscreenIndexBuffer);

	m_CommandBuffer->DrawIndexed(m_FullscreenIndexBuffer);

	RenderPointLights();

	if (m_Skybox)
	{
		m_CommandBuffer->BindPipeline(skyboxPipeline);
		m_CommandBuffer->BindVertexBuffer(m_SkyboxVertexBuffer);
		m_CommandBuffer->Draw(sizeof(skyboxVertices) / (3 * sizeof(float)));
	}

	m_CommandBuffer->EndRecording();
	m_CommandBuffer->EndFrame();

	delete[] lightData;
}

void DeferredRenderer::RenderGizmoPass(const std::shared_ptr<FrameGraph>& graph)
{
	auto pass = graph->GetPass("GizmoPass");
	auto billboardPipeline = pass->GetPipeline("Billboard");
	auto gridPipeline = pass->GetPipeline("Grid");

	for (size_t i = 0; i < m_Gizmos.size(); i++)
	{
		billboardPipeline->UploadTextureSampler(0, i, m_Gizmos[i].BillboardTexture);
	}
	
	CameraInfoUniformBuffer uniformBuffer{};
	uniformBuffer.ViewProj = m_CameraComponent.Proj * m_CameraComponent.View;
	uniformBuffer.CameraPos = m_CameraPos;
	
	billboardPipeline->UploadUniformBufferData(1, &uniformBuffer, sizeof(CameraInfoUniformBuffer));
	gridPipeline->UploadUniformBufferData(1, &uniformBuffer, sizeof(CameraInfoUniformBuffer));
	
	gridPipeline->UploadTextureSampler(0, 0, graph->GetTexture("GBufferDepth"));
	
	m_CommandBuffer->BeginFrame(graph, "GizmoPass");
	m_CommandBuffer->StartRecording();
	
	m_CommandBuffer->SetViewport(graph->GetWidth(), graph->GetHeight());
	m_CommandBuffer->SetScissor(graph->GetWidth(), graph->GetHeight());
	
	m_CommandBuffer->BindPipeline(gridPipeline);
	m_CommandBuffer->BindVertexBuffer(m_FullscreenVertexBuffer);
	m_CommandBuffer->BindIndexBuffer(m_FullscreenIndexBuffer);
	
	m_CommandBuffer->DrawIndexed(m_FullscreenIndexBuffer);
	
	for (size_t i = 0; i < m_Gizmos.size(); i++)
	{
		BillboardPushConstants pushConstants{};
		pushConstants.Position = m_Gizmos[i].Position;
		pushConstants.Scale = m_Gizmos[i].Scale;
		pushConstants.TextureIndex = i;
	
		m_CommandBuffer->BindPipeline(billboardPipeline);
		m_CommandBuffer->BindVertexBuffer(m_FullscreenVertexBuffer);
		m_CommandBuffer->BindIndexBuffer(m_FullscreenIndexBuffer);
	
		m_CommandBuffer->UploadPushConstants(billboardPipeline, 0, (void*)&pushConstants);
		m_CommandBuffer->DrawIndexed(m_FullscreenIndexBuffer);
	}
	
	m_CommandBuffer->EndRecording();
	m_CommandBuffer->EndFrame();
}

void DeferredRenderer::RenderBloomPass(const std::shared_ptr<FrameGraph>& graph, std::string passName, std::string sampledTextureName, bool horizontal)
{
	auto pass = graph->GetPass(passName);
	auto blurPipeline = pass->GetPipeline("Blur");

	blurPipeline->UploadTextureSampler(0, 0, graph->GetTexture(sampledTextureName));

	m_CommandBuffer->BeginFrame(graph, passName);
	m_CommandBuffer->StartRecording();

	m_CommandBuffer->SetViewport(graph->GetWidth(), graph->GetHeight());
	m_CommandBuffer->SetScissor(graph->GetWidth(), graph->GetHeight());

	BlurPushConstants pc{ horizontal ? 1 : 0 };
	m_CommandBuffer->UploadPushConstants(blurPipeline, 0, &pc);

	m_CommandBuffer->BindPipeline(blurPipeline);
	m_CommandBuffer->BindVertexBuffer(m_FullscreenVertexBuffer);
	m_CommandBuffer->BindIndexBuffer(m_FullscreenIndexBuffer);

	m_CommandBuffer->DrawIndexed(m_FullscreenIndexBuffer);

	m_CommandBuffer->EndRecording();
	m_CommandBuffer->EndFrame();
}

void DeferredRenderer::RenderCompositePass(const std::shared_ptr<FrameGraph>& graph)
{
	auto pass = graph->GetPass("CompositePass");
	auto ppPipeline = pass->GetPipeline("PostProcessing");

	ppPipeline->UploadTextureSampler(0, 0, graph->GetTexture("SceneColor"));
	ppPipeline->UploadTextureSampler(1, 0, graph->GetTexture("SceneBrightBlurredVertical"));

	m_CommandBuffer->BeginFrame(graph, "CompositePass");
	m_CommandBuffer->StartRecording();

	m_CommandBuffer->SetViewport(graph->GetWidth(), graph->GetHeight());
	m_CommandBuffer->SetScissor(graph->GetWidth(), graph->GetHeight());

	m_CommandBuffer->BindPipeline(ppPipeline);
	m_CommandBuffer->BindVertexBuffer(m_FullscreenVertexBuffer);
	m_CommandBuffer->BindIndexBuffer(m_FullscreenIndexBuffer);

	m_CommandBuffer->DrawIndexed(m_FullscreenIndexBuffer);

	m_CommandBuffer->EndRecording();
	m_CommandBuffer->EndFrame();
}

void DeferredRenderer::UploadMaterialTextures()
{
	m_AlbedoTextures.clear();
	m_NormalTextures.clear();
	m_ORMTextures.clear();
	m_EmissiveTextures.clear();

	m_Scene->IterateComponents<MeshRendererComponent>(
		[&](Entity e, const MeshRendererComponent& m)
		{
			if (!m.Mesh)
				return;

			auto albedo = m.Material->GetAlbedoMap();
			if (albedo)
			{
				UploadMaterialTexture(albedo->GetTexture(), m_AlbedoTextures, 1);
			}

			auto normal = m.Material->GetNormalMap();
			if (normal)
			{
				UploadMaterialTexture(normal->GetTexture(), m_NormalTextures, 2);
			}

			auto orm = m.Material->GetORMMap();
			if (orm)
			{
				UploadMaterialTexture(orm->GetTexture(), m_ORMTextures, 3);
			}

			auto emissive = m.Material->GetEmissiveMap();
			if (emissive)
			{
				UploadMaterialTexture(emissive->GetTexture(), m_EmissiveTextures, 4);
			}
		});

	m_AlbedoTextures[nullptr] = MAX_TEXTURES + 1;
	m_NormalTextures[nullptr] = MAX_TEXTURES + 1;
	m_ORMTextures[nullptr] = MAX_TEXTURES + 1;
	m_EmissiveTextures[nullptr] = MAX_TEXTURES + 1;
}

void DeferredRenderer::UploadMaterialTexture(const std::shared_ptr<Texture>& texture, std::unordered_map<Texture*, uint32_t>& textureMap, uint32_t descriptorIndex)
{
	if (textureMap.size() >= MAX_TEXTURES)
	{
		HY_ENGINE_ERROR("Exceeded maximum texture limit of {}", MAX_TEXTURES);
		textureMap[texture.get()] = MAX_TEXTURES + 1; // no texture
		return;
	}

	m_FrameGraph->GetPass("GBufferPass")->GetPipeline("GBuffer")->UploadTextureSampler(descriptorIndex, textureMap.size(), texture);
	textureMap[texture.get()] = textureMap.size();
}

void DeferredRenderer::RenderPointLights()
{
	auto pointLightPipeline = m_FrameGraph->GetPass("LightingPass")->GetPipeline("PointLight");

	m_Scene->IterateComponents<PointLightComponent>(
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

			m_CommandBuffer->BindPipeline(pointLightPipeline);
			m_CommandBuffer->BindVertexBuffer(m_SphereVertexBuffer);
			m_CommandBuffer->BindIndexBuffer(m_SphereIndexBuffer);
			m_CommandBuffer->UploadPushConstants(pointLightPipeline, 0, (void*)&pushConstants);
			m_CommandBuffer->DrawIndexed(m_SphereIndexBuffer);
		});
}
