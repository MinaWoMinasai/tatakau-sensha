#pragma once
#include "Struct.h"
#include <d3d12.h>
#include <wrl.h>

class DirectXCommon;

struct InkReticleState {
    Vector2 center{-1.0f, -1.0f}; // viewport pixels; negative selects viewport center
    float spreadDegrees = 0.0f; // major-axis angular half-width
    float verticalSpreadDegrees = 0.0f; // minor-axis half-width; zero for shooters
    float charge = 0.0f; // normalized full charge, supplied by weapon simulation
    float firstChargeRatio = 0.416667f;
    bool stringer = false;
    int projectileCount = 3; // current Stringer family supports one to three arrows
    bool vertical = false; // stringer major axis switches from horizontal to vertical
    bool submerged = false;
    bool outOfInk = false;
    bool target = false; // feedback only; renderer performs no hit detection
    float displayScale = 1.0f; // cosmetic stroke/ring scale; angular projection is unchanged
};

// Draw after post effects / regular HUD sprites into the swapchain target.
// One procedural quad, no textures, descriptors, vertex buffers, or simulation RNG.
// This binds its own pipeline; subsequent sprites must call SpriteCommon::PreDraw.
class InkReticleRenderer final {
public:
    InkReticleRenderer() = default;
    InkReticleRenderer(const InkReticleRenderer&) = delete;
    InkReticleRenderer& operator=(const InkReticleRenderer&) = delete;
    void Initialize(DirectXCommon* dxCommon);
    void Draw(const InkReticleState& state, float viewportWidth,
              float viewportHeight, float verticalFovRadians);
private:
    DirectXCommon* dxCommon_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;
};
