#define _USE_MATH_DEFINES
#include <cmath>

#include "Hydrogen/ProceduralMesh.hpp"

using namespace Hydrogen;

SphereData Hydrogen::GenerateUVSphere(uint32_t sectors, uint32_t stacks)
{
	SphereData data;
	float radius = 1.0f;

	float x, y, z, xy; // vertex position
	float lengthInv = 1.0f / radius; // vertex normal
	float s, t; // vertex texCoordsne 

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

MeshData Hydrogen::GenerateBox(glm::vec3 size)
{
	MeshData data;
	glm::vec3 halfSize = size * 0.5f;

	std::vector<glm::vec3> vertices = {
		// Front face
		glm::vec3(-halfSize.x, -halfSize.y, halfSize.z),  // 0
		glm::vec3(halfSize.x, -halfSize.y, halfSize.z),   // 1
		glm::vec3(halfSize.x, halfSize.y, halfSize.z),    // 2
		glm::vec3(-halfSize.x, halfSize.y, halfSize.z),   // 3
		// Back face
		glm::vec3(-halfSize.x, -halfSize.y, -halfSize.z), // 4
		glm::vec3(halfSize.x, -halfSize.y, -halfSize.z),  // 5
		glm::vec3(halfSize.x, halfSize.y, -halfSize.z),   // 6
		glm::vec3(-halfSize.x, halfSize.y, -halfSize.z),  // 7
	};

	data.Vertices = vertices;

	std::vector<uint32_t> indices = {
		// Front face
		0, 1, 2,
		0, 2, 3,
		// Back face
		4, 6, 5,
		4, 7, 6,
		// Top face
		3, 2, 6,
		3, 6, 7,
		// Bottom face
		4, 5, 1,
		4, 1, 0,
		// Right face
		1, 5, 6,
		1, 6, 2,
		// Left face
		4, 0, 3,
		4, 3, 7,
	};

	data.Indices = indices;
	return data;
}

MeshData Hydrogen::GenerateSphere(float radius, uint32_t sectors, uint32_t stacks)
{
	MeshData data;

	float x, y, z, xy;
	float sectorStep = 2 * M_PI / sectors;
	float stackStep = M_PI / stacks;
	float sectorAngle, stackAngle;

	for (uint32_t i = 0; i <= stacks; ++i)
	{
		stackAngle = M_PI / 2 - i * stackStep;
		xy = radius * cosf(stackAngle);
		z = radius * sinf(stackAngle);

		for (uint32_t j = 0; j <= sectors; ++j)
		{
			sectorAngle = j * sectorStep;

			x = xy * cosf(sectorAngle);
			y = xy * sinf(sectorAngle);

			data.Vertices.push_back(glm::vec3(x, y, z));
		}
	}

	uint32_t k1, k2;
	for (uint32_t i = 0; i < stacks; ++i)
	{
		k1 = i * (sectors + 1);
		k2 = k1 + sectors + 1;

		for (uint32_t j = 0; j < sectors; ++j, ++k1, ++k2)
		{
			if (i != 0)
			{
				data.Indices.push_back(k1);
				data.Indices.push_back(k2);
				data.Indices.push_back(k1 + 1);
			}

			if (i != (stacks - 1))
			{
				data.Indices.push_back(k1 + 1);
				data.Indices.push_back(k2);
				data.Indices.push_back(k2 + 1);
			}
		}
	}

	return data;
}

MeshData Hydrogen::GenerateCapsule(float radius, float height, uint32_t sectors, uint32_t stacks)
{
	MeshData data;
	float halfHeight = height * 0.5f;

	float sectorStep = 2 * M_PI / sectors;
	float stackStep = M_PI / (stacks * 2);

	uint32_t topHemisphereStart = 0;
	for (uint32_t i = 0; i <= stacks; ++i)
	{
		float stackAngle = M_PI / 2 - i * stackStep;
		float xy = radius * cosf(stackAngle);
		float z = radius * sinf(stackAngle) + halfHeight;

		for (uint32_t j = 0; j <= sectors; ++j)
		{
			float sectorAngle = j * sectorStep;
			float x = xy * cosf(sectorAngle);
			float y = xy * sinf(sectorAngle);
			data.Vertices.push_back(glm::vec3(x, y, z));
		}
	}

	for (uint32_t i = 0; i < stacks; ++i)
	{
		uint32_t k1 = topHemisphereStart + i * (sectors + 1);
		uint32_t k2 = k1 + sectors + 1;

		for (uint32_t j = 0; j < sectors; ++j)
		{
			data.Indices.push_back(k1 + j);
			data.Indices.push_back(k2 + j);
			data.Indices.push_back(k1 + j + 1);

			data.Indices.push_back(k1 + j + 1);
			data.Indices.push_back(k2 + j);
			data.Indices.push_back(k2 + j + 1);
		}
	}

	uint32_t cylinderStart = data.Vertices.size();
	for (uint32_t j = 0; j <= sectors; ++j)
	{
		float sectorAngle = j * sectorStep;
		float x = radius * cosf(sectorAngle);
		float y = radius * sinf(sectorAngle);

		data.Vertices.push_back(glm::vec3(x, y, halfHeight));
		data.Vertices.push_back(glm::vec3(x, y, -halfHeight));
	}

	for (uint32_t j = 0; j < sectors; ++j)
	{
		uint32_t topA = cylinderStart + j * 2;
		uint32_t topB = cylinderStart + (j + 1) * 2;
		uint32_t botA = topA + 1;
		uint32_t botB = topB + 1;

		data.Indices.push_back(topA);
		data.Indices.push_back(topB);
		data.Indices.push_back(botA);

		data.Indices.push_back(topB);
		data.Indices.push_back(botB);
		data.Indices.push_back(botA);
	}

	uint32_t bottomHemisphereStart = data.Vertices.size();
	for (uint32_t i = 0; i <= stacks; ++i)
	{
		float stackAngle = M_PI / 2 - i * stackStep;
		float xy = radius * cosf(stackAngle);
		float z = -radius * sinf(stackAngle) - halfHeight;

		for (uint32_t j = 0; j <= sectors; ++j)
		{
			float sectorAngle = j * sectorStep;
			float x = xy * cosf(sectorAngle);
			float y = xy * sinf(sectorAngle);
			data.Vertices.push_back(glm::vec3(x, y, z));
		}
	}

	for (uint32_t i = 0; i < stacks; ++i)
	{
		uint32_t k1 = bottomHemisphereStart + i * (sectors + 1);
		uint32_t k2 = k1 + sectors + 1;

		for (uint32_t j = 0; j < sectors; ++j)
		{
			data.Indices.push_back(k1 + j);
			data.Indices.push_back(k1 + j + 1);
			data.Indices.push_back(k2 + j);

			data.Indices.push_back(k1 + j + 1);
			data.Indices.push_back(k2 + j + 1);
			data.Indices.push_back(k2 + j);
		}
	}

	return data;
}

MeshData Hydrogen::GenerateCylinder(float radius, float height, uint32_t sectors)
{
	MeshData data;
	float halfHeight = height * 0.5f;
	float sectorStep = 2 * M_PI / sectors;

	data.Vertices.push_back(glm::vec3(0.0f, 0.0f, halfHeight));
	uint32_t topCenterIdx = 0;

	for (uint32_t j = 0; j <= sectors; ++j)
	{
		float angle = j * sectorStep;
		float x = radius * cosf(angle);
		float y = radius * sinf(angle);
		data.Vertices.push_back(glm::vec3(x, y, halfHeight));
	}
	uint32_t topCircleStart = 1;

	data.Vertices.push_back(glm::vec3(0.0f, 0.0f, -halfHeight));
	uint32_t bottomCenterIdx = topCircleStart + sectors + 1;

	for (uint32_t j = 0; j <= sectors; ++j)
	{
		float angle = j * sectorStep;
		float x = radius * cosf(angle);
		float y = radius * sinf(angle);
		data.Vertices.push_back(glm::vec3(x, y, -halfHeight));
	}
	uint32_t bottomCircleStart = bottomCenterIdx + 1;

	for (uint32_t j = 0; j < sectors; ++j)
	{
		data.Indices.push_back(topCenterIdx);
		data.Indices.push_back(topCircleStart + j + 1);
		data.Indices.push_back(topCircleStart + j);
	}

	for (uint32_t j = 0; j < sectors; ++j)
	{
		data.Indices.push_back(bottomCenterIdx);
		data.Indices.push_back(bottomCircleStart + j);
		data.Indices.push_back(bottomCircleStart + j + 1);
	}

	for (uint32_t j = 0; j < sectors; ++j)
	{
		uint32_t topA = topCircleStart + j;
		uint32_t topB = topCircleStart + j + 1;
		uint32_t botA = bottomCircleStart + j;
		uint32_t botB = bottomCircleStart + j + 1;

		data.Indices.push_back(topA);
		data.Indices.push_back(topB);
		data.Indices.push_back(botA);

		data.Indices.push_back(botA);
		data.Indices.push_back(topB);
		data.Indices.push_back(botB);
	}

	return data;
}
