#include "NeonBossVisual.h"
#include "DirectX/engine/3d/neon/NeonCharacterStyle.h"
#include "game/debug/NeonPreviewAnimations.h"
#include "RuntimeProfiler.h"
#include "StartupTrace.h"
#include "externals/nlohmann/json.hpp"
#include <fstream>
#include <limits>
#include <stdexcept>

namespace {
constexpr const char* kModelPath = "resources/models/neon_hologram/AvatarSample_B.glb";

// Read only the source alpha metadata, once. BLEND uses the existing Preview's
// alpha-cutout approximation; no alternate material/rendering path is introduced.
void ConfigureAlphaCutout(const cg2::SkinnedModel& model,
    std::vector<cg2::NeonSkinnedSubmeshParams>& surfaces) {
    std::ifstream stream(kModelPath, std::ios::binary);
    uint32_t header[5]{};
    if (!stream.read(reinterpret_cast<char*>(header), sizeof(header)) ||
        header[0] != 0x46546c67 || header[1] != 2 || header[4] != 0x4e4f534a || header[3] > 8 * 1024 * 1024)
        throw std::runtime_error("Neon boss GLB alpha metadata is invalid.");
    std::string text(header[3], '\0');
    if (!stream.read(text.data(), static_cast<std::streamsize>(text.size())))
        throw std::runtime_error("Neon boss GLB metadata is truncated.");
    const auto metadata = nlohmann::json::parse(text);
    for (size_t index = 0; index < surfaces.size(); ++index) {
        for (const auto& material : metadata.at("materials")) {
            if (material.value("name", "") != model.GetSubmesh(index).materialName) continue;
            const auto mode = material.value("alphaMode", "OPAQUE");
            surfaces[index].alphaCutoff = mode == "MASK" ? material.value("alphaCutoff", 0.5f)
                : mode == "BLEND" ? 0.03f : 0.0f;
            break;
        }
    }
}
}

NeonBossVisual::~NeonBossVisual() {
    // Scene destruction happens after its final submitted frame has completed.
    ReleaseResources();
}

void NeonBossVisual::Initialize(cg2::Camera* camera, cg2::DebugCamera* debugCamera) {
    camera_ = camera;
    debugCamera_ = debugCamera;
}

void NeonBossVisual::ResetEncounter() {
    ReleaseResources();
    state_.Reset();
    loadAttempted_ = encounterActive_ = false;
    lastAliveRadius_ = 2.0f;
    params_ = cg2::NeonSkinnedParams{};
    status_.clear();
}

bool NeonBossVisual::EnsureResources() {
    if (HasResources()) return true;
    if (loadAttempted_ || !camera_ || !debugCamera_) return false;
    loadAttempted_ = true;
    try {
        cg2::StartupTrace::Scope scope("NeonBossVisual.Load");
        auto* common = cg2::Object3dCommon::GetInstance();
        model_ = std::make_unique<cg2::SkinnedModel>();
        model_->Initialize(common->GetDxCommon(), common->GetSrvManager(), kModelPath);
        std::string animationError;
        if (!model_->RegisterAnimations(neonpreview::CreateAnimations(model_->GetSkeleton()), &animationError))
            throw std::runtime_error("Neon boss animation registration failed: " + animationError);

        float lowX = 0, highX = 0, lowY = 0, highY = 0, lowZ = 0, highZ = 0;
        if (!cg2::ComputeNeonSkinnedProjectionBounds(*model_, {1,0,0}, lowX, highX) ||
            !cg2::ComputeNeonSkinnedProjectionBounds(*model_, {0,1,0}, lowY, highY) ||
            !cg2::ComputeNeonSkinnedProjectionBounds(*model_, {0,0,1}, lowZ, highZ))
            throw std::runtime_error("Neon boss has invalid skinned model bounds.");
        modelCenter_ = {(lowX + highX) * 0.5f, (lowY + highY) * 0.5f, (lowZ + highZ) * 0.5f};
        modelHeight_ = (std::max)(0.01f, highY - lowY);

        std::vector<cg2::NeonSkinnedSubmeshParams> surfaces(model_->GetSubmeshCount());
        ConfigureAlphaCutout(*model_, surfaces);
        cg2::ApplyRecommendedNeonCharacterStyle(params_, model_.get(), surfaces);
        object_ = std::make_unique<cg2::Object3d>();
        object_->Initialize();
        object_->SetCamera(camera_);
        object_->SetDebugCamera(debugCamera_);
        renderer_ = std::make_unique<cg2::NeonSkinnedRenderer>();
        renderer_->Initialize(common->GetDxCommon(), common->GetSrvManager(), false);
        renderer_->SetSubmeshParams(surfaces);
        ++stats_.resourceCreates;
        status_ = "AvatarSample_B / Recommended Line Art / shared Preview_Idle + Preview_Attack";
        cg2::StartupTrace::Count("neon_boss.resource_creates");
        return true;
    } catch (const std::exception& error) {
        status_ = error.what();
        ReleaseResources();
        return false;
    }
}

void NeonBossVisual::ReleaseResources() {
    const bool hadResources = HasResources();
    // Free the two instance SRVs before dropping the resources. TextureManager
    // owns the cached embedded textures and shares them across later encounters.
    if (renderer_) renderer_->ReleaseGpuResources();
    if (model_) model_->ReleaseGpuResources();
    renderer_.reset();
    object_.reset();
    model_.reset();
    if (hadResources) {
        ++stats_.resourceReleases;
        cg2::StartupTrace::Count("neon_boss.resource_releases");
    }
}

void NeonBossVisual::UpdatePlacement(const NeonBossVisualInput& input) {
    if (!cg2::IsFiniteNeonPosition(input.position)) return;
    const float radius = std::isfinite(input.radius) ? (std::clamp)(input.radius, 0.5f, 10.0f) : 2.0f;
    const float scale = radius * 2.2f / modelHeight_;
    const float aim = std::isfinite(input.aimAngleRadians) ? input.aimAngleRadians : 0.0f;
    // Game uses the XY plane, viewed from negative Z. Center the actual model
    // bounds over the authoritative collider, point its head toward the aim,
    // and pitch just its presentation to reveal volume under the existing camera.
    const auto centered = cg2::MakeTranslateMatrix({-modelCenter_.x, -modelCenter_.y, -modelCenter_.z});
    const auto placed = cg2::MakeAffineMatrix(cg2::Vector3{scale, scale, scale},
        cg2::Vector3{0.35f, 0.0f, aim - cg2::pi * 0.5f},
        cg2::Vector3{input.position.x, input.position.y, input.position.z - 0.45f});
    world_ = cg2::Multiply(centered, placed);
}

cg2::Matrix4x4 NeonBossVisual::GetCameraWorld() const {
    return cg2::Object3dCommon::GetInstance()->GetIsDebugCamera()
        ? cg2::Inverse(debugCamera_->GetViewMatrix()) : camera_->GetWorldMatrix();
}

cg2::Vector3 NeonBossVisual::GetCameraPosition() const {
    return cg2::Object3dCommon::GetInstance()->GetIsDebugCamera()
        ? debugCamera_->GetEyePosition() : camera_->GetTranslate();
}

void NeonBossVisual::StartDeath() {
    model_->SetAnimationPlaying(false); // No death clip exists: retain the exact current skinned pose.
    auto dissolve = params_.dissolve;
    std::string error;
    if (!cg2::MakeNeonDissolveDirection(world_, GetCameraWorld(),
        cg2::NeonDissolveDirection::UpperLeftToLowerRight, dissolve.direction, &error) ||
        !cg2::ComputeNeonSkinnedProjectionBounds(*model_, dissolve.direction, dissolve.scanMin, dissolve.scanMax)) {
        // A finite model-axis scan remains deterministic if a debug camera is invalid.
        dissolve.direction = {1,0,0};
        if (!cg2::ComputeNeonSkinnedProjectionBounds(*model_, dissolve.direction, dissolve.scanMin, dissolve.scanMax)) {
            status_ = "Neon boss dissolve bounds invalid; death will hide and release the visual at its terminal time.";
            dissolve.enabled = 0;
            params_.dissolve = dissolve;
            return;
        }
    }
    dissolve.enabled = 1;
    dissolve.progress = 0.0f;
    dissolve.edgeColor = params_.emissiveColor;
    params_.dissolve = cg2::SanitizeNeonDissolveParams(dissolve);
    cg2::StartupTrace::Count("neon_boss.deaths");
}

void NeonBossVisual::UpdateAnimation(float deltaTime) {
    const auto& mapping = state_.GetMapping();
    if (state_.DidAnimationChange()) {
        const char* name = mapping.motion == NeonBossMotion::Attack ? "Preview_Attack" : "Preview_Idle";
        // Entering a different clip uses the engine's existing pose blend exactly
        // once. Sustained gameplay input cannot reset this animation every frame.
        if (model_->TransitionToAnimation(name, 0.2f)) ++stats_.animationSelections;
    }
    if (state_.DidActionChange()) {
        model_->SetAnimationLoop(mapping.loop);
        model_->SetAnimationPlaybackSpeed(mapping.speed);
        model_->SetAnimationPlaying(true);
        const float entryPose = NeonBossEntryPoseTime(state_.GetAction(), true, model_->GetCurrentAnimationTime());
        if (entryPose >= 0.0f) {
            model_->SeekCurrentAnimation(entryPose);
            model_->SetAnimationPlaying(false);
        }
    }
    model_->Update(std::isfinite(deltaTime) && deltaTime > 0 ? deltaTime : 0.0f);
    if (state_.GetAction() == NeonBossAction::Telegraph && model_->GetCurrentAnimationTime() >= 0.50f)
        model_->SetAnimationPlaying(false); // Existing Attack pull pose; Attack resumes from here.
    ++stats_.modelUpdates;
}

void NeonBossVisual::Update(const NeonBossVisualInput& input, float deltaTime) {
    encounterActive_ = input.encounterActive;
    state_.Update(input.encounterActive, input.alive, input.hp, input.maxHp, input.action, deltaTime);
    if (state_.GetLife() == NeonBossVisualLife::Dormant || state_.GetLife() == NeonBossVisualLife::Finished) {
        if (state_.ShouldReleaseResources()) ReleaseResources();
        return;
    }
    const bool hadResources = HasResources();
    if (!EnsureResources()) return;
    if (!hadResources) UpdatePlacement(input);
    renderer_->BeginFrame();
    if (state_.GetLife() == NeonBossVisualLife::Alive && input.encounterActive) {
        if (std::isfinite(input.radius) && input.radius > 0.0f) lastAliveRadius_ = input.radius;
        UpdatePlacement(input);
        UpdateAnimation(deltaTime);
        const float danger = 1.0f - state_.GetHpRatio();
        const float damage = std::isfinite(input.damageFeedback) ? (std::clamp)(input.damageFeedback, 0.0f, 1.0f) : 0;
        // HP/phase/damage change the local material only; the normal Scene Bloom
        // and the underlying Enemy HP, phases and timing remain authoritative.
        params_.emissiveColor = {1.0f, 0.025f + danger * 0.18f + (input.phaseTwo ? 0.06f : 0.0f),
            0.35f * (1.0f - danger * 0.7f)};
        params_.bodyColor = {0.018f + damage * 0.10f, 0.0025f + damage * 0.015f, 0.012f, 1.0f};
    } else if (state_.DidDeathStart()) {
        // Lethal collision can happen after movement this frame. Use that final
        // authoritative position/aim, retaining the alive scale after Die zeros
        // the collision radius. Only then freeze World for dissolution.
        auto deathInput = input;
        deathInput.radius = lastAliveRadius_;
        UpdatePlacement(deathInput);
        StartDeath();
    }
    params_.dissolve.progress = state_.GetDissolveProgress();
    object_->UpdateWithWorldMatrix(world_);
    renderer_->SetParams(params_);
}

bool NeonBossVisual::IsVisible() const {
    return enabled_ && HasResources() && (state_.GetLife() == NeonBossVisualLife::Dying ||
        (state_.GetLife() == NeonBossVisualLife::Alive && encounterActive_));
}

void NeonBossVisual::Draw() {
    if (!IsVisible()) return;
    cg2::RuntimeProfiler::GpuScope scope("Neon Boss");
    renderer_->Draw(*model_, object_->GetTransformationResource()->GetGPUVirtualAddress(), GetCameraPosition());
    cg2::Object3dCommon::GetInstance()->PreDraw(cg2::kNormal);
    ++stats_.draws;
}

std::string NeonBossVisual::GetAnimationName() const {
    return model_ ? model_->GetAnimation().name : "Released";
}

float NeonBossVisual::GetAnimationTime() const {
    return model_ ? model_->GetCurrentAnimationTime() : 0.0f;
}
