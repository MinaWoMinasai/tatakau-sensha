#include "SkinCluster.h"

#include "Calculation.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "TextureManager.h"
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <stdexcept>

namespace {

QuaternionTransform ConvertTransform(const aiMatrix4x4& matrix) {
	aiVector3D scale;
	aiVector3D translate;
	aiQuaternion rotate;
	matrix.Decompose(scale, rotate, translate);
	return {
		{ scale.x, scale.y, scale.z },
		NormalizeQuaternion({ rotate.x, -rotate.y, -rotate.z, rotate.w }),
		{ -translate.x, translate.y, translate.z },
	};
}

SkeletonNode ReadNode(const aiNode& source) {
	SkeletonNode node;
	node.name = source.mName.C_Str();
	node.transform = ConvertTransform(source.mTransformation);
	node.children.reserve(source.mNumChildren);
	for (uint32_t index = 0; index < source.mNumChildren; ++index) {
		node.children.push_back(ReadNode(*source.mChildren[index]));
	}
	return node;
}

Matrix4x4 ConvertInverseBindPose(const aiMatrix4x4& sourceOffsetMatrix) {
	aiMatrix4x4 bindPose = sourceOffsetMatrix;
	bindPose.Inverse();
	const QuaternionTransform transform = ConvertTransform(bindPose);
	return Inverse(MakeAffineMatrix(transform.scale, transform.rotate, transform.translate));
}

void InsertInfluence(VertexInfluence& influence, float weight, int32_t jointIndex) {
	for (size_t slot = 0; slot < influence.weights.size(); ++slot) {
		if (influence.weights[slot] == 0.0f) {
			influence.weights[slot] = weight;
			influence.jointIndices[slot] = jointIndex;
			return;
		}
	}
	const auto smallest = std::min_element(influence.weights.begin(), influence.weights.end());
	if (weight > *smallest) {
		const size_t slot = static_cast<size_t>(std::distance(influence.weights.begin(), smallest));
		influence.weights[slot] = weight;
		influence.jointIndices[slot] = jointIndex;
	}
}

} // namespace

SkinningModelAsset SkinningModelLoader::LoadFromFile(const std::string& filePath) {
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(filePath, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices);
	if (scene == nullptr || scene->mRootNode == nullptr || scene->mNumMeshes == 0) {
		throw std::runtime_error("Failed to load skinned model: " + filePath + " / " + importer.GetErrorString());
	}

	SkinningModelAsset asset;
	asset.rootNode = ReadNode(*scene->mRootNode);
	const std::filesystem::path sourcePath(filePath);

	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
		const aiMesh& mesh = *scene->mMeshes[meshIndex];
		const uint32_t vertexOffset = static_cast<uint32_t>(asset.modelData.vertices.size());
		asset.modelData.vertices.reserve(asset.modelData.vertices.size() + mesh.mNumVertices);
		for (uint32_t vertexIndex = 0; vertexIndex < mesh.mNumVertices; ++vertexIndex) {
			const aiVector3D& position = mesh.mVertices[vertexIndex];
			const aiVector3D normal = mesh.HasNormals() ? mesh.mNormals[vertexIndex] : aiVector3D{};
			const aiVector3D uv = mesh.HasTextureCoords(0) ? mesh.mTextureCoords[0][vertexIndex] : aiVector3D{};
			asset.modelData.vertices.push_back({
				{ -position.x, position.y, position.z, 1.0f },
				{ uv.x, uv.y },
				{ -normal.x, normal.y, normal.z },
			});
		}

		for (uint32_t faceIndex = 0; faceIndex < mesh.mNumFaces; ++faceIndex) {
			const aiFace& face = mesh.mFaces[faceIndex];
			if (face.mNumIndices != 3) {
				continue;
			}
			asset.modelData.indices.push_back(vertexOffset + face.mIndices[2]);
			asset.modelData.indices.push_back(vertexOffset + face.mIndices[1]);
			asset.modelData.indices.push_back(vertexOffset + face.mIndices[0]);
		}

		for (uint32_t boneIndex = 0; boneIndex < mesh.mNumBones; ++boneIndex) {
			const aiBone& bone = *mesh.mBones[boneIndex];
			JointWeightData& jointWeight = asset.jointWeights[bone.mName.C_Str()];
			jointWeight.inverseBindPoseMatrix = ConvertInverseBindPose(bone.mOffsetMatrix);
			jointWeight.vertexWeights.reserve(jointWeight.vertexWeights.size() + bone.mNumWeights);
			for (uint32_t weightIndex = 0; weightIndex < bone.mNumWeights; ++weightIndex) {
				const aiVertexWeight& weight = bone.mWeights[weightIndex];
				jointWeight.vertexWeights.push_back({ weight.mWeight, vertexOffset + weight.mVertexId });
			}
		}

		if (asset.modelData.material.textureFilePath.empty() && mesh.mMaterialIndex < scene->mNumMaterials) {
			aiString texturePath;
			if (scene->mMaterials[mesh.mMaterialIndex]->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS) {
				asset.modelData.material.textureFilePath =
					(sourcePath.parent_path() / std::filesystem::path(texturePath.C_Str())).generic_string();
			}
		}
	}

	if (asset.modelData.material.textureFilePath.empty()) {
		asset.modelData.material.textureFilePath = "resources/white512x512.png";
	}
	return asset;
}

SkinCluster SkinCluster::Create(const Skeleton& skeleton, const SkinningModelAsset& asset) {
	SkinCluster cluster;
	cluster.inverseBindPoseMatrices_.assign(skeleton.joints.size(), MakeIdentity4x4());
	cluster.palette_.resize(skeleton.joints.size());
	cluster.influences_.resize(asset.modelData.vertices.size());

	for (const auto& [jointName, jointWeight] : asset.jointWeights) {
		const auto joint = skeleton.jointMap.find(jointName);
		if (joint == skeleton.jointMap.end()) {
			continue;
		}
		const int32_t jointIndex = joint->second;
		cluster.inverseBindPoseMatrices_[jointIndex] = jointWeight.inverseBindPoseMatrix;
		for (const VertexWeightData& vertexWeight : jointWeight.vertexWeights) {
			if (vertexWeight.vertexIndex < cluster.influences_.size() && vertexWeight.weight > 0.0f) {
				InsertInfluence(cluster.influences_[vertexWeight.vertexIndex], vertexWeight.weight, jointIndex);
			}
		}
	}

	for (VertexInfluence& influence : cluster.influences_) {
		float total = 0.0f;
		for (float weight : influence.weights) {
			total += weight;
		}
		if (total > 0.0f) {
			for (float& weight : influence.weights) {
				weight /= total;
				if (weight > 0.0f) {
					++cluster.assignedInfluenceCount_;
				}
			}
		} else {
			influence.weights[0] = 1.0f;
			influence.jointIndices[0] = skeleton.root >= 0 ? skeleton.root : 0;
			++cluster.assignedInfluenceCount_;
		}
	}
	cluster.Update(skeleton);
	return cluster;
}

void SkinnedModel::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, const std::string& filePath) {
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	asset_ = SkinningModelLoader::LoadFromFile(filePath);
	skeleton_ = SkeletonSystem::Create(asset_.rootNode);
	skinCluster_ = SkinCluster::Create(skeleton_, asset_);
	animation_ = AnimationLoader::LoadFromFile(filePath);
	animationPlayer_.SetAnimation(&animation_);

	const size_t vertexBytes = sizeof(VertexData) * asset_.modelData.vertices.size();
	vertexResource_ = dxCommon_->CreateBufferResource(vertexBytes);
	VertexData* mappedVertices = nullptr;
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertices));
	std::memcpy(mappedVertices, asset_.modelData.vertices.data(), vertexBytes);
	vertexBufferViews_[0].BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferViews_[0].SizeInBytes = static_cast<UINT>(vertexBytes);
	vertexBufferViews_[0].StrideInBytes = sizeof(VertexData);

	const size_t influenceBytes = sizeof(VertexInfluence) * skinCluster_.GetInfluences().size();
	influenceResource_ = dxCommon_->CreateBufferResource(influenceBytes);
	VertexInfluence* mappedInfluences = nullptr;
	influenceResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedInfluences));
	std::memcpy(mappedInfluences, skinCluster_.GetInfluences().data(), influenceBytes);
	vertexBufferViews_[1].BufferLocation = influenceResource_->GetGPUVirtualAddress();
	vertexBufferViews_[1].SizeInBytes = static_cast<UINT>(influenceBytes);
	vertexBufferViews_[1].StrideInBytes = sizeof(VertexInfluence);

	const size_t indexBytes = sizeof(uint32_t) * asset_.modelData.indices.size();
	indexResource_ = dxCommon_->CreateBufferResource(indexBytes);
	uint32_t* mappedIndices = nullptr;
	indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedIndices));
	std::memcpy(mappedIndices, asset_.modelData.indices.data(), indexBytes);
	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
	indexBufferView_.SizeInBytes = static_cast<UINT>(indexBytes);
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	const size_t paletteBytes = sizeof(SkinningPaletteEntry) * skinCluster_.GetPalette().size();
	paletteResource_ = dxCommon_->CreateBufferResource(paletteBytes);
	paletteResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedPalette_));
	std::memcpy(mappedPalette_, skinCluster_.GetPalette().data(), paletteBytes);
	paletteSrvIndex_ = srvManager_->Allocate();
	srvManager_->CreateSRVforStructuredBuffer(
		paletteSrvIndex_, paletteResource_.Get(),
		static_cast<UINT>(skinCluster_.GetPalette().size()), sizeof(SkinningPaletteEntry));

	TextureManager::GetInstance()->LoadTexture(asset_.modelData.material.textureFilePath);
}

void SkinnedModel::Update(float deltaTime) {
	animationPlayer_.Update(deltaTime);
	SkeletonSystem::ApplyAnimation(skeleton_, animationPlayer_);
	skinCluster_.Update(skeleton_);
	std::memcpy(
		mappedPalette_, skinCluster_.GetPalette().data(),
		sizeof(SkinningPaletteEntry) * skinCluster_.GetPalette().size());
}

void SkinnedModel::UpdateBlended(
	float deltaTime,
	AnimationPlayer& animationA,
	AnimationPlayer& animationB,
	float blendFactor) {
	// 補間中だけでなく両方を常時進めておくことで、切替時に時間が飛ばない。
	animationA.Update(deltaTime);
	animationB.Update(deltaTime);
	SkeletonSystem::ApplyAnimationBlend(skeleton_, animationA, animationB, blendFactor);
	skinCluster_.Update(skeleton_);
	std::memcpy(
		mappedPalette_, skinCluster_.GetPalette().data(),
		sizeof(SkinningPaletteEntry) * skinCluster_.GetPalette().size());
}

void SkinnedModel::Draw() {
	auto commandList = dxCommon_->GetList();
	commandList->IASetVertexBuffers(0, 2, vertexBufferViews_);
	commandList->IASetIndexBuffer(&indexBufferView_);
	commandList->SetGraphicsRootDescriptorTable(
		2, TextureManager::GetInstance()->GetSrvHandleGPU(asset_.modelData.material.textureFilePath));
	srvManager_->SetGraphicsRootDescriptorTable(9, paletteSrvIndex_);
	commandList->DrawIndexedInstanced(static_cast<UINT>(asset_.modelData.indices.size()), 1, 0, 0, 0);
}

void SkinnedModel::DrawShadow() {
	auto commandList = dxCommon_->GetList();
	commandList->IASetVertexBuffers(0, 2, vertexBufferViews_);
	commandList->IASetIndexBuffer(&indexBufferView_);
	srvManager_->SetGraphicsRootDescriptorTable(1, paletteSrvIndex_);
	commandList->DrawIndexedInstanced(static_cast<UINT>(asset_.modelData.indices.size()), 1, 0, 0, 0);
}

void SkinCluster::Update(const Skeleton& skeleton) {
	assert(skeleton.joints.size() == inverseBindPoseMatrices_.size());
	for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex) {
		const Matrix4x4 skinningMatrix = Multiply(
			inverseBindPoseMatrices_[jointIndex], skeleton.joints[jointIndex].skeletonSpaceMatrix);
		palette_[jointIndex].skeletonSpaceMatrix = skinningMatrix;
		palette_[jointIndex].skeletonSpaceInverseTransposeMatrix = Transpose(Inverse(skinningMatrix));
	}
}
