#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// Bounded state shared by combat and graphics-free tests. No scene resources.
namespace tankshooter {
inline constexpr int kChainTargets = 2;
inline constexpr int kBurstChildren = 6;
inline constexpr float kChainRadius = 7.0f;
/// @brief 戻り弾の帰還の進行と移動方向を計算する。
struct ReturnFlight {
    float age = 0;
    bool returning = false;
    /// @brief 現在の状態を1段階更新する。
    /// @param dt この処理で進める経過時間（秒）。
    bool Step(float dt)
    {
        age += (std::max)(0.0f, dt);
        if (!returning && age >= .60f) {
            returning = true;
            return true;
        }
        return false;
    }
};
/// @brief 世代付きの対象IDごとにマーキングの蓄積数と寿命を管理する。
struct MarkLedger {
    /// @brief 1対象のマーキングの蓄積数と有効時間を保持する。
    struct Mark {
        uint64_t id = 0;
        int stacks = 0;
        float remaining = 0;
    };
    std::array<Mark, 128> entries{};
    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    /// @param dt この処理で進める経過時間（秒）。
    void Update(float dt)
    {
        for (auto& m : entries)
            if (m.id) {
                m.remaining -= (std::max)(0.0f, dt);
                if (m.remaining <= 0)
                    m = {};
            }
    }
    /// @brief 対象へ命中した結果を現在の状態へ反映する。
    bool Hit(uint64_t id, bool boss)
    {
        Mark* slot = nullptr;
        for (auto& m : entries) {
            if (m.id == id) {
                slot = &m;
                break;
            }
            if (!slot && !m.id)
                slot = &m;
        }
        if (!slot)
            return false;
        if (slot->id != id)
            *slot = {id, 0, 4};
        slot->remaining = 4;
        if (++slot->stacks >= (boss ? 6 : 4)) {
            *slot = {};
            return true;
        }
        return false;
    }
};
/// @brief 指定ダメージを戦闘状態へ反映する。
inline uint32_t Damage(uint32_t source, float scale, float power = 1)
{
    return static_cast<uint32_t>((std::max)(1.0f, std::round(static_cast<float>(source) * scale * (std::clamp)(power, .1f, 5.0f))));
}
} // namespace tankshooter
