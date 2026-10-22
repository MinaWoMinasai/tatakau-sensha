#pragma once
#include "IScene.h"
#include <memory>
#include <nlohmann/json.hpp>
#include "game/session/TitleDemoStatus.h"

namespace gameplay {
using PostEffectPulse = IScene::PostEffectPulse;
using ScreenEffectState = IScene::ScreenEffectState;
using DeveloperShowcaseState = IScene::DeveloperShowcaseState;

struct GameWorld;

/// @brief 各担当の寿命をまとめ、シーンにゲームの公開窓口を提供する。
class GameSession {
public:
    /// @brief 1回のプレイの状態と担当クラスを生成する。
    GameSession(bool prototypeRun, bool expeditionRun);
    /// @brief セッションが所有するワールドを破棄する。
    ~GameSession();
    /// @brief 初期化
    void Initialize();
    /// @brief 初期化を1ステップ進める。完了したらtrueを返す。
    bool InitializeStep();

    /// @brief シーンの状態に応じて戦闘・演出・進行を更新する。戦闘の呼び出し順は実装内に記載する。
    void Update();

    /// @brief 描画
    void Draw();

    /// @brief 影を描画する。
    void DrawShadow();

    /// @brief 後処理演出3Dを描画する。
    void DrawPostEffect3D();

    /// @brief 後の後処理演出3Dを描画する。
    void DrawAfterPostEffect3D();

    /// @brief 描画
    void DrawSprite();

    /// @brief 終了済みであるか判定する。
    bool IsFinished() const;

    /// @brief 最終差分時間を返す。
    float GetFinalDeltaTime() const;

    /// @brief 後処理ガウシアン強度を返す。
    float GetPostGaussianIntensity() const;

    /// @brief 後処理演出パルスを返す。
    PostEffectPulse GetPostEffectPulse() const;

    /// @brief 画面演出状態を返す。
    ScreenEffectState GetScreenEffectState() const;
    /// @brief ブルームの比較表示（[B]で切替）の設定を返す。
    IScene::BloomViewSettings GetBloomViewSettings() const;

    bool IsNeonShowcaseActive() const;

    DeveloperShowcaseState GetDeveloperShowcaseState();

    /// @brief 開発者用にポストエフェクトのパラメータを記録する。
    void RecordDeveloperPostParameters(const myEngine::BloomParam& param);

    /// @brief 開発者用にフレームを記録する。
    void RecordDeveloperFrame(myEngine::DirectXCommon& dx);

    /// @brief 描画区間の計測値を設定する。
    void SetRenderProfile(const IScene::RenderProfile& profile);

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
    std::string GetNextSceneName() const;

private:
    std::unique_ptr<GameWorld> world_;
};
} // namespace gameplay
