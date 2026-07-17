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
#include <unordered_map>
#include <unordered_set>

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
	std::unordered_map<uint32_t, std::string> materialTextureKeys;

	auto resolveMaterialTexture = [&](uint32_t materialIndex) -> std::string {
		const auto cached = materialTextureKeys.find(materialIndex);
		if (cached != materialTextureKeys.end()) {
			return cached->second;
		}

		std::string textureKey = "resources/white512x512.png";
		if (materialIndex < scene->mNumMaterials) {
			const aiMaterial& material = *scene->mMaterials[materialIndex];
			aiString texturePath;
			aiReturn result = material.GetTexture(aiTextureType_BASE_COLOR, 0, &texturePath);
			if (result != AI_SUCCESS) {
				result = material.GetTexture(aiTextureType_DIFFUSE, 0, &texturePath);
			}
			if (result == AI_SUCCESS && texturePath.length > 0) {
				const aiTexture* embedded = scene->GetEmbeddedTexture(texturePath.C_Str());
				if (embedded != nullptr && embedded->mHeight == 0 && embedded->mWidth > 0) {
					textureKey = filePath + "#embedded/" + texturePath.C_Str();
					const auto alreadyCopied = std::find_if(
						asset.embeddedTextures.begin(), asset.embeddedTextures.end(),
						[&](const SkinningModelAsset::EmbeddedTexture& texture) {
							return texture.textureKey == textureKey;
						});
					if (alreadyCopied == asset.embeddedTextures.end()) {
						SkinningModelAsset::EmbeddedTexture texture;
						texture.textureKey = textureKey;
						const auto* begin = reinterpret_cast<const uint8_t*>(embedded->pcData);
						texture.encodedData.assign(begin, begin + embedded->mWidth);
						asset.embeddedTextures.push_back(std::move(texture));
					}
				} else if (embedded == nullptr) {
					textureKey =
						(sourcePath.parent_path() / std::filesystem::path(texturePath.C_Str())).generic_string();
				}
			}
		}
		materialTextureKeys.emplace(materialIndex, textureKey);
		return textureKey;
	};

	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
		const aiMesh& mesh = *scene->mMeshes[meshIndex];
		const uint32_t vertexOffset = static_cast<uint32_t>(asset.modelData.vertices.size());
		const uint32_t indexStart = static_cast<uint32_t>(asset.modelData.indices.size());
		asset.modelData.vertices.reserve(asset.modelData.vertices.size() + mesh.mNumVertices);
		for (uint32_t vertexIndex = 0; vertexIndex < mesh.mNumVertices; ++vertexIndex) {
			const aiVector3D& position = mesh.mVertices[vertexIndex];
			const aiVector3D normal = mesh.HasNormals() ? mesh.mNormals[vertexIndex] : aiVector3D{};
			const aiVector3D uv = mesh.HasTextureCoords(0) ? mesh.mTextureCoords[0][vertexIndex] : aiVector3D{};
			asset.modelData.vertices.push_back({
				{ -position.x, position.y, position.z, 1.0f },
				// AssimpのglTFインポータはVを下原点へ変換して返すため、
				// DirectX/WICの上原点テクスチャへ合わせて元に戻す。
				{ uv.x, 1.0f - uv.y },
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

		const uint32_t indexCount = static_cast<uint32_t>(asset.modelData.indices.size()) - indexStart;
		if (indexCount > 0) {
			SkinningModelAsset::Submesh submesh;
			submesh.indexStart = indexStart;
			submesh.indexCount = indexCount;
			submesh.textureKey = resolveMaterialTexture(mesh.mMaterialIndex);
			if (mesh.mMaterialIndex < scene->mNumMaterials) {
				int twoSided = 0;
				if (scene->mMaterials[mesh.mMaterialIndex]->Get(AI_MATKEY_TWOSIDED, twoSided) == AI_SUCCESS) {
					submesh.doubleSided = twoSided != 0;
				}
			}
			asset.submeshes.push_back(std::move(submesh));
		}
	}

	if (asset.submeshes.empty()) {
		asset.submeshes.push_back({
			0, static_cast<uint32_t>(asset.modelData.indices.size()), "resources/white512x512.png" });
	}
	asset.modelData.material.textureFilePath = asset.submeshes.front().textureKey;
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
	try {
		animations_ = AnimationLoader::LoadAllFromFile(filePath);
		currentAnimationIndex_ = 0;
		animationPlayer_.SetAnimation(&animations_[currentAnimationIndex_]);
	} catch (const std::exception&) {
		// 骨格付き静止モデルもSkinnedModelで確認できるよう、
		// アニメーションがない場合はバインドポーズを維持する空クリップを使う。
		Animation bindPose;
		bindPose.name = "BindPose";
		bindPose.duration = 0.0f;
		animations_.push_back(std::move(bindPose));
		currentAnimationIndex_ = 0;
		animationPlayer_.SetAnimation(&animations_[currentAnimationIndex_]);
		animationPlayer_.SetPlaying(false);
	}

	std::unordered_set<std::string> embeddedTextureKeys;
	for (const SkinningModelAsset::EmbeddedTexture& embedded : asset_.embeddedTextures) {
		if (TextureManager::GetInstance()->LoadTextureFromMemory(
			embedded.textureKey, embedded.encodedData.data(), embedded.encodedData.size())) {
			embeddedTextureKeys.insert(embedded.textureKey);
		} else {
			for (SkinningModelAsset::Submesh& submesh : asset_.submeshes) {
				if (submesh.textureKey == embedded.textureKey) {
					submesh.textureKey = "resources/white512x512.png";
				}
			}
		}
	}
	for (const SkinningModelAsset::Submesh& submesh : asset_.submeshes) {
		if (!embeddedTextureKeys.contains(submesh.textureKey)) {
			TextureManager::GetInstance()->LoadTexture(submesh.textureKey);
		}
	}

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

}

bool SkinnedModel::SetAnimation(const std::string& name, bool restart) {
	for (size_t index = 0; index < animations_.size(); ++index) {
		if (animations_[index].name == name) {
			return SetAnimation(index, restart);
		}
	}
	return false;
}

bool SkinnedModel::SetAnimation(size_t index, bool restart) {
	if (index >= animations_.size()) {
		return false;
	}
	if (index == currentAnimationIndex_ && animationPlayer_.GetAnimation() != nullptr) {
		return true;
	}
	currentAnimationIndex_ = index;
	animationPlayer_.SetAnimation(&animations_[currentAnimationIndex_], restart);
	animationTransitionActive_ = false;
	return true;
}

bool SkinnedModel::TransitionToAnimation(
	const std::string& name,
	float duration,
	bool synchronizeNormalizedTime) {
	for (size_t index = 0; index < animations_.size(); ++index) {
		if (animations_[index].name == name) {
			return TransitionToAnimation(index, duration, synchronizeNormalizedTime);
		}
	}
	return false;
}

bool SkinnedModel::TransitionToAnimation(
	size_t index,
	float duration,
	bool synchronizeNormalizedTime) {
	if (index >= animations_.size()) {
		return false;
	}
	if (index == currentAnimationIndex_) {
		return true;
	}
	if (duration <= 0.0f || animationPlayer_.GetAnimation() == nullptr) {
		return SetAnimation(index, true);
	}

	animationTransitionStartPose_.clear();
	animationTransitionStartPose_.reserve(skeleton_.joints.size());
	for (const Joint& joint : skeleton_.joints) {
		animationTransitionStartPose_.push_back(joint.transform);
	}
	float normalizedTime = 0.0f;
	if (synchronizeNormalizedTime && animationPlayer_.GetAnimation() != nullptr) {
		const float previousDuration = animationPlayer_.GetAnimation()->duration;
		if (previousDuration > 0.0f) {
			normalizedTime = animationPlayer_.GetTime() / previousDuration;
		}
	}

	currentAnimationIndex_ = index;
	animationPlayer_.SetAnimation(&animations_[currentAnimationIndex_], true);
	if (synchronizeNormalizedTime && animations_[currentAnimationIndex_].duration > 0.0f) {
		animationPlayer_.Seek(normalizedTime * animations_[currentAnimationIndex_].duration);
	}
	animationTransitionDuration_ = duration;
	animationTransitionElapsed_ = 0.0f;
	animationTransitionActive_ = true;
	return true;
}

void SkinnedModel::Update(float deltaTime) {
	if (animationTransitionActive_) {
		animationPlayer_.Update(deltaTime);
		animationTransitionElapsed_ += deltaTime;
		const float linearT = (std::clamp)(
			animationTransitionElapsed_ / animationTransitionDuration_, 0.0f, 1.0f);
		const float smoothT = linearT * linearT * (3.0f - 2.0f * linearT);
		SkeletonSystem::ApplyAnimationBlendFromPose(
			skeleton_, animationTransitionStartPose_, animationPlayer_, smoothT);
		if (linearT >= 1.0f) {
			animationTransitionActive_ = false;
		}
	} else {
		animationPlayer_.Update(deltaTime);
		SkeletonSystem::ApplyAnimation(skeleton_, animationPlayer_);
	}
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
	srvManager_->SetGraphicsRootDescriptorTable(9, paletteSrvIndex_);
	for (const SkinningModelAsset::Submesh& submesh : asset_.submeshes) {
		auto& pso = submesh.doubleSided
			? dxCommon_->GetPSOSkinningDoubleSidedForScene()
			: dxCommon_->GetPSOSkinningForScene();
		commandList->SetPipelineState(pso.graphicsState_.Get());
		commandList->SetGraphicsRootDescriptorTable(
			2, TextureManager::GetInstance()->GetSrvHandleGPU(submesh.textureKey));
		commandList->DrawIndexedInstanced(
			submesh.indexCount, 1, submesh.indexStart, 0, 0);
	}
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
