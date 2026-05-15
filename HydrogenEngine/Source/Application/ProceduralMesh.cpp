#define _USE_MATH_DEFINES
#include <cmath>

#include "Hydrogen/ProceduralMesh.hpp"

using namespace Hydrogen;

SphereData Hydrogen::GenerateUVSphere(uint32_t sectors, uint32_t stacks)
{
    SphereData data;
    float radius = 1.0f;

    float x, y, z, xy; // vertex position
    float nx, ny, nz, lengthInv = 1.0f / radius; // vertex normal
    float s, t; // vertex texCoords

    float sectorStep = 2 * M_PI / sectors;
    float stackStep = M_PI / stacks;
    float sectorAngle, stackAngle;

    for (uint32_t i = 0; i <= stacks; ++i) {
        stackAngle = M_PI / 2 - i * stackStep; // starting from pi/2 to -pi/2
        xy = radius * cosf(stackAngle); // r * cos(u)
        z = radius * sinf(stackAngle); // r * sin(u)

        for (uint32_t j = 0; j <= sectors; ++j) {
            sectorAngle = j * sectorStep; // starting from 0 to 2pi

            // position
            x = xy * cosf(sectorAngle); // r * cos(u) * cos(v)
            y = xy * sinf(sectorAngle); // r * cos(u) * sin(v)

            data.Vertices.push_back(x);
            data.Vertices.push_back(y);
            data.Vertices.push_back(z);

            // uv
            s = (float)j / sectors;
            t = (float)i / stacks;
            data.Vertices.push_back(s);
            data.Vertices.push_back(t);

            // normal
            data.Vertices.push_back(x * lengthInv);
            data.Vertices.push_back(y * lengthInv);
            data.Vertices.push_back(z * lengthInv);
        }
    }

    // indices
    uint32_t k1, k2;
    for (uint32_t i = 0; i < stacks; ++i) {
        k1 = i * (sectors + 1); // beginning of current stack
        k2 = k1 + sectors + 1; // beginning of next stack

        for (uint32_t j = 0; j < sectors; ++j, ++k1, ++k2) {
            if (i != 0) {
                data.Indices.push_back(k1);
                data.Indices.push_back(k2);
                data.Indices.push_back(k1 + 1);
            }

            if (i != (stacks - 1)) {
                data.Indices.push_back(k1 + 1);
                data.Indices.push_back(k2);
                data.Indices.push_back(k2 + 1);
            }
        }
    }
    return data;
}
