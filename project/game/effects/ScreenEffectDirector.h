#pragma once

#include <string>

#include "Struct.h"

// Gameplay-facing coordinator for the existing full-screen post effects.
//
// Game code only raises semantic events here. The director owns interpolation,
// priority and clamping, then writes one transient BloomParam for the renderer.
class ScreenEffectDirector {
public:
	struct Config {
		float justDodgeDuration = 0.20f;
		float justDodgeTimeScale = 0.20f;
		float playerDamageDuration = 0.16f;
		float enemyDefeatDuration = 0.24f;
		float bossEntryDuration = 1.10f;
		float bossPhaseDuration = 0.75f;
		float bossDefeatDuration = 1.35f;
		float bossDefeatHitStop = 0.14f;
		float gameOverDuration = 1.10f;
		float dashDuration = 0.14f;
		float upgradeConfirmDuration = 0.24f;

		float lowHpThreshold = 0.30f;
		float lowHpVignette = 0.28f;
		float upgradeGaussian = 0.34f;
		float phaseBoxFilter = 0.18f;
		float grayscaleStrength = 0.95f;
		float playerDamageVignette = 0.48f;
		float radialBlurIntensity = 0.22f;
		float shockwaveMaxRadius = 0.62f;
		float shockwaveWidth = 0.045f;
		float shockwaveStrength = 0.020f;
		float chromaticAberration = 0.030f;
		float randomIntensity = 0.10f;
		float bloomBoost = 0.85f;
		float cameraShakeDuration = 0.16f;
		float cameraShakeStrength = 0.42f;
		float dissolveSpeed = 1.60f;
		float maxBloomBoost = 1.80f;
		float maxChromaticAberration = 0.065f;
		float maxVignette = 0.72f;
		float maxNoise = 0.20f;
		float maxRandom = 0.18f;
		float maxRadialBlur = 0.32f;
		bool outlineEnabled = true;
		float outlineWidth = 1.0f;
		float outlineThreshold = 0.55f;
		float depthOutlineScale = 0.18f;
	};

	bool LoadConfig(const std::string& filePath);
	const Config& GetConfig() const { return config_; }
	void SetOutlineEnabled(bool enabled) { config_.outlineEnabled = enabled; }
	bool IsOutlineEnabled() const { return config_.outlineEnabled; }

	void TriggerJustDodge(const Vector2& screenPosition);
	void TriggerPlayerDamage(const Vector2& hitDirection);
	void SetLowHpRatio(float ratio);
	void TriggerEnemyDefeat(const Vector2& screenPosition, float importance);
	void TriggerBossEntry();
	void TriggerBossPhaseChange();
	void TriggerBossDefeat(const Vector2& screenPosition);
	void SetUpgradeMenuOpen(bool open);
	void TriggerUpgradeConfirmed(const Vector2& screenPosition);
	void TriggerDash(const Vector2& screenPosition);
	void TriggerGameOver();

	void Update(float deltaTime);
	void ApplyTo(BloomParam& param) const;

	float GetTimeScaleMultiplier() const;
	bool IsActive() const;

private:
	struct TimedEffect {
		float remaining = 0.0f;
		float duration = 0.0f;
		Vector2 center{ 0.5f, 0.5f };
		float importance = 1.0f;
	};

	static float GetEnvelope(const TimedEffect& effect);
	static float GetProgress(const TimedEffect& effect);
	static void Tick(TimedEffect& effect, float deltaTime);
	static float Safe(float value, float fallback = 0.0f);
	void SetShockwave(
		BloomParam& param,
		int& currentPriority,
		int priority,
		const TimedEffect& effect,
		float maxRadius,
		float width,
		float strength) const;

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
	Vector2 damageDirection_{};
	float lowHpRatio_ = 1.0f;
	bool upgradeMenuOpen_ = false;
};
