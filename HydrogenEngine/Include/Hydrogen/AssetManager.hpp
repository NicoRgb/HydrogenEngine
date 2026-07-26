#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/glm.hpp>

#include "Hydrogen/Logger.hpp"
#include "Hydrogen/Core.hpp"
#include "Hydrogen/Renderer/RenderBuffer.hpp"
#include "Hydrogen/Renderer/Texture.hpp"

#include <json.hpp>

#include <filesystem>
#include <unordered_map>
#include <memory>
#include <fstream>
#include <chrono>
#include <string>
#include <variant>

using json = nlohmann::json;

namespace Hydrogen
{
	namespace fs = std::filesystem;

	class Asset
	{
	public:
		explicit Asset(std::string path, json config) : m_Filepath(std::move(path)), m_Config(config) {}
		virtual ~Asset() = default;

		virtual void LoadCache(std::string cachePath) = 0;
		virtual void Cache() = 0;
		
		virtual void Reload() {};

		std::string GetPath() const { return m_Filepath; }

	protected:
		std::string m_Filepath;
		json m_Config;
	};

	class ShaderAsset : public Asset
	{
	public:
		static constexpr const char* GetStaticName() { return "Shader"; }

		ShaderAsset(std::string path, json config) : Asset(path, config)
		{
			std::ifstream fin(path);
			std::stringstream buffer;
			buffer << fin.rdbuf();
			m_Content = std::move(buffer.str());
			fin.close();
		}

		~ShaderAsset() = default;

		void LoadCache(std::string cachePath) override
		{
			HY_ENGINE_INFO("Using cached version for {}", m_Filepath);

			std::ifstream fin(cachePath, std::ios::binary | std::ios::ate);
			HY_ASSERT(fin, "Failed to open file '{}'", cachePath);

			std::streamsize fileSize = fin.tellg();
			HY_ASSERT(fileSize % sizeof(uint32_t) == 0, "File size is not a multiple of uint32_t");

			fin.seekg(0, std::ios::beg);

			m_ByteCode = std::vector<uint32_t>(fileSize / sizeof(uint32_t));
			fin.read(reinterpret_cast<char*>(m_ByteCode.data()), fileSize);

			HY_ASSERT(fin, "Error reading file '{}'", cachePath);
			fin.close();
		}

		void Cache() override
		{
			HY_ENGINE_INFO("Caching version for {}", m_Filepath);

			std::string cachePath = "Caches/" + m_Filepath + ".hycache";
			std::filesystem::create_directories(std::filesystem::path(cachePath).parent_path());

			std::ofstream fout(cachePath, std::ios::binary);
			fout.write((char*)m_ByteCode.data(), m_ByteCode.size() * sizeof(uint32_t));
			fout.close();
		}

		void Compile();
		const std::vector<uint32_t>& GetByteCode() const { return m_ByteCode; }

		std::string GetContent() const { return m_Content; }

	private:
		std::string m_Content;
		std::vector<uint32_t> m_ByteCode;
	};

	class TextureAsset : public Asset
	{
	public:
		static constexpr const char* GetStaticName() { return "Texture"; }

		TextureAsset(std::string path, json config)
			: Asset(path, config)
		{
			Parse(path);
		}

		~TextureAsset() = default;

		void LoadCache(std::string cachePath) override {}
		void Cache() override {}

		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }
		uint8_t GetChannels() const { return m_Channels; }
		const std::vector<uint32_t>& GetImageData() const { return m_Image; }

		const Texture* GetTexture(RenderDevice* device);

	private:
		void Parse(std::string path);

		uint32_t m_Width = 0, m_Height = 0;
		uint8_t m_Channels = 0;

		std::vector<uint32_t> m_Image;
		std::unique_ptr<Texture> m_Texture = nullptr;
	};

	class CubeMapAsset : public Asset
	{
	public:
		static constexpr const char* GetStaticName() { return "CubeMap"; }

		CubeMapAsset(std::string path, json config)
			: Asset(path, config)
		{
			Parse(path);
		}

		~CubeMapAsset() = default;

		void LoadCache(std::string cachePath) override {}
		void Cache() override {}

		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }
		uint8_t GetChannels() const { return m_Channels; }
		const Texture* GetCubeMap();

		void Parse(std::string path);

	private:
		uint32_t m_Width = 0, m_Height = 0;
		uint8_t m_Channels = 0;

		std::vector<uint32_t> m_CubeData;
		std::unique_ptr<Texture> m_CubeMap;
	};

#pragma pack(push, 1)
	struct StaticVertex
	{
		glm::vec3 Position;
		glm::vec2 UV;
		glm::vec3 Normal;
		glm::vec3 Tangent;
	};

	struct SkinnedVertex
	{
		glm::vec3 Position;
		glm::vec2 UV;
		glm::vec3 Normal;
		glm::vec3 Tangent;
		glm::ivec4 BoneIDs = glm::ivec4(-1);
		glm::vec4 Weights = glm::vec4(0.0f);
	};

	struct Joint
	{
		std::string Name;
		int ParentIndex = -1;
		glm::mat4 InverseBindMatrix;
	};

	struct VectorKey { float Time; glm::vec3 Value; };
	struct QuatKey { float Time; glm::quat Value; };

	struct BoneChannel
	{
		std::string BoneName;
		std::vector<VectorKey> PositionKeys;
		std::vector<QuatKey>   RotationKeys;
		std::vector<VectorKey> ScaleKeys;
	};
#pragma pack(pop)

	class SkeletonAsset : public Asset
	{
	public:
		static constexpr const char* GetStaticName() { return "Skeleton"; }

		SkeletonAsset(std::string path, json config)
			: Asset(path, config)
		{
			ReadAssetFile(path);
		}

		~SkeletonAsset() = default;

		void LoadCache(std::string cachePath) override {}
		void Cache() override {}

		const std::vector<Joint>& GetJoints() const { return m_Joints; }
		int FindJointIndex(const std::string& name) const;

		void WriteAssetFile(const std::string& path);
		void ReadAssetFile(const std::string& path);

	private:
		std::vector<Joint> m_Joints;
	};

	class StaticMeshAsset : public Asset
	{
	public:
		static constexpr const char* GetStaticName() { return "StaticMesh"; }

		StaticMeshAsset(std::string path, json config)
			: Asset(path, config)
		{
			ReadAssetFile(path);
		}

		~StaticMeshAsset() = default;

		void LoadCache(std::string cachePath) override {}
		void Cache() override {}

		uint32_t GetIndexCount() const { return static_cast<uint32_t>(m_Indices.size()); }

		RenderBuffer* GetVertexBuffer();
		RenderBuffer* GetIndexBuffer();

		void WriteAssetFile(const std::string& path);
		void ReadAssetFile(const std::string& path);

	private:
		std::vector<StaticVertex> m_Vertices;
		std::vector<uint32_t> m_Indices;

		std::unique_ptr<RenderBuffer> m_VertexBuffer;
		std::unique_ptr<RenderBuffer> m_IndexBuffer;
	};

	class SkeletalMeshAsset : public Asset
	{
	public:
		static constexpr const char* GetStaticName() { return "SkeletalMesh"; }

		SkeletalMeshAsset(std::string path, json config)
			: Asset(path, config)
		{
			ReadAssetFile(path);
		}

		~SkeletalMeshAsset() = default;

		void LoadCache(std::string cachePath) override {}
		void Cache() override {}

		uint32_t GetIndexCount() const { return static_cast<uint32_t>(m_Indices.size()); }

		RenderBuffer* GetVertexBuffer();
		RenderBuffer* GetIndexBuffer();

		void WriteAssetFile(const std::string& path);
		void ReadAssetFile(const std::string& path);

	private:
		std::vector<SkinnedVertex> m_Vertices;
		std::vector<uint32_t> m_Indices;

		std::unique_ptr<RenderBuffer> m_VertexBuffer;
		std::unique_ptr<RenderBuffer> m_IndexBuffer;
	};

	class AnimationAsset : public Asset
	{
	public:
		static constexpr const char* GetStaticName() { return "Animation"; }

		AnimationAsset(std::string path, json config)
			: Asset(path, config)
		{
			ReadAssetFile(path);
		}

		~AnimationAsset() = default;

		void LoadCache(std::string cachePath) override {}
		void Cache() override {}

		float GetDuration() const { return m_Duration; }
		float GetTicksPerSecond() const { return m_TicksPerSecond; }
		const std::vector<BoneChannel>& GetChannels() const { return m_Channels; }

		void WriteAssetFile(const std::string& path);
		void ReadAssetFile(const std::string& path);

	private:
		float m_Duration = 0.0f;
		float m_TicksPerSecond = 0.0f;
		std::vector<BoneChannel> m_Channels;
	};

	class ScriptAsset : public Asset
	{
	public:
		static constexpr const char* GetStaticName() { return "Script"; }

		ScriptAsset(std::string path, json config)
			: Asset(path, config)
		{
			std::ifstream fin(path);
			std::stringstream buffer;
			buffer << fin.rdbuf();
			m_Content = std::move(buffer.str());
			fin.close();
		}

		~ScriptAsset() = default;

		void LoadCache(std::string cachePath) override {}
		void Cache() override {}

		const std::string& GetContent() const { return m_Content; }

	private:
		std::string m_Content;
	};

	class SceneAsset : public Asset
	{
	public:
		static constexpr const char* GetStaticName() { return "Scene"; }

		SceneAsset(std::string path, json config)
			: Asset(path, config)
		{
			std::ifstream fin(path);
			std::stringstream buffer;
			buffer << fin.rdbuf();
			m_Content = std::move(buffer.str());
			fin.close();

			if (m_Content.empty())
			{
				m_Content = "{}";
			}
		}

		~SceneAsset() = default;

		void Load(class AssetManager* assetManager);

		void LoadCache(std::string cachePath) override {}
		void Cache() override {}

		void Save() const;
		void ClearScene();

		class Scene* GetScene() { return m_Scene.get(); }

	private:
		std::string m_Content;
		std::shared_ptr<class Scene> m_Scene;
	};

	enum class ParameterType { Float, Int, Bool, Trigger };

	struct AnimParameter
	{
		std::string Name;
		ParameterType Type = ParameterType::Float;
		std::variant<float, int, bool> Value = 0.0f;
	};

	enum class ConditionMode { Greater, Less, Equal, NotEqual, True, False };

	struct Condition
	{
		std::string ParameterName;
		ConditionMode Mode = ConditionMode::Greater;
		float Threshold = 0.0f;
	};

	struct AnimTransition
	{
		uint64_t ID = 0;
		uint64_t FromStateID = 0;
		uint64_t ToStateID = 0;
		float Duration = 0.25f;
		std::vector<Condition> Conditions;
	};

	struct AnimState
	{
		uint64_t ID = 0;
		std::string Name;
		std::string AnimationClipPath;
		bool IsDefaultState = false;
		float PlaybackSpeed = 1.0f;

		std::vector<AnimTransition> Transitions;

		std::shared_ptr<AnimationAsset> GetAnimationClip() const;
	};

	class AnimationGraphAsset : public Asset
	{
	public:
		static constexpr const char* GetStaticName() { return "Animation Graph"; }

		AnimationGraphAsset(std::string path, json config)
			: Asset(path, config)
		{
			Parse(path);
		}

		~AnimationGraphAsset() = default;

		void LoadCache(std::string cachePath) override {}
		void Cache() override {}

		virtual void Reload() override { Parse(m_Filepath); }

		void Parse(std::string path);

		uint64_t GetDefaultStateID() const { return m_DefaultStateID; }
		const std::unordered_map<uint64_t, AnimState>& GetStates() const { return m_States; }
		const std::unordered_map<std::string, AnimParameter>& GetParameters() const { return m_Parameters; }

	private:
		uint64_t m_DefaultStateID = 0;
		std::unordered_map<uint64_t, AnimState> m_States;
		std::unordered_map<std::string, AnimParameter> m_Parameters;
	};

	class MaterialAsset : public Asset
	{
	public:
		static constexpr const char* GetStaticName() { return "Material"; }

		MaterialAsset(std::string path, json config)
			: Asset(path, config)
		{
			std::ifstream fin(path);
			std::stringstream buffer;
			buffer << fin.rdbuf();
			m_Content = std::move(buffer.str());
			fin.close();

			if (m_Content.empty())
			{
				m_Content = "{}";
			}

			Parse();
		}

		~MaterialAsset() = default;

		void Parse();
		void Save() const;

		void LoadCache(std::string cachePath) override {}
		void Cache() override {}

		std::shared_ptr<TextureAsset> GetAlbedoMap();
		std::shared_ptr<TextureAsset> GetNormalMap();
		std::shared_ptr<TextureAsset> GetORMMap();
		std::shared_ptr<TextureAsset> GetEmissiveMap();

		void SetAlbedoMap(const std::shared_ptr<TextureAsset>& albedo)
		{
			m_AlbedoMap = albedo;
			m_AlbedoMapFilename = albedo ? std::filesystem::path(albedo->GetPath()).filename().string() : "";
		}

		void SetNormalMap(const std::shared_ptr<TextureAsset>& normal)
		{
			m_NormalMap = normal;
			m_NormalMapFilename = normal ? std::filesystem::path(normal->GetPath()).filename().string() : "";
		}

		void SetORMMap(const std::shared_ptr<TextureAsset>& orm)
		{
			m_ORMMap = orm;
			m_ORMMapFilename = orm ? std::filesystem::path(orm->GetPath()).filename().string() : "";
		}

		void SetEmissiveMap(const std::shared_ptr<TextureAsset>& emissive)
		{
			m_EmissiveMap = emissive;
			m_EmissiveMapFilename = emissive ? std::filesystem::path(emissive->GetPath()).filename().string() : "";
		}

		glm::vec3 GetTint() const { return m_Tint; }
		float GetRoughnessFactor() const { return m_RoughnessFactor; }
		float GetMetallicFactor() const { return m_MetallicFactor; }
		glm::vec4 GetEmissive() const { return m_Emissive; }

		void SetTint(glm::vec3 tint) { m_Tint = tint; }
		void SetRoughnessFactor(float roughness) { m_RoughnessFactor = roughness; }
		void SetMetallicFactor(float metallic) { m_MetallicFactor = metallic; }
		void SetEmissive(glm::vec4 emissive) { m_Emissive = emissive; }

	private:
		std::string m_Content;

		std::string m_AlbedoMapFilename;
		std::string m_NormalMapFilename;
		std::string m_ORMMapFilename;
		std::string m_EmissiveMapFilename;

		glm::vec3 m_Tint{ 1.0f };
		float m_RoughnessFactor = 1.0f;
		float m_MetallicFactor = 0.0f;

		glm::vec4 m_Emissive{ 0.0f };

		std::shared_ptr<TextureAsset> m_AlbedoMap;
		std::shared_ptr<TextureAsset> m_NormalMap;
		std::shared_ptr<TextureAsset> m_ORMMap;
		std::shared_ptr<TextureAsset> m_EmissiveMap;
	};

	class AssetManager
	{
	public:
		void LoadAssets(const std::string& directory);

		std::string& GetAssetDirectory() { return m_Directory; }

		template<typename T>
		std::shared_ptr<T> GetAsset(std::string name)
		{
			static_assert(std::is_base_of_v<Asset, T>);

			auto res = std::dynamic_pointer_cast<T>(m_Assets[name]);
			if (res == nullptr)
			{
				LoadAssets(m_Directory);
				res = std::dynamic_pointer_cast<T>(m_Assets[name]);
				HY_ASSERT(res, "Failed to load asset '{}'", name);
			}

			return res;
		}

		template<typename T>
		std::shared_ptr<T> TryGetAsset(const std::string& name)
		{
			static_assert(std::is_base_of_v<Asset, T>);

			auto it = m_Assets.find(name);
			if (it != m_Assets.end())
			{
				return std::dynamic_pointer_cast<T>(it->second);
			}

			return nullptr;
		}

		void ReloadAsset(const std::string& path);

		template<typename T>
		std::unordered_map<std::string, std::shared_ptr<T>> GetAllAssetsOfType()
		{
			static_assert(std::is_base_of_v<Asset, T>);

			std::unordered_map<std::string, std::shared_ptr<T>> result;
			for (const auto& [name, asset] : m_Assets)
			{
				if (auto casted = std::dynamic_pointer_cast<T>(asset))
				{
					result[name] = casted;
				}
			}
			return result;
		}

		void Clear()
		{
			for (auto& [_, asset] : m_Assets)
			{
				asset.reset();
			}
			m_Assets.clear();
		}

	private:
		void LoadAsset(const std::filesystem::path& path);

		std::string m_Directory;
		std::unordered_map<std::string, std::shared_ptr<Asset>> m_Assets;
	};
}
