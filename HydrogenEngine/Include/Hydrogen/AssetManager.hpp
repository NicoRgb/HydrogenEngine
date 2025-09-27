#pragma once

#include "Hydrogen/Logger.hpp"
#include "Hydrogen/Core.hpp"

#include <json.hpp>

#include <filesystem>
#include <unordered_map>
#include <memory>
#include <fstream>
#include <chrono>
#include <string>

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

	protected:
		std::string m_Filepath;
		json m_Config;
	};

	class ShaderAsset : public Asset
	{
	public:
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

	private:
		std::string m_Content;
		std::vector<uint32_t> m_ByteCode;
	};

	class TextureAsset : public Asset
	{
	public:
		TextureAsset(std::string path, json config) : Asset(path, config)
		{
			Parse(path);
		}

		~TextureAsset() = default;

		void LoadCache(std::string cachePath) override
		{
		}

		void Cache() override
		{
		}

		const uint32_t GetWidth() const { return m_Width; }
		const uint32_t GetHeight() const { return m_Height; }
		const uint8_t GetChannels() const { return m_Channels; }

		const std::vector<uint32_t>& GetImage() const { return m_Image; }

	private:
		void Parse(std::string path);

		uint32_t m_Width, m_Height;
		uint8_t m_Channels;

		std::vector<uint32_t> m_Image;
	};

	class AssetManager
	{
	public:
		void LoadAssets(const std::string& directory);

		template<typename T>
		std::shared_ptr<T> GetAsset(std::string name)
		{
			static_assert(std::is_base_of_v<Asset, T>);

			return std::dynamic_pointer_cast<T>(m_Assets[name]);
		}

	private:
		std::unordered_map<std::string, std::shared_ptr<Asset>> m_Assets;
	};
}
