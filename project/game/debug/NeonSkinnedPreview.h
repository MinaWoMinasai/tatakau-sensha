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
#include <memory>
#include <string>
#include <vector>

// GameSceneのDeveloper UIからのみ使用。初期状態は無効、GPU資源は有効化時に作成する。
/// @brief 開発画面でスキニングモデルとネオン材質を表示・調整する。
class NeonSkinnedPreview {
public:
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(cg2::Camera* camera, cg2::DebugCamera* debugCamera);
    // 前フレームのFence完了後、Camera更新後に1フレーム1回呼ぶ。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Update(float deltaTime);
    // Bloom::PreDraw直後のScene HDR / Normal / Material + D24S8内でのみ呼ぶ。
    void Draw();
    /// @brief ImGUIを描画する。
    void DrawImGui();

private:
    /// @brief プレビュー対象の元のマテリアル値を保持する。
    struct SourceMaterial {
        std::string meshName;
        std::string materialName;
        std::string alphaMode;
        float alphaCutoff = 0.0f;
    };
    /// @brief 保存されたデータを読み込む。
    void Load();
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
    cg2::Transform transform_{{8.0f, 8.0f, 8.0f}, {}, {}}; // 正面から顔の内部線を比較する。
    cg2::NeonSkinnedParams params_;
    std::unique_ptr<cg2::SkinnedModel> model_;
    std::unique_ptr<cg2::Object3d> object_;
    cg2::NeonSkinnedRenderer renderer_;
    std::vector<SourceMaterial> sourceMaterials_;
    std::vector<cg2::NeonSkinnedSubmeshParams> submeshParams_;
    size_t sourceAnimationCount_ = 0;
    std::string loadError_;
};
#endif
