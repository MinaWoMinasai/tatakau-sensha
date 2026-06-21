#pragma once

#include "Struct.h"
#include <map>
#include <string>
#include <vector>

template <typename TValue>
struct Keyframe {
	float time = 0.0f;
	TValue value{};
};

template <typename TValue>
struct AnimationCurve {
	std::vector<Keyframe<TValue>> keyframes;
};

struct QuaternionTransform {
	Vector3 scale = { 1.0f, 1.0f, 1.0f };
	Quaternion rotate = { 0.0f, 0.0f, 0.0f, 1.0f };
	Vector3 translate = { 0.0f, 0.0f, 0.0f };
};

struct NodeAnimation {
	AnimationCurve<Vector3> translate;
	AnimationCurve<Quaternion> rotate;
	AnimationCurve<Vector3> scale;
};

struct Animation {
	std::string name;
	float duration = 0.0f;
	std::map<std::string, NodeAnimation> nodeAnimations;
};

Vector3 CalculateValue(const AnimationCurve<Vector3>& curve, float time, const Vector3& fallback);
Quaternion CalculateValue(const AnimationCurve<Quaternion>& curve, float time, const Quaternion& fallback);

class AnimationPlayer {
public:
	void SetAnimation(const Animation* animation, bool restart = true);
	void Update(float deltaTime);
	void Seek(float time);
	QuaternionTransform SampleNode(
		const std::string& nodeName,
		const QuaternionTransform& fallback = {}) const;

	void SetLoop(bool loop) { loop_ = loop; }
	void SetPlaying(bool playing) { playing_ = playing; }
	void SetPlaybackSpeed(float speed) { playbackSpeed_ = speed; }
	bool IsPlaying() const { return playing_; }
	float GetTime() const { return time_; }
	const Animation* GetAnimation() const { return animation_; }

private:
	const Animation* animation_ = nullptr;
	float time_ = 0.0f;
	float playbackSpeed_ = 1.0f;
	bool playing_ = true;
	bool loop_ = true;
};

class AnimationLoader {
public:
	static Animation LoadFromFile(const std::string& filePath, uint32_t animationIndex = 0);
	static std::vector<Animation> LoadAllFromFile(const std::string& filePath);
};
