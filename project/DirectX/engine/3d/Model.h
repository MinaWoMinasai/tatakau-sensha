#pragma once
#include "ModelCommon.h"
#include "TextureManager.h"

namespace cg2 {

/// @brief 頂点・マテリアル・テクスチャを保持し、静的モデルを描画する。
class Model {

public:
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(ModelCommon* modelCommon, const std::string& directorypath, const std::string& filename);
    /// @brief からのモデルデータを初期化する。
    void InitializeFromModelData(ModelCommon* modelCommon, const ModelData& modelData);
    /// @brief 滑らかなNormalsを現在の条件から再計算する。
    void RecalculateSmoothNormals();

    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    void Draw();
    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    void Draw(const std::vector<D3D12_GPU_VIRTUAL_ADDRESS>& materialCbvAddresses);
    /// @brief 専用メッシュを描画する。
    void DrawOnlyMesh();

    /// @brief 材質Templateファイルを読み込む。
    static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

    /// @brief OBJ形式ファイルを読み込む。
    static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);
    /// @brief Assimpファイルを読み込む。
    static ModelData LoadAssimpFile(const std::string& directoryPath, const std::string& filename);
    /// @brief 平面を生成する。
    static ModelData CreatePlane(float width, float depth);
    /// @brief 格子を生成する。
    static ModelData CreateGrid(float width, float depth, uint32_t xSegments, uint32_t zSegments);
    /// @brief 箱を生成する。
    static ModelData CreateBox(const Vector3& size);
    /// @brief Faceted結晶を生成する。
    static ModelData CreateFacetedCrystal(float radius, float height, uint32_t sides = 8);
    /// @brief Cylinderを生成する。
    static ModelData CreateCylinder(float radius, float height, uint32_t segments = 64);
    /// @brief UV球を生成する。
    static ModelData CreateUvSphere(float radius, uint32_t latitudeSegments, uint32_t longitudeSegments);

    /// @brief モデルデータを返す。
    const ModelData& GetModelData() const
    {
        return modelData_;
    }
    /// @brief 頂点リソースを返す。
    const Microsoft::WRL::ComPtr<ID3D12Resource>& GetVertexResource() const
    {
        return vertexResource;
    }
    /// @brief 頂点バッファ視点を返す。
    const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const
    {
        return vertexBufferView;
    }
    /// @brief 添字バッファ視点を返す。
    const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const
    {
        return indexBufferView;
    }

private:
    /// @brief GPUリソースを生成する。
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

} // namespace cg2
