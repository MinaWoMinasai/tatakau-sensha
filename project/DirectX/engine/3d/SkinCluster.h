#pragma once

#include "Skeleton.h"
#include "Struct.h"
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

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
	std::array<int32_t, 4> jointIndices{ -1, -1, -1, -1 };
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
