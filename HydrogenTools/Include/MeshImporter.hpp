#pragma once

#include <Hydrogen/Hydrogen.hpp>

using namespace Hydrogen;

#define ASSIMP_DLL

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

inline glm::mat4 AiToGlmMat(const aiMatrix4x4& from)
{
	glm::mat4 to;
	to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
	to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
	to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
	to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
	return to;
}

class AssimpSkeletonAsset
{
public:
	AssimpSkeletonAsset() = default;

	void BuildFromAssimp(const aiScene* scene);

	const std::vector<Joint>& GetJoints() const { return m_Joints; }
	int FindJointIndex(const std::string& name) const;

	void WriteAssetFile(const std::string& path);

private:
	void TraverseNode(const aiNode* node, int parentIdx, const std::unordered_map<std::string, glm::mat4>& activeBones);

	std::vector<Joint> m_Joints;
};

class AssimpSkeletalMeshAsset
{
public:
	AssimpSkeletalMeshAsset(std::string path, std::shared_ptr<AssimpSkeletonAsset> skeleton, std::shared_ptr<class AssimpAnimationAsset> animation)
		: m_Skeleton(skeleton), m_Animation(animation)
	{
		Parse(path);
	}

	uint32_t GetIndexCount() const { return static_cast<uint32_t>(m_Indices.size()); }

	void WriteAssetFile(const std::string& path);

private:
	void Parse(std::string path);
	void AddBoneWeight(SkinnedVertex& vertex, int boneID, float weight);

	std::vector<SkinnedVertex> m_Vertices;
	std::vector<uint32_t> m_Indices;

	std::shared_ptr<AssimpSkeletonAsset> m_Skeleton;
	std::shared_ptr<class AssimpAnimationAsset> m_Animation;
};

class AssimpStaticMeshAsset
{
public:
	AssimpStaticMeshAsset(std::string path)
	{
		Parse(path);
	}

	uint32_t GetIndexCount() const { return static_cast<uint32_t>(m_Indices.size()); }

	void WriteAssetFile(const std::string& path);

private:
	void Parse(std::string path);

	std::vector<StaticVertex> m_Vertices;
	std::vector<uint32_t> m_Indices;
};

class AssimpAnimationAsset
{
public:
	AssimpAnimationAsset() = default;

	void BuildFromAssimp(const aiScene* scene);

	float GetDuration() const { return m_Duration; }
	float GetTicksPerSecond() const { return m_TicksPerSecond; }
	const std::vector<BoneChannel>& GetChannels() const { return m_Channels; }

	void WriteAssetFile(const std::string& path);

private:
	float m_Duration = 0.0f;
	float m_TicksPerSecond = 0.0f;
	std::vector<BoneChannel> m_Channels;
};
