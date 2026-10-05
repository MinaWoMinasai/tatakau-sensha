#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>

#include "game/player/PlayerMovement.h"
namespace cg2 {
#include "player_component_math.inc"
}
namespace {
void Exact(float value, float expected)
{
    assert(std::bit_cast<uint32_t>(value) == std::bit_cast<uint32_t>(expected));
}
void MovementGoldens()
{
    // direction x/y, velocity xyz, substep xyz from original Player::Update.
    constexpr std::array<std::array<float, 8>, 8> expected{{
        {1,0,.109999999f,-.0700000003f,.0199999996f,0,-0.0f,0},
        {.707106769f,.707106769f,.112193108f,-.0603068918f,.019166667f,.112193108f,-.0603068918f,.019166667f},
        {0,0,.103583336f,-.0659166649f,.0188333336f,.103583336f,-.0659166649f,.0188333336f},
        {.707106769f,.707106769f,.112148046f,-.0605060607f,.0191837884f,.112148046f,-.0605060607f,.0191837884f},
        {0,0,.103766903f,-.0660334826f,.0188667085f,.103766903f,-.0660334826f,.0188667085f},
        {.707106769f,.707106769f,.111299552f,-.0642562285f,.0195061974f,.111299552f,-.0642562285f,.0195061974f},
        {1,0,.109537616f,-.0671432614f,.0191837884f,.109537616f,-.0671432614f,.0191837884f},
        {.707106769f,.707106769f,.15551126f,.131150901f,.00270670466f,.339297295f,.286147416f,.0059055374f}
    }};
    for (size_t fixture = 0; fixture < expected.size(); ++fixture) {
        PlayerMovementInput input{};
        input.moveSpeed = .23f; input.runEnabled = fixture >= 3;
        input.direction = {1,fixture % 2 == 1 ? 1.0f : 0.0f,0};
        input.velocity = {.11f,-.07f,.02f};
        input.deltaTime = fixture == 0 ? 0 : fixture == 7 ? .8f : 1.0f/60;
        if (fixture == 2 || fixture == 4) input.direction = {};
        input.dashing = fixture == 5;
        input.railHeld = input.spinActive = fixture == 6;
        const auto step = CalculatePlayerMovement(input);
        const std::array<float,8> values{step.direction.x,step.direction.y,step.velocity.x,step.velocity.y,step.velocity.z,step.stepMove.x,step.stepMove.y,step.stepMove.z};
        for (size_t i = 0; i < values.size(); ++i) Exact(values[i],expected[fixture][i]);
        assert(step.subStepCount == (fixture == 7 ? 22 : 1));
    }
}
void MovementBoundaries()
{
    PlayerMovementInput input{}; input.runEnabled = true; input.direction = {1,1,0}; input.moveSpeed = .23f;
    input.deltaTime = .8f;
    const auto step = CalculatePlayerMovement(input);
    assert(step.subStepCount > 1 && std::abs(step.stepMove.x) <= .35f && std::abs(step.stepMove.y) <= .35f);
    assert(std::abs(cg2::Length(step.direction) - 1) < 1e-6f);
    input.deltaTime = 0; input.velocity = {.7f,-.2f,0};
    const auto paused = CalculatePlayerMovement(input);
    assert(paused.velocity.x == input.velocity.x && paused.velocity.y == input.velocity.y);
    assert(paused.stepMove.x == 0 && paused.stepMove.y == 0 && paused.subStepCount == 1);
    input.deltaTime = 1.0f/60; input.dashing = true;
    const auto dash = CalculatePlayerMovement(input);
    input.railHeld = input.spinActive = true;
    const auto dashWithModifiers = CalculatePlayerMovement(input);
    assert(dash.velocity.x == dashWithModifiers.velocity.x && dash.velocity.y == dashWithModifiers.velocity.y);
    input.dashing = false;
    const auto slowed = CalculatePlayerMovement(input);
    assert(slowed.velocity.x < dash.velocity.x); // Rail/spin slow desired movement only outside dash.
}
}
int main()
{
    MovementGoldens(); MovementBoundaries();
    std::cout << "PASS: player movement direction, inertia, modifiers and substeps\n";
}