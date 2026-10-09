#pragma once
#include "IScene.h"
#include <memory>
#include <nlohmann/json.hpp>
#include "game/session/TitleDemoStatus.h"
namespace cg2 {
class Object3d;
}
namespace gameplay {
class GameSession;
}

/// @brief シーン管理の呼び出しをゲームセッションへ渡す。
class GameScene : public IScene {
public:
    using TitleDemoStatus = gameplay::TitleDemoStatus;
    /// @brief プレイするモードを設定する。
    explicit GameScene(bool prototypeRun = false, bool expeditionRun = false);
    /// @brief ゲームセッションを終了する。
    ~GameScene() override;
    /// @brief 初期化
    void Initialize() override;

    /// @brief シーンの状態に応じて戦闘・演出・進行を更新する。戦闘の呼び出し順は実装内に記載する。
    void Update() override;

    /// @brief 描画
    void Draw() override;

    /// @brief 影を描画する。
    void DrawShadow() override;

    /// @brief 後処理演出3Dを描画する。
    void DrawPostEffect3D() override;

    /// @brief 後の後処理演出3Dを描画する。
    void DrawAfterPostEffect3D() override;

    /// @brief 描画
    void DrawSprite() override;

    /// @brief 終了済みであるか判定する。
    bool IsFinished() const override;

    /// @brief BallOBJ形式を返す。
    cg2::Object3d* GetBallObj();

    /// @brief 最終差分時間を返す。
    float GetFinalDeltaTime() const override;

    /// @brief 後処理ガウシアン強度を返す。
    float GetPostGaussianIntensity() const override;

    /// @brief 後処理演出パルスを返す。
    PostEffectPulse GetPostEffectPulse() const override;

    /// @brief 画面演出状態を返す。
    ScreenEffectState GetScreenEffectState() const override;

    bool IsNeonShowcaseActive() const;

    DeveloperShowcaseState GetDeveloperShowcaseState() override;

    void RecordDeveloperPostParameters(const cg2::BloomParam& param) override;

    void RecordDeveloperFrame(cg2::DirectXCommon& dx) override;

    /// @brief 描画区間の計測値を設定する。
    void SetRenderProfile(const IScene::RenderProfile& profile) override;

    /// @brief タイトルデモを有効にする。
    void EnableTitleDemo();

    /// @brief タイトルデモであるか判定する。
    bool IsTitleDemo() const;

    /// @brief タイトルデモ状態を返す。
    const TitleDemoStatus& GetTitleDemoStatus() const;

    /// @brief タイトルデモFadeを返す。
    float GetTitleDemoFade() const;

    /// @brief タイトルデモ計測を要求を予約する。
    void RequestTitleDemoCapture(const std::string& name);

    /// @brief タイトルデモ計測をコピーする。
    void CopyTitleDemoCapture();

    /// @brief タイトルデモ計測を予約分を処理する。
    void FlushTitleDemoCapture();

    /// @brief タイトルデモ強化ビルドを返す。
    nlohmann::json GetTitleDemoBuild() const;

    /// @brief 次のシーン名前を返す。
    std::string GetNextSceneName() const override;

private:
    std::unique_ptr<gameplay::GameSession> session_;
};
