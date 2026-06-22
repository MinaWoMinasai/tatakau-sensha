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
	ModelData modelData;
	SkeletonNode rootNode;
	std::map<std::string, JointWeightData> jointWeights;
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
	void Draw();
	void DrawShadow();

	Skeleton& GetSkeleton() { return skeleton_; }
	const SkinCluster& GetSkinCluster() const { return skinCluster_; }
	AnimationPlayer& GetAnimationPlayer() { return animationPlayer_; }
	const SkinningModelAsset& GetAsset() const { return asset_; }

private:
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	SkinningModelAsset asset_;
	Skeleton skeleton_;
	SkinCluster skinCluster_;
	Animation animation_;
	AnimationPlayer animationPlayer_;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> influenceResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> paletteResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViews_[2]{};
	D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
	SkinningPaletteEntry* mappedPalette_ = nullptr;
	uint32_t paletteSrvIndex_ = 0;
};
