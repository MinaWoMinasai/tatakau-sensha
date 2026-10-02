#include "game/effects/ScreenEffectDirector.h"

#include <algorithm>
#include <cmath>
#include <fstream>

#include <nlohmann/json.hpp>

namespace {

/// @brief 浮動小数を読み取る。
float ReadFloat(
	const nlohmann::json& object,
	const char* key,
	float fallback,
	float minimum,
	float maximum)
{
	if (!object.is_object() || !object.contains(key) || !object[key].is_number()) {
		return fallback;
	}
	const float value = object[key].get<float>();
	if (!std::isfinite(value)) {
		return fallback;
	}
	return (std::clamp)(value, minimum, maximum);
}

} // namespace

bool ScreenEffectDirector::LoadConfig(const std::string& filePath)
{
	std::ifstream file(filePath);
	if (!file.is_open()) {
		return false;
	}

	try {
		nlohmann::json root;
		file >> root;
		const nlohmann::json& timing = root.value("timing", nlohmann::json::object());
		const nlohmann::json& intensity = root.value("intensity", nlohmann::json::object());

		config_.justDodgeDuration = ReadFloat(timing, "justDodge", config_.justDodgeDuration, 0.05f, 1.0f);
		config_.justDodgeTimeScale = ReadFloat(timing, "justDodgeTimeScale", config_.justDodgeTimeScale, 0.05f, 1.0f);
		config_.playerDamageDuration = ReadFloat(timing, "playerDamage", config_.playerDamageDuration, 0.05f, 1.0f);
		config_.enemyDefeatDuration = ReadFloat(timing, "enemyDefeat", config_.enemyDefeatDuration, 0.05f, 1.0f);
		config_.bossEntryDuration = ReadFloat(timing, "bossEntry", config_.bossEntryDuration, 0.10f, 4.0f);
		config_.bossPhaseDuration = ReadFloat(timing, "bossPhase", config_.bossPhaseDuration, 0.10f, 3.0f);
		config_.bossDefeatDuration = ReadFloat(timing, "bossDefeat", config_.bossDefeatDuration, 0.20f, 5.0f);
		config_.bossDefeatHitStop = ReadFloat(timing, "bossDefeatHitStop", config_.bossDefeatHitStop, 0.0f, 0.50f);
		config_.bossDefeatImpactDelay = ReadFloat(timing, "bossDefeatImpactDelay", config_.bossDefeatImpactDelay, 0.0f, 1.0f);
		config_.gameOverDuration = ReadFloat(timing, "gameOver", config_.gameOverDuration, 0.20f, 4.0f);
		config_.dashDuration = ReadFloat(timing, "dash", config_.dashDuration, 0.05f, 0.50f);
		config_.upgradeConfirmDuration = ReadFloat(timing, "upgradeConfirm", config_.upgradeConfirmDuration, 0.05f, 1.0f);

		config_.lowHpThreshold = ReadFloat(intensity, "lowHpThreshold", config_.lowHpThreshold, 0.05f, 0.80f);
		config_.lowHpVignette = ReadFloat(intensity, "lowHpVignette", config_.lowHpVignette, 0.0f, 1.0f);
		config_.upgradeGaussian = ReadFloat(intensity, "upgradeGaussian", config_.upgradeGaussian, 0.0f, 1.0f);
		config_.phaseBoxFilter = ReadFloat(intensity, "phaseBoxFilter", config_.phaseBoxFilter, 0.0f, 0.65f);
		config_.phaseNoiseIntensity = ReadFloat(intensity, "phaseNoiseIntensity", config_.phaseNoiseIntensity, 0.0f, 1.0f);
		config_.phaseRandomIntensity = ReadFloat(intensity, "phaseRandomIntensity", config_.phaseRandomIntensity, 0.0f, 1.0f);
		config_.phaseScanlineIntensity = ReadFloat(intensity, "phaseScanlineIntensity", config_.phaseScanlineIntensity, 0.0f, 1.0f);
		config_.phaseGlitchAmount = ReadFloat(intensity, "phaseGlitchAmount", config_.phaseGlitchAmount, 0.0f, 0.08f);
		config_.grayscaleStrength = ReadFloat(intensity, "grayscaleStrength", config_.grayscaleStrength, 0.0f, 1.0f);
		config_.playerDamageVignette = ReadFloat(intensity, "playerDamageVignette", config_.playerDamageVignette, 0.0f, 1.0f);
		config_.radialBlurIntensity = ReadFloat(intensity, "radialBlurIntensity", config_.radialBlurIntensity, 0.0f, 1.0f);
		config_.dashRadialBlurIntensity = ReadFloat(intensity, "dashRadialBlurIntensity", config_.dashRadialBlurIntensity, 0.0f, 1.0f);
		config_.dashRadialBlurWidth = ReadFloat(intensity, "dashRadialBlurWidth", config_.dashRadialBlurWidth, 0.0f, 0.10f);
		config_.shockwaveMaxRadius = ReadFloat(intensity, "shockwaveMaxRadius", config_.shockwaveMaxRadius, 0.0f, 1.5f);
		config_.shockwaveWidth = ReadFloat(intensity, "shockwaveWidth", config_.shockwaveWidth, 0.001f, 0.25f);
		config_.shockwaveStrength = ReadFloat(intensity, "shockwaveStrength", config_.shockwaveStrength, 0.0f, 0.10f);
		config_.chromaticAberration = ReadFloat(intensity, "chromaticAberration", config_.chromaticAberration, 0.0f, 0.20f);
		config_.randomIntensity = ReadFloat(intensity, "randomIntensity", config_.randomIntensity, 0.0f, 1.0f);
		config_.bloomBoost = ReadFloat(intensity, "bloomBoost", config_.bloomBoost, 0.0f, 4.0f);
		config_.cameraShakeDuration = ReadFloat(intensity, "cameraShakeDuration", config_.cameraShakeDuration, 0.0f, 2.0f);
		config_.cameraShakeStrength = ReadFloat(intensity, "cameraShakeStrength", config_.cameraShakeStrength, 0.0f, 4.0f);
		config_.dissolveSpeed = ReadFloat(intensity, "dissolveSpeed", config_.dissolveSpeed, 0.0f, 8.0f);
		config_.maxBloomBoost = ReadFloat(intensity, "maxBloomBoost", config_.maxBloomBoost, 0.0f, 4.0f);
		config_.maxChromaticAberration = ReadFloat(intensity, "maxChromaticAberration", config_.maxChromaticAberration, 0.0f, 0.20f);
		config_.maxVignette = ReadFloat(intensity, "maxVignette", config_.maxVignette, 0.0f, 1.0f);
		config_.maxNoise = ReadFloat(intensity, "maxNoise", config_.maxNoise, 0.0f, 1.0f);
		config_.maxRandom = ReadFloat(intensity, "maxRandom", config_.maxRandom, 0.0f, 1.0f);
		config_.maxRadialBlur = ReadFloat(intensity, "maxRadialBlur", config_.maxRadialBlur, 0.0f, 1.0f);
		config_.outlineEnabled = intensity.value("outlineEnabled", config_.outlineEnabled);
		config_.outlineWidth = ReadFloat(intensity, "outlineWidth", config_.outlineWidth, 0.0f, 4.0f);
		config_.outlineThreshold = ReadFloat(intensity, "outlineThreshold", config_.outlineThreshold, 0.0f, 10.0f);
		config_.depthOutlineScale = ReadFloat(intensity, "depthOutlineScale", config_.depthOutlineScale, 0.0f, 4.0f);
	} catch (const nlohmann::json::exception&) {
		return false;
	}
	return true;
}

void ScreenEffectDirector::TriggerJustDodge(const cg2::Vector2& screenPosition)
{
	justDodge_ = { config_.justDodgeDuration, config_.justDodgeDuration, screenPosition, 1.0f };
}

void ScreenEffectDirector::TriggerPlayerDamage(const cg2::Vector2& hitDirection)
{
	playerDamage_ = { config_.playerDamageDuration, config_.playerDamageDuration, { 0.5f, 0.5f }, 1.0f };
	damageDirection_ = hitDirection;
}

void ScreenEffectDirector::SetLowHpRatio(float ratio)
{
	lowHpRatio_ = (std::clamp)(Safe(ratio, 1.0f), 0.0f, 1.0f);
}

void ScreenEffectDirector::TriggerEnemyDefeat(const cg2::Vector2& screenPosition, float importance)
{
	enemyDefeat_ = {
		config_.enemyDefeatDuration,
		config_.enemyDefeatDuration,
		screenPosition,
		(std::clamp)(Safe(importance, 1.0f), 0.25f, 1.5f)
	};
}

void ScreenEffectDirector::TriggerBossEntry()
{
	bossEntry_ = { config_.bossEntryDuration, config_.bossEntryDuration, { 0.5f, 0.42f }, 1.0f };
}

void ScreenEffectDirector::TriggerBossPhaseChange()
{
	bossPhase_ = { config_.bossPhaseDuration, config_.bossPhaseDuration, { 0.5f, 0.5f }, 1.0f };
}

void ScreenEffectDirector::TriggerBossDefeat(const cg2::Vector2& screenPosition)
{
	bossDefeat_ = { config_.bossDefeatDuration, config_.bossDefeatDuration, screenPosition, 1.0f };
}

void ScreenEffectDirector::SetUpgradeMenuOpen(bool open)
{
	upgradeMenuOpen_ = open;
}

void ScreenEffectDirector::TriggerUpgradeConfirmed(const cg2::Vector2& screenPosition)
{
	upgradeConfirm_ = { config_.upgradeConfirmDuration, config_.upgradeConfirmDuration, screenPosition, 1.0f };
}

void ScreenEffectDirector::TriggerDash(const cg2::Vector2& screenPosition)
{
	dash_ = { config_.dashDuration, config_.dashDuration, screenPosition, 1.0f };
}

void ScreenEffectDirector::TriggerGameOver()
{
	gameOver_ = { config_.gameOverDuration, config_.gameOverDuration, { 0.5f, 0.5f }, 1.0f };
}

void ScreenEffectDirector::Update(float deltaTime)
{
	const float safeDeltaTime = (std::clamp)(Safe(deltaTime), 0.0f, 0.10f);
	Tick(justDodge_, safeDeltaTime);
	Tick(playerDamage_, safeDeltaTime);
	Tick(enemyDefeat_, safeDeltaTime);
	Tick(bossEntry_, safeDeltaTime);
	Tick(bossPhase_, safeDeltaTime);
	Tick(bossDefeat_, safeDeltaTime);
	Tick(gameOver_, safeDeltaTime);
	Tick(dash_, safeDeltaTime);
	Tick(upgradeConfirm_, safeDeltaTime);
}

void ScreenEffectDirector::ApplyTo(cg2::BloomParam& param) const
{
	int shockwavePriority = -1;
	if (config_.outlineEnabled) {
		param.outlineWidth = config_.outlineWidth;
		param.outlineThreshold = config_.outlineThreshold;
		param.outlineColor = { 0.18f, 0.82f, 1.0f };
		param.depthOutlineEnabled = 1.0f;
		param.depthOutlineScale = config_.depthOutlineScale;
	}

	if (upgradeMenuOpen_) {
		param.isGrayscale = (std::max)(param.isGrayscale, config_.grayscaleStrength);
		param.gaussianIntensity = (std::max)(param.gaussianIntensity, config_.upgradeGaussian);
		param.vignetteIntensity = (std::max)(param.vignetteIntensity, 0.16f);
		param.vignetteScale = (std::max)(param.vignetteScale, 1.05f);
	}

	if (lowHpRatio_ > 0.0f && lowHpRatio_ < config_.lowHpThreshold) {
		const float danger = 1.0f - lowHpRatio_ / config_.lowHpThreshold;
		const float pulse = 0.82f + std::sin(lowHpRatio_ * 37.0f) * 0.06f;
		param.vignetteIntensity = (std::max)(
			param.vignetteIntensity,
			config_.lowHpVignette * danger * pulse);
		param.vignetteScale = (std::max)(param.vignetteScale, 1.08f);
	}

	if (justDodge_.remaining > 0.0f) {
		const float envelope = GetEnvelope(justDodge_);
		param.isGrayscale = (std::max)(param.isGrayscale, config_.grayscaleStrength);
		param.intensity += config_.bloomBoost * envelope;
		param.chromAbAmount += config_.chromaticAberration * envelope;
		param.radialBlurCenter = justDodge_.center;
		param.radialBlurWidth = (std::max)(param.radialBlurWidth, 0.018f);
		param.radialBlurIntensity = (std::max)(param.radialBlurIntensity, config_.radialBlurIntensity * envelope);
		SetShockwave(
			param,
			shockwavePriority,
			80,
			justDodge_,
			config_.shockwaveMaxRadius,
			config_.shockwaveWidth,
			config_.shockwaveStrength);
	}

	if (playerDamage_.remaining > 0.0f) {
		const float envelope = GetEnvelope(playerDamage_);
		const float directionalBias = (std::clamp)(std::abs(damageDirection_.x) + std::abs(damageDirection_.y), 0.0f, 1.0f);
		param.vignetteIntensity = (std::max)(
			param.vignetteIntensity,
			(config_.playerDamageVignette + directionalBias * 0.06f) * envelope);
		param.vignetteScale = (std::max)(param.vignetteScale, 1.15f);
		param.chromAbAmount += config_.chromaticAberration * 0.86f * envelope;
		param.noiseIntensity = (std::max)(param.noiseIntensity, config_.randomIntensity * envelope);
		param.randomIntensity = (std::max)(param.randomIntensity, config_.randomIntensity * 0.80f * envelope);
		param.randomScale = 150.0f;
		param.randomTimeScale = 13.0f;
	}

	if (enemyDefeat_.remaining > 0.0f) {
		const float envelope = GetEnvelope(enemyDefeat_) * enemyDefeat_.importance;
		param.intensity += 0.24f * envelope;
		param.chromAbAmount += 0.006f * envelope;
		SetShockwave(param, shockwavePriority, 30, enemyDefeat_, 0.28f, 0.028f, 0.006f);
	}

	if (bossEntry_.remaining > 0.0f) {
		const float envelope = GetEnvelope(bossEntry_);
		param.vignetteIntensity = (std::max)(param.vignetteIntensity, 0.24f * envelope);
		param.vignetteScale = (std::max)(param.vignetteScale, 1.05f);
		param.intensity += 0.35f * envelope;
		param.exposure += 0.06f * envelope;
	}

	if (bossPhase_.remaining > 0.0f) {
		const float progress = GetProgress(bossPhase_);
		const float impactEnvelope = GetEnvelope(bossPhase_);
		const float interferenceAttack = (std::clamp)((progress - 0.06f) / 0.12f, 0.0f, 1.0f);
		const float interferenceRelease = (std::clamp)((1.0f - progress) / 0.30f, 0.0f, 1.0f);
		const float interferenceEnvelope = interferenceAttack * interferenceRelease;
		const float scanlineAttack = (std::clamp)((progress - 0.16f) / 0.12f, 0.0f, 1.0f);
		const float scanlineEnvelope = scanlineAttack * interferenceRelease;
		param.glitchAmount = (std::max)(param.glitchAmount, config_.phaseGlitchAmount * interferenceEnvelope);
		param.noiseIntensity = (std::max)(param.noiseIntensity, config_.phaseNoiseIntensity * interferenceEnvelope);
		param.randomIntensity = (std::max)(param.randomIntensity, config_.phaseRandomIntensity * interferenceEnvelope);
		param.randomScale = 180.0f;
		param.randomTimeScale = 18.0f;
		param.scanlineIntensity = (std::max)(param.scanlineIntensity, config_.phaseScanlineIntensity * scanlineEnvelope);
		param.scanlineFrequency = 90.0f;
		param.chromAbAmount += 0.034f * impactEnvelope;
		param.fullScreenBoxBlurBlend = (std::max)(param.fullScreenBoxBlurBlend, config_.phaseBoxFilter * interferenceEnvelope);
		param.boxBlurRadius = (std::max)(param.boxBlurRadius, 2.0f);
	}

	if (bossDefeat_.remaining > 0.0f) {
		const float envelope = GetEnvelope(bossDefeat_);
		param.intensity += 1.55f * envelope;
		param.chromAbAmount += 0.055f * envelope;
		param.radialBlurCenter = bossDefeat_.center;
		param.radialBlurWidth = (std::max)(param.radialBlurWidth, 0.026f);
		param.radialBlurIntensity = (std::max)(param.radialBlurIntensity, 0.30f * envelope);
		param.vignetteIntensity = (std::max)(param.vignetteIntensity, 0.34f * envelope);
		param.vignetteScale = (std::max)(param.vignetteScale, 1.08f);
		SetShockwave(param, shockwavePriority, 100, bossDefeat_, 0.95f, 0.075f, 0.034f);
	}

	if (gameOver_.remaining > 0.0f) {
		const float envelope = GetEnvelope(gameOver_);
		param.isGrayscale = (std::max)(param.isGrayscale, config_.grayscaleStrength);
		param.vignetteIntensity = (std::max)(param.vignetteIntensity, 0.65f * envelope);
		param.vignetteScale = (std::max)(param.vignetteScale, 1.25f);
		param.glitchAmount = (std::max)(param.glitchAmount, 0.022f * envelope);
		param.noiseIntensity = (std::max)(param.noiseIntensity, 0.10f * envelope);
		param.randomIntensity = (std::max)(param.randomIntensity, 0.07f * envelope);
	}

	if (dash_.remaining > 0.0f) {
		const float envelope = GetEnvelope(dash_);
		param.intensity += 0.18f * envelope;
		param.chromAbAmount += 0.009f * envelope;
		param.radialBlurCenter = dash_.center;
		param.radialBlurWidth = (std::max)(param.radialBlurWidth, config_.dashRadialBlurWidth);
		param.radialBlurIntensity = (std::max)(param.radialBlurIntensity, config_.dashRadialBlurIntensity * envelope);
	}

	if (upgradeConfirm_.remaining > 0.0f) {
		const float envelope = GetEnvelope(upgradeConfirm_);
		param.intensity += 0.55f * envelope;
		SetShockwave(param, shockwavePriority, 60, upgradeConfirm_, 0.48f, 0.040f, 0.012f);
	}

	param.intensity = (std::clamp)(Safe(param.intensity), 0.0f, config_.maxBloomBoost);
	param.chromAbAmount = (std::clamp)(Safe(param.chromAbAmount), 0.0f, config_.maxChromaticAberration);
	param.vignetteIntensity = (std::clamp)(Safe(param.vignetteIntensity), 0.0f, config_.maxVignette);
	param.vignetteScale = (std::clamp)(Safe(param.vignetteScale), 0.0f, 2.0f);
	param.noiseIntensity = (std::clamp)(Safe(param.noiseIntensity), 0.0f, config_.maxNoise);
	param.randomIntensity = (std::clamp)(Safe(param.randomIntensity), 0.0f, config_.maxRandom);
	param.glitchAmount = (std::clamp)(Safe(param.glitchAmount), 0.0f, 0.08f);
	param.gaussianIntensity = (std::clamp)(Safe(param.gaussianIntensity), 0.0f, 0.60f);
	param.fullScreenBoxBlurBlend = (std::clamp)(Safe(param.fullScreenBoxBlurBlend), 0.0f, 0.45f);
	param.radialBlurIntensity = (std::clamp)(Safe(param.radialBlurIntensity), 0.0f, config_.maxRadialBlur);
	param.shockwaveStrength = (std::clamp)(Safe(param.shockwaveStrength), 0.0f, 0.06f);
	param.exposure = (std::clamp)(Safe(param.exposure), 0.0f, 0.25f);
}

float ScreenEffectDirector::GetTimeScaleMultiplier() const
{
	if (bossDefeat_.remaining > 0.0f) {
		const float elapsed = bossDefeat_.duration - bossDefeat_.remaining;
		if (elapsed < config_.bossDefeatHitStop) {
			return 0.0f;
		}
		return 0.22f;
	}
	if (justDodge_.remaining > 0.0f) {
		return config_.justDodgeTimeScale;
	}
	if (bossPhase_.remaining > bossPhase_.duration * 0.72f) {
		return 0.42f;
	}
	if (bossEntry_.remaining > bossEntry_.duration * 0.76f) {
		return 0.35f;
	}
	return 1.0f;
}

bool ScreenEffectDirector::IsActive() const
{
	return config_.outlineEnabled ||
		upgradeMenuOpen_ ||
		(lowHpRatio_ > 0.0f && lowHpRatio_ < config_.lowHpThreshold) ||
		justDodge_.remaining > 0.0f ||
		playerDamage_.remaining > 0.0f ||
		enemyDefeat_.remaining > 0.0f ||
		bossEntry_.remaining > 0.0f ||
		bossPhase_.remaining > 0.0f ||
		bossDefeat_.remaining > 0.0f ||
		gameOver_.remaining > 0.0f ||
		dash_.remaining > 0.0f ||
		upgradeConfirm_.remaining > 0.0f;
}

float ScreenEffectDirector::GetEnvelope(const TimedEffect& effect)
{
	if (effect.remaining <= 0.0f || effect.duration <= 0.0f) {
		return 0.0f;
	}
	const float progress = GetProgress(effect);
	const float attack = (std::min)(1.0f, progress / 0.12f);
	const float release = 1.0f - progress;
	return (std::clamp)(attack * release * release, 0.0f, 1.0f);
}

float ScreenEffectDirector::GetProgress(const TimedEffect& effect)
{
	if (effect.duration <= 0.0f) {
		return 1.0f;
	}
	return (std::clamp)(1.0f - effect.remaining / effect.duration, 0.0f, 1.0f);
}

void ScreenEffectDirector::Tick(TimedEffect& effect, float deltaTime)
{
	effect.remaining = (std::max)(0.0f, effect.remaining - deltaTime);
}

float ScreenEffectDirector::Safe(float value, float fallback)
{
	return std::isfinite(value) ? value : fallback;
}

void ScreenEffectDirector::SetShockwave(
	cg2::BloomParam& param,
	int& currentPriority,
	int priority,
	const TimedEffect& effect,
	float maxRadius,
	float width,
	float strength) const
{
	if (effect.remaining <= 0.0f || priority < currentPriority) {
		return;
	}
	currentPriority = priority;
	param.shockwaveCenter = effect.center;
	param.shockwaveRadius = 0.02f + GetProgress(effect) * maxRadius;
	param.shockwaveWidth = width;
	param.shockwaveStrength = strength * GetEnvelope(effect) * effect.importance;
}
