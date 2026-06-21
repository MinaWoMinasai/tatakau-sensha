#pragma once

#include "Animation.h"
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

struct SkeletonNode {
	QuaternionTransform transform{};
	std::string name;
	std::vector<SkeletonNode> children;
};

struct Joint {
	QuaternionTransform transform{};
	QuaternionTransform bindTransform{};
	Matrix4x4 localMatrix{};
	Matrix4x4 skeletonSpaceMatrix{};
	std::string name;
	std::vector<int32_t> children;
	int32_t index = -1;
	std::optional<int32_t> parent;
};

struct Skeleton {
	int32_t root = -1;
	std::map<std::string, int32_t> jointMap;
	std::vector<Joint> joints;
};

class SkeletonSystem {
public:
	static Skeleton Create(const SkeletonNode& rootNode);
	static void ResetToBindPose(Skeleton& skeleton);
	static void ApplyAnimation(Skeleton& skeleton, const AnimationPlayer& animationPlayer);
	static void Update(Skeleton& skeleton);

private:
	static int32_t CreateJoint(
		const SkeletonNode& node,
		const std::optional<int32_t>& parent,
		std::vector<Joint>& joints);
};
