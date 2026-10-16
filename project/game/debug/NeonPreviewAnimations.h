#pragma once
#include "Skeleton.h"
#include <vector>

namespace myEngine {
class SkinnedModel;
}

// AvatarSample_B用の既存生成クリップ。Previewと本編Presentationで共有し、GLB由来のAnimationとは区別する。
namespace neonpreview {
/// @brief プレビューで使うアニメーションクリップ。
enum class Clip {
    BindPose,
    Idle,
    Attack
};
/// @brief スケルトンからプレビュー用のアニメーションを生成する。
std::vector<myEngine::Animation> CreateAnimations(const myEngine::Skeleton& skeleton);
/// @brief モデルで再生するクリップを切り替える。
bool SelectAnimation(myEngine::SkinnedModel& model, Clip clip);
// 1フレームに一度だけモデルを更新し、非ループAttack終了後は既存ブレンドでIdleへ戻す。
void UpdateAnimation(myEngine::SkinnedModel& model, float deltaTime);
} // namespace neonpreview
