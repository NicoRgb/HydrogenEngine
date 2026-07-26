#pragma once

#include "Hydrogen/Scene.hpp"
#include <algorithm>

using json = nlohmann::json;

namespace Hydrogen
{
	struct SkeletalMeshRendererComponent
	{
		SkeletalMeshRendererComponent(Entity entity);

		std::shared_ptr<class SkeletonAsset> Skeleton;
		std::shared_ptr<class SkeletalMeshAsset> SkeletalMesh;
		std::shared_ptr<class MaterialAsset> Material;

		std::vector<glm::mat4> Bones;

		std::vector<glm::mat4>& GetBones() { return Bones; }
		const std::shared_ptr<class SkeletonAsset>& GetSkeleton() const;

		static void ToJson(json& j, const SkeletalMeshRendererComponent& t);
		static void FromJson(const json& j, SkeletalMeshRendererComponent& t, class AssetManager* assetManager);
	};

	struct AnimPose
	{
		std::vector<glm::mat4> LocalTransforms;
	};

	struct AnimatorComponent
	{
		AnimatorComponent(Entity entity);

		std::shared_ptr<class AnimationGraphAsset> AnimationGraph;

		static void ToJson(json& j, const AnimatorComponent& a);
		static void FromJson(const json& j, AnimatorComponent& a, class AssetManager* assetManager);

		void SetFloat(const std::string& name, float val) { m_Parameters[name].Value = val; }
		void SetBool(const std::string& name, bool val) { m_Parameters[name].Value = val; }
		void SetInt(const std::string& name, int val) { m_Parameters[name].Value = val; }

		void UpdateAnimation(float dt);
		void UpdateGraph();

	private:
		void SetBindPose();

		AnimPose SamplePose(const std::shared_ptr<AnimationAsset>& clip, float time, const std::shared_ptr<class SkeletonAsset>& skeleton);
		glm::mat4 GetBindPoseTransform(const Joint& joint, const std::shared_ptr<class SkeletonAsset>& skeleton);
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

		Entity m_Entity;

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
	};
}
