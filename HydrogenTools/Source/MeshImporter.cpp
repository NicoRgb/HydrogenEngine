#include "MeshImporter.hpp"
#include <Hydrogen/Hydrogen.hpp>

#include <fstream>

void AssimpSkeletonAsset::BuildFromAssimp(const aiScene* scene)
{
	if (!scene || !scene->mRootNode) return;

	std::unordered_map<std::string, glm::mat4> uniqueBones;
	for (uint32_t m = 0; m < scene->mNumMeshes; ++m)
	{
		aiMesh* mesh = scene->mMeshes[m];
		for (uint32_t b = 0; b < mesh->mNumBones; ++b)
		{
			aiBone* bone = mesh->mBones[b];
			uniqueBones[bone->mName.C_Str()] = AiToGlmMat(bone->mOffsetMatrix);
		}
	}

	TraverseNode(scene->mRootNode, -1, uniqueBones);
}

int AssimpSkeletonAsset::FindJointIndex(const std::string& name) const
{
	for (size_t i = 0; i < m_Joints.size(); ++i)
	{
		if (m_Joints[i].Name == name) return static_cast<int>(i);
	}
	return -1;
}

void AssimpSkeletonAsset::WriteAssetFile(const std::string& path)
{
	std::ofstream fout(path, std::ios::binary);
	if (!fout.is_open())
	{
		HY_APP_ERROR("Failed to open file for writing: {}", path);
		return;
	}

	size_t jointsSize = m_Joints.size();
	fout.write(reinterpret_cast<const char*>(&jointsSize), sizeof(size_t));

	for (const auto& joint : m_Joints)
	{
		size_t nameSize = joint.Name.size();
		fout.write(reinterpret_cast<const char*>(&nameSize), sizeof(size_t));
		fout.write(joint.Name.data(), nameSize);

		fout.write(reinterpret_cast<const char*>(&joint.ParentIndex), sizeof(int));
		fout.write(reinterpret_cast<const char*>(&joint.InverseBindMatrix), sizeof(glm::mat4));
	}
	fout.close();
}

void AssimpSkeletonAsset::TraverseNode(const aiNode* node, int parentIdx, const std::unordered_map<std::string, glm::mat4>& activeBones)
{
	std::string nodeName = node->mName.C_Str();
	int currentIdx = parentIdx;

	auto it = activeBones.find(nodeName);
	if (it != activeBones.end())
	{
		Joint joint;
		joint.Name = nodeName;
		joint.ParentIndex = parentIdx;
		joint.InverseBindMatrix = it->second;

		m_Joints.push_back(joint);
		currentIdx = static_cast<int>(m_Joints.size()) - 1;
	}

	for (uint32_t i = 0; i < node->mNumChildren; ++i)
	{
		TraverseNode(node->mChildren[i], currentIdx, activeBones);
	}
}

void AssimpSkeletalMeshAsset::WriteAssetFile(const std::string& path)
{
	std::ofstream fout(path, std::ios::binary);
	if (!fout.is_open())
	{
		HY_APP_ERROR("Failed to open file for writing: {}", path);
		return;
	}

	size_t vertexCount = m_Vertices.size();
	size_t indexCount = m_Indices.size();

	fout.write(reinterpret_cast<const char*>(&vertexCount), sizeof(size_t));
	fout.write(reinterpret_cast<const char*>(&indexCount), sizeof(size_t));

	fout.write(reinterpret_cast<const char*>(m_Vertices.data()), vertexCount * sizeof(SkinnedVertex));
	fout.write(reinterpret_cast<const char*>(m_Indices.data()), indexCount * sizeof(uint32_t));

	fout.close();
}

void AssimpSkeletalMeshAsset::Parse(std::string path)
{
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_GenNormals | aiProcess_FlipUVs);
	HY_ASSERT(scene, "Assimp failed to load mesh: {}", importer.GetErrorString());

	if (m_Skeleton)
		m_Skeleton->BuildFromAssimp(scene);

	if (m_Animation)
		m_Animation->BuildFromAssimp(scene);

	uint32_t vertexOffset = 0;
	for (uint32_t m = 0; m < scene->mNumMeshes; ++m)
	{
		aiMesh* mesh = scene->mMeshes[m];

		for (uint32_t v = 0; v < mesh->mNumVertices; ++v)
		{
			SkinnedVertex vertex;
			vertex.Position = glm::vec3(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z);
			vertex.Normal = glm::vec3(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z);

			if (mesh->mTextureCoords[0])
				vertex.UV = glm::vec2(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y);
			else
				vertex.UV = glm::vec2(0.0f);

			if (mesh->mTangents)
				vertex.Tangent = glm::vec3(mesh->mTangents[v].x, mesh->mTangents[v].y, mesh->mTangents[v].z);

			m_Vertices.push_back(vertex);
		}

		for (uint32_t b = 0; b < mesh->mNumBones; ++b)
		{
			aiBone* bone = mesh->mBones[b];
			int boneID = m_Skeleton->FindJointIndex(bone->mName.C_Str());

			if (boneID != -1)
			{
				for (uint32_t w = 0; w < bone->mNumWeights; ++w)
				{
					uint32_t vertexID = vertexOffset + bone->mWeights[w].mVertexId;
					float weight = bone->mWeights[w].mWeight;
					AddBoneWeight(m_Vertices[vertexID], boneID, weight);
				}
			}
		}

		for (uint32_t f = 0; f < mesh->mNumFaces; ++f)
		{
			aiFace face = mesh->mFaces[f];
			for (uint32_t i = 0; i < face.mNumIndices; ++i)
			{
				m_Indices.push_back(vertexOffset + face.mIndices[i]);
			}
		}

		vertexOffset += mesh->mNumVertices;
	}
}

void AssimpSkeletalMeshAsset::AddBoneWeight(SkinnedVertex& vertex, int boneID, float weight)
{
	for (int i = 0; i < 4; ++i)
	{
		if (vertex.BoneIDs[i] == -1)
		{
			vertex.BoneIDs[i] = boneID;
			vertex.Weights[i] = weight;
			return;
		}
	}
}

void AssimpStaticMeshAsset::WriteAssetFile(const std::string& path)
{
	std::ofstream fout(path, std::ios::binary);
	if (!fout.is_open())
	{
		HY_APP_ERROR("Failed to open file for writing: {}", path);
		return;
	}

	size_t vertexCount = m_Vertices.size();
	size_t indexCount = m_Indices.size();

	fout.write(reinterpret_cast<const char*>(&vertexCount), sizeof(size_t));
	fout.write(reinterpret_cast<const char*>(&indexCount), sizeof(size_t));

	fout.write(reinterpret_cast<const char*>(m_Vertices.data()), vertexCount * sizeof(StaticVertex));
	fout.write(reinterpret_cast<const char*>(m_Indices.data()), indexCount * sizeof(uint32_t));

	fout.close();
}

void AssimpStaticMeshAsset::Parse(std::string path)
{
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_GenNormals | aiProcess_FlipUVs);
	HY_ASSERT(scene, "Assimp failed to load mesh: {}", importer.GetErrorString());

	uint32_t vertexOffset = 0;
	for (uint32_t m = 0; m < scene->mNumMeshes; ++m)
	{
		aiMesh* mesh = scene->mMeshes[m];

		for (uint32_t v = 0; v < mesh->mNumVertices; ++v)
		{
			StaticVertex vertex;
			vertex.Position = glm::vec3(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z);
			vertex.Normal = glm::vec3(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z);

			if (mesh->mTextureCoords[0])
				vertex.UV = glm::vec2(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y);
			else
				vertex.UV = glm::vec2(0.0f);

			if (mesh->mTangents)
				vertex.Tangent = glm::vec3(mesh->mTangents[v].x, mesh->mTangents[v].y, mesh->mTangents[v].z);

			m_Vertices.push_back(vertex);
		}

		for (uint32_t f = 0; f < mesh->mNumFaces; ++f)
		{
			aiFace face = mesh->mFaces[f];
			for (uint32_t i = 0; i < face.mNumIndices; ++i)
			{
				m_Indices.push_back(vertexOffset + face.mIndices[i]);
			}
		}

		vertexOffset += mesh->mNumVertices;
	}
}

void AssimpAnimationAsset::BuildFromAssimp(const aiScene* scene)
{
	HY_ASSERT(scene && scene->HasAnimations(), "Assimp failed to extract animation track data.");

	aiAnimation* aiAnim = scene->mAnimations[0];
	m_Duration = static_cast<float>(aiAnim->mDuration);
	m_TicksPerSecond = aiAnim->mTicksPerSecond != 0 ? static_cast<float>(aiAnim->mTicksPerSecond) : 25.0f;

	for (uint32_t i = 0; i < aiAnim->mNumChannels; ++i)
	{
		aiNodeAnim* aiChan = aiAnim->mChannels[i];
		BoneChannel channel;
		channel.BoneName = aiChan->mNodeName.C_Str();

		// Positions
		for (uint32_t p = 0; p < aiChan->mNumPositionKeys; ++p)
		{
			channel.PositionKeys.push_back({
				static_cast<float>(aiChan->mPositionKeys[p].mTime),
				glm::vec3(aiChan->mPositionKeys[p].mValue.x, aiChan->mPositionKeys[p].mValue.y, aiChan->mPositionKeys[p].mValue.z)
				});
		}

		// Rotations
		for (uint32_t r = 0; r < aiChan->mNumRotationKeys; ++r)
		{
			channel.RotationKeys.push_back({
				static_cast<float>(aiChan->mRotationKeys[r].mTime),
				glm::quat(aiChan->mRotationKeys[r].mValue.w, aiChan->mRotationKeys[r].mValue.x, aiChan->mRotationKeys[r].mValue.y, aiChan->mRotationKeys[r].mValue.z)
				});
		}

		// Scales
		for (uint32_t s = 0; s < aiChan->mNumScalingKeys; ++s)
		{
			channel.ScaleKeys.push_back({
				static_cast<float>(aiChan->mScalingKeys[s].mTime),
				glm::vec3(aiChan->mScalingKeys[s].mValue.x, aiChan->mScalingKeys[s].mValue.y, aiChan->mScalingKeys[s].mValue.z)
				});
		}

		m_Channels.push_back(channel);
	}
}

void AssimpAnimationAsset::WriteAssetFile(const std::string& path)
{
	std::ofstream fout(path, std::ios::binary);
	if (!fout.is_open())
	{
		HY_APP_ERROR("Failed to open file for writing: {}", path);
		return;
	}

	fout.write(reinterpret_cast<const char*>(&m_Duration), sizeof(float));
	fout.write(reinterpret_cast<const char*>(&m_TicksPerSecond), sizeof(float));

	size_t channelCount = m_Channels.size();
	fout.write(reinterpret_cast<const char*>(&channelCount), sizeof(size_t));

	for (const auto& channel : m_Channels)
	{
		size_t nameSize = channel.BoneName.size();
		fout.write(reinterpret_cast<const char*>(&nameSize), sizeof(size_t));
		fout.write(channel.BoneName.data(), nameSize);

		size_t posSize = channel.PositionKeys.size();
		fout.write(reinterpret_cast<const char*>(&posSize), sizeof(size_t));
		fout.write(reinterpret_cast<const char*>(channel.PositionKeys.data()), posSize * sizeof(VectorKey));

		size_t rotSize = channel.RotationKeys.size();
		fout.write(reinterpret_cast<const char*>(&rotSize), sizeof(size_t));
		fout.write(reinterpret_cast<const char*>(channel.RotationKeys.data()), rotSize * sizeof(QuatKey));

		size_t scaleSize = channel.ScaleKeys.size();
		fout.write(reinterpret_cast<const char*>(&scaleSize), sizeof(size_t));
		fout.write(reinterpret_cast<const char*>(channel.ScaleKeys.data()), scaleSize * sizeof(VectorKey));
	}
	fout.close();
}
