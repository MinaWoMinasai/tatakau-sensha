#include "game/session/GameplayQueries.h"
#include "game/session/GameplaySystems.h"
#include "game/weapon/CombatTypes.h"
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

#include "game/session/GameplayHelpers.h"

namespace gameplay {

using namespace detail;

cg2::Vector2 GameplayQueries::WorldToScreenUv(const cg2::Vector3& worldPos) const
{
    const cg2::Vector2 screen = world_.gameplayHud->WorldToScreen(worldPos);
    return {(std::clamp)(screen.x / static_cast<float>(cg2::WinApp::kClientWidth), 0.0f, 1.0f),
            (std::clamp)(screen.y / static_cast<float>(cg2::WinApp::kClientHeight), 0.0f, 1.0f)};
}

IScene::ScreenEffectState GameplayQueries::GetScreenEffectState() const
{
    IScene::ScreenEffectState state{};
    if (IsNeonShowcaseActive()) {
        state.suppressPostEffectDebugUi = true;
        state.suppressOutlines = true;
        return state;
    }
    const bool evolutionUiOpen = world_.resources.player_ && world_.resources.player_->IsChangeMode();
    state.bloomScale = evolutionUiOpen ? 0.38f : 1.0f;
    state.suppressPostEffectDebugUi = evolutionUiOpen;
    state.suppressOutlines = world_.run.prototypeRun_;
    state.suppressTemporal = world_.resources.neonDepthCameraScoped_;
    state.active = world_.combat.screenEffectDirector_.IsActive();
    if (state.active) {
        world_.combat.screenEffectDirector_.ApplyTo(state.param);
    }
    return state;
}

IScene::DeveloperShowcaseState GameplayQueries::GetDeveloperShowcaseState()
{
    IScene::DeveloperShowcaseState state{};
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    if (IsNeonShowcaseActive()) {
        state.active = true;
        state.camera = world_.resources.neonSkinnedPreview_->GetShowcaseCamera();
        state.diagnostic = world_.resources.neonSkinnedPreview_->GetShowcaseDiagnostic();
        state.threshold = world_.resources.neonSkinnedPreview_->GetShowcaseBloomThreshold();
        state.intensity = world_.resources.neonSkinnedPreview_->GetShowcaseBloomIntensity();
        state.exposure = world_.resources.neonSkinnedPreview_->GetShowcaseExposure();
        state.bloomComparisonMode = world_.resources.neonSkinnedPreview_->GetShowcaseBloomMode();
        state.bloomSoftKnee = world_.resources.neonSkinnedPreview_->GetShowcaseBloomSoftKnee();
        state.bloomScatter = world_.resources.neonSkinnedPreview_->GetShowcaseBloomScatter();
        state.bloomRadius = world_.resources.neonSkinnedPreview_->GetShowcaseBloomRadius();
        state.bloomGain = world_.resources.neonSkinnedPreview_->GetShowcaseBloomGain();
        state.toneMappingMode = world_.resources.neonSkinnedPreview_->GetShowcaseToneMappingMode();
    } else if (!world_.demo.titleDemo_) {
        state.bloomComparisonMode = world_.presentation.developerBloomComparisonMode_;
        state.toneMappingMode = world_.presentation.developerToneMappingMode_;
        // Continuous scenario readback does not set either comparison flag.
        // Unresolved recording fences fail before another held temporal draw.
        state.comparisonFreeze = world_.presentation.developerBloomFreeze_ || world_.resources.neonBossDeveloperFreeze_;
    }
#endif
    return state;
}

void GameplayQueries::RecordDeveloperPostParameters(const cg2::BloomParam& param)
{
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    world_.presentation.developerCompositeParams_ = param;
    world_.presentation.developerCompositeParamsAvailable_ = true;
#else
    (void)param;
#endif
}

void GameplayQueries::RecordDeveloperFrame(cg2::DirectXCommon& dx)
{
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    if (world_.resources.neonSkinnedPreview_)
        world_.resources.neonSkinnedPreview_->RecordShowcaseCapture(dx);
    world_.bossPresentation->RecordNeonBossDeveloperFrame(dx);
    world_.gameplayScenarioRunner->RecordGameplayScenarioCapture(dx);
    if (world_.presentation.developerGameCapture_.HasRequest())
        world_.presentation.developerGameCapture_.SetFrameMetadata(MakeDeveloperGameCaptureMetadata(dx));
    world_.presentation.developerGameCapture_.Record(dx);
#else
    (void)dx;
#endif
}

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
nlohmann::json GameplayQueries::MakeDeveloperGameCaptureMetadata(cg2::DirectXCommon& dx) const
{
    auto vector = [](const cg2::Vector3& value) {
        return nlohmann::json::array({value.x, value.y, value.z});
    };
    const auto& post = world_.presentation.developerCompositeParams_;
    const auto counts = world_.resources.bulletManager_->GetBulletCounts();
    nlohmann::json metadata = {
        {"schemaVersion", 1},
        {"build", "Development"},
        {"scene", "GameScene"},
        {"comparisonFreeze", world_.presentation.developerBloomFreeze_},
        {"samePoseBatch", false},
        {"comparisonMethod",
         world_.presentation.developerBloomFreeze_
             ? "Manual captures; game update frozen, source transforms/trails/particles retained; verify recorded conditions."
             : "Live gameplay captures; moving scenes are not a fixed-pose A/B."},
        {"bloomOverrideMode", world_.presentation.developerBloomComparisonMode_},
        {"toneMappingOverride", world_.presentation.developerToneMappingMode_},
        {"globalPostAvailable", world_.presentation.developerCompositeParamsAvailable_},
        {"globalPost",
         {{"mode", post.bloomMode},
          {"threshold", post.threshold},
          {"legacyIntensity", post.intensity},
          {"gain", post.bloomGain},
          {"softKnee", post.bloomSoftKnee},
          {"scatter", post.bloomScatter},
          {"radius", post.bloomRadius},
          {"exposure", post.exposure},
          {"toneMappingMode", post.toneMappingMode},
          {"taa", post.temporalEnabled},
          {"jitter", post.temporalJitterEnabled},
          {"grayscale", post.isGrayscale},
          {"ssao", post.ssaoEnabled},
          {"ssr", post.ssrEnabled},
          {"renderDebugMode", post.renderDebugMode}}},
        {"localCategories", world_.gameplaySettings->BuildGamePostEffectConfig()},
        {"visualAppearance", world_.gameplaySettings->BuildGameVisualConfig()},
        {"sourceResolution",
         {{"trailLegacyScale", 0.5f},
          {"trailQualityAndOffScale", 1.0f},
          {"sharedModelLegacyScale", 0.5f},
          {"sharedModelQualityAndOffScale", 1.0f},
          {"note", "Legacy preserves the half-resolution projectile/model captures; new/OFF preserves full-resolution source cores."}}},
        {"sourceCounts",
         {{"playerBullets", counts.player},
          {"enemyBullets", counts.enemy},
          {"hostileExpBullets", counts.hostileExpEnemy},
          {"enemies", world_.resources.enemyManager_->GetEnemyCount()},
          {"particles", cg2::ParticleManager::GetInstance()->GetActiveCount()},
          {"particlesCountIsCpuOnly", true},
          {"particlesGpuUpdate", cg2::ParticleManager::GetInstance()->IsUseGpuUpdate()},
          {"particlesDrawReady", cg2::ParticleManager::GetInstance()->HasDrawableParticles()},
          {"neonTriangles", world_.presentation.neonTriangleParticles_.size()}}},
        {"camera",
         {{"debug", cg2::Object3dCommon::GetInstance()->GetIsDebugCamera()},
          {"gamePosition", vector(world_.resources.camera->GetTranslate())},
          {"gameRotation", vector(world_.resources.camera->GetRotate())},
          {"debugEye", vector(world_.resources.debugCamera->GetEyePosition())}}},
        {"sourcePositions",
         {{"player", vector(world_.resources.player_->GetWorldPosition())}, {"boss", vector(world_.resources.enemy_->GetWorldPosition())}}},
        {"textGlow",
         {{"enabled", world_.combat.gameTextNeonEnabled_},
          {"sourceBrightness", world_.combat.gameTextNeonStyle_.sourceBrightness},
          {"threshold", world_.combat.gameTextNeonStyle_.threshold},
          {"innerIntensity", world_.combat.gameTextNeonStyle_.innerIntensity},
          {"outerIntensity", world_.combat.gameTextNeonStyle_.outerIntensity},
          {"color",
           {world_.combat.gameTextNeonStyle_.glowColor.x, world_.combat.gameTextNeonStyle_.glowColor.y,
            world_.combat.gameTextNeonStyle_.glowColor.z, world_.combat.gameTextNeonStyle_.glowColor.w}}}},
        {"validation", {{"d3d12DebugLayer", dx.IsD3D12DebugLayerEnabled()}, {"gpuBasedValidation", dx.IsGpuBasedValidationEnabled()}}},
        {"gpuTiming",
         {{"captureActive", cg2::RuntimeProfiler::Get().IsCaptureActive()},
          {"captureComplete", cg2::RuntimeProfiler::Get().IsCaptureComplete()},
          {"note", "Actual per-scope timestamps are in the separate profiler CSV, not inferred from this image."}}}};
    for (const auto& row : world_.resources.debugCamera->GetViewProjectionMatrix().m)
        metadata["camera"]["debugViewProjection"].push_back({row[0], row[1], row[2], row[3]});
    for (const auto& row : world_.resources.camera->GetViewProjectionMatrix().m)
        metadata["camera"]["gameViewProjection"].push_back({row[0], row[1], row[2], row[3]});
    for (const Bullet* bullet : world_.resources.bulletManager_->GetBulletPtrs())
        metadata["sourcePositions"]["bullets"].push_back(vector(bullet->GetWorldPosition()));
    for (const ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs())
        metadata["sourcePositions"]["experienceEnemies"].push_back(vector(enemy->GetWorldPosition()));
    UINT64 frequency = 0;
    if (SUCCEEDED(dx.GetQueue()->GetTimestampFrequency(&frequency)))
        metadata["queueTimestampFrequencyHz"] = frequency;
    Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter;
    if (SUCCEEDED(dx.GetDxgiFactory()->EnumAdapterByLuid(dx.GetDevice()->GetAdapterLuid(), IID_PPV_ARGS(&adapter)))) {
        DXGI_ADAPTER_DESC3 description{};
        if (SUCCEEDED(adapter->GetDesc3(&description))) {
            char name[256]{};
            WideCharToMultiByte(CP_UTF8, 0, description.Description, -1, name, sizeof(name), nullptr, nullptr);
            metadata["gpu"] = {{"adapter", name}, {"vendor", description.VendorId}, {"device", description.DeviceId}};
        }
    }
    return metadata;
}
#endif

std::string GameplayQueries::GetNextSceneName() const
{
    return world_.combat.nextSceneName_;
}

/// @brief 終了済みであるか判定する。
bool GameplayQueries::IsFinished() const
{
    return world_.combat.finished_;
}

/// @brief BallOBJ形式を返す。
cg2::Object3d* GameplayQueries::GetBallObj()
{
    return world_.resources.ballObj_.get();
}

/// @brief 最終差分時間を返す。
float GameplayQueries::GetFinalDeltaTime() const
{
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    if (world_.presentation.developerBloomFreeze_ || world_.resources.neonBossDeveloperFreeze_)
        return 1.0f / 60.0f; // Freeze is a comparison, not the grayscale slow-motion effect.
#endif
    if (IsNeonShowcaseActive())
        return 1.0f / 60.0f;
    return world_.combat.finalDeltaTime;
}

/// @brief 後処理ガウシアン強度を返す。
float GameplayQueries::GetPostGaussianIntensity() const
{
    if (IsNeonShowcaseActive())
        return 0.0f;
    return world_.presentation.sceneFadeBlurIntensity_;
}

/// @brief 後処理演出パルスを返す。
PostEffectPulse GameplayQueries::GetPostEffectPulse() const
{
    if (IsNeonShowcaseActive())
        return {};
    return world_.presentation.deathPostPulse_;
}

bool GameplayQueries::IsNeonShowcaseActive() const
{
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    return world_.resources.neonSkinnedPreview_ && world_.resources.neonSkinnedPreview_->IsShowcaseActive();
#else
    return false;
#endif
}

/// @brief 遠征以外、または遠征でライバルが有効な場合true。死亡判定は別に必要。
bool GameplayQueries::IsRunRivalActive() const
{
    return !world_.run.expeditionRun_ || world_.run.tankExpeditionRivalActive_;
}

/// @brief 通常チュートリアルが有効で、敵AI・特殊戦闘・通常衝突を抑制する場合true。
bool GameplayQueries::IsTutorialCombatSuppressed() const
{
    return world_.combat.tutorialConfig_.enabled;
}
} // namespace gameplay
