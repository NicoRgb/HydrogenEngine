#pragma once

#include "Hydrogen/Scene/Components.hpp"
#include "Hydrogen/AssetManager.hpp"
#include <algorithm>

using json = nlohmann::json;

namespace Hydrogen
{
	struct SkeletalMeshRendererComponent : public GenericComponent
	{
		SkeletalMeshRendererComponent(Entity entity)
			: GenericComponent(entity)
		{
		}

		std::shared_ptr<SkeletonAsset> Skeleton;
		std::shared_ptr<SkeletalMeshAsset> SkeletalMesh;
		std::shared_ptr<MaterialAsset> Material;

		std::vector<glm::mat4> Bones;

		std::vector<glm::mat4>& GetBones() { return Bones; }
		const std::shared_ptr<SkeletonAsset>& GetSkeleton() const { return Skeleton; }

		BEGIN_COMPONENT_REFLECTION(SkeletalMeshRendererComponent)
			REFLECT_MEMBER(Skeleton)
			REFLECT_MEMBER(SkeletalMesh)
			REFLECT_MEMBER(Material)
		END_COMPONENT_REFLECTION()
	};
	REGISTER_COMPONENT(SkeletalMeshRendererComponent, "SkeletalMeshRendererComponent")

	struct AnimPose
	{
		std::vector<glm::mat4> LocalTransforms;
	};

	struct AnimatorComponent : public GenericComponent
	{
		AnimatorComponent(Entity entity)
			: GenericComponent(entity)
		{
		}

		virtual void Deserialize(const json& j) override
		{
			GenericComponent::Deserialize(j);
			UpdateGraph();
		}

		std::shared_ptr<AnimationGraphAsset> AnimationGraph;

		void SetFloat(const std::string& name, float val) { m_Parameters[name].Value = val; }
		void SetBool(const std::string& name, bool val) { m_Parameters[name].Value = val; }
		void SetInt(const std::string& name, int val) { m_Parameters[name].Value = val; }

		void UpdateAnimation(float dt);
		void UpdateGraph();

	private:
		void SetBindPose();

		AnimPose SamplePose(const std::shared_ptr<AnimationAsset>& clip, float time, const std::shared_ptr<SkeletonAsset>& skeleton);
		glm::mat4 GetBindPoseTransform(const Joint& joint, const std::shared_ptr<SkeletonAsset>& skeleton);
		AnimPose BlendPoses(const AnimPose& poseA, const AnimPose& poseB, float factor);
		bool EvaluateCondition(const Condition& cond);
		glm::mat4 GetChannelTransform(const std::shared_ptr<AnimationAsset>& clip, const Joint& joint, const std::shared_ptr<SkeletonAsset>& skeleton, float time);

		template <typename KeyFrameType>
		size_t FindKeyframeIndex(const std::vector<KeyFrameType>& keyframes, float time, size_t& cachedIndex)
		{
			if (keyframes.empty())
				return 0;

			if (cachedIndex < keyframes.size() - 1 && time >= keyframes[cachedIndex].Time && time < keyframes[cachedIndex + 1].Time)
			{
				return cachedIndex;
			}

			auto it = std::lower_bound(
				keyframes.begin(),
				keyframes.end(),
				time,
				[](const KeyFrameType& frame, float t) { return frame.Time < t; }
			);

			size_t index = std::distance(keyframes.begin(), it);
			index = (index > 0) ? index - 1 : 0;
			index = std::min(index, keyframes.size() - 2);

			cachedIndex = index;
			return index;
		}

		// State Machine

		uint64_t m_DefaultStateID = 0;
		std::unordered_map<uint64_t, AnimState> m_States;
		std::unordered_map<std::string, AnimParameter> m_Parameters;

		uint64_t CurrentStateID = 0;
		float CurrentStateTime = 0.0f;

		bool IsTransitioning = false;
		uint64_t TargetStateID = 0;
		float TargetStateTime = 0.0f;
		float TransitionProgress = 0.0f;
		float TransitionDuration = 0.25f;

		// Cache

		struct CachedChannelState
		{
			size_t PositionIndex = 0;
			size_t RotationIndex = 0;
			size_t ScaleIndex = 0;
		};

		std::unordered_map<std::string, CachedChannelState> m_ChannelCache;

		BEGIN_COMPONENT_REFLECTION(AnimatorComponent)
			REFLECT_MEMBER(AnimationGraph)
		END_COMPONENT_REFLECTION()
	};
	REGISTER_COMPONENT(AnimatorComponent, "AnimatorComponent")
}
