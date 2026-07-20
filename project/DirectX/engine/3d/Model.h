#pragma once
#include "ModelCommon.h"
#include "TextureManager.h"

class Model
{

public:
	void Initialize(ModelCommon* modelCommon, const std::string& directorypath, const std::string& filename);
	void InitializeFromModelData(ModelCommon* modelCommon, const ModelData& modelData);
	void RecalculateSmoothNormals();

	void Draw();
	void Draw(const std::vector<D3D12_GPU_VIRTUAL_ADDRESS>& materialCbvAddresses);
	void DrawOnlyMesh();

	static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

	static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);
	static ModelData LoadAssimpFile(const std::string& directoryPath, const std::string& filename);
	static ModelData CreatePlane(float width, float depth);
	static ModelData CreateBox(const Vector3& size);
	static ModelData CreateCylinder(float radius, float height, uint32_t segments = 64);
	static ModelData CreateUvSphere(float radius, uint32_t latitudeSegments, uint32_t longitudeSegments);

	ModelData& GetModelData() { return modelData_; }
	const ModelData& GetModelData() const { return modelData_; }
	Microsoft::WRL::ComPtr<ID3D12Resource>& GetVertexResource() { return vertexResource; }
	D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() { return vertexBufferView; }
	D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() { return indexBufferView; }

private:
	void CreateGpuResources();

	ModelCommon* modelCommon_;

	// Objファイルのデータ
	ModelData modelData_;

	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	// バッファリソース内のデータをさすポインタ
	VertexData* vertexData;
	// バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;
	uint32_t* indexData = nullptr;
	D3D12_INDEX_BUFFER_VIEW indexBufferView{};

	Texture texture;
};

