#pragma once
#include <algorithm>
#include <cmath>
#include "Calculation.h"

// Stageとの衝突と入力の読み取りはこの計算の外で行う。速度は
// 確立済みの60FPS基準フレームの値で、deltaTimeは秒単位。
struct PlayerMovementInput {
    myEngine::Vector3 direction{}, velocity{};
    float moveSpeed = 0.2f, acceleration = 2.5f, deceleration = 3.5f;
    float deltaTime = 1.0f / 60.0f;
    bool runEnabled = false, dashing = false, railHeld = false, spinActive = false;
};

struct PlayerMovementStep {
    myEngine::Vector3 direction{}, velocity{}, stepMove{};
    int subStepCount = 1;
};

inline PlayerMovementStep CalculatePlayerMovement(const PlayerMovementInput& input)
{
    PlayerMovementStep result{};
    result.direction = input.direction;
    result.velocity = input.velocity;
    if (myEngine::Length(result.direction) > 1.0f) {
        result.direction = myEngine::Normalize(result.direction);
    }
    myEngine::Vector3 targetVelocity = result.direction * input.moveSpeed;
    if (input.railHeld && !input.dashing) {
        targetVelocity = targetVelocity * 0.78f;
    }
    if (input.spinActive && !input.dashing) {
        targetVelocity = targetVelocity * 0.55f;
    }
    const float accel = myEngine::Length(result.direction) > 0.0f ? input.acceleration : input.deceleration;
    if (input.runEnabled) {
        const float response = input.dashing ? 1.5f : accel;
        result.velocity += (targetVelocity - result.velocity) * (1.0f - std::exp(-response * input.deltaTime));
    } else {
        result.velocity += (targetVelocity - result.velocity) * accel * input.deltaTime;
    }
    const float timeWeight = input.deltaTime * 60.0f;
    const myEngine::Vector3 frameMove = result.velocity * timeWeight;
    constexpr float maxStep = 0.35f;
    result.subStepCount = (std::max)(1, static_cast<int>((std::max)(std::abs(frameMove.x), std::abs(frameMove.y)) / maxStep) + 1);
    result.stepMove = frameMove / static_cast<float>(result.subStepCount);
    return result;
}