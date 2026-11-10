#pragma once
#include "Struct.h"
#include "game/run/SurvivorLoot.h"
#include <array>
#include <cstdint>
#include <random>
#include <vector>

namespace myEngine {
class NeonGridRenderer;
}

namespace gameplay {
/// @brief 育つ戦車の試作の、床に落ちている物（計画書 p3 の W3・X1）。どれも近づくと寄ってきて、触れると拾う。
/// @note 欠片（SurvivorPickups）と同じく、1つずつの Object3d にはせず、ネオンの格子のパスに小さな塗りの多角形として積む。
/// チップと武器は、レア度の色の輪を脈打たせて目立たせる。
class SurvivorLootField {
public:
    void SetConfig(const survivor::LootConfig& config)
    {
        config_ = config;
    }

    void SetGlow(const survivor::GlowConfig& glow)
    {
        glow_ = glow;
    }

    /// @brief item を position の周りに落とす（少し散らばる）。
    void Drop(const myEngine::Vector3& position, const survivor::LootItem& item, std::mt19937& random);
    /// @brief 物を動かし、触れた物を拾って返す。
    /// @param acceptAmmo 弾の種類ごとに、拾えるか（予備が上限なら拾わずに残す）。
    std::vector<survivor::LootItem> Update(float dt, const myEngine::Vector3& player,
                                           const std::array<bool, survivor::kAmmoTypeCount>& acceptAmmo);
    /// @brief 画面の近くにある物を積む。
    void Queue(myEngine::NeonGridRenderer& renderer, const myEngine::Vector3& cameraRight, const myEngine::Vector3& cameraUp,
               const myEngine::Vector3& viewCenter, float halfWidth, float halfHeight) const;
    /// @brief position から maxDistance 以内で最も近い物の位置を返す。無ければ false。
    bool FindNearest(const myEngine::Vector3& position, float maxDistance, myEngine::Vector3& out) const;

    void Clear()
    {
        items_.clear();
        flashes_.clear();
    }

    size_t GetCount() const
    {
        return items_.size();
    }

private:
    /// @brief 光の尾の点の数（古い順に詰める）。
    static constexpr size_t kTrailPoints = 8;

    struct Item {
        survivor::LootItem loot{};
        myEngine::Vector3 position{};
        myEngine::Vector3 velocity{};
        float age = 0.0f;
        bool attracted = false;
        bool taken = false;
        /// @brief 吸い込まれている間の、少し前の位置（光の尾）。
        std::array<myEngine::Vector3, kTrailPoints> trail{};
        size_t trailCount = 0;
        float trailTimer = 0.0f;
    };

    /// @brief 拾ったところに広がる光の輪。
    struct Flash {
        myEngine::Vector3 position{};
        myEngine::Vector4 color{};
        float age = 0.0f;
    };

    static myEngine::Vector4 ItemColor(const survivor::LootItem& loot);

    survivor::LootConfig config_;
    survivor::GlowConfig glow_;
    std::vector<Item> items_;
    std::vector<Flash> flashes_;
};
} // namespace gameplay
