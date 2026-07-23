#include "Hydrogen/Application.hpp"
#include "Hydrogen/Animation.hpp"
#include "Hydrogen/AssetManager.hpp"
#include "Hydrogen/Scene.hpp"

using namespace Hydrogen;

SkeletalMeshRendererComponent::SkeletalMeshRendererComponent(Entity entity)
{
	Material = Application::Get()->MainAssetManager.GetAsset<MaterialAsset>("DefaultMaterial.hymat");
}

const std::shared_ptr<class SkeletonAsset>& SkeletalMeshRendererComponent::GetSkeleton() const
{
	return Skeleton;
}

void SkeletalMeshRendererComponent::OnImGuiRender(SkeletalMeshRendererComponent& t)
{
	if (ImGui::TreeNode("Skeletal Mesh Renderer"))
	{
		if (t.SkeletalMesh)
		{
			ImGui::Text(t.SkeletalMesh->GetPath().c_str());
		}
		else
		{
			ImGui::Text("NULL");
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE"))
			{
				std::filesystem::path newPath((const char*)payload->Data);
				auto asset = Application::Get()->MainAssetManager.GetAsset<SkeletalMeshAsset>(newPath.filename().string());
				if (asset)
				{
					t.SkeletalMesh = asset;
				}
			}
			ImGui::EndDragDropTarget();
		}

		if (t.Material)
		{
			ImGui::Text(t.Material->GetPath().c_str());
		}
		else
		{
			ImGui::Text("NULL");
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE"))
			{
				std::filesystem::path newPath((const char*)payload->Data);
				auto asset = Application::Get()->MainAssetManager.GetAsset<MaterialAsset>(newPath.filename().string());
				if (asset)
				{
					t.Material = asset;
				}
			}
			ImGui::EndDragDropTarget();
		}

		if (t.Skeleton)
		{
			ImGui::Text(t.Skeleton->GetPath().c_str());
		}
		else
		{
			ImGui::Text("NULL");
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE"))
			{
				std::filesystem::path newPath((const char*)payload->Data);
				auto asset = Application::Get()->MainAssetManager.GetAsset<SkeletonAsset>(newPath.filename().string());
				if (asset)
				{
					t.Skeleton = asset;
					t.Bones.resize(t.Skeleton->GetJoints().size());
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::TreePop();
	}
}

void SkeletalMeshRendererComponent::ToJson(json& j, const SkeletalMeshRendererComponent& t)
{
	j = json();
	if (t.Material)
		j["Material"] = std::filesystem::path(t.Material->GetPath()).filename().string();
	if (t.SkeletalMesh)
		j["SkeletalMesh"] = std::filesystem::path(t.SkeletalMesh->GetPath()).filename().string();
	if (t.Skeleton)
		j["Skeleton"] = std::filesystem::path(t.Skeleton->GetPath()).filename().string();
}

void SkeletalMeshRendererComponent::FromJson(const json& j, SkeletalMeshRendererComponent& t, AssetManager* assetManager)
{
	auto materialPath = j.value("Material", "");
	auto meshPath = j.value("SkeletalMesh", "");
	auto skeletonPath = j.value("Skeleton", "");

	if (!materialPath.empty())
	{
		t.Material = assetManager->GetAsset<MaterialAsset>(materialPath);
	}
	else
	{
		t.Material = assetManager->GetAsset<MaterialAsset>("DefaultMaterial.hymat");
	}
	if (!meshPath.empty())
	{
		t.SkeletalMesh = assetManager->GetAsset<SkeletalMeshAsset>(meshPath);
	}
	if (!skeletonPath.empty())
	{
		t.Skeleton = assetManager->GetAsset<SkeletonAsset>(skeletonPath);
		t.Bones.resize(t.Skeleton->GetJoints().size());
	}
}

AnimatorComponent::AnimatorComponent(Entity entity)
{
	m_Entity = entity;
	LoadAnimation();
}

glm::mat4 AnimatorComponent::GetChannelTransform(const std::string& jointName, float time)
{
	const BoneChannel* targetChannel = nullptr;
	for (const auto& channel : AnimationClip->GetChannels())
	{
		if (channel.BoneName == jointName)
		{
			targetChannel = &channel;
			break;
		}
	}

	if (!targetChannel)
	{
		return glm::mat4(1.0f);
	}

	auto& cache = m_ChannelCache[jointName];

	glm::vec3 translation(0.0f);
	if (!targetChannel->PositionKeys.empty())
	{
		if (targetChannel->PositionKeys.size() == 1)
		{
			translation = targetChannel->PositionKeys[0].Value;
		}
		else
		{
			size_t index = FindKeyframeIndex(targetChannel->PositionKeys, time, cache.PositionIndex);
			size_t nextIndex = index + 1;
			float factor = (time - targetChannel->PositionKeys[index].Time) /
				(targetChannel->PositionKeys[nextIndex].Time - targetChannel->PositionKeys[index].Time);

			translation = glm::mix(targetChannel->PositionKeys[index].Value,
				targetChannel->PositionKeys[nextIndex].Value,
				glm::clamp(factor, 0.0f, 1.0f));
		}
	}

	glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	if (!targetChannel->RotationKeys.empty())
	{
		if (targetChannel->RotationKeys.size() == 1)
		{
			rotation = targetChannel->RotationKeys[0].Value;
		}
		else
		{
			size_t index = FindKeyframeIndex(targetChannel->RotationKeys, time, cache.RotationIndex);
			size_t nextIndex = index + 1;
			float factor = (time - targetChannel->RotationKeys[index].Time) /
				(targetChannel->RotationKeys[nextIndex].Time - targetChannel->RotationKeys[index].Time);

			rotation = glm::slerp(targetChannel->RotationKeys[index].Value,
				targetChannel->RotationKeys[nextIndex].Value,
				glm::clamp(factor, 0.0f, 1.0f));
		}
	}

	glm::vec3 scale(1.0f);
	if (!targetChannel->ScaleKeys.empty())
	{
		if (targetChannel->ScaleKeys.size() == 1)
		{
			scale = targetChannel->ScaleKeys[0].Value;
		}
		else
		{
			size_t index = FindKeyframeIndex(targetChannel->ScaleKeys, time, cache.ScaleIndex);
			size_t nextIndex = index + 1;
			float factor = (time - targetChannel->ScaleKeys[index].Time) /
				(targetChannel->ScaleKeys[nextIndex].Time - targetChannel->ScaleKeys[index].Time);

			scale = glm::mix(targetChannel->ScaleKeys[index].Value,
				targetChannel->ScaleKeys[nextIndex].Value,
				glm::clamp(factor, 0.0f, 1.0f));
		}
	}

	glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), translation);
	glm::mat4 rotationMatrix = glm::mat4_cast(rotation);
	glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scale);

	return translationMatrix * rotationMatrix * scaleMatrix;
}

void AnimatorComponent::LoadAnimation()
{
	CurrentTime = 0.0f;
	m_ChannelCache.clear();

	glm::mat4 rootCorrection = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

	if (!m_Entity.HasComponent<SkeletalMeshRendererComponent>() || !AnimationClip)
		return;

	const auto& skeleton = m_Entity.GetComponent<SkeletalMeshRendererComponent>().GetSkeleton();
	auto& bones = m_Entity.GetComponent<SkeletalMeshRendererComponent>().GetBones();

	const auto& joints = skeleton->GetJoints();

	std::vector<glm::mat4> globalTransforms(joints.size());
	bones.resize(joints.size());

	for (size_t i = 0; i < joints.size(); ++i)
	{
		const auto& joint = joints[i];
		glm::mat4 localTransform = GetChannelTransform(joint.Name, CurrentTime);

		if (joint.ParentIndex == -1)
			globalTransforms[i] = rootCorrection * localTransform;
		else
			globalTransforms[i] = globalTransforms[joint.ParentIndex] * localTransform;

		bones[i] = globalTransforms[i] * joint.InverseBindMatrix;
	}
}

void AnimatorComponent::UpdateAnimation(float dt)
{
	glm::mat4 rootCorrection = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

	if (!m_Entity.HasComponent<SkeletalMeshRendererComponent>() || !AnimationClip)
		return;

	const auto& skeleton = m_Entity.GetComponent<SkeletalMeshRendererComponent>().GetSkeleton();
	auto& bones = m_Entity.GetComponent<SkeletalMeshRendererComponent>().GetBones();

	float previousTime = CurrentTime;
	CurrentTime += AnimationClip->GetTicksPerSecond() * dt;
	CurrentTime = fmod(CurrentTime, AnimationClip->GetDuration());

	if (CurrentTime < previousTime)
	{
		m_ChannelCache.clear();
	}

	const auto& joints = skeleton->GetJoints();

	std::vector<glm::mat4> globalTransforms(joints.size());
	bones.resize(joints.size());

	for (size_t i = 0; i < joints.size(); ++i)
	{
		const auto& joint = joints[i];
		glm::mat4 localTransform = GetChannelTransform(joint.Name, CurrentTime);

		if (joint.ParentIndex == -1)
			globalTransforms[i] = rootCorrection * localTransform;
		else
			globalTransforms[i] = globalTransforms[joint.ParentIndex] * localTransform;

		bones[i] = globalTransforms[i] * joint.InverseBindMatrix;
	}
}

void AnimatorComponent::OnImGuiRender(AnimatorComponent& a)
{
	if (ImGui::TreeNode("Animator"))
	{
		if (a.AnimationClip)
		{
			ImGui::Text(a.AnimationClip->GetPath().c_str());
		}
		else
		{
			ImGui::Text("NULL");
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE"))
			{
				std::filesystem::path newPath((const char*)payload->Data);
				auto asset = Application::Get()->MainAssetManager.GetAsset<AnimationAsset>(newPath.filename().string());
				if (asset)
				{
					a.AnimationClip = asset;
					a.CurrentTime = 0.0f;
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::TreePop();
	}
}

void AnimatorComponent::ToJson(json& j, const AnimatorComponent& a)
{
	j = json();
	if (a.AnimationClip)
		j["AnimationClip"] = std::filesystem::path(a.AnimationClip->GetPath()).filename().string();
}

void AnimatorComponent::FromJson(const json& j, AnimatorComponent& a, AssetManager* assetManager)
{
	auto animationClip = j.value("AnimationClip", "");
	if (!animationClip.empty())
	{
		a.AnimationClip = assetManager->GetAsset<AnimationAsset>(animationClip);
	}

	a.LoadAnimation();
}
