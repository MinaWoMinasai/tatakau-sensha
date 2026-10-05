#pragma once
#include "DeveloperTools.h"

// Releaseでは明示的なDeveloperTools overrideがあってもPreviewをコンパイルしない。
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Object3d.h"
#include "SkinCluster.h"
#include "DirectX/engine/3d/neon/NeonSkinnedRenderer.h"
#include "NeonShowcaseCapture.h"
#include "NeonDissolvePreviewController.h"
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// GameSceneのDeveloper UIからのみ使用。初期状態は無効、GPU資源は有効化時に作成する。
/// @brief 開発画面でスキニングモデルとネオン材質を表示・調整する。
class NeonSkinnedPreview {
public:
    ~NeonSkinnedPreview();
    struct ResourceLifecycleValidation {
        bool passed = false;
        uint32_t initialDescriptors = 0;
        uint32_t sharedCacheBaseline = 0;
        uint32_t completedRepeats = 0;
        uint32_t maxDescriptorsWhileLoaded = 0;
        uint32_t descriptorsAfterTeardown = 0;
        std::string error;
    };
    // Developer scenario adapter only. Call after the last submitted frame's
    // fence completes while cameras and engine managers remain alive. Warms
    // shared textures, then checks actual D3D instance descriptor ownership.
    // No draw commands, gameplay state or ordinary Preview state are changed.
    static ResourceLifecycleValidation ValidateResourceLifecycle(
        cg2::Camera* camera, cg2::DebugCamera* debugCamera, unsigned repeats = 3);
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(cg2::Camera* camera, cg2::DebugCamera* debugCamera);
    // 前フレームのFence完了後、Camera更新後に1フレーム1回呼ぶ。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Update(float deltaTime);
    // Bloom::PreDraw直後のScene HDR / Normal / Material + D24S8内でのみ呼ぶ。
    void Draw();
    /// @brief ImGUIを描画する。
    void DrawImGui();
    bool IsShowcaseActive() const { return showcaseActive_; }
    cg2::Camera* GetShowcaseCamera() { return showcaseActive_ ? &showcaseCamera_ : nullptr; }
    int GetShowcaseDiagnostic() const { return showcaseDiagnostic_; }
    float GetShowcaseBloomThreshold() const { return showcaseBloomThreshold_; }
    float GetShowcaseBloomIntensity() const { return showcaseBloomIntensity_; }
    float GetShowcaseExposure() const { return showcaseExposure_; }
    int GetShowcaseBloomMode() const { return showcaseBloomMode_; }
    float GetShowcaseBloomSoftKnee() const { return showcaseBloomSoftKnee_; }
    float GetShowcaseBloomScatter() const { return showcaseBloomScatter_; }
    float GetShowcaseBloomRadius() const { return showcaseBloomRadius_; }
    float GetShowcaseBloomGain() const { return showcaseBloomGain_; }
    int GetShowcaseToneMappingMode() const { return showcaseToneMappingMode_; }
    void RecordShowcaseCapture(cg2::DirectXCommon& dx);
    void DrawShowcaseWindow();

private:
    enum class GeometryPreset { Off, Subtle, FullMeshDiagnostic, Custom };
    /// @brief プレビュー対象の元のマテリアル値を保持する。
    struct SourceMaterial {
        std::string meshName;
        std::string materialName;
        std::string alphaMode;
        float alphaCutoff = 0.0f;
    };
    struct FeatureMaskBinding {
        std::string path;
        std::string distancePath;
        std::string status;
        std::string coverageSha256, distanceSha256, authoringSha256, authoringVersionSha256, authoringRevision;
        std::optional<uint32_t> srvIndex;
    };
    /// @brief 保存されたデータを読み込む。
    void Load();
    // The owner releases instance descriptors at a GPU-complete boundary.
    // Shared TextureManager entries remain cached across Preview encounters.
    void ReleaseResources();
    // 外観だけを変更し、再生状態・Transform・Alpha Cutoutは保持する。
    void ApplyRecommendedLineArtPreset();
    void ApplyRecommendedBloomPresentation();
    void ApplyLegacyNeonPreset();
    // Geometry設定だけを変更する。モデル・再生状態・既存の線設定は維持する。
    void ApplyGeometryPreset(GeometryPreset preset);
    void DrawGeometryLinesImGui();
    void LoadFeatureMaskCandidates();
    void DrawFeatureMaskImGui();
    void SelectQualityCandidate(int candidate);
    void DrawQualityCandidateImGui();
    void EnterShowcase();
    void LeaveShowcase();
    void UpdateShowcaseCamera(float deltaTime);
    void DrawShowcaseImGui();
    void DrawAnimationImGui();
    void StartShowcaseComparison();
    void AdvanceShowcaseComparison();
    void ApplyShowcaseComparisonCase();
    void FinishShowcaseComparison(const std::string& status);
    void StartBloomComparison();
    void AdvanceBloomComparison();
    void FinishBloomComparison(const std::string& status);
    bool TriggerDissolve();
    void ResetDissolve();
    void DrawDissolveImGui();
    void StartDissolveComparison();
    void AdvanceDissolveComparison();
    void ApplyDissolveComparisonCase();
    void FinishDissolveComparison(const std::string& status);
    void StartDissolveSequence();
    void UpdateDissolveSequence(bool afterPoseUpdate);
    void FinishDissolveSequence();
    nlohmann::json MakeShowcaseMetadata() const;
    /// @brief プレビュー対象をカメラの前へ配置する。
    void PlaceInFrontOfCamera();
    /// @brief カメラ位置を返す。
    cg2::Vector3 GetCameraPosition() const;

    cg2::Camera* camera_ = nullptr;
    cg2::DebugCamera* debugCamera_ = nullptr;
    bool enabled_ = false;
    bool neonMode_ = true;
    bool ready_ = false;
    bool alphaCutoutEnabled_ = true;
    GeometryPreset geometryPreset_ = GeometryPreset::Off;
    cg2::Transform transform_{{8.0f, 8.0f, 8.0f}, {}, {}}; // 正面から顔の内部線を比較する。
    cg2::NeonSkinnedParams params_;
    std::unique_ptr<cg2::SkinnedModel> model_;
    std::unique_ptr<cg2::Object3d> object_;
    cg2::NeonSkinnedRenderer renderer_;
    std::vector<SourceMaterial> sourceMaterials_;
    std::vector<cg2::NeonSkinnedSubmeshParams> submeshParams_;
    std::vector<FeatureMaskBinding> featureMaskBindings_;
    std::vector<std::optional<uint32_t>> featureMaskIndices_;
    std::vector<std::optional<uint32_t>> featureDistanceIndices_;
    std::vector<FeatureMaskBinding> originalMaskBindings_;
    std::vector<std::optional<uint32_t>> originalMaskIndices_;
    struct QualityTextureProvenance {
        uint32_t srvIndex = 0;
        std::string imageSha256, authoringSha256, authoringVersionSha256, authoringRevision;
    };
    std::unordered_map<std::string,QualityTextureProvenance> qualityTextureProvenance_;
    int qualityCandidate_ = 0;
    std::string qualityError_;
    bool featureMasksLoadAttempted_ = false;
    std::string featureMaskError_;
    size_t sourceAnimationCount_ = 0;
    size_t generatedAnimationCount_ = 0;
    std::string animationError_;
    std::string loadError_;
    struct PreviewCheckpoint {
        bool enabled = false, neonMode = true, alphaCutout = true;
        GeometryPreset geometryPreset = GeometryPreset::Off;
        cg2::Transform transform{};
        cg2::NeonSkinnedParams params{};
        std::vector<cg2::NeonSkinnedSubmeshParams> surfaces;
        std::vector<std::optional<uint32_t>> masks;
        std::vector<std::optional<uint32_t>> distances;
        std::vector<FeatureMaskBinding> bindings;
        int qualityCandidate = 0;
        bool masksLoadAttempted = false;
        std::string maskError, qualityError;
        bool usingDebugCamera = false;
        cg2::Vector3 gameCameraPosition{}, gameCameraRotation{};
        cg2::SkinnedModel::AnimationPlaybackState playback;
    } checkpoint_;
    PreviewCheckpoint comparisonCheckpoint_;
    bool showcaseComparisonActive_ = false, showcaseComparisonFrameRecorded_ = false;
    unsigned showcaseComparisonIndex_ = 0;
    std::string showcaseComparisonDirectory_, showcaseComparisonStatus_;
    bool showcaseActive_ = false, showcaseOrbit_ = false;
    cg2::Camera showcaseCamera_;
    cg2::Transform showcaseTransform_{{1,1,1},{},{}};
    int showcaseFraming_ = 0, showcaseView_ = 0, showcaseDiagnostic_ = 0;
    float showcaseYaw_ = 0.0f, showcaseOrbitSpeed_ = 0.35f;
    float showcaseBloomThreshold_ = 0.65f, showcaseBloomIntensity_ = 0.75f, showcaseExposure_ = 0.75f;
    int showcaseBloomMode_ = 2, showcaseToneMappingMode_ = 1;
    float showcaseBloomSoftKnee_ = 0.5f, showcaseBloomScatter_ = 0.55f, showcaseBloomRadius_ = 1.0f, showcaseBloomGain_ = 0.8f;
    bool bloomComparisonActive_ = false, bloomComparisonFrameRecorded_ = false;
    unsigned bloomComparisonIndex_ = 0;
    int bloomComparisonOriginalMode_ = 2, bloomComparisonOriginalDiagnostic_ = 0;
    std::string bloomComparisonDirectory_, bloomComparisonStatus_;
    NeonShowcaseCapture showcaseCapture_;
    unsigned showcaseCaptureNumber_ = 0, showcaseSequenceFrame_ = 0;
    bool showcaseSequence_ = false, showcaseSequenceAttackStarted_ = false;
    char showcaseCaptureLabel_[64] = "baseline";
    std::string showcaseCaptureDirectory_;
    std::string showcaseSequenceDirectory_;
    std::string showcaseTimingStatus_;
    NeonDissolvePreviewController dissolve_;
    cg2::NeonDissolveParams dissolveSettings_{};
    int dissolveDirection_ = 0;
    float dissolveWait_ = 0.15f, dissolveDuration_ = 2.0f;
    bool restorePoseThisFrame_ = false;
    bool dissolveComparisonActive_ = false, dissolveComparisonFrameRecorded_ = false;
    unsigned dissolveComparisonIndex_ = 0;
    cg2::NeonDissolveParams dissolveComparisonParams_{};
    NeonDissolvePreviewController dissolveComparisonController_;
    cg2::SkinnedModel::AnimationPlaybackState dissolveComparisonPlayback_;
    cg2::NeonDissolveParams dissolveComparisonOriginalParams_{};
    std::string dissolveComparisonDirectory_, dissolveComparisonStatus_;
    bool dissolveSequence_ = false;
    unsigned dissolveSequenceLastEventFrame_ = UINT32_MAX;
    cg2::SkinnedModel::AnimationPlaybackState dissolveSequencePlayback_;
    bool dissolveSequenceOriginalOrbit_ = false;
    float dissolveSequenceOriginalYaw_ = 0.0f;
};
#endif
