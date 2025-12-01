#pragma once
#include "ModelCommon.h"
#include "TextureManager.h"

class Model
{

public:
	void Initialize(ModelCommon* modelCommon);

	void Draw();

	static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

	static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);

private:
	ModelCommon* modelCommon_;

	// Objファイルのデータ
	ModelData modelData_;

	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
	// バッファリソース内のデータをさすポインタ
	VertexData* vertexData;
	Material* materialData;
	// バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

	Texture texture;
};

