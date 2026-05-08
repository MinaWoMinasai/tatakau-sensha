#include "TrailInstance.h"

void TrailInstance::Update(float deltaTime, const Vector3& tipPos, const Vector3& basePos, const TrailConfig& config) {
    config_ = config;

    // 各頂点の経過時間を更新
    for (auto& point : points_) {
        point.currentTime += deltaTime;
    }

    if (isActive_) {
        // 振っている間は新しい座標を追加
        points_.push_front({ tipPos, basePos, 0.0f });
    }

    // 寿命を過ぎた古い頂点を削除 (後ろから削除)
    while (!points_.empty() && points_.back().currentTime > config_.lifetime) {
        points_.pop_back();
    }

    // 最大数制限
    while (points_.size() > config_.maxPoints) {
        points_.pop_back();
    }
}
