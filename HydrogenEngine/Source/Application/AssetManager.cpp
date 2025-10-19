#include "Hydrogen/AssetManager.hpp"

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
	HY_ASSERT(fs::exists(directory), "Asset directory '{}' does not exist", directory);

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
			if (ext == ".png" || ext == ".jpg")
			{
				assetType = "Texture";
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
}
