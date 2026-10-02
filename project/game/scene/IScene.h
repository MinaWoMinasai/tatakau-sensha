#pragma once
#include <string>
#include "Struct.h"

/// @brief 初期化・更新・描画と画面効果の共通契約を定義する。
class IScene {
public:
    /// @brief 一時的な画面効果の強度と継続時間を表す。
    struct PostEffectPulse {
        float bloomBoost = 0.0f;
        float chromAbAmount = 0.0f;
        cg2::Vector2 center = {0.5f, 0.5f};
        float radius = 0.0f;
        float width = 0.05f;
        float strength = 0.0f;
    };
    /// @brief 現在の画面効果の合成結果を描画側へ渡す。
    struct ScreenEffectState {
        bool active = false;
        float bloomScale = 1.0f;
        bool suppressPostEffectDebugUi = false;
        // Suppress generated screen-space edges, preserving authored neon geometry.
        bool suppressOutlines = false;
        cg2::BloomParam param{};
    };
    /// @brief 水面に関連する画面効果の設定を表す。
    struct WaterPostProcessSettings {
        bool diagnosticsEnabled = false;
        bool taaEnabled = true;
        bool bloomEnabled = true;
        float historyWeight = 0.08f;
        int debugMode = 0;
    };
    /// @brief シーン描画の区間ごとの所要時間を記録する。
    struct RenderProfile {
        float frameTotalMs = 0.0f;
        float messagePumpMs = 0.0f;
        float inputImGuiBeginMs = 0.0f;
        float engineUpdateMs = 0.0f;
        float sceneUpdateMs = 0.0f;
        float imguiBuildMs = 0.0f;
        float drawSetupMs = 0.0f;
        float drawRecordMs = 0.0f;
        float scenePostMs = 0.0f;
        float globalBloomMs = 0.0f;
        float afterPostMs = 0.0f;
        float spriteMs = 0.0f;
        float imguiDrawMs = 0.0f;
        float postDrawMs = 0.0f;
        float submitCloseMs = 0.0f;
        float submitExecuteMs = 0.0f;
        float presentMs = 0.0f;
        float fenceWaitMs = 0.0f;
        float fpsLimitMs = 0.0f;
        float submitResetMs = 0.0f;
    };

    /// @brief この型の終了処理を行う。所有している資源の寿命を終了させる。
    virtual ~IScene() = default;
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    virtual void Initialize() = 0;
    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    virtual void Update() = 0;
    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    virtual void Draw() = 0;
    /// @brief 影を描画する。
    virtual void DrawShadow() {}
    /// @brief 後処理演出3Dを描画する。
    virtual void DrawPostEffect3D() {}
    /// @brief 後の後処理演出3Dを描画する。
    virtual void DrawAfterPostEffect3D() {}
    /// @brief スプライトを描画する。
    virtual void DrawSprite() = 0;
    /// @brief 最終差分時間を返す。
    virtual float GetFinalDeltaTime() const
    {
        return 1.0f / 60.0f;
    } // デフォルトは60FPS
    virtual float GetPostGaussianIntensity() const
    {
        return 0.0f;
    }
    /// @brief 後処理演出パルスを返す。
    virtual PostEffectPulse GetPostEffectPulse() const
    {
        return {};
    }
    /// @brief 画面演出状態を返す。
    virtual ScreenEffectState GetScreenEffectState() const
    {
        return {};
    }
    /// @brief 水面後処理Process設定を返す。
    virtual WaterPostProcessSettings GetWaterPostProcessSettings() const
    {
        return {};
    }
    /// @brief 描画区間の計測値を設定する。
    virtual void SetRenderProfile(const RenderProfile& profile)
    {
        (void)profile;
    }

    // シーン終了判定（SceneManagerがチェックする）
    virtual bool IsFinished() const = 0;

    /// @brief 次のシーン名前を返す。
    virtual std::string GetNextSceneName() const
    {
        return "";
    }
};
