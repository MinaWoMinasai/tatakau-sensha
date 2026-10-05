#pragma once
#include <algorithm>
#include <cmath>
#include "Calculation.h"

// Stage collision and input sampling remain outside this calculation. Speeds
// are the established 60 FPS reference-frame values; deltaTime is seconds.
struct PlayerMovementInput {
    cg2::Vector3 direction{}, velocity{};
    float moveSpeed = 0.2f, acceleration = 2.5f, deceleration = 3.5f;
    float deltaTime = 1.0f / 60.0f;
    bool runEnabled = false, dashing = false, railHeld = false, spinActive = false;
};

struct PlayerMovementStep {
    cg2::Vector3 direction{}, velocity{}, stepMove{};
    int subStepCount = 1;
};

inline PlayerMovementStep CalculatePlayerMovement(const PlayerMovementInput& input)
{
    PlayerMovementStep result{};
    result.direction = input.direction;
    result.velocity = input.velocity;
    if (cg2::Length(result.direction) > 1.0f) result.direction = cg2::Normalize(result.direction);
    cg2::Vector3 targetVelocity = result.direction * input.moveSpeed;
    if (input.railHeld && !input.dashing) targetVelocity = targetVelocity * 0.78f;
    if (input.spinActive && !input.dashing) targetVelocity = targetVelocity * 0.55f;
    const float accel = cg2::Length(result.direction) > 0.0f ? input.acceleration : input.deceleration;
    if (input.runEnabled) {
        const float response = input.dashing ? 1.5f : accel;
        result.velocity += (targetVelocity - result.velocity) * (1.0f - std::exp(-response * input.deltaTime));
    } else {
        result.velocity += (targetVelocity - result.velocity) * accel * input.deltaTime;
    }
    const float timeWeight = input.deltaTime * 60.0f;
    const cg2::Vector3 frameMove = result.velocity * timeWeight;
    constexpr float maxStep = 0.35f;
    result.subStepCount = (std::max)(1, static_cast<int>((std::max)(std::abs(frameMove.x), std::abs(frameMove.y)) / maxStep) + 1);
    result.stepMove = frameMove / static_cast<float>(result.subStepCount);
    return result;
}