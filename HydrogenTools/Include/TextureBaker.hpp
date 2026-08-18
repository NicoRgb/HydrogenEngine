#pragma once

#include <Hydrogen/Hydrogen.hpp>

using namespace Hydrogen;

struct TextureBakerConfig
{
    std::vector<std::string> InputPaths;

    // Channel routing indexes (-1 means map is omitted / defaults used)
    int AlbedoInputIndex = -1;
    int NormalInputIndex = -1;

    int OcclusionInputIndex = -1;
    int OcclusionChannel = 0; // Red channel

    int RoughnessInputIndex = -1;
    int RoughnessChannel = 1; // Green channel
    bool InvertRoughness = false;

    int MetallicInputIndex = -1;
    int MetallicChannel = 2; // Blue channel

    int EmissiveInputIndex = -1;

    bool FlipNormalY = false;

    std::string OutputAlbedoPath;
    std::string OutputNormalPath;
    std::string OutputOrmPath;
    std::string OutputEmissivePath;
};

class TextureBaker
{
public:
    static void Bake(const TextureBakerConfig& config);
};
