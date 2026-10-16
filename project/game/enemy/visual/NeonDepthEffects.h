#pragma once
// 既存の描画を使う、シーン所有の不変なDepthエフェクトの範囲。
#include "NeonDepthEffectGeometry.h"
#include "NeonGridRenderer.h"
#include <memory>
#include <string>

namespace neondepth {
/// @brief エフェクト描画の処理回数。検証で使う。
struct EffectsStats {
    unsigned resourceCreates = 0, resourceReleases = 0, resourceFailures = 0;
    uint64_t updates = 0, duplicateUpdates = 0, invalidFrames = 0, capacityRejected = 0;
    uint64_t airDraws = 0, floorDraws = 0, depthBindingRejected = 0;
    unsigned vertices = 0, airVertices = 0, floorVertices = 0, lineCommands = 0, fillCommands = 0, launchLatches = 0;
};
} // namespace neondepth

// シーンが所有する演出用リソース。スナップショットとソケットは値のコピー。
// Initialize/Release/Reset/Updateは既存のNeonGridRendererのフレームフェンスの約束事に従い、
// マップしたバッファを変える前に、前回送信した使用が完了している必要がある。
class NeonDepthEffects {
public:
    ~NeonDepthEffects()
    {
        Release();
    }

    /// @brief 描画リソースを作成する。
    bool Initialize(myEngine::DirectXCommon* dx, const std::string& texture = "resources/white512x512.png");
    /// @brief 描画リソースを解放する。
    void Release();

    bool HasResources() const
    {
        return renderer_ != nullptr;
    }

    /// @brief 新しい戦闘のために状態をリセットする。
    void ResetEncounter();
    // 表示/モデルのUpdateの後、同じ最終カメラを使って毎フレーム1回呼ぶ。
    // frameIdは単調に変わるシーンのフレーム識別子で、動きのための時計ではない。
    bool Update(const neondepth::EffectsInput& input);
    // シーンのHDR + 現在のシーンDSV。深度は読むだけで書かず、遮蔽を保つ。
    void DrawAir();
    // ルートのローカルオーバーレイか既存のObjectPostキャプチャが所有・バインドする、
    // シーン互換のHDR・深度なしの描画先。同じ固定VPを使う。DrawRangeSolidやHUDのフォーマットは使わない。
    void DrawFloor();

    const neondepth::EffectsStats& GetStats() const
    {
        return stats_;
    }

    const neondepth::EffectGeometryFrame& GetGeometry() const
    {
        return builder_.GetFrame();
    }

    const std::string& GetStatus() const
    {
        return status_;
    }

private:
    myEngine::DirectXCommon* dx_ = nullptr;
    std::unique_ptr<myEngine::NeonGridRenderer> renderer_;
    neondepth::EffectGeometryBuilder builder_;
    neondepth::EffectsStats stats_{};
    myEngine::Matrix4x4 viewProjection_{};
    uint64_t lastFrameId_ = 0;
    uint32_t airStart_ = 0, airCount_ = 0, floorStart_ = 0, floorCount_ = 0;
    bool framePrepared_ = false;
    std::string status_;
};
