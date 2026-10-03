#pragma once
#include "Skeleton.h"
#include <vector>

namespace cg2 { class SkinnedModel; }

// AvatarSample_BのDeveloper Preview専用。GLB由来のAnimationとは区別する。
namespace neonpreview {
enum class Clip { BindPose, Idle, Attack };
std::vector<cg2::Animation> CreateAnimations(const cg2::Skeleton& skeleton);
bool SelectAnimation(cg2::SkinnedModel& model, Clip clip);
// 1フレームに一度だけモデルを更新し、非ループAttack終了後は既存ブレンドでIdleへ戻す。
void UpdateAnimation(cg2::SkinnedModel& model, float deltaTime);
}
