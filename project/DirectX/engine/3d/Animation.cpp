#include "Animation.h"

#include "Calculation.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {

template <typename TValue, typename TInterpolate>
TValue CalculateCurveValue(
	const AnimationCurve<TValue>& curve,
	float time,
	const TValue& fallback,
	TInterpolate interpolate) {
	if (curve.keyframes.empty()) {
		return fallback;
	}
	if (curve.keyframes.size() == 1 || time <= curve.keyframes.front().time) {
		return curve.keyframes.front().value;
	}
	for (size_t index = 0; index + 1 < curve.keyframes.size(); ++index) {
		const Keyframe<TValue>& current = curve.keyframes[index];
		const Keyframe<TValue>& next = curve.keyframes[index + 1];
		if (time <= next.time) {
			const float duration = next.time - current.time;
			const float t = duration > 0.000001f ? (time - current.time) / duration : 0.0f;
			return interpolate(current.value, next.value, (std::clamp)(t, 0.0f, 1.0f));
		}
	}
	return curve.keyframes.back().value;
}

float TicksPerSecond(const aiAnimation& animation) {
	return animation.mTicksPerSecond > 0.0 ? static_cast<float>(animation.mTicksPerSecond) : 1.0f;
}

Animation ConvertAnimation(const aiAnimation& source, uint32_t index) {
	Animation result;
	result.name = source.mName.length > 0 ? source.mName.C_Str() : "Animation_" + std::to_string(index);
	const float ticksPerSecond = TicksPerSecond(source);
	result.duration = static_cast<float>(source.mDuration) / ticksPerSecond;

	for (uint32_t channelIndex = 0; channelIndex < source.mNumChannels; ++channelIndex) {
		const aiNodeAnim& sourceNode = *source.mChannels[channelIndex];
		NodeAnimation& node = result.nodeAnimations[sourceNode.mNodeName.C_Str()];

		node.translate.keyframes.reserve(sourceNode.mNumPositionKeys);
		for (uint32_t keyIndex = 0; keyIndex < sourceNode.mNumPositionKeys; ++keyIndex) {
			const aiVectorKey& key = sourceNode.mPositionKeys[keyIndex];
			node.translate.keyframes.push_back({
				static_cast<float>(key.mTime) / ticksPerSecond,
				{ -key.mValue.x, key.mValue.y, key.mValue.z },
			});
		}

		node.rotate.keyframes.reserve(sourceNode.mNumRotationKeys);
		for (uint32_t keyIndex = 0; keyIndex < sourceNode.mNumRotationKeys; ++keyIndex) {
			const aiQuatKey& key = sourceNode.mRotationKeys[keyIndex];
			node.rotate.keyframes.push_back({
				static_cast<float>(key.mTime) / ticksPerSecond,
				NormalizeQuaternion({ key.mValue.x, -key.mValue.y, -key.mValue.z, key.mValue.w }),
			});
		}

		node.scale.keyframes.reserve(sourceNode.mNumScalingKeys);
		for (uint32_t keyIndex = 0; keyIndex < sourceNode.mNumScalingKeys; ++keyIndex) {
			const aiVectorKey& key = sourceNode.mScalingKeys[keyIndex];
			node.scale.keyframes.push_back({
				static_cast<float>(key.mTime) / ticksPerSecond,
				{ key.mValue.x, key.mValue.y, key.mValue.z },
			});
		}
	}
	return result;
}

} // namespace

Vector3 CalculateValue(const AnimationCurve<Vector3>& curve, float time, const Vector3& fallback) {
	return CalculateCurveValue(curve, time, fallback, [](const Vector3& a, const Vector3& b, float t) {
		return a + (b - a) * t;
	});
}

Quaternion CalculateValue(const AnimationCurve<Quaternion>& curve, float time, const Quaternion& fallback) {
	return CalculateCurveValue(curve, time, fallback, [](const Quaternion& a, const Quaternion& b, float t) {
		return SlerpQuaternion(a, b, t);
	});
}

void AnimationPlayer::SetAnimation(const Animation* animation, bool restart) {
	animation_ = animation;
	if (restart) {
		time_ = 0.0f;
	}
}

void AnimationPlayer::Update(float deltaTime) {
	if (!playing_ || animation_ == nullptr || animation_->duration <= 0.0f) {
		return;
	}
	time_ += deltaTime * playbackSpeed_;
	if (loop_) {
		time_ = std::fmod(time_, animation_->duration);
		if (time_ < 0.0f) {
			time_ += animation_->duration;
		}
	} else if (time_ >= animation_->duration) {
		time_ = animation_->duration;
		playing_ = false;
	} else if (time_ < 0.0f) {
		time_ = 0.0f;
		playing_ = false;
	}
}

void AnimationPlayer::Seek(float time) {
	if (animation_ == nullptr || animation_->duration <= 0.0f) {
		time_ = 0.0f;
		return;
	}
	time_ = (std::clamp)(time, 0.0f, animation_->duration);
}

QuaternionTransform AnimationPlayer::SampleNode(
	const std::string& nodeName,
	const QuaternionTransform& fallback) const {
	if (animation_ == nullptr) {
		return fallback;
	}
	const auto node = animation_->nodeAnimations.find(nodeName);
	if (node == animation_->nodeAnimations.end()) {
		return fallback;
	}
	return {
		CalculateValue(node->second.scale, time_, fallback.scale),
		CalculateValue(node->second.rotate, time_, fallback.rotate),
		CalculateValue(node->second.translate, time_, fallback.translate),
	};
}

Animation AnimationLoader::LoadFromFile(const std::string& filePath, uint32_t animationIndex) {
	std::vector<Animation> animations = LoadAllFromFile(filePath);
	if (animationIndex >= animations.size()) {
		throw std::out_of_range("Animation index is out of range: " + filePath);
	}
	return std::move(animations[animationIndex]);
}

std::vector<Animation> AnimationLoader::LoadAllFromFile(const std::string& filePath) {
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(filePath, 0);
	if (scene == nullptr) {
		throw std::runtime_error("Failed to load animation: " + filePath + " / " + importer.GetErrorString());
	}
	if (scene->mNumAnimations == 0) {
		throw std::runtime_error("Animation data was not found: " + filePath);
	}

	std::vector<Animation> animations;
	animations.reserve(scene->mNumAnimations);
	for (uint32_t index = 0; index < scene->mNumAnimations; ++index) {
		animations.push_back(ConvertAnimation(*scene->mAnimations[index], index));
	}
	return animations;
}
