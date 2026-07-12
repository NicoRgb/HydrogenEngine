#include "Hydrogen/AssetManager.hpp"
#include "Hydrogen/Scene.hpp"
#include "Hydrogen/Application.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

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
			else if (ext == ".obj")
			{
				assetType = "Mesh";
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
		else if (assetConfig["type"] == "Mesh")
		{
			auto mesh = std::make_shared<MeshAsset>(filePath, assetConfig);
			m_Assets[entry.path().filename().string()] = std::move(mesh);
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

	//m_Texture = Texture::Create(m_RenderContext, TextureFormat::FormatR8G8B8A8, m_Width, m_Height);
	//m_Texture->UploadData((void*)m_Image.data());
}

const RenderBuffer* MeshAsset::GetVertexBuffer(RenderDevice* device)
{
	if (!m_VertexBuffer)
	{
		BufferDescription vertexBufferDesc;
		vertexBufferDesc.cpuVisible = false;
		vertexBufferDesc.size = m_Vertices.size() * sizeof(float);
		vertexBufferDesc.type = BufferType::Vertex;

		m_VertexBuffer = std::make_unique<RenderBuffer>(device, vertexBufferDesc);
		m_VertexBuffer->UploadDataStaging(m_Vertices.data(), vertexBufferDesc.size);
	}

	return m_VertexBuffer.get();
}

const RenderBuffer* MeshAsset::GetIndexBuffer(RenderDevice* device)
{
	if (!m_IndexBuffer)
	{
		BufferDescription indexBufferDesc;
		indexBufferDesc.cpuVisible = false;
		indexBufferDesc.size = m_Indices.size() * sizeof(uint32_t);
		indexBufferDesc.type = BufferType::Index;

		m_IndexBuffer = std::make_unique<RenderBuffer>(device, indexBufferDesc);
		m_IndexBuffer->UploadDataStaging(m_Indices.data(), indexBufferDesc.size);
	}

	return m_IndexBuffer.get();
}

void MeshAsset::Parse(std::string path)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;

	HY_ASSERT(tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str()), "Failed to load object {}", err);

	for (const auto& shape : shapes)
	{
		for (size_t i = 0; i < shape.mesh.indices.size(); i += 3)
		{
			tinyobj::index_t idx0 = shape.mesh.indices[i + 0];
			tinyobj::index_t idx1 = shape.mesh.indices[i + 1];
			tinyobj::index_t idx2 = shape.mesh.indices[i + 2];

			// positions
			glm::vec3 p0(attrib.vertices[3 * idx0.vertex_index + 0], attrib.vertices[3 * idx0.vertex_index + 1], attrib.vertices[3 * idx0.vertex_index + 2]);
			glm::vec3 p1(attrib.vertices[3 * idx1.vertex_index + 0], attrib.vertices[3 * idx1.vertex_index + 1], attrib.vertices[3 * idx1.vertex_index + 2]);
			glm::vec3 p2(attrib.vertices[3 * idx2.vertex_index + 0], attrib.vertices[3 * idx2.vertex_index + 1], attrib.vertices[3 * idx2.vertex_index + 2]);

			// UVs
			glm::vec2 uv0(attrib.texcoords[2 * idx0.texcoord_index + 0], 1.0f - attrib.texcoords[2 * idx0.texcoord_index + 1]);
			glm::vec2 uv1(attrib.texcoords[2 * idx1.texcoord_index + 0], 1.0f - attrib.texcoords[2 * idx1.texcoord_index + 1]);
			glm::vec2 uv2(attrib.texcoords[2 * idx2.texcoord_index + 0], 1.0f - attrib.texcoords[2 * idx2.texcoord_index + 1]);

			// tangents
			glm::vec3 edge1 = p1 - p0;
			glm::vec3 edge2 = p2 - p0;
			glm::vec2 deltaUV1 = uv1 - uv0;
			glm::vec2 deltaUV2 = uv2 - uv0;

			float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

			glm::vec3 tangent;
			if (std::isinf(f) || std::isnan(f))
			{
				tangent = glm::vec3(1.0f, 0.0f, 0.0f);
			}
			else
			{
				tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
				tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
				tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
				tangent = glm::normalize(tangent);
			}

			auto PushVertex = [&](const tinyobj::index_t& index, const glm::vec3& tng) {
				// position
				m_Vertices.push_back(attrib.vertices[3 * index.vertex_index + 0]);
				m_Vertices.push_back(attrib.vertices[3 * index.vertex_index + 1]);
				m_Vertices.push_back(attrib.vertices[3 * index.vertex_index + 2]);

				// UVs
				m_Vertices.push_back(attrib.texcoords[2 * index.texcoord_index + 0]);
				m_Vertices.push_back(1.0f - attrib.texcoords[2 * index.texcoord_index + 1]);

				// normals
				if (index.normal_index >= 0) {
					m_Vertices.push_back(attrib.normals[3 * index.normal_index + 0]);
					m_Vertices.push_back(attrib.normals[3 * index.normal_index + 1]);
					m_Vertices.push_back(attrib.normals[3 * index.normal_index + 2]);
				}
				else {
					m_Vertices.push_back(0.0f);
					m_Vertices.push_back(0.0f);
					m_Vertices.push_back(1.0f);
				}

				// tangents
				m_Vertices.push_back(tng.x);
				m_Vertices.push_back(tng.y);
				m_Vertices.push_back(tng.z);

				m_Indices.push_back((uint32_t)m_Indices.size());
				};

			PushVertex(idx0, tangent);
			PushVertex(idx1, tangent);
			PushVertex(idx2, tangent);
		}
	}

	//m_VertexBuffer = VertexBuffer::Create(m_RenderContext,
	//	{
	//		{VertexElementType::Float3}, // position
	//		{VertexElementType::Float2}, // UV
	//		{VertexElementType::Float3}, // normal
	//		{VertexElementType::Float3}  // tangent
	//	},
	//	(void*)m_Vertices.data(),
	//	m_Vertices.size() / 11
	//);
	
	//m_IndexBuffer = IndexBuffer::Create(m_RenderContext, m_Indices);
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
