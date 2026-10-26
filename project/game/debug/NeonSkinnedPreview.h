#pragma once
#include "DeveloperTools.h"

// Releaseでは明示的なDeveloperTools overrideがあってもPreviewをコンパイルしない。
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Object3d.h"
#include "SkinCluster.h"
#include "game/render/neon/NeonSkinnedRenderer.h"
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

    /// @brief インスタンス用ディスクリプタの確保と解放を検証した結果。
    struct ResourceLifecycleValidation {
        bool passed = false;
        uint32_t initialDescriptors = 0;
        uint32_t sharedCacheBaseline = 0;
        uint32_t completedRepeats = 0;
        uint32_t maxDescriptorsWhileLoaded = 0;
        uint32_t descriptorsAfterTeardown = 0;
        std::string error;
    };

    // Developer用シナリオアダプタ専用。最後に送信したフレームのフェンスが完了し、
    // カメラとエンジンのマネージャーが生きている間に呼ぶ。共有テクスチャを先に読み込んでから、
    // D3Dのインスタンス用ディスクリプタの実際の所有状況を確認する。
    // 描画コマンド、ゲームプレイの状態、通常のプレビュー状態は変更しない。
    static ResourceLifecycleValidation ValidateResourceLifecycle(myEngine::Camera* camera, myEngine::DebugCamera* debugCamera,
                                                                 unsigned repeats = 3);
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(myEngine::Camera* camera, myEngine::DebugCamera* debugCamera);
    // 前フレームのFence完了後、Camera更新後に1フレーム1回呼ぶ。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Update(float deltaTime);
    // Bloom::PreDraw直後のScene HDR / Normal / Material + D24S8内でのみ呼ぶ。
    void Draw();
    /// @brief ImGUIを描画する。
    void DrawImGui();

    bool IsShowcaseActive() const
    {
        return showcaseActive_;
    }

    myEngine::Camera* GetShowcaseCamera()
    {
        return showcaseActive_ ? &showcaseCamera_ : nullptr;
    }

    int GetShowcaseDiagnostic() const
    {
        return showcaseDiagnostic_;
    }

    float GetShowcaseBloomThreshold() const
    {
        return showcaseBloomThreshold_;
    }

    float GetShowcaseBloomIntensity() const
    {
        return showcaseBloomIntensity_;
    }

    float GetShowcaseExposure() const
    {
        return showcaseExposure_;
    }

    int GetShowcaseBloomMode() const
    {
        return showcaseBloomMode_;
    }

    float GetShowcaseBloomSoftKnee() const
    {
        return showcaseBloomSoftKnee_;
    }

    float GetShowcaseBloomScatter() const
    {
        return showcaseBloomScatter_;
    }

    float GetShowcaseBloomRadius() const
    {
        return showcaseBloomRadius_;
    }

    float GetShowcaseBloomGain() const
    {
        return showcaseBloomGain_;
    }

    int GetShowcaseToneMappingMode() const
    {
        return showcaseToneMappingMode_;
    }

    /// @brief ショーケース表示中のフレームをキャプチャする。
    void RecordShowcaseCapture(myEngine::DirectXCommon& dx);
    /// @brief ショーケース用の操作ウィンドウを描画する。
    void DrawShowcaseWindow();

private:
    /// @brief ジオメトリ線の設定プリセット。
    enum class GeometryPreset {
        Off,
        Subtle,
        FullMeshDiagnostic,
        Custom
    };

    /// @brief プレビュー対象の元のマテリアル値を保持する。
    struct SourceMaterial {
        std::string meshName;
        std::string materialName;
        std::string alphaMode;
        float alphaCutoff = 0.0f;
    };

    /// @brief 特徴線マスクの候補テクスチャと、その読み込み状態。
    struct FeatureMaskBinding {
        std::string path;
        std::string distancePath;
        std::string status;
        std::string coverageSha256, distanceSha256, authoringSha256, authoringVersionSha256, authoringRevision;
        std::optional<uint32_t> srvIndex;
    };

    /// @brief 保存されたデータを読み込む。
    void Load();
    // 所有者は、GPUの処理が完了した境界でインスタンス用ディスクリプタを解放する。
    // TextureManagerの共有エントリは、プレビューのたびにキャッシュされたまま残る。
    void ReleaseResources();
    // 外観だけを変更し、再生状態・Transform・Alpha Cutoutは保持する。
    void ApplyRecommendedLineArtPreset();
    /// @brief 推奨のBloom表示設定を適用する。
    void ApplyRecommendedBloomPresentation();
    /// @brief 旧来のネオン表示設定を適用する。
    void ApplyLegacyNeonPreset();
    // Geometry設定だけを変更する。モデル・再生状態・既存の線設定は維持する。
    void ApplyGeometryPreset(GeometryPreset preset);
    /// @brief ジオメトリ線の設定UIを描画する。
    void DrawGeometryLinesImGui();
    /// @brief 特徴線マスクの候補テクスチャを読み込む。
    void LoadFeatureMaskCandidates();
    /// @brief 特徴線マスクの設定UIを描画する。
    void DrawFeatureMaskImGui();
    /// @brief 品質比較用の候補を選ぶ。
    void SelectQualityCandidate(int candidate);
    /// @brief 品質比較用の候補を選ぶUIを描画する。
    void DrawQualityCandidateImGui();
    /// @brief ショーケース表示に切り替える。
    void EnterShowcase();
    /// @brief ショーケース表示を終えて通常のプレビューに戻る。
    void LeaveShowcase();
    /// @brief ショーケース用のカメラを動かす。
    void UpdateShowcaseCamera(float deltaTime);
    /// @brief ショーケースの設定UIを描画する。
    void DrawShowcaseImGui();
    /// @brief アニメーションの設定UIを描画する。
    void DrawAnimationImGui();
    /// @brief ショーケースの比較撮影を開始する。
    void StartShowcaseComparison();
    /// @brief ショーケースの比較撮影を次のケースへ進める。
    void AdvanceShowcaseComparison();
    /// @brief 現在のショーケース比較ケースの設定を適用する。
    void ApplyShowcaseComparisonCase();
    /// @brief ショーケースの比較撮影を終了し、結果を記録する。
    void FinishShowcaseComparison(const std::string& status);
    /// @brief Bloomの比較撮影を開始する。
    void StartBloomComparison();
    /// @brief Bloomの比較撮影を次のケースへ進める。
    void AdvanceBloomComparison();
    /// @brief Bloomの比較撮影を終了し、結果を記録する。
    void FinishBloomComparison(const std::string& status);
    /// @brief ディゾルブを開始する。開始できたらtrueを返す。
    bool TriggerDissolve();
    /// @brief ディゾルブを解除して元の表示に戻す。
    void ResetDissolve();
    /// @brief ディゾルブの設定UIを描画する。
    void DrawDissolveImGui();
    /// @brief ディゾルブの比較撮影を開始する。
    void StartDissolveComparison();
    /// @brief ディゾルブの比較撮影を次のケースへ進める。
    void AdvanceDissolveComparison();
    /// @brief 現在のディゾルブ比較ケースの設定を適用する。
    void ApplyDissolveComparisonCase();
    /// @brief ディゾルブの比較撮影を終了し、結果を記録する。
    void FinishDissolveComparison(const std::string& status);
    /// @brief 待機・ディゾルブ・復帰を順に行う連続再生を開始する。
    void StartDissolveSequence();
    /// @brief ディゾルブの連続再生を進める。ポーズ更新の前後どちらで呼ばれたかを受け取る。
    void UpdateDissolveSequence(bool afterPoseUpdate);
    /// @brief ディゾルブの連続再生を終了する。
    void FinishDissolveSequence();
    /// @brief ショーケース撮影用のメタデータを作る。
    nlohmann::json MakeShowcaseMetadata() const;
    /// @brief プレビュー対象をカメラの前へ配置する。
    void PlaceInFrontOfCamera();
    /// @brief カメラ位置を返す。
    myEngine::Vector3 GetCameraPosition() const;

    myEngine::Camera* camera_ = nullptr;
    myEngine::DebugCamera* debugCamera_ = nullptr;
    bool enabled_ = false;
    bool neonMode_ = true;
    bool ready_ = false;
    bool alphaCutoutEnabled_ = true;
    GeometryPreset geometryPreset_ = GeometryPreset::Off;
    myEngine::Transform transform_{{8.0f, 8.0f, 8.0f}, {}, {}}; // 正面から顔の内部線を比較する。
    myEngine::NeonSkinnedParams params_;
    std::unique_ptr<myEngine::SkinnedModel> model_;
    std::unique_ptr<myEngine::Object3d> object_;
    myEngine::NeonSkinnedRenderer renderer_;
    std::vector<SourceMaterial> sourceMaterials_;
    std::vector<myEngine::NeonSkinnedSubmeshParams> submeshParams_;
    std::vector<FeatureMaskBinding> featureMaskBindings_;
    std::vector<std::optional<uint32_t>> featureMaskIndices_;
    std::vector<std::optional<uint32_t>> featureDistanceIndices_;
    std::vector<FeatureMaskBinding> originalMaskBindings_;
    std::vector<std::optional<uint32_t>> originalMaskIndices_;

    /// @brief 品質比較用テクスチャの出所を確認するための情報。
    struct QualityTextureProvenance {
        uint32_t srvIndex = 0;
        std::string imageSha256, authoringSha256, authoringVersionSha256, authoringRevision;
    };

    std::unordered_map<std::string, QualityTextureProvenance> qualityTextureProvenance_;
    int qualityCandidate_ = 0;
    std::string qualityError_;
    bool featureMasksLoadAttempted_ = false;
    std::string featureMaskError_;
    size_t sourceAnimationCount_ = 0;
    size_t generatedAnimationCount_ = 0;
    std::string animationError_;
    std::string loadError_;

    /// @brief ショーケースから戻るときに復元するプレビューの状態。
    struct PreviewCheckpoint {
        bool enabled = false, neonMode = true, alphaCutout = true;
        GeometryPreset geometryPreset = GeometryPreset::Off;
        myEngine::Transform transform{};
        myEngine::NeonSkinnedParams params{};
        std::vector<myEngine::NeonSkinnedSubmeshParams> surfaces;
        std::vector<std::optional<uint32_t>> masks;
        std::vector<std::optional<uint32_t>> distances;
        std::vector<FeatureMaskBinding> bindings;
        int qualityCandidate = 0;
        bool masksLoadAttempted = false;
        std::string maskError, qualityError;
        bool usingDebugCamera = false;
        myEngine::Vector3 gameCameraPosition{}, gameCameraRotation{};
        myEngine::SkinnedModel::AnimationPlaybackState playback;
    } checkpoint_;

    PreviewCheckpoint comparisonCheckpoint_;
    bool showcaseComparisonActive_ = false, showcaseComparisonFrameRecorded_ = false;
    unsigned showcaseComparisonIndex_ = 0;
    std::string showcaseComparisonDirectory_, showcaseComparisonStatus_;
    bool showcaseActive_ = false, showcaseOrbit_ = false;
    myEngine::Camera showcaseCamera_;
    myEngine::Transform showcaseTransform_{{1, 1, 1}, {}, {}};
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
    myEngine::NeonDissolveParams dissolveSettings_{};
    int dissolveDirection_ = 0;
    float dissolveWait_ = 0.15f, dissolveDuration_ = 2.0f;
    bool restorePoseThisFrame_ = false;
    bool dissolveComparisonActive_ = false, dissolveComparisonFrameRecorded_ = false;
    unsigned dissolveComparisonIndex_ = 0;
    myEngine::NeonDissolveParams dissolveComparisonParams_{};
    NeonDissolvePreviewController dissolveComparisonController_;
    myEngine::SkinnedModel::AnimationPlaybackState dissolveComparisonPlayback_;
    myEngine::NeonDissolveParams dissolveComparisonOriginalParams_{};
    std::string dissolveComparisonDirectory_, dissolveComparisonStatus_;
    bool dissolveSequence_ = false;
    unsigned dissolveSequenceLastEventFrame_ = UINT32_MAX;
    myEngine::SkinnedModel::AnimationPlaybackState dissolveSequencePlayback_;
    bool dissolveSequenceOriginalOrbit_ = false;
    float dissolveSequenceOriginalYaw_ = 0.0f;
};
#endif
