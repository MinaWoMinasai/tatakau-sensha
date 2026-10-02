#pragma once

#include "TrailManager.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

// Opt-in GPU A/B fixture. The caller owns the diagnostic environment switch
// and the render target/bloom capture. Merely constructing this class allocates
// nothing. All trails are in clip space, independent of scene/camera/game RNG.
namespace cg2 {

/// @brief 多数の軌跡を発生させ、描画負荷とバッファ再確保を検証する。
class TrailStressFixture {
public:
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(DirectXCommon* dxCommon, Object3dCommon* objectCommon, uint32_t count)
    {
        Shutdown();
        if (!dxCommon || count == 0)
            return;
        count = (std::min)(count, 512u);
        columns_ = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<float>(count))));
        rows_ = (count + columns_ - 1u) / columns_;
        manager_ = std::make_unique<TrailManager>();
        manager_->Initialize(dxCommon, objectCommon, "resources/white512x512.png");

        // Match the production bullet trail's density and fade convention.
        config_.interpolationSteps = 5;
        config_.maxPoints = 22;
        config_.lifetime = 0.24f;
        config_.startColor = {0.23f, 1.035f, 1.15f, 1.0f};
        config_.endColor = {0.09f, 0.405f, 0.45f, 0.0f};
        config_.startWidthScale = 1.0f;
        config_.endWidthScale = 0.15f;
        config_.widthCurvePower = 1.0f;
        config_.colorCurvePower = 1.0f;
        trails_.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            auto* trail = manager_->CreateInstance();
            trail->SetIsPermanent(true);
            trail->SetConfig(config_);
            trails_.push_back(trail);
        }
        // Start measurements with a full, stable history rather than counting
        // the initial 4-point visibility and lifetime ramp as a performance win.
        for (uint32_t i = 0; i < config_.maxPoints + 2u; ++i)
            Update(1.0f / 60.0f);
    }

    // Pass the same fixed dt for both A/B runs, irrespective of wall-clock FPS.
    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    void Update(float fixedDeltaTime)
    {
        if (!manager_ || !std::isfinite(fixedDeltaTime) || fixedDeltaTime <= 0.0f)
            return;
        elapsed_ += static_cast<double>(fixedDeltaTime);
        const float cellWidth = 1.84f / static_cast<float>(columns_);
        const float cellHeight = 1.76f / static_cast<float>(rows_);
        const float halfWidth = (std::min)(cellWidth, cellHeight) * 0.028f;
        for (size_t i = 0; i < trails_.size(); ++i) {
            const uint32_t column = static_cast<uint32_t>(i) % columns_;
            const uint32_t row = static_cast<uint32_t>(i) / columns_;
            const float centerX = -0.92f + (static_cast<float>(column) + 0.5f) * cellWidth;
            const float centerY = 0.88f - (static_cast<float>(row) + 0.5f) * cellHeight;
            const float phase = static_cast<float>(std::fmod(elapsed_ * 2.4 + static_cast<double>(i) * 0.173, 6.283185307179586));
            const float sine = std::sin(phase);
            const float cosine = std::cos(phase);
            const float x = centerX + cosine * cellWidth * 0.34f;
            const float y = centerY + sine * cellHeight * 0.20f;
            const float tangentX = -sine * cellWidth * 0.34f;
            const float tangentY = cosine * cellHeight * 0.20f;
            const float inverseLength = 1.0f / std::sqrt(tangentX * tangentX + tangentY * tangentY);
            const float normalX = -tangentY * inverseLength * halfWidth;
            const float normalY = tangentX * inverseLength * halfWidth;
            trails_[i]->Update(fixedDeltaTime, {x + normalX, y + normalY, 0.0f}, {x - normalX, y - normalY, 0.0f}, config_);
        }
        manager_->Update(fixedDeltaTime);
    }

    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    void Draw()
    {
        if (!manager_)
            return;
        Matrix4x4 identity{};
        identity.m[0][0] = identity.m[1][1] = identity.m[2][2] = identity.m[3][3] = 1.0f;
        manager_->DrawAll(identity);
    }

    /// @brief 現在の性能値を返す。
    TrailManager::DrawStats GetStats() const
    {
        return manager_ ? manager_->GetDrawStats() : TrailManager::DrawStats{};
    }

    // Like other renderers, destroy only between frames / after GPU completion.
    /// @brief 利用を終了し、保持している資源と計測状態を解放する。
    void Shutdown()
    {
        trails_.clear();
        manager_.reset();
        elapsed_ = 0.0;
        columns_ = rows_ = 0;
    }

private:
    std::unique_ptr<TrailManager> manager_;
    std::vector<TrailInstance*> trails_;
    TrailConfig config_{};
    uint32_t columns_ = 0;
    uint32_t rows_ = 0;
    double elapsed_ = 0.0;
};

} // namespace cg2
