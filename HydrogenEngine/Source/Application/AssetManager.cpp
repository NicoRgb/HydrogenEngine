#include "Hydrogen/AssetManager.hpp"

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

void AssetManager::LoadAssets(const std::string& directory, const std::shared_ptr<RenderContext>& renderContext)
{
	HY_ASSERT(fs::exists(directory), "Asset directory '{}' does not exist", directory);

	m_Directory = directory;
	m_RenderContext = renderContext;

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
			auto texture = std::make_shared<TextureAsset>(filePath, assetConfig, renderContext);
			m_Assets[entry.path().filename().string()] = std::move(texture);
		}
		else if (assetConfig["type"] == "Mesh")
		{
			auto mesh = std::make_shared<MeshAsset>(filePath, assetConfig, renderContext);
			m_Assets[entry.path().filename().string()] = std::move(mesh);
		}
		else
		{
			HY_ENGINE_ERROR("Unknown asset type for file '{}'", filePath);
		}
	}
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

	m_Image.resize(m_Width * m_Height * 4);

	memcpy(m_Image.data(), data, m_Image.size());
	stbi_image_free(data);

	m_Texture = Texture::Create(m_RenderContext, TextureFormat::FormatR8G8B8A8, m_Width, m_Height);
	m_Texture->UploadData((void*)m_Image.data());
}

void MeshAsset::Parse(std::string path)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;
	
	HY_ASSERT(tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str()), "Failed to load object {}", err);

	for (const auto& shape : shapes) // TODO: this is really rudimentary -> no support for materials, textures and duplicate vertices
	{
		for (const auto& index : shape.mesh.indices)
		{
			m_Vertices.push_back(attrib.vertices[3 * index.vertex_index + 0]);
			m_Vertices.push_back(attrib.vertices[3 * index.vertex_index + 1]);
			m_Vertices.push_back(attrib.vertices[3 * index.vertex_index + 2]);
			m_Vertices.push_back(1.0f);
			m_Vertices.push_back(1.0f);
			m_Vertices.push_back(1.0f);
			m_Vertices.push_back(attrib.texcoords[2 * index.texcoord_index + 0]);
			m_Vertices.push_back(1.0f - attrib.texcoords[2 * index.texcoord_index + 1]);

			m_Indices.push_back((uint32_t)m_Indices.size());
		}
	}

	m_VertexBuffer = VertexBuffer::Create(m_RenderContext, { {VertexElementType::Float3}, {VertexElementType::Float3}, {VertexElementType::Float2} }, (void*)m_Vertices.data(), m_Vertices.size() / 8);
	m_IndexBuffer = IndexBuffer::Create(m_RenderContext, m_Indices);
}
