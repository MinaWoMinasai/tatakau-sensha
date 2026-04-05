#include "TrailInstance.h"

void TrailInstance::Update(const Vector3& tipPos, const Vector3& basePos, const TrailConfig& config) {
    config_ = config;

    if (isActive_) {
        // 振っている間は新しい座標を追加
        points_.push_front({ tipPos, basePos });
    } else if (!points_.empty()) {
        // 振るのを止めた（isActive_ = false）ら、古い座標を1つずつ消していく
        // これにより、軌跡が「スッ...」と消える演出になる
        points_.pop_back();
    }

    // 最大数制限
    while (points_.size() > config_.maxPoints) {
        points_.pop_back();
    }
}