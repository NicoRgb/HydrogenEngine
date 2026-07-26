#include "Hydrogen/Application.hpp"
#include "Hydrogen/Animation.hpp"
#include "Hydrogen/AssetManager.hpp"
#include "Hydrogen/Scene.hpp"

using namespace Hydrogen;

SkeletalMeshRendererComponent::SkeletalMeshRendererComponent(Entity entity)
{
}

const std::shared_ptr<class SkeletonAsset>& SkeletalMeshRendererComponent::GetSkeleton() const
{
	return Skeleton;
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
}

void AnimatorComponent::ToJson(json& j, const AnimatorComponent& a)
{
	j = json();
	if (a.AnimationGraph)
		j["AnimationGraph"] = std::filesystem::path(a.AnimationGraph->GetPath()).filename().string();
}

void AnimatorComponent::FromJson(const json& j, AnimatorComponent& a, AssetManager* assetManager)
{
	auto animationGraph = j.value("AnimationGraph", "");
	if (!animationGraph.empty())
	{
		a.AnimationGraph = assetManager->GetAsset<AnimationGraphAsset>(animationGraph);
		a.UpdateGraph();
	}
}

void AnimatorComponent::UpdateAnimation(float dt)
{
	if (!m_Entity.HasComponent<SkeletalMeshRendererComponent>())
		return;

	const auto& skeleton = m_Entity.GetComponent<SkeletalMeshRendererComponent>().GetSkeleton();
	auto& bones = m_Entity.GetComponent<SkeletalMeshRendererComponent>().GetBones();
	const auto& joints = skeleton->GetJoints();

	if (CurrentStateID == 0)
	{
		CurrentStateID = m_DefaultStateID;
		CurrentStateTime = 0.0f;
	}

	if (m_States.find(CurrentStateID) == m_States.end())
		return;

	auto& currentState = m_States[CurrentStateID];

	if (!IsTransitioning)
	{
		for (const auto& transition : currentState.Transitions)
		{
			bool allConditionsMet = true;
			for (const auto& cond : transition.Conditions)
			{
				if (!EvaluateCondition(cond))
				{
					allConditionsMet = false;
					break;
				}
			}

			if (allConditionsMet)
			{
				IsTransitioning = true;
				TargetStateID = transition.ToStateID;
				TargetStateTime = 0.0f;
				TransitionProgress = 0.0f;
				TransitionDuration = transition.Duration;
				break;
			}
		}
	}

	if (currentState.GetAnimationClip())
	{
		CurrentStateTime += currentState.GetAnimationClip()->GetTicksPerSecond() * currentState.PlaybackSpeed * dt;
		CurrentStateTime = fmod(CurrentStateTime, currentState.GetAnimationClip()->GetDuration());
	}

	AnimPose finalPose;

	if (IsTransitioning)
	{
		auto& targetState = m_States[TargetStateID];
		if (targetState.GetAnimationClip())
		{
			TargetStateTime += targetState.GetAnimationClip()->GetTicksPerSecond() * targetState.PlaybackSpeed * dt;
			TargetStateTime = fmod(TargetStateTime, targetState.GetAnimationClip()->GetDuration());
		}

		TransitionProgress += dt;
		float alpha = TransitionProgress / TransitionDuration;

		AnimPose poseA = SamplePose(currentState.GetAnimationClip(), CurrentStateTime, skeleton);
		AnimPose poseB = SamplePose(targetState.GetAnimationClip(), TargetStateTime, skeleton);

		finalPose = BlendPoses(poseA, poseB, alpha);

		if (TransitionProgress >= TransitionDuration)
		{
			IsTransitioning = false;
			CurrentStateID = TargetStateID;
			CurrentStateTime = TargetStateTime;
			TargetStateID = 0;
		}
	}
	else
	{
		finalPose = SamplePose(currentState.GetAnimationClip(), CurrentStateTime, skeleton);
	}

	glm::mat4 rootCorrection = glm::mat4(1.0f);
	std::vector<glm::mat4> globalTransforms(joints.size());
	bones.resize(joints.size());

	for (size_t i = 0; i < joints.size(); ++i)
	{
		const auto& joint = joints[i];
		glm::mat4 localTransform = finalPose.LocalTransforms[i];

		if (joint.ParentIndex == -1)
		{
			globalTransforms[i] = rootCorrection * localTransform;
		}
		else
		{
			globalTransforms[i] = globalTransforms[joint.ParentIndex] * localTransform;
		}

		bones[i] = globalTransforms[i] * joint.InverseBindMatrix;
	}
}

void AnimatorComponent::UpdateGraph()
{
	if (!AnimationGraph)
	{
		return;
	}

	m_DefaultStateID = AnimationGraph->GetDefaultStateID();
	m_States = AnimationGraph->GetStates();
	m_Parameters = AnimationGraph->GetParameters();

	m_ChannelCache.clear();
	SetBindPose();
}

void AnimatorComponent::SetBindPose()
{
	if (!m_Entity.HasComponent<SkeletalMeshRendererComponent>())
		return;

	const auto& skeleton = m_Entity.GetComponent<SkeletalMeshRendererComponent>().GetSkeleton();
	auto& bones = m_Entity.GetComponent<SkeletalMeshRendererComponent>().GetBones();

	if (!skeleton)
		return;

	const auto& joints = skeleton->GetJoints();
	std::vector<glm::mat4> globalTransforms(joints.size());
	bones.resize(joints.size());

	glm::mat4 rootCorrection = glm::mat4(1.0f);

	for (size_t i = 0; i < joints.size(); ++i)
	{
		const auto& joint = joints[i];
		glm::mat4 localBindTransform = glm::inverse(joint.InverseBindMatrix);

		if (joint.ParentIndex != -1)
		{
			glm::mat4 parentGlobalBind = glm::inverse(joints[joint.ParentIndex].InverseBindMatrix);
			localBindTransform = glm::inverse(parentGlobalBind) * localBindTransform;
		}

		if (joint.ParentIndex == -1)
		{
			globalTransforms[i] = rootCorrection * localBindTransform;
		}
		else
		{
			globalTransforms[i] = globalTransforms[joint.ParentIndex] * localBindTransform;
		}

		bones[i] = globalTransforms[i] * joint.InverseBindMatrix;
	}
}

AnimPose AnimatorComponent::SamplePose(const std::shared_ptr<AnimationAsset>& clip, float time, const std::shared_ptr<class SkeletonAsset>& skeleton)
{
	AnimPose pose;
	const auto& joints = skeleton->GetJoints();
	pose.LocalTransforms.resize(joints.size());

	if (!clip)
	{
		for (size_t i = 0; i < joints.size(); ++i)
		{
			pose.LocalTransforms[i] = GetBindPoseTransform(joints[i], skeleton);
		}
		return pose;
	}

	for (size_t i = 0; i < joints.size(); ++i)
	{
		pose.LocalTransforms[i] = GetChannelTransform(clip, joints[i], skeleton, time);
	}
	return pose;
}

glm::mat4 AnimatorComponent::GetBindPoseTransform(const Joint& joint, const std::shared_ptr<class SkeletonAsset>& skeleton)
{
	glm::mat4 localBindTransform = glm::inverse(joint.InverseBindMatrix);

	if (joint.ParentIndex != -1)
	{
		const auto& parentJoint = skeleton->GetJoints()[joint.ParentIndex];
		glm::mat4 parentGlobalBind = glm::inverse(parentJoint.InverseBindMatrix);

		localBindTransform = glm::inverse(parentGlobalBind) * localBindTransform;
	}

	return localBindTransform;
}

AnimPose AnimatorComponent::BlendPoses(const AnimPose& poseA, const AnimPose& poseB, float factor)
{
	factor = glm::clamp(factor, 0.0f, 1.0f);
	AnimPose result;
	result.LocalTransforms.resize(poseA.LocalTransforms.size());

	for (size_t i = 0; i < poseA.LocalTransforms.size(); ++i)
	{
		glm::vec3 posA, posB, scaleA, scaleB;
		glm::quat rotA, rotB;
		glm::vec3 skew; glm::vec4 proj;

		glm::decompose(poseA.LocalTransforms[i], scaleA, rotA, posA, skew, proj);
		glm::decompose(poseB.LocalTransforms[i], scaleB, rotB, posB, skew, proj);

		glm::vec3 blendedPos = glm::mix(posA, posB, factor);
		glm::quat blendedRot = glm::slerp(rotA, rotB, factor);
		glm::vec3 blendedScale = glm::mix(scaleA, scaleB, factor);

		glm::mat4 T = glm::translate(glm::mat4(1.0f), blendedPos);
		glm::mat4 R = glm::mat4_cast(blendedRot);
		glm::mat4 S = glm::scale(glm::mat4(1.0f), blendedScale);

		result.LocalTransforms[i] = T * R * S;
	}

	return result;
}

bool AnimatorComponent::EvaluateCondition(const Condition& cond)
{
	auto it = m_Parameters.find(cond.ParameterName);
	if (it == m_Parameters.end()) return false;

	const auto& param = it->second;

	float val = 0.0f;
	if (std::holds_alternative<float>(param.Value)) val = std::get<float>(param.Value);
	else if (std::holds_alternative<int>(param.Value)) val = static_cast<float>(std::get<int>(param.Value));
	else if (std::holds_alternative<bool>(param.Value)) val = std::get<bool>(param.Value) ? 1.0f : 0.0f;

	switch (cond.Mode)
	{
	case ConditionMode::Greater:  return val > cond.Threshold;
	case ConditionMode::Less:     return val < cond.Threshold;
	case ConditionMode::Equal:    return glm::epsilonEqual(val, cond.Threshold, 0.001f);
	case ConditionMode::NotEqual: return !glm::epsilonEqual(val, cond.Threshold, 0.001f);
	case ConditionMode::True:     return val >= 0.5f;
	case ConditionMode::False:    return val < 0.5f;
	}
	return false;
}

glm::mat4 AnimatorComponent::GetChannelTransform(const std::shared_ptr<AnimationAsset>& clip, const Joint& joint, const std::shared_ptr<SkeletonAsset>& skeleton, float time)
{
	const BoneChannel* targetChannel = nullptr;
	if (clip)
	{
		for (const auto& channel : clip->GetChannels())
		{
			if (channel.BoneName == joint.Name)
			{
				targetChannel = &channel;
				break;
			}
		}
	}

	if (!targetChannel)
	{
		return GetBindPoseTransform(joint, skeleton);
	}

	glm::mat4 bindPoseLocal = GetBindPoseTransform(joint, skeleton);
	glm::vec3 bindTranslation, bindScale, skew;
	glm::quat bindRotation;
	glm::vec4 perspective;
	glm::decompose(bindPoseLocal, bindScale, bindRotation, bindTranslation, skew, perspective);

	auto& cache = m_ChannelCache[joint.Name];

	glm::vec3 translation = bindTranslation;
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

	glm::quat rotation = bindRotation;
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

	glm::vec3 scale = bindScale;
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
