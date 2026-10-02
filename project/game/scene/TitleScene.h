#pragma once
#include "game/weapon/CombatTypes.h"
#include "Player.h"
#include "Fade.h"
#include "IScene.h"
#include "game/ui/NeonTextEffect.h"
#include <array>
#include <string_view>

class GameScene;
/// @brief タイトルの操作・背景デモ・ゲーム開始への遷移を管理する。
class TitleScene : public IScene {
public:
    /// @brief インスタンスの初期値と利用先を設定する。
    TitleScene();
    /// @brief この型の終了処理を行う。所有している資源の寿命を終了させる。
    ~TitleScene() override;
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize() override;
    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    void Update() override;
    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    void Draw() override;
    /// @brief 影を描画する。
    void DrawShadow() override;
    /// @brief 後処理演出3Dを描画する。
    void DrawPostEffect3D() override;
    /// @brief 後の後処理演出3Dを描画する。
    void DrawAfterPostEffect3D() override;
    /// @brief スプライトを描画する。
    void DrawSprite() override;
    /// @brief 後処理ガウシアン強度を返す。
    float GetPostGaussianIntensity() const override
    {
        return 0.88f;
    }
    /// @brief 画面演出状態を返す。
    ScreenEffectState GetScreenEffectState() const override;
    /// @brief 終了済みであるか判定する。
    bool IsFinished() const override
    {
        return finished_;
    }
    /// @brief 次のシーン名前を返す。
    std::string GetNextSceneName() const override
    {
        return nextSceneName_;
    }

private:
    /// @brief シーン利用可能であるか判定する。
    bool IsSceneAvailable(std::string_view name) const;
    /// @brief 遷移If利用可能を開始する。
    bool StartTransitionIfAvailable(std::string_view name, float duration);
    /// @brief メニュー表示情報を更新する。
    void UpdateMenuVisuals();
    /// @brief メニュー利用可能であるか判定する。
    bool IsMenuAvailable(int selection) const;
    /// @brief 入力位置がメニュー項目に重なるか判定する。
    int HitTestMenu(const cg2::Vector2& mouse) const;
    /// @brief デモ検証を書き込む。
    void WriteDemoValidation(bool fadeComplete);
    std::unique_ptr<GameScene> demo_;
    std::unique_ptr<Fade> fade_;
    std::unique_ptr<cg2::Sprite> backgroundVeil_;
    cg2::Input* input_ = nullptr;
    std::unique_ptr<cg2::TextLabel> title_, subtitle_, hint_, demoCaption_;
    std::array<std::unique_ptr<cg2::TextLabel>, 1> menu_;
    /// @brief タイトルデモの1時点の状態と表示用の計測値を表す。
    struct DemoSample {
        std::string build;
        int shots = 0, kills = 0, dashes = 0;
        float maxHomingTurnRate = 0;
        uint32_t maxDashExplosionsEmitted = 0;
        uint64_t wallBouncesObserved = 0, actorPiercesObserved = 0, splitChildrenObserved = 0;
    };
    std::array<DemoSample, 4> stageSamples_{};
    std::unique_ptr<NeonTextEffect> titleTextNeonEffect_;
    Phase phase_ = Phase::kFadeIn;
    std::string nextSceneName_;
    bool finished_ = false, demoFrozen_ = false, autoTest_ = false, startupAutoTest_ = false;
    bool submissionCaptured_ = false;
    bool capturedSceneFadeOut_ = false, capturedSceneFadeIn_ = false;
    float maxObservedBurstAge_ = 0;
    uint32_t capturedStages_ = 0;
    float frozenAt_ = 0, blinkTimer_ = 0;
    int menuSelection_ = 0;
    bool menuHovered_ = false;
    cg2::Vector2 previousMousePosition_{};
};
