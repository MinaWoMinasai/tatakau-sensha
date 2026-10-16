#pragma once
// 値だけの設定、実際のバインドによる配置、不変の攻撃演出。
#include "game/enemy/actor/NeonDepthCombat.h"
#include "NeonDepthPlacement.h"
#include "NeonDepthAnimations.h"
#include "NeonDepthLifecycle.h"
#include <algorithm>
#include <cstdint>

namespace neondepth {

// コピーだけで扱う演出設定。Enemy、Camera、Stage、Inputへのポインタは持たない。
struct PresentationConfig {
    float worldHeight = 18.0f;
    float hoverHeight = 3.0f;
    myEngine::Vector2 floorOffset{0, 2};
    bool reducedMotion = false;
    float motionAmplitude = 1.0f;
    float collapseSeconds = .35f;
    float dissolveSeconds = 1.2f;
    myEngine::Vector3 footOffsetLocal{};
    float forwardYawOffsetRadians = myEngine::pi * .5f;
};

/// @brief 演出設定が有効な範囲かを返す。
inline bool Valid(const PresentationConfig& value)
{
    return InRange(value.worldHeight, 4, 40) && InRange(value.hoverHeight, 0, 15) && InRange(value.floorOffset.x, -12, 12) &&
           InRange(value.floorOffset.y, -12, 12) && InRange(value.motionAmplitude, 0, 1.5f) && InRange(value.collapseSeconds, .15f, .8f) &&
           InRange(value.dissolveSeconds, .4f, 2) && InRange(value.footOffsetLocal.x, -.5f, .5f) &&
           InRange(value.footOffsetLocal.y, -.5f, .5f) && InRange(value.footOffsetLocal.z, -.5f, .5f) &&
           InRange(value.forwardYawOffsetRadians, -2 * myEngine::pi, 2 * myEngine::pi);
}

/// @brief 値を0..1に収める。非有限なら0を返す。
inline float Unit(float value)
{
    return Finite(value) ? (std::clamp)(value, 0.0f, 1.0f) : 0;
}

/// @brief 0..1の値にスムーズステップを適用する。
inline float Smooth(float value)
{
    const float t = Unit(value);
    return t * t * (3 - 2 * t);
}

/// @brief 演出で進める時間を0..0.1秒に収める。
inline float PresentationDelta(float value)
{
    return InRange(value, 0, .1f) ? value : (Finite(value) && value > 0 ? .1f : 0);
}

/// @brief バインドポーズでのモデルの境界。
struct BindBounds {
    myEngine::Vector3 low{}, high{};
};

/// @brief バインドポーズから求めた足元の位置と高さ。
struct BindPlacement {
    myEngine::Vector3 stableFootLocal{};
    float boundsHeight = 0;
    unsigned footBonesFound = 0;
};

// Initializeの直後、生成したアニメーションをサンプリングする前に呼ぶ。
// X/Zはローダーが変換した実際のスケルトン行列で決める。足裏のYはバインド境界の底で決め、
// 足首の関節のY（約0.1155）はあえて足裏として使わない。
inline bool TryBindPlacement(const myEngine::Skeleton& skeleton, const BindBounds& bounds, BindPlacement& output)
{
    if (!ValidateDepthBindSkeleton(skeleton) || !Finite(bounds.low) || !Finite(bounds.high) || bounds.high.x < bounds.low.x ||
        bounds.high.z < bounds.low.z || !InRange(bounds.high.y - bounds.low.y, .001f, 1000)) {
        return false;
    }
    BindPlacement candidate;
    candidate.boundsHeight = bounds.high.y - bounds.low.y;
    candidate.stableFootLocal = {(bounds.low.x + bounds.high.x) * .5f, bounds.low.y, (bounds.low.z + bounds.high.z) * .5f};
    float footX = 0, footZ = 0;
    for (const char* name : {"J_Bip_L_Foot", "J_Bip_R_Foot"}) {
        const auto found = skeleton.jointMap.find(name);
        if (found == skeleton.jointMap.end()) {
            continue;
        }
        const auto& matrix = skeleton.joints[static_cast<size_t>(found->second)].skeletonSpaceMatrix;
        const myEngine::Vector3 point{matrix.m[3][0], matrix.m[3][1], matrix.m[3][2]};
        if (!Finite(point)) {
            return false;
        }
        footX += point.x;
        footZ += point.z;
        ++candidate.footBonesFound;
    }
    if (candidate.footBonesFound) {
        candidate.stableFootLocal.x = footX / candidate.footBonesFound;
        candidate.stableFootLocal.z = footZ / candidate.footBonesFound;
    }
    output = candidate;
    return true;
}

/// @brief バインドポーズの情報と演出設定から配置の設定を作る。
inline PlacementConfig MakePlacementConfig(const BindPlacement& bind, const PresentationConfig& profile)
{
    PlacementConfig result;
    result.stableFootLocal = {bind.stableFootLocal.x + profile.footOffsetLocal.x, bind.stableFootLocal.y + profile.footOffsetLocal.y,
                              bind.stableFootLocal.z + profile.footOffsetLocal.z};
    result.boundsHeight = bind.boundsHeight;
    result.worldHeight = profile.worldHeight;
    result.forwardYawOffset = profile.forwardYawOffsetRadians;
    return result;
}

/// @brief エフェクトの発生位置として使うモデル上の部位。
enum class Socket {
    Chest,
    LeftHand,
    RightHand
};

// モデルはこのフレーム分のサンプリングを済ませている。ソケットは表示上の端点にすぎず、
// その高さやアニメーションで床の危険範囲や壁で切り詰めた当たり区間が変わることはない。
inline bool TrySocketWorld(const myEngine::Skeleton& skeleton, const myEngine::Matrix4x4& world, Socket socket, myEngine::Vector3& output)
{
    const char* name = socket == Socket::Chest ? "J_Bip_C_Chest" : socket == Socket::LeftHand ? "J_Bip_L_Hand" : "J_Bip_R_Hand";
    const auto found = skeleton.jointMap.find(name);
    if (found == skeleton.jointMap.end() || found->second < 0 || static_cast<size_t>(found->second) >= skeleton.joints.size() ||
        !Finite(world)) {
        return false;
    }
    const auto& matrix = skeleton.joints[static_cast<size_t>(found->second)].skeletonSpaceMatrix;
    if (!Finite(matrix)) {
        return false;
    }
    const auto jointWorld = myEngine::Multiply(matrix, world);
    myEngine::Vector3 candidate;
    if (!TryHomogeneousPoint({0, 0, 0}, jointWorld, candidate)) {
        return false;
    }
    output = candidate;
    return true;
}

/// @brief 戦闘の段階に対応するモーションの再生設定。
struct MotionMapping {
    Motion motion = Motion::Hover;
    float speed = 1;
    bool loop = true, playing = true;
    float entryTime = -1; // フェーズが変わったときだけシークする。-1ならクリップの時刻とブレンドを保つ。
};

/// @brief 戦闘のスナップショットから再生するモーションを決める。
inline MotionMapping MapDepthMotion(const Snapshot& snapshot, bool collapsing = false, bool frozen = false, float collapseSeconds = .35f)
{
    if (collapsing || frozen) {
        return {Motion::Defeat, 1 / (std::max)(collapseSeconds, .15f), false, !frozen, -1};
    }
    if (snapshot.phase == Phase::Intro || snapshot.phase == Phase::Reposition || snapshot.phase == Phase::Defeated ||
        snapshot.phase == Phase::Aborted) {
        return {};
    }
    const float duration = (std::max)(Finite(snapshot.duration) ? snapshot.duration : 0, .05f);
    if (snapshot.plan.attack == Attack::Beam) {
        switch (snapshot.phase) {
        case Phase::Telegraph:
            return {Motion::BeamTwist, .35f / duration, false, true, -1};
        case Phase::Locked:
        case Phase::Airborne:
            return {Motion::BeamTwist, 0, false, false, .35f};
        case Phase::Active:
            return {Motion::BeamTwist, .37f / duration, false, true, .35f};
        case Phase::Recovery:
            return {Motion::BeamTwist, .28f / duration, false, true, .72f};
        default:
            return {};
        }
    }
    const bool dive = snapshot.plan.attack == Attack::Dive;
    switch (snapshot.phase) {
    case Phase::Telegraph:
        return {dive ? Motion::DivePull : Motion::VolleyCharge, 1 / duration, false, true, -1};
    case Phase::Locked:
        return {dive ? Motion::DivePull : Motion::VolleyCharge, 0, false, false, 1};
    case Phase::Airborne:
        return {dive ? Motion::DiveDrop : Motion::VolleyRelease, 1 / duration, false, true, -1};
    case Phase::Active:
        return dive ? MotionMapping{Motion::DiveLand, 1 / duration, false, true, -1} : MotionMapping{};
    default:
        return {};
    }
}

/// @brief モーションの切り替え指示。
struct MotionCommand {
    MotionMapping mapping{};
    bool select = false, restartSameClip = false, phaseChanged = false;
};

// 実行時の選択を守るためだけのもの。ゲームプレイのフェーズや当たりイベントは制御しない。
class MotionGate {
public:
    /// @brief 初期状態に戻す。
    void Reset()
    {
        *this = MotionGate{};
    }

    /// @brief スナップショットを受け取り、必要ならモーションの切り替えを指示する。
    MotionCommand Step(const Snapshot& snapshot, bool collapsing = false, bool frozen = false, float collapseSeconds = .35f)
    {
        MotionCommand command;
        command.mapping = MapDepthMotion(snapshot, collapsing, frozen, collapseSeconds);
        command.select =
            !valid_ || generation_ != snapshot.generation || instance_ != snapshot.plan.instance || motion_ != command.mapping.motion;
        command.restartSameClip = valid_ && command.select && motion_ == command.mapping.motion;
        command.phaseChanged = command.select || phase_ != snapshot.phase || collapsing_ != collapsing || frozen_ != frozen;
        generation_ = snapshot.generation;
        instance_ = snapshot.plan.instance;
        motion_ = command.mapping.motion;
        phase_ = snapshot.phase;
        collapsing_ = collapsing;
        frozen_ = frozen;
        valid_ = true;
        return command;
    }

private:
    uint64_t generation_ = 0, instance_ = 0;
    Motion motion_ = Motion::Hover;
    Phase phase_ = Phase::Aborted;
    bool valid_ = false, collapsing_ = false, frozen_ = false;
};

/// @brief 登場時の形成の進み具合（部位ごとに0..1）。
struct Formation {
    float core = 1, connector = 1, body = 1;
};

/// @brief 戦闘のスナップショットから形成の進み具合を求める。
inline Formation MapFormation(const Snapshot& snapshot)
{
    if (snapshot.phase == Phase::Aborted) {
        return {0, 0, 0};
    }
    if (snapshot.phase != Phase::Intro) {
        return {};
    }
    const float p = Unit(snapshot.progress);
    return {Smooth(p / .25f), Smooth((p - .12f) / .38f), Smooth((p - .25f) / .65f)};
}

/// @brief 攻撃に合わせて体に加える動き。
struct BodyMotion {
    myEngine::Vector2 extraFloorOffset{};
    float extraHoverHeight = 0;
    myEngine::Vector3 localLean{};
};

// 不変の計画の進行度 -> 毎回新しく求める範囲付きの変換。前のポーズへの加算、
// 壁や中心の解決、画面上のずらし、攻撃の発動、共有の乱数は使わない。
inline BodyMotion MapBodyMotion(const Snapshot& snapshot, const PresentationConfig& profile, float hitFeedback = 0)
{
    BodyMotion result;
    const float p = Unit(snapshot.progress);
    const float amplitude = profile.motionAmplitude * (profile.reducedMotion ? .35f : 1.0f);
    const float wave = p > 0 && p < 1 ? std::sin(myEngine::pi * p) : 0;
    const bool dive = snapshot.plan.attack == Attack::Dive;
    const bool beam = snapshot.plan.attack == Attack::Beam;
    switch (snapshot.phase) {
    case Phase::Telegraph:
    case Phase::Locked:
        result.extraFloorOffset.y = amplitude * 3 * (snapshot.phase == Phase::Locked ? 1 : Smooth(p));
        result.extraHoverHeight = amplitude * (dive ? 4 : 1.5f) * (snapshot.phase == Phase::Locked ? 1 : Smooth(p));
        result.localLean.x = amplitude * .08f * (snapshot.phase == Phase::Locked ? 1 : Smooth(p));
        break;
    case Phase::Airborne:
        result.extraFloorOffset.x = dive ? -profile.floorOffset.x * Smooth(p) : 0;
        result.extraFloorOffset.y = amplitude * 3 * (1 - Smooth(p)) - (dive ? profile.floorOffset.y * Smooth(p) : 0);
        result.extraHoverHeight =
            dive ? amplitude * 4 * (1 - Smooth(p)) - profile.hoverHeight * Smooth(p) : amplitude * 1.5f * (1 - Smooth(p));
        result.localLean.x = amplitude * (.08f * (1 - Smooth(p)) + (dive ? .10f : -.06f) * wave);
        break;
    case Phase::Active:
        if (dive) {
            result.extraFloorOffset = {-profile.floorOffset.x, -profile.floorOffset.y};
            result.extraHoverHeight = -profile.hoverHeight * (1 - Smooth(p));
            result.localLean.x = amplitude * .08f * wave;
        } else if (beam) {
            result.extraFloorOffset.y = amplitude * 3;
            result.extraHoverHeight = amplitude * 1.5f;
            result.localLean.x = amplitude * .08f;
        }
        break;
    case Phase::Recovery:
        if (dive) {
            result.extraFloorOffset = {-profile.floorOffset.x * (1 - Smooth(p)), -profile.floorOffset.y * (1 - Smooth(p))};
        } else if (beam) {
            result.extraFloorOffset.y = amplitude * 3 * (1 - Smooth(p));
            result.extraHoverHeight = amplitude * 1.5f * (1 - Smooth(p));
            result.localLean.x = amplitude * .08f * (1 - Smooth(p));
        }
        result.extraHoverHeight += amplitude * .15f * wave;
        break;
    case Phase::Intro:
        result.extraHoverHeight = amplitude * 2 * (1 - Smooth(p));
        break;
    default:
        break;
    }
    result.localLean.z += amplitude * .035f * Unit(hitFeedback);
    return result;
}

} // namespace neondepth
