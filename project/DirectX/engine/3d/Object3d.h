#pragma once
#include "Object3dCommon.h"
#include "Resource.h"
#include "ModelManager.h"

class Object3d
{
public:

	void Initialize();

	void Update();

	void Draw();

	void SetModel(Model* model) { model_ = model; }

	void SetModel(const std::string& filePath);

	Vector3& GetScale() { return transform_.scale; }
	Vector3& GetRotate() { return transform_.rotate; }
	Vector3& GetTranslate() { return transform_.translate; }

	void SetTransform(const Transform& transform) { transform_ = transform; }

	void SetScale(const Vector3& scale) { transform_.scale = scale; }
	void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform_.translate = translate; }

	void SetCamera(Camera* camera) { camera_ = camera; }
	void SetDebugCamera(DebugCamera* debugCamera) { debugCamera_ = debugCamera; }

	Vector4& GetColor() {
		return materialData_->color;
	}

	void SetColor(const Vector4& color) {
		materialData_->color = color;
	}

	void SetAlpha(const float& color) {
		materialData_->color.w = color;
	}

	void SetLighting(bool enable) {
		materialData_->enableLighting = enable;
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

private:

	Object3dCommon* object3dCommon_;

	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource;
	
	TransformationMatrix* transformationMatrixData;
	DirectionalLight* directionalLightData;
	
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Material* materialData_ = nullptr;
	
	Texture texture;
	Resource resource;

	Transform transform_;

	Model* model_ = nullptr;

	Camera* camera_ = nullptr;
	DebugCamera* debugCamera_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;
	CameraData* cameraData_ = nullptr;
};

