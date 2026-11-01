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
/// @brief 育つ戦車の試作の、床に落ちている物（計画書 p3 の W3）。弾は近づくと寄ってきて触れると拾い、素材・チップ・武器はキーで拾う。
/// @note 欠片（SurvivorPickups）と同じく、1つずつの Object3d にはせず、ネオンの格子のパスに小さな塗りの多角形として積む。
/// チップと武器は、レア度の色の輪を脈打たせて目立たせる。
class SurvivorLootField {
public:
    void SetConfig(const survivor::LootConfig& config)
    {
        config_ = config;
    }

    /// @brief item を position の周りに落とす（少し散らばる）。
    void Drop(const myEngine::Vector3& position, const survivor::LootItem& item, std::mt19937& random);
    /// @brief 物を動かし、触れた弾を拾う。拾った弾の数を種類ごとに返す。
    /// @param acceptAmmo 種類ごとに、拾えるか（予備が上限なら拾わずに残す）。
    std::array<int, survivor::kAmmoTypeCount> Update(float dt, const myEngine::Vector3& player,
                                                     const std::array<bool, survivor::kAmmoTypeCount>& acceptAmmo);
    /// @brief position から pickupRadius 以内の、キーで拾う物を全部取り出す。
    std::vector<survivor::LootItem> TakeNear(const myEngine::Vector3& position);
    /// @brief position から pickupRadius 以内の、キーで拾う物の数と、一番近い物（無ければ nullptr）。
    size_t CountNear(const myEngine::Vector3& position, const survivor::LootItem** nearest = nullptr) const;
    /// @brief 画面の近くにある物を積む。
    void Queue(myEngine::NeonGridRenderer& renderer, const myEngine::Vector3& cameraRight, const myEngine::Vector3& cameraUp,
               const myEngine::Vector3& viewCenter, float halfWidth, float halfHeight) const;
    /// @brief position から maxDistance 以内で最も近い物の位置を返す。無ければ false。
    bool FindNearest(const myEngine::Vector3& position, float maxDistance, myEngine::Vector3& out) const;

    void Clear()
    {
        items_.clear();
    }

    size_t GetCount() const
    {
        return items_.size();
    }

private:
    struct Item {
        survivor::LootItem loot{};
        myEngine::Vector3 position{};
        myEngine::Vector3 velocity{};
        float age = 0.0f;
        bool attracted = false;
        bool taken = false;
    };

    static bool IsKeyPickup(const survivor::LootItem& loot)
    {
        return loot.kind != survivor::LootKind::Ammo;
    }

    survivor::LootConfig config_;
    std::vector<Item> items_;
};
} // namespace gameplay
