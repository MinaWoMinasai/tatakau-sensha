#pragma once
#include "DirectXCommon.h"
#include "TextureManager.h"

namespace myEngine {

/// @brief 手続き生成する基本の形。
enum class PrimitiveShape {
    Plane,          // XZ 平面の四角（両面）
    Grid,           // XZ 平面の格子（segments × rings に分ける）
    Box,            // 箱（両面）
    FacetedCrystal, // 上下がとがった多角柱の結晶（segments 面、5〜16）
    Cylinder,       // 円柱（segments 分割）
    UvSphere,       // 緯度・経度で分けた球（rings × segments）
};

/// @brief 基本の形の寸法。形ごとに使う値だけを読む。
struct PrimitiveParams {
    /// @brief 幅（x）・高さ（y）・奥行き（z）。Plane・Grid は x と z、Box は全部、FacetedCrystal・Cylinder は y を高さに使う。
    Vector3 size{1.0f, 1.0f, 1.0f};
    /// @brief 半径。FacetedCrystal・Cylinder・UvSphere で使う。
    float radius = 0.5f;
    /// @brief 横の分割数。Grid の x、FacetedCrystal の面の数、Cylinder・UvSphere の周の分割。0 なら形の既定値（結晶 8・円柱 64・ほか 1）。
    uint32_t segments = 0;
    /// @brief 縦の分割数。Grid の z、UvSphere の緯度の分割。0 なら形の既定値（球 16・ほか 1）。
    uint32_t rings = 0;
};

/// @brief 形の名前（"plane"・"grid"・"box"・"facetedCrystal"・"cylinder"・"uvSphere"）を読む。知らない名前なら false。
bool ParsePrimitiveShape(const std::string& name, PrimitiveShape& shape);

/// @brief 頂点・マテリアル・テクスチャを保持し、静的モデルを描画する。
class Model {
public:
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(DirectXCommon* dxCommon, const std::string& directorypath, const std::string& filename);
    /// @brief 読み込み済みのモデルデータから初期化する。
    void InitializeFromModelData(DirectXCommon* dxCommon, const ModelData& modelData);
    /// @brief 面の向きから、なめらかな頂点法線を計算し直す。
    void RecalculateSmoothNormals();

    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    void Draw();
    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    /// @param baseColorTexture nullptr でなければ、すべてのマテリアルの基本色のテクスチャの代わりに使う。
    void Draw(const std::vector<D3D12_GPU_VIRTUAL_ADDRESS>& materialCbvAddresses,
              const D3D12_GPU_DESCRIPTOR_HANDLE* baseColorTexture = nullptr);
    /// @brief 専用メッシュを描画する。
    void DrawOnlyMesh();

    /// @brief 材質のテンプレートファイルを読み込む。
    static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

    /// @brief OBJ形式ファイルを読み込む。
    static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);
    /// @brief Assimpファイルを読み込む。
    static ModelData LoadAssimpFile(const std::string& directoryPath, const std::string& filename);
    /// @brief 基本の形を生成する（既定の材質付き）。名前で共有するときは ModelManager::CreateModelFromData に渡す。
    static ModelData CreatePrimitive(PrimitiveShape shape, const PrimitiveParams& params);

    /// @brief モデルデータを返す。
    const ModelData& GetModelData() const
    {
        return modelData_;
    }

    /// @brief 頂点バッファと添字バッファを、描画の入力に結び付ける（描画は呼び出し側でする。インスタンス描画など）。
    void BindMeshBuffers(ID3D12GraphicsCommandList* list) const
    {
        list->IASetVertexBuffers(0, 1, &vertexBufferView);
        list->IASetIndexBuffer(&indexBufferView);
    }

private:
    /// @brief GPUリソースを生成する。
    void CreateGpuResources();

    DirectXCommon* dxCommon_ = nullptr;

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

} // namespace myEngine
