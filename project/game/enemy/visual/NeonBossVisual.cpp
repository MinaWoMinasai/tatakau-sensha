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
    if (depthProfilePending_) {
        depthProfile_ = pendingDepthProfile_;
        depthProfilePending_ = false;
    }
    depthMode_ = depthModeLatched_ = false;
    depthPlacementValid_ = depthPoseFrozen_ = depthFormationBoundsReady_ = depthBodySuppressed_ = false;
    depthProfileInstance_ = 0;
    depthBind_ = {};
    depthPlacement_ = deathDepthPlacement_ = {};
    depthMotionGate_.Reset();
    depthFormation_ = {0, 0, 0};
    depthFormationDissolve_ = {};
}

bool NeonBossVisual::SetDepthProfile(const neondepth::PresentationConfig& profile) {
    if (!neondepth::Valid(profile)) return false;
    if (state_.GetLife() == NeonBossVisualLife::Dormant && !HasResources()) {
        depthProfile_ = profile;
        depthProfilePending_ = false;
    } else {
        pendingDepthProfile_ = profile;
        depthProfilePending_ = true;
    }
    return true;
}

bool NeonBossVisual::GetDepthSocketWorld(neondepth::Socket socket, cg2::Vector3& output) const {
    return depthMode_ && HasResources() && depthPlacementValid_ &&
        neondepth::TrySocketWorld(model_->GetSkeleton(), world_, socket, output);
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

bool NeonBossVisual::EnsureDepthResources() {
    if (HasResources()) return true;
    if (loadAttempted_ || !camera_ || !debugCamera_) return false;
    loadAttempted_ = true;
    try {
        cg2::StartupTrace::Scope scope("NeonBossVisual.DepthLoad");
        auto* common = cg2::Object3dCommon::GetInstance();
        model_ = std::make_unique<cg2::SkinnedModel>();
        model_->Initialize(common->GetDxCommon(), common->GetSrvManager(), kModelPath);
        std::string error;
        if (!neondepth::ValidateDepthBindSkeleton(model_->GetSkeleton(), &error))
            throw std::runtime_error(error);
        neondepth::BindBounds bounds;
        if (!cg2::ComputeNeonSkinnedProjectionBounds(*model_, {1,0,0}, bounds.low.x, bounds.high.x) ||
            !cg2::ComputeNeonSkinnedProjectionBounds(*model_, {0,1,0}, bounds.low.y, bounds.high.y) ||
            !cg2::ComputeNeonSkinnedProjectionBounds(*model_, {0,0,1}, bounds.low.z, bounds.high.z) ||
            !neondepth::TryBindPlacement(model_->GetSkeleton(), bounds, depthBind_))
            throw std::runtime_error("Depth boss bind bounds/foot anchor are invalid.");
        auto motion = neondepth::CreateDepthAnimations(model_->GetSkeleton());
        if (!motion.safeToRegister) throw std::runtime_error(motion.bindError);
        const size_t missing = motion.skippedMotionJoints.size();
        const bool rootOnly = motion.rootOnlyFallback;
        // This branch intentionally bypasses the Preview builder's required-bone
        // check: valid bind skeletons can safely use partial/empty Depth clips.
        if (!model_->RegisterAnimations(std::move(motion.animations), &error))
            throw std::runtime_error("Depth boss animation registration failed: " + error);
        modelHeight_ = depthBind_.boundsHeight;
        modelCenter_ = {(bounds.low.x + bounds.high.x) * .5f, (bounds.low.y + bounds.high.y) * .5f,
            (bounds.low.z + bounds.high.z) * .5f};
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
        if (!depthPlacementValid_) world_ = cg2::MakeIdentity4x4();
        ++stats_.resourceCreates;
        if (missing || depthBind_.footBonesFound != 2) ++stats_.depthFallbackLoads;
        status_ = "AvatarSample_B / Depth generated motions / stable bind foot";
        if (missing) status_ += rootOnly ? " / valid-bind root fallback" : " / partial motion joints: " + std::to_string(missing);
        if (depthBind_.footBonesFound != 2) status_ += " / bounds sole fallback";
        cg2::StartupTrace::Count("neon_boss.resource_creates");
        return true;
    } catch (const std::exception& error) {
        status_ = std::string(error.what()).substr(0, 256);
        depthBodySuppressed_ = true;
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
    // Presentation mode is fixed for an encounter. Caller Reset owns generation
    // changes and resource release after the previous GPU frame has completed.
    if (!depthModeLatched_ && input.encounterActive) {
        depthMode_ = input.depthEncounter;
        depthModeLatched_ = true;
    }
    if (depthMode_) {
        UpdateDepth(input, deltaTime);
        return;
    }
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
    if (depthMode_ && (depthBodySuppressed_ || !depthPlacementValid_ || depthFormation_.body <= 0)) return false;
    return enabled_ && HasResources() && (state_.GetLife() == NeonBossVisualLife::Dying ||
        (state_.GetLife() == NeonBossVisualLife::Alive && encounterActive_));
}

bool NeonBossVisual::ApplyDepthMotion(const neondepth::MotionCommand& command) {
    if (command.select) {
        const char* name = neondepth::kMotionNames[static_cast<size_t>(command.mapping.motion)];
        const bool selected = command.restartSameClip ? model_->SetAnimation(name, true)
            : model_->TransitionToAnimation(name, .12f);
        if (!selected) {
            status_ = "Depth motion missing; preserving the last valid pose.";
            model_->SetAnimationPlaying(false);
            return false;
        }
        ++stats_.animationSelections;
    }
    if (command.phaseChanged) {
        model_->SetAnimationLoop(command.mapping.loop);
        model_->SetAnimationPlaybackSpeed(command.mapping.speed);
        model_->SetAnimationPlaying(command.mapping.playing);
        if (command.mapping.entryTime >= 0) model_->SeekCurrentAnimation(command.mapping.entryTime);
    }
    return true;
}

bool NeonBossVisual::UpdateDepthPlacement(const NeonBossVisualInput& input, const neondepth::BodyMotion& motion) {
    if (!cg2::IsFiniteNeonPosition(input.position)) return false;
    neondepth::PlacementInput placed;
    placed.floorAnchor = {input.position.x, input.position.y, 0};
    placed.floorOffset = {depthProfile_.floorOffset.x + motion.extraFloorOffset.x,
        depthProfile_.floorOffset.y + motion.extraFloorOffset.y};
    placed.hoverHeight = depthProfile_.hoverHeight + motion.extraHoverHeight;
    placed.bodyHeading = std::isfinite(input.aimAngleRadians) ? input.aimAngleRadians : depthPlacement_.bodyHeading;
    placed.localLean = motion.localLean;
    neondepth::PlacementResult result;
    if (!neondepth::TryPlacement({}, neondepth::MakePlacementConfig(depthBind_, depthProfile_), placed, result)) return false;
    depthPlacement_ = placed;
    world_ = result.world;
    depthPlacementValid_ = true;
    return true;
}

void NeonBossVisual::StartDepthCollapse(const NeonBossVisualInput& input) {
    deathDepthProfile_ = depthProfile_;
    if (!depthPlacementValid_ && !UpdateDepthPlacement(input, {})) depthBodySuppressed_ = true;
    deathDepthPlacement_ = depthPlacement_;
    if (cg2::IsFiniteNeonPosition(input.position))
        deathDepthPlacement_.floorAnchor = {input.position.x, input.position.y, 0};
    // Alive formation is a separate reveal. HP0 first restores the whole current
    // body; the terminal death scan is configured only after actual collapse.
    params_.dissolve.enabled = 0;
    depthFormationBoundsReady_ = false;
    ApplyDepthMotion(depthMotionGate_.Step(input.depth, true, false,
        state_.GetDepthLifecycle().GetLatchedTiming().collapseSeconds));
    cg2::StartupTrace::Count("neon_boss.depth_collapses");
}

void NeonBossVisual::UpdateDepthCollapse(float collapseProgress) {
    auto placed = deathDepthPlacement_;
    const float p = neondepth::Smooth(collapseProgress);
    const float amplitude = deathDepthProfile_.motionAmplitude * (deathDepthProfile_.reducedMotion ? .35f : 1.0f);
    const float finalHover = (std::min)(deathDepthPlacement_.hoverHeight, .8f);
    placed.hoverHeight = deathDepthPlacement_.hoverHeight * (1 - p) + finalHover * p;
    placed.localLean.x = deathDepthPlacement_.localLean.x * (1 - p) + amplitude * .12f * p;
    placed.localLean.y = deathDepthPlacement_.localLean.y * (1 - p);
    placed.localLean.z = deathDepthPlacement_.localLean.z * (1 - p);
    neondepth::PlacementResult result;
    if (neondepth::TryPlacement({}, neondepth::MakePlacementConfig(depthBind_, deathDepthProfile_), placed, result)) {
        world_ = result.world;
        depthPlacementValid_ = true;
    }
}

void NeonBossVisual::UpdateDepthFormation() {
    if (state_.GetDepthLifecycle().GetLife() != neondepth::Life::Intro) {
        params_.dissolve.enabled = 0;
        return;
    }
    if (depthFormation_.body <= 0) return;
    if (!depthFormationBoundsReady_) {
        auto formation = params_.dissolve;
        formation.direction = {0, -1, 0};
        if (!cg2::ComputeNeonSkinnedProjectionBounds(*model_, formation.direction, formation.scanMin, formation.scanMax)) {
            status_ = "Depth intro bounds invalid; body suppressed for this encounter.";
            depthBodySuppressed_ = true;
            return;
        }
        formation.enabled = 1;
        formation.progress = 1;
        formation.edgeColor = params_.emissiveColor;
        depthFormationDissolve_ = cg2::SanitizeNeonDissolveParams(formation);
        depthFormationBoundsReady_ = true;
        ++stats_.depthFormationBounds;
    }
    params_.dissolve = depthFormationDissolve_;
    params_.dissolve.progress = 1 - depthFormation_.body;
}

void NeonBossVisual::UpdateDepth(const NeonBossVisualInput& input, float permittedDelta) {
    encounterActive_ = input.encounterActive;
    const bool alive = input.alive && input.hp > 0;
    if (alive && input.depth.plan.instance != depthProfileInstance_) {
        if (depthProfilePending_) {
            depthProfile_ = pendingDepthProfile_;
            depthProfilePending_ = false;
        }
        depthProfileInstance_ = input.depth.plan.instance;
    }
    neondepth::LifecycleInput lifeInput;
    lifeInput.encounterActive = input.encounterActive;
    lifeInput.alive = alive;
    lifeInput.hp = input.hp; lifeInput.maxHp = input.maxHp;
    lifeInput.intro = input.depth.phase == neondepth::Phase::Intro;
    lifeInput.aborted = input.depth.phase == neondepth::Phase::Aborted;
    lifeInput.introProgress = input.depth.progress;
    state_.UpdateDepth(lifeInput, {depthProfile_.collapseSeconds, depthProfile_.dissolveSeconds}, permittedDelta);
    const auto& lifecycle = state_.GetDepthLifecycle();
    depthFormation_ = neondepth::MapFormation(input.depth);
    if (state_.GetLife() == NeonBossVisualLife::Dying) {
        const float remaining = 1 - state_.GetDissolveProgress();
        depthFormation_ = {remaining, remaining, remaining};
    }
    if (state_.GetLife() == NeonBossVisualLife::Dormant || state_.GetLife() == NeonBossVisualLife::Finished) {
        depthFormation_ = {0, 0, 0};
        if (state_.ShouldReleaseResources()) ReleaseResources();
        return;
    }
    if (!EnsureDepthResources()) return;
    renderer_->BeginFrame();
    if (state_.GetLife() == NeonBossVisualLife::Alive && input.encounterActive) {
        const auto command = depthMotionGate_.Step(input.depth);
        ApplyDepthMotion(command);
        // Combat does one phase transition and discards spike remainder. New
        // phase entry with elapsed0 also samples at0 rather than advancing early.
        const float clock = command.phaseChanged && input.depth.elapsed <= 0 ? 0 : neondepth::PresentationDelta(permittedDelta);
        model_->Update(clock);
        ++stats_.modelUpdates;
        if (!UpdateDepthPlacement(input, neondepth::MapBodyMotion(input.depth, depthProfile_, input.damageFeedback)))
            status_ = "Depth placement invalid; preserving the last valid world matrix.";
        const float danger = 1 - state_.GetHpRatio();
        const float damage = neondepth::Unit(input.damageFeedback) * (depthProfile_.reducedMotion ? .4f : 1.0f);
        params_.emissiveColor = {1, .025f + danger * .18f + (input.phaseTwo ? .06f : 0), .35f * (1 - danger * .7f)};
        params_.bodyColor = {.018f + damage * .10f, .0025f + damage * .015f, .012f, 1};
        UpdateDepthFormation();
    } else if (state_.GetLife() == NeonBossVisualLife::Dying) {
        if (state_.DidDeathStart()) StartDepthCollapse(input);
        if (!depthPoseFrozen_) {
            UpdateDepthCollapse(lifecycle.GetCollapseProgress());
            model_->Update(lifecycle.GetCollapseDelta());
            ++stats_.modelUpdates;
            if (lifecycle.ShouldFreezePose()) {
                model_->SetAnimationPlaying(false);
                depthPoseFrozen_ = true;
                ++stats_.depthPoseFreezes;
                StartDeath(); // Scan this exact frozen pose/palette/world once.
                if (params_.dissolve.enabled == 0) depthBodySuppressed_ = true;
            }
        }
        params_.dissolve.progress = state_.GetDissolveProgress();
    }
    object_->UpdateWithWorldMatrix(world_);
    renderer_->SetParams(params_);
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
