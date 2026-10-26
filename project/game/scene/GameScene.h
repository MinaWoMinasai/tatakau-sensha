#pragma once
#include "IScene.h"
#include <memory>
#include <nlohmann/json.hpp>
#include "game/session/TitleDemoStatus.h"

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
    void Initialize() override;
    /// @brief 初期化を1ステップ進める。完了したらtrueを返す。
    bool InitializeStep() override;

    /// @brief シーンの状態に応じて戦闘・演出・進行を更新する。戦闘の呼び出し順は実装内に記載する。
    void Update() override;

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

    /// @brief 最終差分時間を返す。
    float GetFinalDeltaTime() const override;

    /// @brief 後処理ガウシアン強度を返す。
    float GetPostGaussianIntensity() const override;

    /// @brief 後処理演出パルスを返す。
    PostEffectPulse GetPostEffectPulse() const override;

    /// @brief 画面演出状態を返す。
    ScreenEffectState GetScreenEffectState() const override;
    /// @brief ベクター管モニター風のアリーナ表示設定を返す。
    ArenaLookSettings GetArenaLookSettings() const override;
    /// @brief ブルームの比較表示の設定を返す。
    BloomViewSettings GetBloomViewSettings() const override;

    bool IsNeonShowcaseActive() const;

    DeveloperShowcaseState GetDeveloperShowcaseState() override;

    /// @brief 開発者用にポストエフェクトのパラメータを記録する。
    void RecordDeveloperPostParameters(const myEngine::BloomParam& param) override;

    /// @brief 開発者用にフレームを記録する。
    void RecordDeveloperFrame(myEngine::DirectXCommon& dx) override;

    /// @brief 描画区間の計測値を設定する。
    void SetRenderProfile(const IScene::RenderProfile& profile) override;

    /// @brief タイトルデモを有効にする。
    void EnableTitleDemo();

    /// @brief タイトルデモであるか判定する。
    bool IsTitleDemo() const;

    /// @brief タイトルデモ状態を返す。
    const TitleDemoStatus& GetTitleDemoStatus() const;

    /// @brief タイトルデモのフェードの量を返す。
    float GetTitleDemoFade() const;

    /// @brief タイトルデモの画面を、指定した名前でPNGに保存するよう予約する。
    void RequestTitleDemoCapture(const std::string& name);

    /// @brief タイトルデモの画面を、保存用にコピーする。
    void CopyTitleDemoCapture();

    /// @brief 予約していたタイトルデモの画面を、PNGに書き出す。
    void FlushTitleDemoCapture();

    /// @brief タイトルデモ強化ビルドを返す。
    nlohmann::json GetTitleDemoBuild() const;

    /// @brief 次のシーン名前を返す。
    std::string GetNextSceneName() const override;

private:
    std::unique_ptr<gameplay::GameSession> session_;
};
