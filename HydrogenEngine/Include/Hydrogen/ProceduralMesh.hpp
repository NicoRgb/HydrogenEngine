#pragma once

#include <vector>
#include <glm/glm.hpp>

namespace Hydrogen
{
	struct MeshData
	{
		std::vector<glm::vec3> Vertices;
		std::vector<uint32_t> Indices;
	};

	struct SphereData
	{
		std::vector<float> Vertices;
		std::vector<uint32_t> Indices;
	};

	SphereData GenerateUVSphere(uint32_t sectors, uint32_t stacks);
	MeshData GenerateBox(glm::vec3 size);
	MeshData GenerateSphere(float radius, uint32_t sectors = 16, uint32_t stacks = 16);
	MeshData GenerateCapsule(float radius, float height, uint32_t sectors = 16, uint32_t stacks = 8);
	MeshData GenerateCylinder(float radius, float height, uint32_t sectors = 16);
}
