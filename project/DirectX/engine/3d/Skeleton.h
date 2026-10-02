#pragma once

#include "Animation.h"
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace cg2 {

/// @brief 骨格階層を構成するノードの初期姿勢と子ノードを表す。
struct SkeletonNode {
    QuaternionTransform transform{};
    std::string name;
    std::vector<SkeletonNode> children;
};

/// @brief 骨格の1関節の姿勢と親子関係を保持する。
struct Joint {
    QuaternionTransform transform{};
    QuaternionTransform bindTransform{};
    Matrix4x4 localMatrix{};
    Matrix4x4 skeletonSpaceMatrix{};
    std::string name;
    std::vector<int32_t> children;
    int32_t index = -1;
    std::optional<int32_t> parent;
};

/// @brief 関節配列と名前からの索引をまとめ、骨格の姿勢計算に使う。
struct Skeleton {
    int32_t root = -1;
    std::map<std::string, int32_t> jointMap;
    std::vector<Joint> joints;
};

/// @brief ノード階層から骨格を構築し、関節のワールド姿勢を更新する。
class SkeletonSystem {
public:
    /// @brief 指定条件でインスタンスまたはリソースを生成する。
    static Skeleton Create(const SkeletonNode& rootNode);
    /// @brief への結合姿勢を初期状態へ戻す。
    static void ResetToBindPose(Skeleton& skeleton);
    /// @brief アニメーションを現在の状態へ適用する。
    static void ApplyAnimation(Skeleton& skeleton, const AnimationPlayer& animationPlayer);
    /// @brief アニメーション合成を現在の状態へ適用する。
    static void ApplyAnimationBlend(Skeleton& skeleton, const AnimationPlayer& animationA, const AnimationPlayer& animationB,
                                    float blendFactor);
    /// @brief アニメーション合成からの姿勢を現在の状態へ適用する。
    static void ApplyAnimationBlendFromPose(Skeleton& skeleton, const std::vector<QuaternionTransform>& startPose,
                                            const AnimationPlayer& targetAnimation, float blendFactor);
    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    static void Update(Skeleton& skeleton);

private:
    /// @brief 関節を生成する。
    static int32_t CreateJoint(const SkeletonNode& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints);
};

} // namespace cg2
