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
class TrailStressFixture {
public:
    void Initialize(DirectXCommon* dxCommon, Object3dCommon* objectCommon, uint32_t count) {
        Shutdown();
        if (!dxCommon || count == 0) return;
        count = (std::min)(count, 512u);
        columns_ = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<float>(count))));
        rows_ = (count + columns_ - 1u) / columns_;
        manager_ = std::make_unique<TrailManager>();
        manager_->Initialize(dxCommon, objectCommon, "resources/white512x512.png");

        // Match the production bullet trail's density and fade convention.
        config_.interpolationSteps = 5;
        config_.maxPoints = 22;
        config_.lifetime = 0.24f;
        config_.startColor = { 0.23f, 1.035f, 1.15f, 1.0f };
        config_.endColor = { 0.09f, 0.405f, 0.45f, 0.0f };
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
        for (uint32_t i = 0; i < config_.maxPoints + 2u; ++i) Update(1.0f / 60.0f);
    }

    // Pass the same fixed dt for both A/B runs, irrespective of wall-clock FPS.
    void Update(float fixedDeltaTime) {
        if (!manager_ || !std::isfinite(fixedDeltaTime) || fixedDeltaTime <= 0.0f) return;
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
            trails_[i]->Update(fixedDeltaTime,
                { x + normalX, y + normalY, 0.0f },
                { x - normalX, y - normalY, 0.0f }, config_);
        }
        manager_->Update(fixedDeltaTime);
    }

    void Draw() {
        if (!manager_) return;
        Matrix4x4 identity{};
        identity.m[0][0] = identity.m[1][1] = identity.m[2][2] = identity.m[3][3] = 1.0f;
        manager_->DrawAll(identity);
    }

    TrailManager::DrawStats GetStats() const {
        return manager_ ? manager_->GetDrawStats() : TrailManager::DrawStats{};
    }

    // Like other renderers, destroy only between frames / after GPU completion.
    void Shutdown() {
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
