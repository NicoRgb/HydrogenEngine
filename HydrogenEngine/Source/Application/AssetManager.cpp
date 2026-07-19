#include "Hydrogen/AssetManager.hpp"
#include "Hydrogen/Scene.hpp"
#include "Hydrogen/Application.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <shaderc/shaderc.hpp>

using namespace Hydrogen;

static const std::vector<uint32_t> CompileShader(const std::string& source, shaderc_shader_kind kind)
{
	shaderc::Compiler compiler;
	shaderc::CompileOptions options;

	options.SetOptimizationLevel(shaderc_optimization_level_performance);
	shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(
		source, kind, "shader", options
	);

	HY_ASSERT(module.GetCompilationStatus() == shaderc_compilation_status_success, "Vulkan shader compilation failed: {}", module.GetErrorMessage());
	return { module.cbegin(), module.cend() };
}

void ShaderAsset::Compile()
{
	std::string shaderStage = m_Config["preferences"]["stage"];

	shaderc_shader_kind shaderKind = shaderc_vertex_shader;
	if (shaderStage == "vertex")
	{
		shaderKind = shaderc_vertex_shader;
	}
	else if (shaderStage == "fragment")
	{
		shaderKind = shaderc_fragment_shader;
	}
	else
	{
		HY_ENGINE_ERROR("Unknown shader stage '{}' for '{}' -> Defaulting to vertex", shaderStage, m_Filepath);
	}

	HY_ENGINE_INFO("Compiling shader '{}'", m_Filepath);
	m_ByteCode = CompileShader(m_Content, shaderKind);
}

void AssetManager::LoadAssets(const std::string& directory)
{
	Clear();
	HY_ASSERT(fs::exists(directory), "Asset directory '{}' does not exist", directory);

	m_Directory = directory;

	for (const auto& entry : fs::recursive_directory_iterator(directory))
	{
		if (!entry.is_regular_file() || entry.path().extension() == ".hyasset")
		{
			continue;
		}

		std::string ext = entry.path().extension().string();
		std::string filePath = entry.path().string();
		std::string cachePath = "Caches/" + entry.path().string() + ".hycache";

		std::string assetFilePath = filePath + ".hyasset";

		json assetConfig;
		if (fs::exists(assetFilePath))
		{
			std::ifstream fin(assetFilePath);
			assetConfig = json::parse(fin);
			fin.close();
		}
		else
		{
			HY_ENGINE_WARN("No asset configuration file for '{}' -> Create default", filePath);

			std::string assetType = "";
			if (ext == ".glsl")
			{
				assetType = "Shader";
				assetConfig["preferences"]["stage"] = "vertex";
			}
			else if (ext == ".png" || ext == ".jpg")
			{
				assetType = "Texture";
			}
			else if (ext == ".hymesh")
			{
				assetType = "StaticMesh";
			}
			else if (ext == ".hydynmesh")
			{
				assetType = "SkeletalMesh";
			}
			else if (ext == ".hyskel")
			{
				assetType = "Skeleton";
			}
			else if (ext == ".hyanim")
			{
				assetType = "Animation";
			}
			else if (ext == ".lua")
			{
				assetType = "Script";
			}
			else if (ext == ".hyscene")
			{
				assetType = "Scene";
			}
			else if (ext == ".hymat")
			{
				assetType = "Material";
			}
			else if (ext == ".hycube")
			{
				assetType = "CubeMap";
			}
			else
			{
				HY_ENGINE_WARN("Ignoring file '{}'", filePath);
				continue;
			}

			assetConfig["name"] = entry.path().filename();
			assetConfig["type"] = assetType;

			std::ofstream fout(assetFilePath);
			fout << assetConfig.dump();
			fout.close();
		}

		if (assetConfig["type"] == "Shader")
		{
			if (fs::exists(cachePath) && fs::last_write_time(cachePath) > fs::last_write_time(filePath))
			{
				auto shader = std::make_shared<ShaderAsset>(filePath, assetConfig);
				shader->LoadCache(cachePath);
				m_Assets[entry.path().filename().string()] = std::move(shader);
			}
			else
			{
				auto shader = std::make_shared<ShaderAsset>(filePath, assetConfig);
				shader->Compile();
				shader->Cache();
				m_Assets[entry.path().filename().string()] = std::move(shader);
			}
		}
		else if (assetConfig["type"] == "Texture")
		{
			auto texture = std::make_shared<TextureAsset>(filePath, assetConfig);
			m_Assets[entry.path().filename().string()] = std::move(texture);
		}
		else if (assetConfig["type"] == "StaticMesh")
		{
			auto mesh = std::make_shared<StaticMeshAsset>(filePath, assetConfig);
			m_Assets[entry.path().filename().string()] = std::move(mesh);
		}
		else if (assetConfig["type"] == "SkeletalMesh")
		{
			auto mesh = std::make_shared<SkeletalMeshAsset>(filePath, assetConfig);
			m_Assets[entry.path().filename().string()] = std::move(mesh);
		}
		else if (assetConfig["type"] == "Skeleton")
		{
			auto sekleton = std::make_shared<SkeletonAsset>(filePath, assetConfig);
			m_Assets[entry.path().filename().string()] = std::move(sekleton);
		}
		else if (assetConfig["type"] == "Animation")
		{
			auto animation = std::make_shared<AnimationAsset>(filePath, assetConfig);
			m_Assets[entry.path().filename().string()] = std::move(animation);
		}
		else if (assetConfig["type"] == "Script")
		{
			auto script = std::make_shared<ScriptAsset>(filePath, assetConfig);
			m_Assets[entry.path().filename().string()] = std::move(script);
		}
		else if (assetConfig["type"] == "Scene")
		{
			auto scene = std::make_shared<SceneAsset>(filePath, assetConfig);
			m_Assets[entry.path().filename().string()] = std::move(scene);
		}
		else if (assetConfig["type"] == "Material")
		{
			auto material = std::make_shared<MaterialAsset>(filePath, assetConfig);
			m_Assets[entry.path().filename().string()] = std::move(material);
		}
		else if (assetConfig["type"] == "CubeMap")
		{
			auto cubeMap = std::make_shared<CubeMapAsset>(filePath, assetConfig);
			m_Assets[entry.path().filename().string()] = std::move(cubeMap);
		}
		else
		{
			HY_ENGINE_ERROR("Unknown asset type for file '{}'", filePath);
		}
	}
}

const Texture* TextureAsset::GetTexture(RenderDevice* device)
{
	if (!m_Texture)
	{
		TextureDescription textureDesc;
		textureDesc.width = m_Width;
		textureDesc.height = m_Height;
		textureDesc.usageFlags = TextureUsage::SampledImage;

		m_Texture = std::make_unique<Texture>(device, textureDesc);
		m_Texture->UploadData(m_Image.data(), m_Width, m_Height);
	}

	return m_Texture.get();
}

void TextureAsset::Parse(std::string path)
{
	int x, y, channels;

	HY_ASSERT(stbi_info(path.c_str(), &x, &y, &channels), "Failed to load image");
	
	m_Width = x;
	m_Height = y;

	unsigned char* data = stbi_load(path.c_str(), &x, &y, &channels, STBI_rgb_alpha);
	HY_ASSERT(data, "Failed to load image");

	m_Channels = 4;

	m_Image.resize(m_Width * m_Height);

	memcpy(m_Image.data(), data, m_Width * m_Height * 4);
	stbi_image_free(data);
}

int SkeletonAsset::FindJointIndex(const std::string& name) const
{
	for (size_t i = 0; i < m_Joints.size(); ++i)
	{
		if (m_Joints[i].Name == name) return static_cast<int>(i);
	}
	return -1;
}

void SkeletonAsset::WriteAssetFile(const std::string& path)
{
	std::ofstream fout(path, std::ios::binary);
	if (!fout.is_open())
	{
		HY_APP_ERROR("Failed to open file for writing: {}", path);
		return;
	}

	size_t jointsSize = m_Joints.size();
	fout.write(reinterpret_cast<const char*>(&jointsSize), sizeof(size_t));

	for (const auto& joint : m_Joints)
	{
		size_t nameSize = joint.Name.size();
		fout.write(reinterpret_cast<const char*>(&nameSize), sizeof(size_t));
		fout.write(joint.Name.data(), nameSize);

		fout.write(reinterpret_cast<const char*>(&joint.ParentIndex), sizeof(int));
		fout.write(reinterpret_cast<const char*>(&joint.InverseBindMatrix), sizeof(glm::mat4));
	}
	fout.close();
}

void SkeletonAsset::ReadAssetFile(const std::string& path)
{
	std::ifstream fin(path, std::ios::binary);
	if (!fin.is_open())
	{
		HY_APP_ERROR("Failed to open file for reading: {}", path);
		return;
	}

	size_t jointsSize = 0;
	fin.read(reinterpret_cast<char*>(&jointsSize), sizeof(size_t));

	m_Joints.resize(jointsSize);
	for (size_t i = 0; i < jointsSize; ++i)
	{
		size_t nameSize = 0;
		fin.read(reinterpret_cast<char*>(&nameSize), sizeof(size_t));

		m_Joints[i].Name.resize(nameSize);
		fin.read(&m_Joints[i].Name[0], nameSize);

		fin.read(reinterpret_cast<char*>(&m_Joints[i].ParentIndex), sizeof(int));
		fin.read(reinterpret_cast<char*>(&m_Joints[i].InverseBindMatrix), sizeof(glm::mat4));
	}
	fin.close();
}

RenderBuffer* StaticMeshAsset::GetVertexBuffer()
{
	if (!m_VertexBuffer)
	{
		BufferDescription vertexBufferDesc;
		vertexBufferDesc.cpuVisible = false;
		vertexBufferDesc.size = m_Vertices.size() * sizeof(StaticVertex);
		vertexBufferDesc.type = BufferType::Vertex;

		m_VertexBuffer = std::make_unique<RenderBuffer>(Application::Get()->GetRenderDevice(), vertexBufferDesc);
		m_VertexBuffer->UploadDataStaging(m_Vertices.data(), vertexBufferDesc.size);
	}

	return m_VertexBuffer.get();
}

RenderBuffer* StaticMeshAsset::GetIndexBuffer()
{
	if (!m_IndexBuffer)
	{
		BufferDescription indexBufferDesc;
		indexBufferDesc.cpuVisible = false;
		indexBufferDesc.size = m_Indices.size() * sizeof(uint32_t);
		indexBufferDesc.type = BufferType::Index;

		m_IndexBuffer = std::make_unique<RenderBuffer>(Application::Get()->GetRenderDevice(), indexBufferDesc);
		m_IndexBuffer->UploadDataStaging(m_Indices.data(), indexBufferDesc.size);
	}

	return m_IndexBuffer.get();
}

void StaticMeshAsset::WriteAssetFile(const std::string& path)
{
	std::ofstream fout(path, std::ios::binary);
	if (!fout.is_open())
	{
		HY_APP_ERROR("Failed to open file for writing: {}", path);
		return;
	}

	size_t vertexCount = m_Vertices.size();
	size_t indexCount = m_Indices.size();

	fout.write(reinterpret_cast<const char*>(&vertexCount), sizeof(size_t));
	fout.write(reinterpret_cast<const char*>(&indexCount), sizeof(size_t));

	fout.write(reinterpret_cast<const char*>(m_Vertices.data()), vertexCount * sizeof(StaticVertex));
	fout.write(reinterpret_cast<const char*>(m_Indices.data()), indexCount * sizeof(uint32_t));

	fout.close();
}

void StaticMeshAsset::ReadAssetFile(const std::string& path)
{
	std::ifstream fin(path, std::ios::binary);
	if (!fin.is_open())
	{
		HY_APP_ERROR("Failed to open file for reading: {}", path);
		return;
	}

	size_t vertexCount = 0;
	size_t indexCount = 0;

	fin.read(reinterpret_cast<char*>(&vertexCount), sizeof(size_t));
	fin.read(reinterpret_cast<char*>(&indexCount), sizeof(size_t));

	m_Vertices.resize(vertexCount);
	m_Indices.resize(indexCount);

	fin.read(reinterpret_cast<char*>(m_Vertices.data()), vertexCount * sizeof(StaticVertex));
	fin.read(reinterpret_cast<char*>(m_Indices.data()), indexCount * sizeof(uint32_t));

	fin.close();
}

RenderBuffer* SkeletalMeshAsset::GetVertexBuffer()
{
	if (!m_VertexBuffer)
	{
		BufferDescription vertexBufferDesc;
		vertexBufferDesc.cpuVisible = false;
		vertexBufferDesc.size = m_Vertices.size() * sizeof(SkinnedVertex);
		vertexBufferDesc.type = BufferType::Vertex;

		m_VertexBuffer = std::make_unique<RenderBuffer>(Application::Get()->GetRenderDevice(), vertexBufferDesc);
		m_VertexBuffer->UploadDataStaging(m_Vertices.data(), vertexBufferDesc.size);
	}

	return m_VertexBuffer.get();
}

RenderBuffer* SkeletalMeshAsset::GetIndexBuffer()
{
	if (!m_IndexBuffer)
	{
		BufferDescription indexBufferDesc;
		indexBufferDesc.cpuVisible = false;
		indexBufferDesc.size = m_Indices.size() * sizeof(uint32_t);
		indexBufferDesc.type = BufferType::Index;

		m_IndexBuffer = std::make_unique<RenderBuffer>(Application::Get()->GetRenderDevice(), indexBufferDesc);
		m_IndexBuffer->UploadDataStaging(m_Indices.data(), indexBufferDesc.size);
	}

	return m_IndexBuffer.get();
}

void SkeletalMeshAsset::WriteAssetFile(const std::string& path)
{
	std::ofstream fout(path, std::ios::binary);
	if (!fout.is_open())
	{
		HY_APP_ERROR("Failed to open file for writing: {}", path);
		return;
	}

	size_t vertexCount = m_Vertices.size();
	size_t indexCount = m_Indices.size();

	fout.write(reinterpret_cast<const char*>(&vertexCount), sizeof(size_t));
	fout.write(reinterpret_cast<const char*>(&indexCount), sizeof(size_t));

	fout.write(reinterpret_cast<const char*>(m_Vertices.data()), vertexCount * sizeof(SkinnedVertex));
	fout.write(reinterpret_cast<const char*>(m_Indices.data()), indexCount * sizeof(uint32_t));

	fout.close();
}

void SkeletalMeshAsset::ReadAssetFile(const std::string& path)
{
	std::ifstream fin(path, std::ios::binary);
	if (!fin.is_open()) {
		HY_APP_ERROR("Failed to open file for reading: {}", path);
		return;
	}

	size_t vertexCount = 0;
	size_t indexCount = 0;

	fin.read(reinterpret_cast<char*>(&vertexCount), sizeof(size_t));
	fin.read(reinterpret_cast<char*>(&indexCount), sizeof(size_t));

	m_Vertices.resize(vertexCount);
	m_Indices.resize(indexCount);

	fin.read(reinterpret_cast<char*>(m_Vertices.data()), vertexCount * sizeof(SkinnedVertex));
	fin.read(reinterpret_cast<char*>(m_Indices.data()), indexCount * sizeof(uint32_t));

	fin.close();
}

void AnimationAsset::WriteAssetFile(const std::string& path)
{
	std::ofstream fout(path, std::ios::binary);
	if (!fout.is_open())
	{
		HY_APP_ERROR("Failed to open file for writing: {}", path);
		return;
	}

	fout.write(reinterpret_cast<const char*>(&m_Duration), sizeof(float));
	fout.write(reinterpret_cast<const char*>(&m_TicksPerSecond), sizeof(float));

	size_t channelCount = m_Channels.size();
	fout.write(reinterpret_cast<const char*>(&channelCount), sizeof(size_t));

	for (const auto& channel : m_Channels)
	{
		size_t nameSize = channel.BoneName.size();
		fout.write(reinterpret_cast<const char*>(&nameSize), sizeof(size_t));
		fout.write(channel.BoneName.data(), nameSize);

		size_t posSize = channel.PositionKeys.size();
		fout.write(reinterpret_cast<const char*>(&posSize), sizeof(size_t));
		fout.write(reinterpret_cast<const char*>(channel.PositionKeys.data()), posSize * sizeof(VectorKey));

		size_t rotSize = channel.RotationKeys.size();
		fout.write(reinterpret_cast<const char*>(&rotSize), sizeof(size_t));
		fout.write(reinterpret_cast<const char*>(channel.RotationKeys.data()), rotSize * sizeof(QuatKey));

		size_t scaleSize = channel.ScaleKeys.size();
		fout.write(reinterpret_cast<const char*>(&scaleSize), sizeof(size_t));
		fout.write(reinterpret_cast<const char*>(channel.ScaleKeys.data()), scaleSize * sizeof(VectorKey));
	}
	fout.close();
}

void AnimationAsset::ReadAssetFile(const std::string& path)
{
	std::ifstream fin(path, std::ios::binary);
	if (!fin.is_open())
	{
		HY_APP_ERROR("Failed to open file for reading: {}", path);
		return;
	}

	fin.read(reinterpret_cast<char*>(&m_Duration), sizeof(float));
	fin.read(reinterpret_cast<char*>(&m_TicksPerSecond), sizeof(float));

	size_t channelCount = 0;
	fin.read(reinterpret_cast<char*>(&channelCount), sizeof(size_t));

	m_Channels.resize(channelCount);
	for (size_t i = 0; i < channelCount; ++i)
	{
		auto& channel = m_Channels[i];

		size_t nameSize = 0;
		fin.read(reinterpret_cast<char*>(&nameSize), sizeof(size_t));
		channel.BoneName.resize(nameSize);
		fin.read(&channel.BoneName[0], nameSize);

		size_t posSize = 0;
		fin.read(reinterpret_cast<char*>(&posSize), sizeof(size_t));
		channel.PositionKeys.resize(posSize);
		fin.read(reinterpret_cast<char*>(channel.PositionKeys.data()), posSize * sizeof(VectorKey));

		size_t rotSize = 0;
		fin.read(reinterpret_cast<char*>(&rotSize), sizeof(size_t));
		channel.RotationKeys.resize(rotSize);
		fin.read(reinterpret_cast<char*>(channel.RotationKeys.data()), rotSize * sizeof(QuatKey));

		size_t scaleSize = 0;
		fin.read(reinterpret_cast<char*>(&scaleSize), sizeof(size_t));
		channel.ScaleKeys.resize(scaleSize);
		fin.read(reinterpret_cast<char*>(channel.ScaleKeys.data()), scaleSize * sizeof(VectorKey));
	}
	fin.close();
}

void SceneAsset::Load(AssetManager* assetManager)
{
	m_Scene = std::make_shared<Scene>();
	m_Scene->DeserializeScene(json::parse(m_Content), assetManager);
}

void SceneAsset::Save() const
{
	std::string content = m_Scene->SerializeScene().dump();

	std::ofstream fout(m_Filepath);
	fout << content;
	fout.close();
}

void SceneAsset::ClearScene()
{
	m_Scene = std::make_shared<Scene>();
}

void MaterialAsset::Parse()
{
	auto material = json::parse(m_Content);

	m_AlbedoMapFilename = material.value("Albedo", "");
	m_NormalMapFilename = material.value("Normal", "");
	m_ORMMapFilename = material.value("ORM", "");
	m_EmissiveMapFilename = material.value("EmissiveMap", "");

	m_RoughnessFactor = material.value("Roughness", 0.5f);
	m_MetallicFactor = material.value("Metallic", 0.0f);

	const auto& emissive = material.value("Emissive", nlohmann::json::object());

	float r = emissive.value("r", 0.0f);
	float g = emissive.value("g", 0.0f);
	float b = emissive.value("b", 0.0f);
	float intensity = emissive.value("intensity", 0.0f);

	m_Emissive = glm::vec4(r, g, b, intensity);

	const auto& tint = material.value("Tint", nlohmann::json::object());

	r = tint.value("r", 1.0f);
	g = tint.value("g", 1.0f);
	b = tint.value("b", 1.0f);

	m_Tint = glm::vec3(r, g, b);
}

void MaterialAsset::Save() const
{
	auto material = json();

	material["Albedo"] = m_AlbedoMapFilename;
	material["Normal"] = m_NormalMapFilename;
	material["ORM"] = m_ORMMapFilename;
	material["EmissiveMap"] = m_EmissiveMapFilename;

	material["Roughness"] = m_RoughnessFactor;
	material["Metallic"] = m_MetallicFactor;

	material["Emissive"] = { { "r", m_Emissive.r }, { "g", m_Emissive.g }, { "b", m_Emissive.b }, { "intensity", m_Emissive.a }};
	material["Tint"] = { { "r", m_Tint.r }, { "g", m_Tint.g }, { "b", m_Tint.b } };

	std::ofstream fout(m_Filepath);
	fout << material;
	fout.close();
}

std::shared_ptr<TextureAsset> MaterialAsset::GetAlbedoMap()
{
	if (m_AlbedoMapFilename == "")
		return nullptr;

	if (m_AlbedoMap)
		return m_AlbedoMap;

	m_AlbedoMap = Application::Get()->MainAssetManager.GetAsset<TextureAsset>(m_AlbedoMapFilename);
	return m_AlbedoMap;
}

std::shared_ptr<TextureAsset> MaterialAsset::GetNormalMap()
{
	if (m_NormalMapFilename == "")
		return nullptr;

	if (m_NormalMap)
		return m_NormalMap;

	m_NormalMap = Application::Get()->MainAssetManager.GetAsset<TextureAsset>(m_NormalMapFilename);
	return m_NormalMap;
}

std::shared_ptr<TextureAsset> MaterialAsset::GetORMMap()
{
	if (m_ORMMapFilename == "")
		return nullptr;

	if (m_ORMMap)
		return m_ORMMap;

	m_ORMMap = Application::Get()->MainAssetManager.GetAsset<TextureAsset>(m_ORMMapFilename);
	return m_ORMMap;
}

std::shared_ptr<TextureAsset> MaterialAsset::GetEmissiveMap()
{
	if (m_EmissiveMapFilename == "")
		return nullptr;

	if (m_EmissiveMap)
		return m_EmissiveMap;

	m_EmissiveMap = Application::Get()->MainAssetManager.GetAsset<TextureAsset>(m_EmissiveMapFilename);
	return m_EmissiveMap;
}

void CubeMapAsset::Parse(std::string path)
{
	std::ifstream fin(path);
	std::stringstream buffer;
	buffer << fin.rdbuf();
	std::string content = std::move(buffer.str());
	fin.close();

	auto map = json::parse(content);

	auto& faces = map.at("faces");
	std::vector<std::string> faceAssets(6);
	for (size_t i = 0; i < 6; i++)
	{
		if (i < faces.size())
		{
			faceAssets[i] = faces[i].get<std::string>();
		}
		else
		{
			HY_ENGINE_WARN("Not enough faces specified for cubemap '{}'", path);
			faceAssets[i] = "";
			return;
		}
	}

	std::vector<std::shared_ptr<TextureAsset>> textures(6);
	for (size_t i = 0; i < 6; i++)
	{
		if (faceAssets[i] != "")
		{
			textures[i] = Application::Get()->MainAssetManager.GetAsset<TextureAsset>(faceAssets[i]);
		}
		else
		{
			HY_ENGINE_WARN("Face {} of cubemap '{}' is not specified", i, path);
			return;
		}

		if (textures[i]->GetWidth() != textures[0]->GetWidth() ||
			textures[i]->GetHeight() != textures[0]->GetHeight())
		{
			HY_ENGINE_ERROR("All cubemap faces must have the same dimensions");
			return;
		}
	}

	std::vector<uint32_t> cubeData;
	for (size_t i = 0; i < 6; i++)
	{
		const auto& imageData = textures[i]->GetImageData();
		cubeData.insert(cubeData.end(), imageData.begin(), imageData.end());
	}

	//m_CubeMap = CubeMap::Create(m_RenderContext, TextureFormat::FormatR8G8B8A8, textures[0]->GetWidth(), textures[0]->GetHeight());
	//m_CubeMap->UploadData(cubeData.data());
}
