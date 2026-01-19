#pragma once
#include "ModelCommon.h"
#include "TextureManager.h"

class Model
{

public:
	void Initialize(ModelCommon* modelCommon, const std::string& directorypath, const std::string& filename);

	void Draw();

	static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

	static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);

private:
	ModelCommon* modelCommon_;

	// Objファイルのデータ
	ModelData modelData_;

	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	// バッファリソース内のデータをさすポインタ
	VertexData* vertexData;
	// バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

	Texture texture;
};

