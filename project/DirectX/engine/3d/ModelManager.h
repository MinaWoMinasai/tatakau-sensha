#pragma once
#include <map>
#include <string>
#include <memory>
#include <Model.h>

namespace cg2 {

/// @brief モデルを識別子で読み込み・生成し、同じモデルを共有する。
class ModelManager {

public:
    /// @brief 共有インスタンスを返す。呼び出し側は取得したポインターをdeleteしない。
    static ModelManager* GetInstance();

    /// @brief 利用終了時の資源と状態を解放する。
    void Finalize();

    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(DirectXCommon* dxCommon);

    /// @brief モデルを読み込む。
    void LoadModel(const std::string& filePath);
    /// @brief 平面モデルを生成する。
    void CreatePlaneModel(const std::string& modelName, float width = 1.0f, float depth = 1.0f);
    /// @brief 格子モデルを生成する。
    void CreateGridModel(const std::string& modelName, float width, float depth, uint32_t xSegments, uint32_t zSegments);
    /// @brief 箱モデルを生成する。
    void CreateBoxModel(const std::string& modelName, const Vector3& size = {1.0f, 1.0f, 1.0f});
    /// @brief Faceted結晶モデルを生成する。
    void CreateFacetedCrystalModel(const std::string& modelName, float radius = 1.0f, float height = 3.0f, uint32_t sides = 8);
    /// @brief Cylinderモデルを生成する。
    void CreateCylinderModel(const std::string& modelName, float radius = 1.0f, float height = 2.0f, uint32_t segments = 64);
    /// @brief UV球モデルを生成する。
    void CreateUvSphereModel(const std::string& modelName, float radius = 1.0f, uint32_t latitudeSegments = 64,
                             uint32_t longitudeSegments = 128);

    /// @brief モデルを検索する。
    Model* FindModel(const std::string& filePath);
    /// @brief モデルを返す。
    const std::map<std::string, std::unique_ptr<Model>>& GetModels() const
    {
        return models;
    }

private:
    /// @brief インスタンスの初期値と利用先を設定する。
    ModelManager() = default;
    /// @brief この型の終了処理を行う。所有している資源の寿命を終了させる。
    ~ModelManager() = default;
    ModelManager(ModelManager&) = delete;
    ModelManager& operator=(const ModelManager&) = delete;

    // モデルデータ
    std::map<std::string, std::unique_ptr<Model>> models;

    std::unique_ptr<ModelCommon> modelCommon;
};

} // namespace cg2
