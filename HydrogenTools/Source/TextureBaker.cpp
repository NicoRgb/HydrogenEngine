#include <TextureBaker.hpp>

#include <stb_image.h>
#include <stb_image_write.h>

#include <stdexcept>
#include <algorithm>
#include <iostream>

struct RawImageBuffer
{
	int Width = 0;
	int Height = 0;
	int Channels = 0;
	std::vector<unsigned char> Pixels;
};

static RawImageBuffer LoadTextureMap(const std::string& filepath)
{
	RawImageBuffer img;

	int desiredChannels = 4;
	unsigned char* data = stbi_load(filepath.c_str(), &img.Width, &img.Height, &img.Channels, desiredChannels);

	if (!data)
	{
		throw std::runtime_error(std::string("[StbLoader] Failed to load texture: ") + filepath);
	}

	size_t dataSize = static_cast<size_t>(img.Width) * img.Height * desiredChannels;
	img.Pixels.assign(data, data + dataSize);
	img.Channels = desiredChannels;

	stbi_image_free(data);

	return img;
}

static void SaveTextureMapPNG(const std::string& outputPath, int width, int height, int channels, const std::vector<unsigned char>& pixels)
{
	int strideInBytes = width * channels;

	int success = stbi_write_png(
		outputPath.c_str(),
		width,
		height,
		channels,
		pixels.data(),
		strideInBytes
	);

	if (!success)
	{
		throw std::runtime_error(std::string("[StbWriter] Failed to save PNG to: ") + outputPath);
	}
}

static unsigned char SampleNearest(const RawImageBuffer& img, int targetX, int targetY, int targetWidth, int targetHeight, int channelIndex, unsigned char defaultValue)
{
	if (img.Pixels.empty() || img.Width <= 0 || img.Height <= 0)
	{
		return defaultValue;
	}

	int srcX = static_cast<int>((static_cast<float>(targetX) / targetWidth) * img.Width);
	int srcY = static_cast<int>((static_cast<float>(targetY) / targetHeight) * img.Height);

	srcX = std::clamp(srcX, 0, img.Width - 1);
	srcY = std::clamp(srcY, 0, img.Height - 1);

	int index = (srcY * img.Width + srcX) * 4 + channelIndex;
	if (index >= 0 && index < static_cast<int>(img.Pixels.size()))
	{
		return img.Pixels[index];
	}
	return defaultValue;
}

void TextureBaker::Bake(const TextureBakerConfig& config)
{
	std::vector<RawImageBuffer> loadedInputs;
	loadedInputs.reserve(config.InputPaths.size());

	int targetWidth = 0;
	int targetHeight = 0;

	for (const auto& path : config.InputPaths)
	{
		if (path.empty())
		{
			loadedInputs.push_back({});
			continue;
		}

		RawImageBuffer buf = LoadTextureMap(path);
		if (targetWidth == 0 && targetHeight == 0)
		{
			targetWidth = buf.Width;
			targetHeight = buf.Height;
		}
		loadedInputs.push_back(buf);
	}

	if (targetWidth == 0 || targetHeight == 0)
	{
		throw std::runtime_error("[TextureBaker] No valid input textures provided to determine bake resolution.");
	}

	auto getChannelVal = [&](int inputIdx, int channel, unsigned char defaultVal, int px, int py) -> unsigned char
		{
			if (inputIdx < 0 || inputIdx >= static_cast<int>(loadedInputs.size()))
			{
				return defaultVal;
			}
			return SampleNearest(loadedInputs[inputIdx], px, py, targetWidth, targetHeight, channel, defaultVal);
		};

	std::vector<unsigned char> albedoPixels(targetWidth * targetHeight * 4, 255);
	std::vector<unsigned char> normalPixels(targetWidth * targetHeight * 4, 255);
	std::vector<unsigned char> ormPixels(targetWidth * targetHeight * 4, 255);
	std::vector<unsigned char> emissivePixels(targetWidth * targetHeight * 4, 0);

	for (int y = 0; y < targetHeight; ++y)
	{
		for (int x = 0; x < targetWidth; ++x)
		{
			int pixelIdx = (y * targetWidth + x) * 4;

			if (config.AlbedoInputIndex >= 0)
			{
				albedoPixels[pixelIdx + 0] = getChannelVal(config.AlbedoInputIndex, 0, 255, x, y);
				albedoPixels[pixelIdx + 1] = getChannelVal(config.AlbedoInputIndex, 1, 255, x, y);
				albedoPixels[pixelIdx + 2] = getChannelVal(config.AlbedoInputIndex, 2, 255, x, y);
				albedoPixels[pixelIdx + 3] = getChannelVal(config.AlbedoInputIndex, 3, 255, x, y);
			}

			if (config.NormalInputIndex >= 0)
			{
				unsigned char nx = getChannelVal(config.NormalInputIndex, 0, 128, x, y);
				unsigned char ny = getChannelVal(config.NormalInputIndex, 1, 128, x, y);
				unsigned char nz = getChannelVal(config.NormalInputIndex, 2, 255, x, y);

				if (config.FlipNormalY)
				{
					ny = 255 - ny;
				}

				normalPixels[pixelIdx + 0] = nx;
				normalPixels[pixelIdx + 1] = ny;
				normalPixels[pixelIdx + 2] = nz;
				normalPixels[pixelIdx + 3] = 255;
			}
			else
			{
				normalPixels[pixelIdx + 0] = 128;
				normalPixels[pixelIdx + 1] = 128;
				normalPixels[pixelIdx + 2] = 255;
				normalPixels[pixelIdx + 3] = 255;
			}

			ormPixels[pixelIdx + 0] = (config.OcclusionInputIndex >= 0)
				? getChannelVal(config.OcclusionInputIndex, config.OcclusionChannel, 255, x, y) : 255;

			if (config.RoughnessInputIndex >= 0)
			{
				unsigned char rough = getChannelVal(config.RoughnessInputIndex, config.RoughnessChannel, 128, x, y);
				if (config.InvertRoughness) rough = 255 - rough;
				ormPixels[pixelIdx + 1] = rough;
			}
			else
			{
				ormPixels[pixelIdx + 1] = 128;
			}

			ormPixels[pixelIdx + 2] = (config.MetallicInputIndex >= 0)
				? getChannelVal(config.MetallicInputIndex, config.MetallicChannel, 0, x, y) : 0;

			ormPixels[pixelIdx + 3] = 255;

			if (config.EmissiveInputIndex >= 0)
			{
				emissivePixels[pixelIdx + 0] = getChannelVal(config.EmissiveInputIndex, 0, 0, x, y);
				emissivePixels[pixelIdx + 1] = getChannelVal(config.EmissiveInputIndex, 1, 0, x, y);
				emissivePixels[pixelIdx + 2] = getChannelVal(config.EmissiveInputIndex, 2, 0, x, y);
				emissivePixels[pixelIdx + 3] = 255;
			}
		}
	}

	if (!config.OutputAlbedoPath.empty())
		SaveTextureMapPNG(config.OutputAlbedoPath, targetWidth, targetHeight, 4, albedoPixels);

	if (!config.OutputNormalPath.empty())
		SaveTextureMapPNG(config.OutputNormalPath, targetWidth, targetHeight, 4, normalPixels);

	if (!config.OutputOrmPath.empty())
		SaveTextureMapPNG(config.OutputOrmPath, targetWidth, targetHeight, 4, ormPixels);

	if (!config.OutputEmissivePath.empty())
		SaveTextureMapPNG(config.OutputEmissivePath, targetWidth, targetHeight, 4, emissivePixels);
}
