#pragma once
#include <memory>
#include <string_view>
#include <vector>
#include "AbstractSceneFactory.h"

/// @brief 現在と次のシーンを所有し、切り替えと更新・描画を管理する。
class SceneManager {
public:
    // シングルトン
    static SceneManager* GetInstance();

    /// @brief シーンファクトリーを設定する。
    bool SetSceneFactory(std::unique_ptr<AbstractSceneFactory> sceneFactory);
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    bool Initialize(const std::string& firstSceneName);
    /// @brief 利用終了時の資源と状態を解放する。
    void Finalize();
    /// @brief 指定したシーン名が登録されているか判定する。
    bool ContainsScene(std::string_view sceneName) const;
    /// @brief 登録済みシーン名前を返す。
    std::vector<std::string> GetRegisteredSceneNames() const;
    /// @brief 現在シーン名前を返す。
    const std::string& GetCurrentSceneName() const
    {
        return currentSceneName_;
    }
    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    void Update();
    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    void Draw();
    /// @brief 後処理演出3Dを描画する。
    void DrawPostEffect3D();
    /// @brief 後の後処理演出3Dを描画する。
    void DrawAfterPostEffect3D();
    /// @brief 影を描画する。
    void DrawShadow();
    /// @brief スプライトを描画する。
    void DrawSprite();

    /// @brief 最終差分時間を返す。
    float GetFinalDeltaTime();
    /// @brief 後処理ガウシアン強度を返す。
    float GetPostGaussianIntensity();
    /// @brief 後処理演出パルスを返す。
    IScene::PostEffectPulse GetPostEffectPulse();
    /// @brief 画面演出状態を返す。
    IScene::ScreenEffectState GetScreenEffectState();
    IScene::DeveloperShowcaseState GetDeveloperShowcaseState();
    void RecordDeveloperFrame(cg2::DirectXCommon& dx);
    /// @brief 水面後処理Process設定を返す。
    IScene::WaterPostProcessSettings GetWaterPostProcessSettings();
    /// @brief 描画区間の計測値を設定する。
    void SetRenderProfile(const IScene::RenderProfile& profile);

private:
    // 現在のシーンを抽象的な型で保持
    std::unique_ptr<IScene> currentScene_ = nullptr;
    std::unique_ptr<AbstractSceneFactory> sceneFactory_ = nullptr;

    // 現在のシーン名を保持（必要な場合のみ）
    std::string currentSceneName_;
    std::string failedTransitionFromSceneName_;
    std::string failedTransitionToSceneName_;
};
