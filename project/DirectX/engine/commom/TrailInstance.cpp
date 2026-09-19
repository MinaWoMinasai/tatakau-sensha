#include "TrailInstance.h"

void TrailInstance::SetConfig(const TrailConfig& config) {
    // Compare fields, not padding bytes. Merely advancing point ages does not
    // affect this renderer until a point expires.
    const auto sameColor = [](const Vector4& a, const Vector4& b) {
        return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
    };
    if (!sameColor(config_.startColor, config.startColor) ||
        !sameColor(config_.endColor, config.endColor) ||
        config_.interpolationSteps != config.interpolationSteps ||
        config_.maxPoints != config.maxPoints || config_.lifetime != config.lifetime ||
        config_.startWidthScale != config.startWidthScale ||
        config_.endWidthScale != config.endWidthScale ||
        config_.widthCurvePower != config.widthCurvePower ||
        config_.colorCurvePower != config.colorCurvePower) {
        config_ = config;
        ++geometryRevision_;
    }
}

void TrailInstance::Update(float deltaTime, const Vector3& tipPos, const Vector3& basePos, const TrailConfig& config) {
    SetConfig(config);
    bool geometryChanged = false;

    // 各頂点の経過時間を更新
    for (auto& point : points_) {
        point.currentTime += deltaTime;
    }

    if (isActive_) {
        // 振っている間は新しい座標を追加
        points_.push_front({ tipPos, basePos, 0.0f });
        geometryChanged = true;
    }

    // 寿命を過ぎた古い頂点を削除 (後ろから削除)
    while (!points_.empty() && points_.back().currentTime > config_.lifetime) {
        points_.pop_back();
        geometryChanged = true;
    }

    // 最大数制限
    while (points_.size() > config_.maxPoints) {
        points_.pop_back();
        geometryChanged = true;
    }
    if (geometryChanged) ++geometryRevision_;
}
