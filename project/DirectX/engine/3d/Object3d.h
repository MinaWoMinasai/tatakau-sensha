#pragma once
#include "Object3dCommon.h"
#include "Resource.h"
#include "ModelManager.h"
#include <vector>

class SkinnedModel;

class Object3d
{
public:

	void Initialize();

	void Update();
	// Skeleton Jointなど、親側で完成したワールド行列をそのまま利用する。
	// 装備品・アクセサリ・子オブジェクトの追従用途を想定。
	void UpdateWithWorldMatrix(const Matrix4x4& worldMatrix);

	void Draw();
	void DrawSkinned(SkinnedModel& model);
	void DrawSkinnedShadow(SkinnedModel& model);
	void DrawShadow();

	void SetModel(Model* model);

	void SetModel(const std::string& filePath);

	Vector3& GetScale() { return transform_.scale; }
	Vector3& GetRotate() { return transform_.rotate; }
	Vector3& GetTranslate() { return transform_.translate; }

	void SetTransform(const Transform& transform) { transform_ = transform; useQuaternionRotate_ = false; }

	void SetScale(const Vector3& scale) { transform_.scale = scale; }
	void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; useQuaternionRotate_ = false; }
	void SetQuaternionRotate(const Quaternion& rotate) { quaternionRotate_ = rotate; useQuaternionRotate_ = true; }
	void UseEulerRotate() { useQuaternionRotate_ = false; }
	void SetTranslate(const Vector3& translate) { transform_.translate = translate; }

	void SetCamera(Camera* camera) { camera_ = camera; }
	void SetDebugCamera(DebugCamera* debugCamera) { debugCamera_ = debugCamera; }

	Vector4& GetColor() {
		return materialData_->color;
	}

	void SetColor(const Vector4& color) {
		materialData_->color = color;
		userColorOverride_ = true;
	}

	void SetAlpha(const float& color) {
		materialData_->color.w = color;
		userColorOverride_ = true;
	}

	void SetLighting(bool enable) {
		materialData_->enableLighting = enable;
	}
	void SetLightingMode(int32_t mode) {
		materialData_->lightingMode = mode;
	}
	bool IsLightingEnabled() const {
		return materialData_->enableLighting != 0;
	}
	void SetDirectionalLightDirection(const Vector3& direction) {
		directionalLightData->direction = Normalize(direction);
	}
	void SetShininess(float shininess) {
		materialData_->shininess = shininess;
	}
	void SetInsensity(float insensity) {
		directionalLightData->intensity = insensity;
	}

	Vector3 GetPointLightDirection() {
		return directionalLightData->direction;
	}

	Vector3 GetPointLightPosition() {
		return pointLightData->position;
	}

	void SetPointLightDirection(const Vector3& direction) {
		directionalLightData->direction = Normalize(direction);
	}

	void SetPointLightPosition(const Vector3& position) {
		pointLightData->position = position;
	}

	ID3D12Resource* GetMaterialResource() { return materialResource_.Get(); }
	ID3D12Resource* GetTransformationResource() { return transformationMatrixResource.Get(); }
	Model* GetModel() { return model_; }

	void SetEnvironmentMap(uint32_t srvIndex) { environmentMapIndex_ = srvIndex; }
	void SetEnvironmentCoefficient(float coefficient) { materialData_->environmentCoefficient = coefficient; }
	float GetEnvironmentCoefficient() const { return materialData_->environmentCoefficient; }
	void SetMetallic(float metallic) { materialData_->metallic = metallic; userMetallicOverride_ = true; }
	void SetRoughness(float roughness) { materialData_->roughness = roughness; userRoughnessOverride_ = true; }
	void SetAmbientOcclusion(float ambientOcclusion) { materialData_->ambientOcclusion = ambientOcclusion; userAmbientOcclusionOverride_ = true; }
	void SetEmissive(const Vector3& color, float intensity) {
		materialData_->emissiveColor = color;
		materialData_->emissiveIntensity = intensity;
		userEmissiveOverride_ = true;
	}
	void SetIBLIntensity(float diffuseIntensity, float specularIntensity) {
		materialData_->iblDiffuseIntensity = diffuseIntensity;
		materialData_->iblSpecularIntensity = specularIntensity;
	}
	void SetIBLMaxMipLevel(float maxMipLevel) { materialData_->iblMaxMipLevel = maxMipLevel; }
	void SetPBREnvironmentMode(float mode) { materialData_->pbrEnvironmentMode = mode; }
	void SetShadowReceiveStrength(float strength) { materialData_->shadowReceiveStrength = strength; }
	void SetNormalDetail(float strength, float scale) {
		materialData_->normalDetailStrength = strength;
		materialData_->normalDetailScale = scale;
	}
	void SetNormalMapStrength(float strength) { materialData_->normalMapStrength = strength; }
	void SetMetallicMapStrength(float strength) { materialData_->metallicMapStrength = strength; }
	void SetRoughnessMapStrength(float strength) { materialData_->roughnessMapStrength = strength; }
	void SetMetallicRoughnessMapStrength(float strength) {
		materialData_->metallicMapStrength = strength;
		materialData_->roughnessMapStrength = strength;
	}
	void SetOcclusionMapStrength(float strength) { materialData_->occlusionMapStrength = strength; }
	void SetPackedMaterialMapChannels(float metallicChannel, float roughnessChannel, float occlusionChannel) {
		materialData_->metallicMapChannel = metallicChannel;
		materialData_->roughnessMapChannel = roughnessChannel;
		materialData_->occlusionMapChannel = occlusionChannel;
	}
	void SetShadowFilter(float depthBias, float slopeBias, float pcfRadius) {
		materialData_->shadowDepthBias = depthBias;
		materialData_->shadowSlopeBias = slopeBias;
		materialData_->shadowPcfRadius = pcfRadius;
	}
	void SetMaterialDebugMode(int32_t mode) { materialData_->materialDebugMode = static_cast<float>(mode); }
	void SetCharacterShading(
		float lightWrap,
		float shadowSoftness,
		float shadowStrength,
		float rimStrength,
		float rimPower,
		float specularStrength,
		float specularPower) {
		materialData_->characterLightWrap = lightWrap;
		materialData_->characterShadowSoftness = shadowSoftness;
		materialData_->characterShadowStrength = shadowStrength;
		materialData_->characterRimStrength = rimStrength;
		materialData_->characterRimPower = rimPower;
		materialData_->characterSpecularStrength = specularStrength;
		materialData_->characterSpecularPower = specularPower;
	}

private:
	void UpdateMatrixConstants(const Matrix4x4& worldMatrix);
	void ApplyModelMaterialData();
	void ResetMaterialOverrideFlags();
	void EnsureMaterialInstanceResources(size_t materialCount);
	void UpdateMaterialInstanceData();
	void UpdateMaterialInstanceData(const ModelData& modelData);
	Material BuildMaterialForModelMaterial(const MaterialData& materialData) const;

	Object3dCommon* object3dCommon_;

	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource;

	TransformationMatrixWithShadow* transformationMatrixData;
	DirectionalLight* directionalLightData;
	PointLightData* pointLightData;
	
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Material* materialData_ = nullptr;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> materialInstanceResources_;
	std::vector<Material*> materialInstanceData_;
	
	Texture texture;
	Resource resource;

	Transform transform_;
	Quaternion quaternionRotate_ = { 0.0f, 0.0f, 0.0f, 1.0f };
	bool useQuaternionRotate_ = false;

	Model* model_ = nullptr;

	Camera* camera_ = nullptr;
	DebugCamera* debugCamera_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;
	CameraData* cameraData_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> shadowDataResource;
	ShadowData* shadowData;

	Matrix4x4 lightViewProjection_;

	uint32_t environmentMapIndex_ = 0; // デフォルトのSRVインデックス

	bool userColorOverride_ = false;
	bool userMetallicOverride_ = false;
	bool userRoughnessOverride_ = false;
	bool userAmbientOcclusionOverride_ = false;
	bool userEmissiveOverride_ = false;
};

