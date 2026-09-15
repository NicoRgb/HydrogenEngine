#pragma once

#include "Renderer.hpp"

namespace Hydrogen
{
	/*struct DisplaySettings
	{
		uint64_t Width = 1920;
		uint64_t Height = 1080;

		bool RenderToSwapChain = true;
	};

	struct Gizmo
	{
		enum class Type
		{
			Billboard,
			WireframeBox,
			WireframeSphere,
			WireframeCapsule
		};

		Type GizmoType = Type::Billboard;
		std::shared_ptr<TextureAsset> BillboardTexture;
		glm::vec3 Position;
		glm::vec2 Scale;

		glm::vec3 WireframeColor = glm::vec3(0.0f, 1.0f, 0.0f);
		glm::quat Rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

		glm::vec3 BoxSize = glm::vec3(1.0f);
		float SphereRadius = 1.0f;
		float CapsuleRadius = 0.5f;
		float CapsuleHeight = 1.0f;
		float CylinderRadius = 0.5f;
		float CylinderHeight = 1.0f;
	};

	struct DebugSettings
	{
		bool WireframeMode = false;
		bool RenderGrid = false;
		std::vector<Gizmo> Gizmos;
	};

	enum class AntiAliasingMode
	{
		None
	};

	struct PostProcessingSettings
	{
		uint8_t BloomIterations = 3;
		bool BloomEnabled = true;
		bool ToneMapping = true;
		AntiAliasingMode AntiAliasing = AntiAliasingMode::None;
	};

	struct LightingSettings
	{
		glm::vec3 AmbientFactor = glm::vec3(0.1f, 0.1f, 0.1f);
	};

	struct RenderingSettings
	{
		std::shared_ptr<CubeMapAsset> Skybox = nullptr;
	};

	struct RenderSettings
	{
		DisplaySettings Display;
		DebugSettings Debug;
		PostProcessingSettings PostProcessing;
		RenderingSettings Rendering;
		LightingSettings Lighting;
	};*/

	struct RenderContext
	{
		Renderer* MainRenderer;

		Scene* Scene;
		const CameraComponent& Camera;
		glm::vec3 CameraPosition;

		RenderSettings Settings;
	};

	class SceneExtractor
	{
	public:
		SceneExtractor() = default;
		~SceneExtractor() = default;

		void Reset()
		{
			m_RenderableEntities.clear();
			m_Textures.clear();
			m_Bones.clear();
		}
		void ExtractSceneData(const Scene* scene);

		struct RenderableEntityData
		{
			Entity Entity;

			int32_t AlbedoTextureIndex;
			int32_t NormalTextureIndex;
			int32_t ORMTextureIndex;
			int32_t EmissiveTextureIndex;

			int32_t BoneBaseIndex;

			std::shared_ptr<Asset> Mesh;
			std::shared_ptr<MaterialAsset> Material;
		};

		std::vector<RenderableEntityData>& GetRenderableEntities() { return m_RenderableEntities; }

		const std::vector<const Texture*>& GetTextures() const { return m_Textures; }
		const std::vector<glm::mat4>& GetBones() const { return m_Bones; }

	private:
		const Scene* m_Scene;

		std::vector<const Texture*> m_Textures;
		std::vector<glm::mat4> m_Bones;
		std::vector<RenderableEntityData> m_RenderableEntities;
	};

	class DeferredRenderer
	{
	public:
		static RgTextureView RenderSceneDeferred(Renderer* renderer, RenderSettings settings, const CameraComponent& camera, glm::vec3 cameraPos, Scene* scene);
		static void Reset()
		{
			s_SphereVertexBuffer.reset();
			s_SphereIndexBuffer.reset();
		}

	private:
		static const std::vector<DescriptorBindingValue> RenderFunc(RenderGraph* graph, const RenderContext& context);
		static void AddGBufferPass(RenderGraph* graph, const RenderContext& context);

		static void CreateSphereBuffers();

		static SceneExtractor s_SceneExtractor;

		static std::unique_ptr<RenderBuffer> s_SphereVertexBuffer;
		static std::unique_ptr<RenderBuffer> s_SphereIndexBuffer;
	};
}
