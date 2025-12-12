#pragma once
#include "Object3dCommon.h"
#include "Resource.h"
#include "ModelManager.h"

class Object3d
{
public:

	void Initialize(Object3dCommon* object3dCommon);

	void Update();

	void Draw();

	void SetModel(Model* model) { model_ = model; }

	void SetModel(const std::string& filePath);

	Vector3& GetScale() { return transform.scale; }
	Vector3& GetRotate() { return transform.rotate; }
	Vector3& GetTranslate() { return transform.translate; }

	void SetScale(const Vector3& scale) { transform.scale = scale; }
	void SetRotate(const Vector3& rotate) { transform.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform.translate = translate; }

	void SetCamera(Camera* camera) { camera_ = camera; }
	void SetDebugCamera(DebugCamera* debugCamera) { debugCamera_ = debugCamera; }

private:

	Object3dCommon* object3dCommon_;

	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource;
	
	TransformationMatrix* transformationMatrixData;
	DirectionalLight* directionalLightData;

	Texture texture;
	Resource resource;

	Transform transform;

	Model* model_ = nullptr;

	Camera* camera_ = nullptr;
	DebugCamera* debugCamera_ = nullptr;
};

