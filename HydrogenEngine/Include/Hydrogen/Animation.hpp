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

		static void OnImGuiRender(SkeletalMeshRendererComponent& t);
		static void ToJson(json& j, const SkeletalMeshRendererComponent& t);
		static void FromJson(const json& j, SkeletalMeshRendererComponent& t, class AssetManager* assetManager);
	};

	struct AnimatorComponent
	{
		AnimatorComponent(Entity entity);

		std::shared_ptr<class AnimationAsset> AnimationClip;
		float CurrentTime;

		void LoadAnimation();
		void UpdateAnimation(float dt);

		static void OnImGuiRender(AnimatorComponent& a);
		static void ToJson(json& j, const AnimatorComponent& a);
		static void FromJson(const json& j, AnimatorComponent& a, class AssetManager* assetManager);

	private:
		struct CachedChannelState
		{
			size_t PositionIndex = 0;
			size_t RotationIndex = 0;
			size_t ScaleIndex = 0;
		};

		glm::mat4 GetChannelTransform(const std::string& jointName, float time);

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
		std::unordered_map<std::string, CachedChannelState> m_ChannelCache;
	};
}
