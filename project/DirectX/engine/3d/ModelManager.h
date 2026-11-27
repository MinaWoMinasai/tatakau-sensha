#pragma once
#include <map>
#include <set>
#include <string>
#include <memory>
#include <future>
#include <Model.h>

namespace myEngine {

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
    /// @brief OBJモデルの読み込みをバックグラウンドで開始する。
    void PreloadObjModel(const std::string& filePath);
    /// @brief 読み込み済みOBJのマテリアルテクスチャも裏で先読みさせる。メインスレッドで呼ぶ。
    void PumpPreloads();
    /// @brief 要求したOBJモデルの先読みがすべて完了したかを返す。
    bool ArePreloadsReady() const;
    /// @brief 手続き生成した静的メッシュを名前で共有する。既存の名前は再生成しない。
    /// 形状は Model::CreatePrimitive などで作って渡す。
    void CreateModelFromData(const std::string& modelName, const ModelData& data);
    /// @brief 基本の形のプリセット（JSON の presets：name・shape・size・radius・segments・rings）を読み、名前で登録する。
    /// @param filePath 素材の場所（ResourcePaths::GameRoot）からの場所。ファイルが無ければ何もしない。
    /// @return 登録した数。読めない項目は飛ばし、理由を error に足す。
    int LoadPrimitivePresets(const std::string& filePath, std::string& error);

    /// @brief モデルを検索する。
    Model* FindModel(const std::string& filePath);

private:
    ModelManager() = default;
    ~ModelManager() = default;
    ModelManager(ModelManager&) = delete;
    ModelManager& operator=(const ModelManager&) = delete;

    // モデルデータ
    std::map<std::string, std::unique_ptr<Model>> models;

    DirectXCommon* dxCommon_ = nullptr;
    std::map<std::string, std::shared_future<ModelData>> pendingModels_;
    std::set<std::string> texturePreloadsQueued_;
};

} // namespace myEngine
