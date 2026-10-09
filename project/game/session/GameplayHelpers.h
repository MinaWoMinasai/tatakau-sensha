#pragma once
#include "game/weapon/CombatTypes.h"
#include "game/session/GameWorld.h"
#include "RuntimeProfiler.h"
#include "StartupTrace.h"
#include "GameStartMode.h"
#include "game/ui/TankCombatNeonGeometry.h"
#include "game/effects/TankSpecialNeonGeometry.h"
#include "CollisionConfig.h"
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace gameplay::detail {

inline void ApplyGameplayNeonBloomPreset(cg2::BloomParam& param, float gain = 1.2f)
{
    // Match the projectile halo while composing the unchanged sharp source separately.
    // Legacy intensity is independent: the pyramid applies this linear gain once.
    param.bloomMode = 2;
    param.threshold = 0.0f;
    param.bloomSoftKnee = 0.5f;
    param.bloomGain = gain;
    param.bloomScatter = 0.55f;
    param.bloomRadius = 1.0f;
}

/// @brief 衝突開発表示色を返す。
inline cg2::Vector4 GetCollisionDebugColor(uint32_t attribute)
{
    if (attribute & kCollisionAttributePlayer) {
        return {0.15f, 1.0f, 0.95f, 0.85f};
    }
    if (attribute & kCollisionAttributePlayerDrone) {
        return {0.25f, 0.75f, 1.0f, 0.85f};
    }
    if (attribute & kCollisionAttributeExpEnemy) {
        return {1.0f, 0.72f, 0.15f, 0.85f};
    }
    if (attribute & kCollisionAttributeEnemy) {
        return {1.0f, 0.18f, 0.35f, 0.85f};
    }
    if (attribute & kCollisionAttributePlayerBullet) {
        return {1.0f, 0.95f, 0.2f, 0.75f};
    }
    if (attribute & kCollisionAttributeEnemyBullet) {
        return {1.0f, 0.35f, 0.1f, 0.75f};
    }
    if (attribute & kCollisionAttributeHostileExpEnemyBullet) {
        return {1.0f, 0.12f, 0.04f, 0.8f};
    }
    return {1.0f, 1.0f, 1.0f, 0.7f};
}

/// @brief 弾Colliderであるか判定する。
inline bool IsBulletCollider(uint32_t attribute)
{
    return (attribute & (kCollisionAttributePlayerBullet | kCollisionAttributeEnemyBullet | kCollisionAttributeHostileExpEnemyBullet)) != 0;
}

/// @brief 弾の所有者を表示用の名前へ変換する。
inline const char* BulletOwnerName(BulletOwner owner)
{
    switch (owner) {
    case kPlayer:
        return "プレイヤー";
    case kEnemy:
        return "敵";
    case kExpEnemyHostile:
        return "EXP敵";
    default:
        return "不明";
    }
}

#ifdef USE_IMGUI
/// @brief 弾の所有者に対応する開発表示の色を返す。
inline ImU32 BulletOwnerDebugColor(BulletOwner owner)
{
    switch (owner) {
    case kPlayer:
        return IM_COL32(255, 238, 80, 255);
    case kEnemy:
        return IM_COL32(255, 94, 120, 255);
    case kExpEnemyHostile:
        return IM_COL32(255, 48, 24, 255);
    default:
        return IM_COL32(255, 255, 255, 255);
    }
}
#endif

/// @brief 独自設定整数を読み取る。
inline int ReadCustomInt(const nlohmann::json& customProperties, const char* key, int fallback)
{
    if (!customProperties.is_object() || !customProperties.contains(key) || !customProperties[key].is_number()) {
        return fallback;
    }
    return customProperties[key].get<int>();
}

/// @brief 項目モデルを取得を試みる。
inline bool TryGetItemModel(const std::string& prefab, std::string& model)
{
    if (prefab == "Default" || prefab == "Heal") {
        model = "jewelry.obj";
        return true;
    }
    if (prefab == "Power") {
        model = "ball.obj";
        return true;
    }
    if (prefab == "Exp") {
        model = "bloomBall.obj";
        return true;
    }
    return false;
}

/// @brief 独自設定Boolを読み取る。
inline bool ReadCustomBool(const nlohmann::json& customProperties, const char* key, bool fallback)
{
    if (!customProperties.is_object() || !customProperties.contains(key) || !customProperties[key].is_boolean()) {
        return fallback;
    }
    return customProperties[key].get<bool>();
}

/// @brief 独自設定浮動小数を読み取る。
inline float ReadCustomFloat(const nlohmann::json& customProperties, const char* key, float fallback)
{
    if (!customProperties.is_object() || !customProperties.contains(key) || !customProperties[key].is_number()) {
        return fallback;
    }
    return customProperties[key].get<float>();
}

/// @brief 独自設定文字列を読み取る。
inline std::string ReadCustomString(const nlohmann::json& customProperties, const char* key, const std::string& fallback)
{
    if (!customProperties.is_object() || !customProperties.contains(key) || !customProperties[key].is_string()) {
        return fallback;
    }
    return customProperties[key].get<std::string>();
}

/// @brief ボス段階表示名前を返す。
inline std::string GetBossPhaseDisplayName(const LevelBossPhase& phase)
{
    const std::string configuredDisplayName = ReadCustomString(phase.customProperties, "displayName", "");
    if (!configuredDisplayName.empty()) {
        return configuredDisplayName;
    }

    if (phase.name == "phase_75_add_exp_pressure") {
        return "PHASE 2";
    }
    if (phase.name == "phase_50_add_hazard") {
        return "HAZARD DEPLOYED";
    }
    if (phase.name == "phase_25_final_pressure") {
        return "FINAL PHASE";
    }
    return "PHASE CHANGE";
}

/// @brief Jsonベクトル4を読み取る。
inline cg2::Vector4 ReadJsonVector4(const nlohmann::json& json, const cg2::Vector4& fallback)
{
    if (!json.is_object()) {
        return fallback;
    }

    cg2::Vector4 value = fallback;
    if (json.contains("x") && json["x"].is_number())
        value.x = json["x"].get<float>();
    if (json.contains("y") && json["y"].is_number())
        value.y = json["y"].get<float>();
    if (json.contains("z") && json["z"].is_number())
        value.z = json["z"].get<float>();
    if (json.contains("w") && json["w"].is_number())
        value.w = json["w"].get<float>();
    return value;
}

/// @brief Jsonベクトル3を読み取る。
inline cg2::Vector3 ReadJsonVector3(const nlohmann::json& json, const cg2::Vector3& fallback)
{
    if (!json.is_object()) {
        return fallback;
    }

    cg2::Vector3 value = fallback;
    if (json.contains("x") && json["x"].is_number())
        value.x = json["x"].get<float>();
    if (json.contains("y") && json["y"].is_number())
        value.y = json["y"].get<float>();
    if (json.contains("z") && json["z"].is_number())
        value.z = json["z"].get<float>();
    return value;
}

/// @brief Jsonベクトル2を読み取る。
inline cg2::Vector2 ReadJsonVector2(const nlohmann::json& json, const cg2::Vector2& fallback)
{
    if (!json.is_object()) {
        return fallback;
    }

    cg2::Vector2 value = fallback;
    if (json.contains("x") && json["x"].is_number())
        value.x = json["x"].get<float>();
    if (json.contains("y") && json["y"].is_number())
        value.y = json["y"].get<float>();
    return value;
}

/// @brief Jsonベクトル2を書き込む。
inline nlohmann::json WriteJsonVector2(const cg2::Vector2& value)
{
    return {{"x", value.x}, {"y", value.y}};
}

/// @brief Jsonベクトル3を書き込む。
inline nlohmann::json WriteJsonVector3(const cg2::Vector3& value)
{
    return {{"x", value.x}, {"y", value.y}, {"z", value.z}};
}

/// @brief Jsonベクトル4を書き込む。
inline nlohmann::json WriteJsonVector4(const cg2::Vector4& value)
{
    return {{"x", value.x}, {"y", value.y}, {"z", value.z}, {"w", value.w}};
}

/// @brief 弾軌跡設定Jsonを書き込む。
inline nlohmann::json WriteBulletTrailSettingsJson(const BulletTrailSettings& settings)
{
    return {{"playerHalfWidth", settings.playerHalfWidth},
            {"enemyHalfWidth", settings.enemyHalfWidth},
            {"lifetime", settings.lifetime},
            {"maxPoints", settings.maxPoints},
            {"interpolationSteps", settings.interpolationSteps},
            {"headWidthScale", settings.headWidthScale},
            {"tailWidthScale", settings.tailWidthScale},
            {"widthCurvePower", settings.widthCurvePower},
            {"colorCurvePower", settings.colorCurvePower},
            {"useObjectColorForTrail", settings.useObjectColorForTrail},
            {"trailHeadIntensity", settings.trailHeadIntensity},
            {"trailTailIntensity", settings.trailTailIntensity},
            {"trailHeadAlpha", settings.trailHeadAlpha},
            {"trailTailAlpha", settings.trailTailAlpha},
            {"playerObjectColor", WriteJsonVector4(settings.playerObjectColor)},
            {"enemyObjectColor", WriteJsonVector4(settings.enemyObjectColor)},
            {"reflectableObjectColor", WriteJsonVector4(settings.reflectableObjectColor)},
            {"startColor", WriteJsonVector4(settings.startColor)},
            {"playerEndColor", WriteJsonVector4(settings.playerEndColor)},
            {"enemyEndColor", WriteJsonVector4(settings.enemyEndColor)},
            {"reflectableEndColor", WriteJsonVector4(settings.reflectableEndColor)}};
}

/// @brief 弾軌跡設定Jsonを読み取る。
inline void ReadBulletTrailSettingsJson(const nlohmann::json& json, BulletTrailSettings& settings)
{
    if (!json.is_object()) {
        return;
    }
    settings.playerHalfWidth = ReadCustomFloat(json, "playerHalfWidth", settings.playerHalfWidth);
    settings.enemyHalfWidth = ReadCustomFloat(json, "enemyHalfWidth", settings.enemyHalfWidth);
    settings.lifetime = ReadCustomFloat(json, "lifetime", settings.lifetime);
    settings.maxPoints = ReadCustomInt(json, "maxPoints", settings.maxPoints);
    settings.interpolationSteps = ReadCustomInt(json, "interpolationSteps", settings.interpolationSteps);
    settings.headWidthScale = ReadCustomFloat(json, "headWidthScale", settings.headWidthScale);
    settings.tailWidthScale = ReadCustomFloat(json, "tailWidthScale", settings.tailWidthScale);
    settings.widthCurvePower = ReadCustomFloat(json, "widthCurvePower", settings.widthCurvePower);
    settings.colorCurvePower = ReadCustomFloat(json, "colorCurvePower", settings.colorCurvePower);
    settings.useObjectColorForTrail = ReadCustomBool(json, "useObjectColorForTrail", settings.useObjectColorForTrail);
    settings.trailHeadIntensity = ReadCustomFloat(json, "trailHeadIntensity", settings.trailHeadIntensity);
    settings.trailTailIntensity = ReadCustomFloat(json, "trailTailIntensity", settings.trailTailIntensity);
    settings.trailHeadAlpha = ReadCustomFloat(json, "trailHeadAlpha", settings.trailHeadAlpha);
    settings.trailTailAlpha = ReadCustomFloat(json, "trailTailAlpha", settings.trailTailAlpha);
    if (json.contains("playerObjectColor"))
        settings.playerObjectColor = ReadJsonVector4(json["playerObjectColor"], settings.playerObjectColor);
    if (json.contains("enemyObjectColor"))
        settings.enemyObjectColor = ReadJsonVector4(json["enemyObjectColor"], settings.enemyObjectColor);
    if (json.contains("reflectableObjectColor"))
        settings.reflectableObjectColor = ReadJsonVector4(json["reflectableObjectColor"], settings.reflectableObjectColor);
    if (json.contains("startColor"))
        settings.startColor = ReadJsonVector4(json["startColor"], settings.startColor);
    if (json.contains("playerEndColor"))
        settings.playerEndColor = ReadJsonVector4(json["playerEndColor"], settings.playerEndColor);
    if (json.contains("enemyEndColor"))
        settings.enemyEndColor = ReadJsonVector4(json["enemyEndColor"], settings.enemyEndColor);
    if (json.contains("reflectableEndColor"))
        settings.reflectableEndColor = ReadJsonVector4(json["reflectableEndColor"], settings.reflectableEndColor);
}

/// @brief ブルームパラメーターJsonを書き込む。
inline nlohmann::json WriteBloomParamJson(const cg2::BloomParam& param)
{
    return {{"threshold", param.threshold},
            {"intensity", param.intensity},
            {"bloomMode", param.bloomMode},
            {"bloomSoftKnee", param.bloomSoftKnee},
            {"bloomScatter", param.bloomScatter},
            {"bloomRadius", param.bloomRadius},
            {"bloomGain", param.bloomGain},
            {"vignetteIntensity", param.vignetteIntensity},
            {"vignetteScale", param.vignetteScale},
            {"distortionAmount", param.distortionAmount},
            {"chromAbAmount", param.chromAbAmount},
            {"noiseIntensity", param.noiseIntensity},
            {"scanlineIntensity", param.scanlineIntensity},
            {"scanlineFrequency", param.scanlineFrequency},
            {"curvature", param.curvature},
            {"borderSharp", param.borderSharp},
            {"glitchAmount", param.glitchAmount},
            {"gaussianIntensity", param.gaussianIntensity},
            {"dissolveThreshold", param.dissolveThreshold},
            {"outlineWidth", param.outlineWidth},
            {"outlineThreshold", param.outlineThreshold},
            {"boxBlurIntensity", param.boxBlurIntensity},
            {"outlineColor", WriteJsonVector3(param.outlineColor)},
            {"outlineBloomIntensity", param.outlineBloomIntensity},
            {"outlineBloomWidth", param.outlineBloomWidth},
            {"boxBlurRadius", param.boxBlurRadius},
            {"fullScreenBoxBlurBlend", param.fullScreenBoxBlurBlend},
            {"depthOutlineEnabled", param.depthOutlineEnabled},
            {"depthNearClip", param.depthNearClip},
            {"depthFarClip", param.depthFarClip},
            {"depthOutlineScale", param.depthOutlineScale},
            {"radialBlurCenter", WriteJsonVector2(param.radialBlurCenter)},
            {"radialBlurWidth", param.radialBlurWidth},
            {"radialBlurIntensity", param.radialBlurIntensity},
            {"dissolveEdgeColor", WriteJsonVector3(param.dissolveEdgeColor)},
            {"dissolveEdgeWidth", param.dissolveEdgeWidth},
            {"dissolveNoiseScale", param.dissolveNoiseScale},
            {"dissolveNoiseSpeed", param.dissolveNoiseSpeed}};
}

/// @brief ブルームパラメーターJsonを読み取る。
inline void ReadBloomParamJson(const nlohmann::json& json, cg2::BloomParam& param)
{
    if (!json.is_object()) {
        return;
    }
    param.threshold = ReadCustomFloat(json, "threshold", param.threshold);
    param.intensity = ReadCustomFloat(json, "intensity", param.intensity);
    auto readBloomSetting = [&](const char* name, float fallback, float minimum, float maximum) {
        const float value = ReadCustomFloat(json, name, fallback);
        return std::isfinite(value) ? (std::clamp)(value, minimum, maximum) : fallback;
    };
    param.bloomMode = static_cast<uint32_t>(readBloomSetting("bloomMode", static_cast<float>(param.bloomMode), 0, 3));
    param.bloomSoftKnee = readBloomSetting("bloomSoftKnee", param.bloomSoftKnee, 0, 1);
    param.bloomScatter = readBloomSetting("bloomScatter", param.bloomScatter, 0, 0.8f);
    param.bloomRadius = readBloomSetting("bloomRadius", param.bloomRadius, 0.25f, 2);
    param.bloomGain = readBloomSetting("bloomGain", param.bloomGain, 0, 4);
    param.vignetteIntensity = ReadCustomFloat(json, "vignetteIntensity", param.vignetteIntensity);
    param.vignetteScale = ReadCustomFloat(json, "vignetteScale", param.vignetteScale);
    param.distortionAmount = ReadCustomFloat(json, "distortionAmount", param.distortionAmount);
    param.chromAbAmount = ReadCustomFloat(json, "chromAbAmount", param.chromAbAmount);
    param.noiseIntensity = ReadCustomFloat(json, "noiseIntensity", param.noiseIntensity);
    param.scanlineIntensity = ReadCustomFloat(json, "scanlineIntensity", param.scanlineIntensity);
    param.scanlineFrequency = ReadCustomFloat(json, "scanlineFrequency", param.scanlineFrequency);
    param.curvature = ReadCustomFloat(json, "curvature", param.curvature);
    param.borderSharp = ReadCustomFloat(json, "borderSharp", param.borderSharp);
    param.glitchAmount = ReadCustomFloat(json, "glitchAmount", param.glitchAmount);
    param.gaussianIntensity = ReadCustomFloat(json, "gaussianIntensity", param.gaussianIntensity);
    param.dissolveThreshold = ReadCustomFloat(json, "dissolveThreshold", param.dissolveThreshold);
    param.outlineWidth = ReadCustomFloat(json, "outlineWidth", param.outlineWidth);
    param.outlineThreshold = ReadCustomFloat(json, "outlineThreshold", param.outlineThreshold);
    param.boxBlurIntensity = ReadCustomFloat(json, "boxBlurIntensity", param.boxBlurIntensity);
    if (json.contains("outlineColor")) {
        param.outlineColor = ReadJsonVector3(json["outlineColor"], param.outlineColor);
    }
    param.outlineBloomIntensity = ReadCustomFloat(json, "outlineBloomIntensity", param.outlineBloomIntensity);
    param.outlineBloomWidth = ReadCustomFloat(json, "outlineBloomWidth", param.outlineBloomWidth);
    param.boxBlurRadius = ReadCustomFloat(json, "boxBlurRadius", param.boxBlurRadius);
    param.fullScreenBoxBlurBlend = ReadCustomFloat(json, "fullScreenBoxBlurBlend", param.fullScreenBoxBlurBlend);
    param.depthOutlineEnabled = ReadCustomFloat(json, "depthOutlineEnabled", param.depthOutlineEnabled);
    param.depthNearClip = ReadCustomFloat(json, "depthNearClip", param.depthNearClip);
    param.depthFarClip = ReadCustomFloat(json, "depthFarClip", param.depthFarClip);
    param.depthOutlineScale = ReadCustomFloat(json, "depthOutlineScale", param.depthOutlineScale);
    if (json.contains("radialBlurCenter")) {
        param.radialBlurCenter = ReadJsonVector2(json["radialBlurCenter"], param.radialBlurCenter);
    }
    param.radialBlurWidth = ReadCustomFloat(json, "radialBlurWidth", param.radialBlurWidth);
    param.radialBlurIntensity = ReadCustomFloat(json, "radialBlurIntensity", param.radialBlurIntensity);
    if (json.contains("dissolveEdgeColor")) {
        param.dissolveEdgeColor = ReadJsonVector3(json["dissolveEdgeColor"], param.dissolveEdgeColor);
    }
    param.dissolveEdgeWidth = ReadCustomFloat(json, "dissolveEdgeWidth", param.dissolveEdgeWidth);
    param.dissolveNoiseScale = ReadCustomFloat(json, "dissolveNoiseScale", param.dissolveNoiseScale);
    param.dissolveNoiseSpeed = ReadCustomFloat(json, "dissolveNoiseSpeed", param.dissolveNoiseSpeed);
}

/// @brief 衝突Ring設定を作成して返す。
inline cg2::RingEffectConfig MakeCollisionRingConfig(float radius, const cg2::Vector4& color)
{
    cg2::RingEffectConfig config{};
    config.lifeTime = 0.045f;
    config.startRadius = radius;
    config.endRadius = radius;
    config.startWidth = 0.045f;
    config.endWidth = 0.045f;
    config.rotate = {0.0f, 0.0f, 0.0f};
    config.startColor = color;
    config.endColor = color;
    config.divisions = 64;
    return config;
}

/// @brief Collider開発表示Ringsを発生させる。
inline void EmitColliderDebugRings(cg2::RingManager& ringManager, Collider& collider)
{
    const cg2::Vector4 color = GetCollisionDebugColor(collider.GetCollisionAttribute());
    if (collider.GetShape() == ColliderShape::Capsule) {
        const cg2::Segment& segment = collider.GetSegment();
        const cg2::Vector3 start = segment.origin;
        const cg2::Vector3 end = segment.origin + segment.diff;
        const cg2::Vector3 middle = start + segment.diff * 0.5f;
        cg2::RingEffectConfig config = MakeCollisionRingConfig(collider.GetCapsuleRadius(), color);
        ringManager.Emit(start, config);
        ringManager.Emit(middle, config);
        ringManager.Emit(end, config);
        return;
    }

    ringManager.Emit(collider.GetWorldPosition(), MakeCollisionRingConfig(collider.GetRadius(), color));
}

/// @brief 有効カメラ位置を返す。
inline cg2::Vector3 GetActiveCameraPosition(cg2::Camera* camera, cg2::DebugCamera* debugCamera)
{
    return cg2::Object3dCommon::GetInstance()->GetIsDebugCamera() && debugCamera ? debugCamera->GetEyePosition() : camera->GetTranslate();
}

/// @brief XY平面で点と有限線分の最短距離を返す。Z座標は距離に含めない。
/// @param outT 非nullなら最近点の線分上の割合（0～1）を書き込む。XYの長さの二乗が0.0001以下なら0。
inline float DistancePointToSegment2D(const cg2::Vector3& point, const cg2::Vector3& start, const cg2::Vector3& end, float* outT = nullptr)
{
    const cg2::Vector3 segment = end - start;
    const cg2::Vector3 toPoint = point - start;
    const float lengthSq = segment.x * segment.x + segment.y * segment.y;
    float t = 0.0f;
    if (lengthSq > 0.0001f) {
        t = (std::clamp)((toPoint.x * segment.x + toPoint.y * segment.y) / lengthSq, 0.0f, 1.0f);
    }
    if (outT) {
        *outT = t;
    }
    const cg2::Vector3 closest = start + segment * t;
    const cg2::Vector3 diff = point - closest;
    return std::sqrt(diff.x * diff.x + diff.y * diff.y);
}

/// @brief XY平面のベクトルをangleRadラジアンだけ回転し、Z成分は保持する。
inline cg2::Vector3 RotateVector2D(const cg2::Vector3& value, float angleRad)
{
    const float c = std::cos(angleRad);
    const float s = std::sin(angleRad);
    return {value.x * c - value.y * s, value.x * s + value.y * c, value.z};
}

} // namespace gameplay::detail
