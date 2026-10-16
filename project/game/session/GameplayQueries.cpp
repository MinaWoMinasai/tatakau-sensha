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
#include <d3d12sdklayers.h>
#include <limits>
#include <sstream>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include "game/session/GameplayHelpers.h"

namespace gameplay {

using namespace detail;

myEngine::Vector2 GameplayQueries::WorldToScreenUv(const myEngine::Vector3& worldPos) const
{
    const myEngine::Vector2 screen = world_.gameplayHud->WorldToScreen(worldPos);
    return {(std::clamp)(screen.x / static_cast<float>(myEngine::WinApp::kClientWidth), 0.0f, 1.0f),
            (std::clamp)(screen.y / static_cast<float>(myEngine::WinApp::kClientHeight), 0.0f, 1.0f)};
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
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
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
        state.renderDebugOverride = world_.presentation.developerStageSurfaceDebugView_;
        state.ssaoOverride = world_.presentation.developerStageSurfaceSSAO_;
        state.ssrOverride = world_.presentation.developerStageSurfaceSSR_;
        state.bloomComparisonMode = world_.presentation.developerBloomComparisonMode_;
        state.toneMappingMode = world_.presentation.developerToneMappingMode_;
        // Continuous scenario readback does not set either comparison flag.
        // Unresolved recording fences fail before another held temporal draw.
        state.comparisonFreeze = world_.presentation.developerBloomFreeze_ || world_.resources.neonBossDeveloperFreeze_;
    }
#endif
    return state;
}

void GameplayQueries::RecordDeveloperPostParameters(const myEngine::BloomParam& param)
{
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    world_.presentation.developerCompositeParams_ = param;
    world_.presentation.developerCompositeParamsAvailable_ = true;
#else
    (void)param;
#endif
}

void GameplayQueries::RecordDeveloperFrame(myEngine::DirectXCommon& dx)
{
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    if (world_.resources.neonSkinnedPreview_) {
        world_.resources.neonSkinnedPreview_->RecordShowcaseCapture(dx);
    }
    world_.bossPresentation->RecordNeonBossDeveloperFrame(dx);
    world_.gameplayScenarioRunner->RecordGameplayScenarioCapture(dx);
    if (world_.presentation.developerGameCapture_.HasRequest()) {
        world_.presentation.developerGameCapture_.SetFrameMetadata(MakeDeveloperGameCaptureMetadata(dx));
    }
    world_.presentation.developerGameCapture_.Record(dx);
#else
    (void)dx;
#endif
}

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
nlohmann::json GameplayQueries::MakeDeveloperGameCaptureMetadata(myEngine::DirectXCommon& dx) const
{
    auto vector = [](const myEngine::Vector3& value) {
        return nlohmann::json::array({value.x, value.y, value.z});
    };
    const auto& post = world_.presentation.developerCompositeParams_;
    const bool surfaceTrail = world_.run.prototypeRun_ && world_.presentation.stageSurface_.enabled;
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
         {{"trailLegacyScale", surfaceTrail ? 1.0f : 0.5f},
          {"trailQualityAndOffScale", 1.0f},
          {"sharedModelLegacyScale", 0.5f},
          {"sharedModelQualityAndOffScale", 1.0f},
          {"note",
           "Phase 1 uses full-resolution projectile cores with Scene depth in every Bloom mode; bodies stay outside emission captures."}}},
        {"sourceCounts",
         {{"playerBullets", counts.player},
          {"enemyBullets", counts.enemy},
          {"hostileExpBullets", counts.hostileExpEnemy},
          {"enemies", world_.resources.enemyManager_->GetEnemyCount()},
          {"particles", myEngine::ParticleManager::GetInstance()->GetActiveCount()},
          {"particlesCountIsCpuOnly", true},
          {"particlesGpuUpdate", myEngine::ParticleManager::GetInstance()->IsUseGpuUpdate()},
          {"particlesDrawReady", myEngine::ParticleManager::GetInstance()->HasDrawableParticles()},
          {"neonTriangles", world_.presentation.neonTriangleParticles_.size()}}},
        {"camera",
         {{"debug", myEngine::Object3dCommon::GetInstance()->GetIsDebugCamera()},
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
         {{"captureActive", myEngine::RuntimeProfiler::Get().IsCaptureActive()},
          {"captureComplete", myEngine::RuntimeProfiler::Get().IsCaptureComplete()},
          {"note", "Actual per-scope timestamps are in the separate profiler CSV, not inferred from this image."}}}};
    for (const auto& row : world_.resources.debugCamera->GetViewProjectionMatrix().m) {
        metadata["camera"]["debugViewProjection"].push_back({row[0], row[1], row[2], row[3]});
    }
    for (const auto& row : world_.resources.camera->GetViewProjectionMatrix().m) {
        metadata["camera"]["gameViewProjection"].push_back({row[0], row[1], row[2], row[3]});
    }
    for (const Bullet* bullet : world_.resources.bulletManager_->GetBulletPtrs()) {
        metadata["sourcePositions"]["bullets"].push_back(vector(bullet->GetWorldPosition()));
    }
    for (const ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
        metadata["sourcePositions"]["experienceEnemies"].push_back(vector(enemy->GetWorldPosition()));
    }
    UINT64 frequency = 0;
    if (SUCCEEDED(dx.GetQueue()->GetTimestampFrequency(&frequency))) {
        metadata["queueTimestampFrequencyHz"] = frequency;
    }
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> messages;
    if (SUCCEEDED(dx.GetDevice()->QueryInterface(IID_PPV_ARGS(&messages)))) {
        const UINT64 count = messages->GetNumStoredMessagesAllowedByRetrievalFilter();
        unsigned errors = 0, warnings = 0;
        const UINT64 inspected = (std::min)(count, UINT64{4096});
        for (UINT64 index = 0; index < inspected; ++index) {
            SIZE_T bytes = 0;
            if (FAILED(messages->GetMessage(index, nullptr, &bytes)) || !bytes) {
                continue;
            }
            std::vector<unsigned char> storage(bytes);
            auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
            if (FAILED(messages->GetMessage(index, message, &bytes))) {
                continue;
            }
            if (message->Severity == D3D12_MESSAGE_SEVERITY_ERROR || message->Severity == D3D12_MESSAGE_SEVERITY_CORRUPTION) {
                ++errors;
            }
            if (message->Severity == D3D12_MESSAGE_SEVERITY_WARNING) {
                ++warnings;
            }
            if ((message->Severity == D3D12_MESSAGE_SEVERITY_ERROR || message->Severity == D3D12_MESSAGE_SEVERITY_CORRUPTION) &&
                metadata["validation"]["firstErrors"].size() < 8) {
                metadata["validation"]["firstErrors"].push_back(
                    {{"id", static_cast<int>(message->ID)},
                     {"description", std::string(message->pDescription, (std::min)(message->DescriptionByteLength, SIZE_T{1200}))}});
            }
            if (message->Severity == D3D12_MESSAGE_SEVERITY_WARNING && metadata["validation"]["firstWarnings"].size() < 8) {
                metadata["validation"]["firstWarnings"].push_back(
                    {{"id", static_cast<int>(message->ID)},
                     {"description", std::string(message->pDescription, (std::min)(message->DescriptionByteLength, SIZE_T{1200}))}});
            }
        }
        metadata["validation"]["storedMessages"] = count;
        metadata["validation"]["inspectedMessages"] = inspected;
        metadata["validation"]["errors"] = errors;
        metadata["validation"]["warnings"] = warnings;
    }
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
myEngine::Object3d* GameplayQueries::GetBallObj()
{
    return world_.resources.ballObj_.get();
}

/// @brief 最終差分時間を返す。
float GameplayQueries::GetFinalDeltaTime() const
{
    // The boss defeat cinematic is a full-color finale, not the grayscale pause/slow-motion cue.
    if (world_.combat.combatFlow_.GetState() == GameFlowState::BossDefeatSequence && world_.resources.neonDepthCameraScoped_) {
        return 1.0f / 60.0f;
    }
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    if (world_.presentation.developerBloomFreeze_ || world_.resources.neonBossDeveloperFreeze_) {
        return 1.0f / 60.0f; // Freeze is a comparison, not the grayscale slow-motion effect.
    }
#endif
    if (IsNeonShowcaseActive()) {
        return 1.0f / 60.0f;
    }
    return world_.combat.finalDeltaTime;
}

/// @brief 後処理ガウシアン強度を返す。
float GameplayQueries::GetPostGaussianIntensity() const
{
    if (IsNeonShowcaseActive()) {
        return 0.0f;
    }
    return world_.presentation.sceneFadeBlurIntensity_;
}

/// @brief 後処理演出パルスを返す。
PostEffectPulse GameplayQueries::GetPostEffectPulse() const
{
    if (IsNeonShowcaseActive()) {
        return {};
    }
    return world_.presentation.deathPostPulse_;
}

bool GameplayQueries::IsNeonShowcaseActive() const
{
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
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
