#include "Skeleton.h"

#include "Calculation.h"
#include <cassert>
#include <stdexcept>

int32_t SkeletonSystem::CreateJoint(
	const SkeletonNode& node,
	const std::optional<int32_t>& parent,
	std::vector<Joint>& joints) {
	Joint joint;
	joint.name = node.name;
	joint.transform = node.transform;
	joint.bindTransform = node.transform;
	joint.localMatrix = MakeAffineMatrix(
		joint.transform.scale, joint.transform.rotate, joint.transform.translate);
	joint.skeletonSpaceMatrix = MakeIdentity4x4();
	joint.index = static_cast<int32_t>(joints.size());
	joint.parent = parent;
	const int32_t jointIndex = joint.index;
	joints.push_back(joint);

	for (const SkeletonNode& child : node.children) {
		const int32_t childIndex = CreateJoint(child, jointIndex, joints);
		joints[jointIndex].children.push_back(childIndex);
	}
	return jointIndex;
}

Skeleton SkeletonSystem::Create(const SkeletonNode& rootNode) {
	Skeleton skeleton;
	skeleton.root = CreateJoint(rootNode, std::nullopt, skeleton.joints);
	for (const Joint& joint : skeleton.joints) {
		if (!skeleton.jointMap.emplace(joint.name, joint.index).second) {
			throw std::runtime_error("Duplicate joint name: " + joint.name);
		}
	}
	Update(skeleton);
	return skeleton;
}

void SkeletonSystem::ResetToBindPose(Skeleton& skeleton) {
	for (Joint& joint : skeleton.joints) {
		joint.transform = joint.bindTransform;
	}
	Update(skeleton);
}

void SkeletonSystem::ApplyAnimation(Skeleton& skeleton, const AnimationPlayer& animationPlayer) {
	const Animation* animation = animationPlayer.GetAnimation();
	if (animation == nullptr) {
		return;
	}
	for (Joint& joint : skeleton.joints) {
		if (animation->nodeAnimations.contains(joint.name)) {
			joint.transform = animationPlayer.SampleNode(joint.name, joint.bindTransform);
		} else {
			joint.transform = joint.bindTransform;
		}
	}
	Update(skeleton);
}

void SkeletonSystem::Update(Skeleton& skeleton) {
	if (skeleton.root < 0 || static_cast<size_t>(skeleton.root) >= skeleton.joints.size()) {
		return;
	}
	// CreateJointは必ず親を子より先に追加するため、index順で階層更新できる。
	for (Joint& joint : skeleton.joints) {
		joint.localMatrix = MakeAffineMatrix(
			joint.transform.scale, joint.transform.rotate, joint.transform.translate);
		if (joint.parent.has_value()) {
			assert(*joint.parent >= 0 && static_cast<size_t>(*joint.parent) < skeleton.joints.size());
			joint.skeletonSpaceMatrix = Multiply(
				joint.localMatrix, skeleton.joints[*joint.parent].skeletonSpaceMatrix);
		} else {
			joint.skeletonSpaceMatrix = joint.localMatrix;
		}
	}
}
