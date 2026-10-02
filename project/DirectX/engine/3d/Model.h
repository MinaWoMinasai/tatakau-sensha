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
	static ModelData CreateGrid(float width, float depth, uint32_t xSegments, uint32_t zSegments);
	static ModelData CreateBox(const Vector3& size);
	static ModelData CreateFacetedCrystal(float radius, float height, uint32_t sides = 8);
	static ModelData CreateCylinder(float radius, float height, uint32_t segments = 64);
	static ModelData CreateUvSphere(float radius, uint32_t latitudeSegments, uint32_t longitudeSegments);

	const ModelData& GetModelData() const { return modelData_; }
	const Microsoft::WRL::ComPtr<ID3D12Resource>& GetVertexResource() const { return vertexResource; }
	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { return vertexBufferView; }
	const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const { return indexBufferView; }

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

