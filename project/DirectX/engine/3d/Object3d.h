#pragma once
#include "Object3dCommon.h"
#include "Resource.h"
#include "Model.h"

class Object3d
{
public:

	void Initialize(Object3dCommon* object3dCommon);

	void Update(const Matrix4x4& cameraMatrix);

	void Draw();

	void SetModel(Model* model) { model_ = model; }

	Vector3& GetScale() { return transform.scale; }
	Vector3& GetRotate() { return transform.rotate; }
	Vector3& GetTranslate() { return transform.translate; }

	void SetScale(const Vector3& scale) { transform.scale = scale; }
	void SetRotate(const Vector3& rotate) { transform.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform.translate = translate; }


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
};

