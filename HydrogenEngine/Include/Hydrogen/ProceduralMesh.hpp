#pragma once

#include <vector>

namespace Hydrogen
{
    struct SphereData
    {
        std::vector<float> Vertices;
        std::vector<uint32_t> Indices;
    };

    SphereData GenerateUVSphere(uint32_t sectors, uint32_t stacks);
}
