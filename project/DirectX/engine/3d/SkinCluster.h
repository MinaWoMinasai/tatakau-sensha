#pragma once

#include "Skeleton.h"
#include "Struct.h"
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <d3d12.h>
#include <wrl.h>

class DirectXCommon;
class SrvManager;

struct VertexWeightData {
	float weight = 0.0f;
	uint32_t vertexIndex = 0;
};

struct JointWeightData {
	Matrix4x4 inverseBindPoseMatrix{};
	std::vector<VertexWeightData> vertexWeights;
};

struct SkinningModelAsset {
	struct Submesh {
		uint32_t indexStart = 0;
		uint32_t indexCount = 0;
		std::string textureKey;
		bool doubleSided = false;
	};

	struct EmbeddedTexture {
		std::string textureKey;
		std::vector<uint8_t> encodedData;
	};

	ModelData modelData;
	SkeletonNode rootNode;
	std::map<std::string, JointWeightData> jointWeights;
	std::vector<Submesh> submeshes;
	std::vector<EmbeddedTexture> embeddedTextures;
};

struct VertexInfluence {
	std::array<float, 4> weights{};
	std::array<int32_t, 4> jointIndices{};
};

struct SkinningPaletteEntry {
	Matrix4x4 skeletonSpaceMatrix{};
	Matrix4x4 skeletonSpaceInverseTransposeMatrix{};
};

class SkinningModelLoader {
public:
	static SkinningModelAsset LoadFromFile(const std::string& filePath);
};

class SkinCluster {
public:
	static SkinCluster Create(const Skeleton& skeleton, const SkinningModelAsset& asset);
	void Update(const Skeleton& skeleton);

	const std::vector<VertexInfluence>& GetInfluences() const { return influences_; }
	const std::vector<SkinningPaletteEntry>& GetPalette() const { return palette_; }
	const std::vector<Matrix4x4>& GetInverseBindPoseMatrices() const { return inverseBindPoseMatrices_; }
	uint32_t GetAssignedInfluenceCount() const { return assignedInfluenceCount_; }

private:
	std::vector<Matrix4x4> inverseBindPoseMatrices_;
	std::vector<VertexInfluence> influences_;
	std::vector<SkinningPaletteEntry> palette_;
	uint32_t assignedInfluenceCount_ = 0;
};

class SkinnedModel {
public:
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, const std::string& filePath);
	void Update(float deltaTime);
	void UpdateBlended(
		float deltaTime,
		AnimationPlayer& animationA,
		AnimationPlayer& animationB,
		float blendFactor);
	void Draw();
	void DrawShadow();
	bool SetAnimation(const std::string& name, bool restart = true);
	bool SetAnimation(size_t index, bool restart = true);
	bool TransitionToAnimation(
		const std::string& name,
		float duration,
		bool synchronizeNormalizedTime = false);
	bool TransitionToAnimation(
		size_t index,
		float duration,
		bool synchronizeNormalizedTime = false);

	Skeleton& GetSkeleton() { return skeleton_; }
	void SetAnimationLoop(bool loop) { animationPlayer_.SetLoop(loop); }
	void SetAnimationPlaybackSpeed(float speed) { animationPlayer_.SetPlaybackSpeed(speed); }
	void SeekCurrentAnimation(float time) { animationPlayer_.Seek(time); }
	float GetCurrentAnimationTime() const { return animationPlayer_.GetTime(); }
	float GetCurrentAnimationDuration() const {
		return animationPlayer_.GetAnimation() ? animationPlayer_.GetAnimation()->duration : 0.0f;
	}
	bool IsCurrentAnimationPlaying() const { return animationPlayer_.IsPlaying(); }
	bool IsCurrentAnimationLooping() const { return animationPlayer_.IsLooping(); }
	const SkinCluster& GetSkinCluster() const { return skinCluster_; }
	AnimationPlayer& GetAnimationPlayer() { return animationPlayer_; }
	const Animation& GetAnimation() const { return animations_[currentAnimationIndex_]; }
	const std::vector<Animation>& GetAnimations() const { return animations_; }
	size_t GetCurrentAnimationIndex() const { return currentAnimationIndex_; }
	const SkinningModelAsset& GetAsset() const { return asset_; }

private:
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	SkinningModelAsset asset_;
	Skeleton skeleton_;
	SkinCluster skinCluster_;
	std::vector<Animation> animations_;
	size_t currentAnimationIndex_ = 0;
	AnimationPlayer animationPlayer_;
	std::vector<QuaternionTransform> animationTransitionStartPose_;
	float animationTransitionDuration_ = 0.0f;
	float animationTransitionElapsed_ = 0.0f;
	bool animationTransitionActive_ = false;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> influenceResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> paletteResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViews_[2]{};
	D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
	SkinningPaletteEntry* mappedPalette_ = nullptr;
	uint32_t paletteSrvIndex_ = 0;
};
