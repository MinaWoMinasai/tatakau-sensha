#pragma once
#include "Object3dCommon.h"
#include "Resource.h"
#include "TextureManager.h"

class Object3d
{
public:

	void Initialize(Object3dCommon* object3dCommon);

	void Update(const Matrix4x4& cameraMatrix);

	void Draw();

	static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

	static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);

private:

	Object3dCommon* object3dCommon_;

	// Objファイルのデータ
	ModelData modelData_;

	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource;
	// バッファリソース内のデータをさすポインタ
	VertexData* vertexData;
	Material* materialData;
	TransformationMatrix* transformationMatrixData;
	DirectionalLight* directionalLightData;
	// バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

	Texture texture;
	Resource resource;

	Transform transform;
};

