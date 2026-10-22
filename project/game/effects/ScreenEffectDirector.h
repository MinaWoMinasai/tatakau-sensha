#pragma once

#include <string>

#include "Struct.h"

// Gameplay-facing coordinator for the existing full-screen post effects.
//
// Game code only raises semantic events here. The director owns interpolation,
// priority and clamping, then writes one transient BloomParam for the renderer.
/// @brief 複数の一時的な画面演出を時間に応じて合成する。
class ScreenEffectDirector {
public:
    /// @brief ScreenEffectDirectorで使う設定値をまとめる。生成・更新される実行状態とは分けて扱う。
    struct Config {
        float justDodgeDuration = 0.20f;
        float justDodgeTimeScale = 0.20f;
        float playerDamageDuration = 0.16f;
        float enemyDefeatDuration = 0.24f;
        float bossEntryDuration = 1.10f;
        float bossPhaseDuration = 0.90f;
        float bossDefeatDuration = 1.35f;
        float bossDefeatHitStop = 0.14f;
        float bossDefeatImpactDelay = 0.30f;
        float gameOverDuration = 1.10f;
        float dashDuration = 0.18f;
        float upgradeConfirmDuration = 0.24f;

        float lowHpThreshold = 0.30f;
        float lowHpVignette = 0.28f;
        float upgradeGaussian = 0.34f;
        float phaseBoxFilter = 0.20f;
        float phaseNoiseIntensity = 0.15f;
        float phaseRandomIntensity = 0.15f;
        float phaseScanlineIntensity = 0.19f;
        float phaseGlitchAmount = 0.048f;
        float grayscaleStrength = 0.95f;
        float playerDamageVignette = 0.48f;
        float radialBlurIntensity = 0.22f;
        float dashRadialBlurIntensity = 0.12f;
        float dashRadialBlurWidth = 0.012f;
        float shockwaveMaxRadius = 0.62f;
        float shockwaveWidth = 0.045f;
        float shockwaveStrength = 0.020f;
        float chromaticAberration = 0.030f;
        float randomIntensity = 0.10f;
        float bloomBoost = 0.85f;
        float cameraShakeDuration = 0.16f;
        float cameraShakeStrength = 0.42f;
        float dissolveSpeed = 1.28f;
        float maxBloomBoost = 1.80f;
        float maxChromaticAberration = 0.065f;
        float maxVignette = 0.72f;
        float maxNoise = 0.20f;
        float maxRandom = 0.18f;
        float maxRadialBlur = 0.32f;
        bool outlineEnabled = true;
        float outlineWidth = 1.0f;
        float outlineThreshold = 0.55f;
        float depthOutlineScale = 0.216f;
    };

    /// @brief 設定を読み込む。
    bool LoadConfig(const std::string& filePath);

    /// @brief 設定を返す。
    const Config& GetConfig() const
    {
        return config_;
    }

    /// @brief 輪郭有効を設定する。
    void SetOutlineEnabled(bool enabled)
    {
        config_.outlineEnabled = enabled;
    }

    /// @brief 輪郭有効であるか判定する。
    bool IsOutlineEnabled() const
    {
        return config_.outlineEnabled;
    }

    /// @brief JustDodgeを発動させる。
    void TriggerJustDodge(const myEngine::Vector2& screenPosition);
    /// @brief 自機ダメージを発動させる。
    void TriggerPlayerDamage(const myEngine::Vector2& hitDirection);
    /// @brief 低下したHP比率を設定する。
    void SetLowHpRatio(float ratio);
    /// @brief 敵撃破を発動させる。
    void TriggerEnemyDefeat(const myEngine::Vector2& screenPosition, float importance);
    /// @brief ボス登場の演出を始める。
    void TriggerBossEntry();
    /// @brief ボスの段階が変わったときの演出を始める。
    void TriggerBossPhaseChange();
    /// @brief ボス撃破を発動させる。
    void TriggerBossDefeat(const myEngine::Vector2& screenPosition);
    /// @brief 強化メニューが開いているかを設定する。
    void SetUpgradeMenuOpen(bool open);
    /// @brief 強化を決定したときの演出を始める。
    void TriggerUpgradeConfirmed(const myEngine::Vector2& screenPosition);
    /// @brief ダッシュを発動させる。
    void TriggerDash(const myEngine::Vector2& screenPosition);
    /// @brief ゲームオーバーの演出を始める。
    void TriggerGameOver();

    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Update(float deltaTime);
    /// @brief へのを現在の状態へ適用する。
    void ApplyTo(myEngine::BloomParam& param) const;

    /// @brief 演出による時間の倍率を返す。
    float GetTimeScaleMultiplier() const;
    /// @brief 有効であるか判定する。
    bool IsActive() const;

private:
    /// @brief 開始済みの画面効果と残り時間を保持する。
    struct TimedEffect {
        float remaining = 0.0f;
        float duration = 0.0f;
        myEngine::Vector2 center{0.5f, 0.5f};
        float importance = 1.0f;
    };

    /// @brief Envelopeを返す。
    static float GetEnvelope(const TimedEffect& effect);
    /// @brief 進行度を返す。
    static float GetProgress(const TimedEffect& effect);
    /// @brief 演出の残り時間と進行状態を1回更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    static void Tick(TimedEffect& effect, float deltaTime);
    /// @brief 計算に使う値の無効な状態を補正する。
    static float Safe(float value, float fallback = 0.0f);
    /// @brief Shockwaveを設定する。
    void SetShockwave(myEngine::BloomParam& param, int& currentPriority, int priority, const TimedEffect& effect, float maxRadius,
                      float width, float strength) const;

    Config config_{};
    TimedEffect justDodge_{};
    TimedEffect playerDamage_{};
    TimedEffect enemyDefeat_{};
    TimedEffect bossEntry_{};
    TimedEffect bossPhase_{};
    TimedEffect bossDefeat_{};
    TimedEffect gameOver_{};
    TimedEffect dash_{};
    TimedEffect upgradeConfirm_{};
    myEngine::Vector2 damageDirection_{};
    float lowHpRatio_ = 1.0f;
    bool upgradeMenuOpen_ = false;
};
