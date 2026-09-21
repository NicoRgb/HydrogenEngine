#include "Hydrogen/Renderer/DeferredRenderer.hpp"
#include "Hydrogen/Renderer/Renderer.hpp"
#include "Hydrogen/Scene/Animation.hpp"
#include "Hydrogen/Application.hpp"
#include "Hydrogen/ProceduralMesh.hpp"
#include "Tracy/Tracy.hpp"

using namespace Hydrogen;

void SceneExtractor::ExtractSceneData(const Scene* scene)
{
	m_Scene = scene;

	m_Textures.clear();
	m_Bones.clear();
	m_RenderableEntities.clear();

	m_Scene->IterateAnyComponents<MeshRendererComponent, SkeletalMeshRendererComponent>(
		[&](Entity e)
		{
			RenderableEntityData renderData;
			renderData.Entity = e;

			renderData.PreviousModelMatrix = glm::mat4(1.0f);
			for (auto& transformData : m_RenderableEntityTransforms)
			{
				if (transformData.Entity == renderData.Entity)
				{
					renderData.PreviousModelMatrix = transformData.ModelMatrix;
					break;
				}
			}

			if (e.HasComponent<MeshRendererComponent>())
			{
				const auto& component = e.GetComponent<MeshRendererComponent>();
				renderData.Mesh = std::dynamic_pointer_cast<Asset>(component.Mesh);
				renderData.Material = component.Material;
			}
			else if (e.HasComponent<SkeletalMeshRendererComponent>())
			{
				const auto& component = e.GetComponent<SkeletalMeshRendererComponent>();
				renderData.Mesh = std::dynamic_pointer_cast<Asset>(component.SkeletalMesh);
				renderData.Material = component.Material;

				if (!component.Skeleton)
					return;

				renderData.BoneBaseIndex = static_cast<uint32_t>(m_Bones.size());
				m_Bones.insert(m_Bones.begin(), component.Bones.begin(), component.Bones.end());
			}
			else
			{
				HY_ASSERT(false, "Entity does not have a valid renderable component");
				return;
			}

			if (!renderData.Mesh || !renderData.Material)
				return;

			auto albedo = renderData.Material->GetAlbedoMap();
			if (albedo)
			{
				renderData.AlbedoTextureIndex = static_cast<uint32_t>(m_Textures.size());
				m_Textures.push_back(albedo->GetTexture(Application::Get()->GetRenderDevice()));
			}

			auto normal = renderData.Material->GetNormalMap();
			if (normal)
			{
				renderData.NormalTextureIndex = static_cast<uint32_t>(m_Textures.size());
				m_Textures.push_back(normal->GetTexture(Application::Get()->GetRenderDevice()));
			}

			auto orm = renderData.Material->GetORMMap();
			if (orm)
			{
				renderData.ORMTextureIndex = static_cast<uint32_t>(m_Textures.size());
				m_Textures.push_back(orm->GetTexture(Application::Get()->GetRenderDevice()));
			}

			auto emissive = renderData.Material->GetEmissiveMap();
			if (emissive)
			{
				renderData.EmissiveTextureIndex = static_cast<uint32_t>(m_Textures.size());
				m_Textures.push_back(emissive->GetTexture(Application::Get()->GetRenderDevice()));
			}

			m_RenderableEntities.push_back(renderData);
		});

	if (m_Bones.empty()) // TODO: handle storage buffer with size 0 in the RenderGraph
		m_Bones.push_back(glm::mat4(1.0f));
}

void SceneExtractor::UpdateRenderableEntityTransforms(const Scene* scene)
{
	for (auto& renderableEntity : m_RenderableEntities)
	{
		if (!renderableEntity.Entity.IsValid() || !renderableEntity.Entity.HasComponent<TransformComponent>())
			continue;

		auto& transform = renderableEntity.Entity.GetComponent<TransformComponent>();
		glm::mat4 modelMatrix = transform.GetModel();
		m_RenderableEntityTransforms.push_back({ renderableEntity.Entity, modelMatrix });
	}
}

SceneExtractor DeferredRenderer::s_SceneExtractor;
std::unique_ptr<RenderBuffer> DeferredRenderer::s_SphereVertexBuffer;
std::unique_ptr<RenderBuffer> DeferredRenderer::s_SphereIndexBuffer;

struct CameraInfoUniformBuffer
{
	glm::mat4 PreviousViewProj = glm::mat4(1.0f);
	glm::mat4 View;
	glm::mat4 Proj;
	glm::vec3 ViewPos;
	float Padding;
};

static void PopulateCameraInfoUniformBuffer(const RenderContext& context, CameraInfoUniformBuffer &cameraInfo)
{
	cameraInfo.View = context.Camera.View;
	cameraInfo.Proj = context.Camera.Proj;
	cameraInfo.ViewPos = context.CameraPosition;
}

struct GeometryPassPushConstants
{
	glm::mat4 Model;
	glm::mat4 PreviousModel;

	int32_t AlbedoIndex;
	int32_t NormalIndex;
	int32_t ORMIndex;
	int32_t EmissiveIndex;

	glm::vec4 Tint;

	float Roughness;
	float Metallic;

	int32_t BoneBaseIndex;
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

struct BlurPushConstants
{
	int Horizontal;
};

CameraInfoUniformBuffer g_CameraInfo;
RgTextureView DeferredRenderer::RenderSceneDeferred(Renderer* renderer, RenderSettings settings, const CameraComponent& camera, glm::vec3 cameraPos, Scene* scene)
{
	RenderContext context = {
		.MainRenderer = renderer,
		.Scene = scene,
		.Camera = camera,
		.CameraPosition = cameraPos,
		.Settings = settings
	};

	if (!s_SphereVertexBuffer)
		CreateSphereBuffers();

	s_SceneExtractor.ExtractSceneData(scene);
	const auto& outputs = renderer->Render([context](RenderGraph* graph) { return RenderFunc(graph, context); }, settings.Display.RenderToSwapChain);
	g_CameraInfo.PreviousViewProj = context.Camera.Proj * context.Camera.View;
	s_SceneExtractor.Reset();

	if (settings.Display.RenderToSwapChain)
		return RgTextureView{};

	return outputs[0];
}

const std::vector<DescriptorBindingValue> DeferredRenderer::RenderFunc(RenderGraph* graph, const RenderContext& context)
{
	uint32_t textureWidth = static_cast<uint32_t>(context.Settings.Display.Width);
	uint32_t textureHeight = static_cast<uint32_t>(context.Settings.Display.Height);

	auto gBufferPosition = graph->CreateTexture({ .Width = textureWidth, .Height = textureHeight, .Format = TextureFormat::RGBA16_SFLOAT });
	auto gBufferNormal = graph->CreateTexture({ .Width = textureWidth, .Height = textureHeight, .Format = TextureFormat::RGBA16_SFLOAT });
	auto gBufferAlbedoRoughness = graph->CreateTexture({ .Width = textureWidth, .Height = textureHeight, .Format = TextureFormat::RGBA8_SRGB });
	auto gBufferMetallicAO = graph->CreateTexture({ .Width = textureWidth, .Height = textureHeight, .Format = TextureFormat::RGBA8_SRGB });
	auto gBufferEmissive = graph->CreateTexture({ .Width = textureWidth, .Height = textureHeight, .Format = TextureFormat::RGBA16_SFLOAT });
	auto motionVectors = graph->CreateTexture({ .Width = textureWidth, .Height = textureHeight, .Format = TextureFormat::RGBA16_SFLOAT });
	auto gBufferDepth = graph->CreateTexture({ .Width = textureWidth, .Height = textureHeight, .Format = TextureFormat::D32_SFLOAT });

	graph->AddPass("GBuffer",
		{
			{ 0, DescriptorType::CombinedImageSampler, 1000, ShaderStage::Fragment, DescriptorBindingFlags::VariableDescriptorCount },
			{ 1, DescriptorType::StorageBuffer, 1, ShaderStage::Vertex }
		},
		{
			{.Textures = s_SceneExtractor.GetTextures() },
			{.Size = s_SceneExtractor.GetBones().size() * sizeof(glm::mat4), .Data = (uint32_t*)s_SceneExtractor.GetBones().data() }
		},

		[&](RgPassBuilder& builder)
		{
			builder.WriteColor(gBufferPosition);
			builder.WriteColor(gBufferNormal);
			builder.WriteColor(gBufferAlbedoRoughness);
			builder.WriteColor(gBufferMetallicAO);
			builder.WriteColor(gBufferEmissive);
			builder.WriteColor(motionVectors);

			builder.WriteDepth(gBufferDepth);
		},
		[&](RgCommandList& cmd)
		{
			ZoneScopedN("GBuffer Pass");

			auto staticMeshVertexShader = Application::Get()->MainAssetManager.GetAsset<ShaderAsset>("GBufferVertexShader.glsl");
			auto skinnedVertexShader = Application::Get()->MainAssetManager.GetAsset<ShaderAsset>("GBufferSkinnedVertexShader.glsl");
			auto meshFragmentShader = Application::Get()->MainAssetManager.GetAsset<ShaderAsset>("GBufferFragmentShader.glsl");

			PipelineSpec gBufferStaticMeshPipeline = {};
			gBufferStaticMeshPipeline.VertexBufferLayout = { {VertexElementType::Float3}, {VertexElementType::Float2}, {VertexElementType::Float3}, {VertexElementType::Float3} };
			gBufferStaticMeshPipeline.PushConstants = { { sizeof(GeometryPassPushConstants), (ShaderStage)((uint32_t)ShaderStage::Fragment | (uint32_t)ShaderStage::Vertex) } };
			gBufferStaticMeshPipeline.CullMode = ShaderCullMode::Back;
			gBufferStaticMeshPipeline.ColorBlending = { BlendMode::None, BlendMode::None, BlendMode::None, BlendMode::None, BlendMode::None, BlendMode::None };
			gBufferStaticMeshPipeline.DepthSpec = { .DepthTest = true, .DepthWrite = true, .Operator = DepthTestOp::Less };
			if (context.Settings.Debug.WireframeMode)
			{
				gBufferStaticMeshPipeline.PolygonMode = PolygonModeStyle::Line;
			}

			PipelineSpec gBufferSkinnedMeshPipeline = gBufferStaticMeshPipeline;
			gBufferSkinnedMeshPipeline.VertexBufferLayout = {	{VertexElementType::Float3},	{VertexElementType::Float2},	{VertexElementType::Float3},
																{VertexElementType::Float3},	{VertexElementType::Int4},		{VertexElementType::Float4} };

			for (auto& renderData : s_SceneExtractor.GetRenderableEntities())
			{
				if (!renderData.Mesh || !renderData.Material)
				{
					return;
				}

				GeometryPassPushConstants pushConstants{};
				pushConstants.Model = renderData.Entity.GetComponent<TransformComponent>().GetModel();

				pushConstants.AlbedoIndex = -1;
				pushConstants.NormalIndex = -1;
				pushConstants.ORMIndex = -1;
				pushConstants.EmissiveIndex = -1;

				if (renderData.Material->GetAlbedoMap())
				{
					pushConstants.AlbedoIndex = renderData.AlbedoTextureIndex;
				}
				if (renderData.Material->GetNormalMap())
				{
					pushConstants.NormalIndex = renderData.NormalTextureIndex;
				}
				if (renderData.Material->GetORMMap())
				{
					pushConstants.ORMIndex = renderData.ORMTextureIndex;
				}
				if (renderData.Material->GetEmissiveMap())
				{
					pushConstants.EmissiveIndex = renderData.EmissiveTextureIndex;
				}

				pushConstants.Tint = glm::vec4(renderData.Material->GetTint(), 1.0);
				pushConstants.Roughness = renderData.Material->GetRoughnessFactor();
				pushConstants.Metallic = renderData.Material->GetMetallicFactor();
				pushConstants.Emissive = renderData.Material->GetEmissive();

				if (renderData.Entity.HasComponent<MeshRendererComponent>())
				{
					cmd.BindPipeline(staticMeshVertexShader, meshFragmentShader, gBufferStaticMeshPipeline);
					cmd.PushConstants(&pushConstants, sizeof(GeometryPassPushConstants), 0, (ShaderStage)((uint32_t)ShaderStage::Fragment | (uint32_t)ShaderStage::Vertex));

					auto staticMesh = std::dynamic_pointer_cast<StaticMeshAsset>(renderData.Mesh);

					cmd.BindVertexBuffer(staticMesh->GetVertexBuffer());
					cmd.BindIndexBuffer(staticMesh->GetIndexBuffer());
					cmd.DrawIndexed(staticMesh->GetIndexCount());
					continue;
				}

				pushConstants.BoneBaseIndex = renderData.BoneBaseIndex;

				cmd.BindPipeline(skinnedVertexShader, meshFragmentShader, gBufferSkinnedMeshPipeline);
				cmd.PushConstants(&pushConstants, sizeof(GeometryPassPushConstants), 0, (ShaderStage)((uint32_t)ShaderStage::Fragment | (uint32_t)ShaderStage::Vertex));

				auto skeletalMesh = std::dynamic_pointer_cast<SkeletalMeshAsset>(renderData.Mesh);

				cmd.BindVertexBuffer(skeletalMesh->GetVertexBuffer());
				cmd.BindIndexBuffer(skeletalMesh->GetIndexBuffer());
				cmd.DrawIndexed(skeletalMesh->GetIndexCount());
			}
		});

	if (!context.Settings.Display.RenderToSwapChain)
		graph->AddOutput(gBufferAlbedoRoughness);

	PopulateCameraInfoUniformBuffer(context, g_CameraInfo);

	graph->Compile({ { 0, DescriptorType::UniformBuffer, 1, (ShaderStage)((uint32_t)ShaderStage::Vertex | (uint32_t)ShaderStage::Fragment) } });
	return { { sizeof(CameraInfoUniformBuffer), (uint32_t*)&g_CameraInfo } };
}

void DeferredRenderer::CreateSphereBuffers()
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
