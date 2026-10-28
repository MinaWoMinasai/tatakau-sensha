#pragma once
#include <algorithm>
#include <array>
#include <cstdio>
#include <random>
#include <string>
#include <vector>
#include "game/player/TankRunModifiers.h"

namespace survivor {
/// @brief 育つ戦車の試作の部品。並びは保存や設定の名前と結び付けないので、途中に足してよい。
enum class PartId {
    Armor,     // 装甲板
    Ram,       // 衝角
    Warhead,   // 弾頭
    Loader,    // 装填機
    Thruster,  // 推進器
    Collector, // 回収器
    Count
};

inline constexpr size_t kPartCount = static_cast<size_t>(PartId::Count);

/// @brief 部品の分類（設計案 5.2）。形態の「得意な分類」で効きが変わる（後のカード）。
enum class PartCategory {
    Bullet,
    Drone,
    Dash,
    Ram,
    Heat,
    Shield,
    Kill,
    Shard,
    Body
};

/// @brief 部品の定義。数値は1段（Lv）ごとの上がり幅。
struct PartDefinition {
    PartId id = PartId::Armor;
    const char* key = "";  // 設定ファイルでの名前
    const char* name = ""; // 画面に出す名前
    const char* stat = ""; // 1段ごとに上がるもの（説明文に使う）
    PartCategory category = PartCategory::Body;
    int weight = 1;        // 積載量に使う重さ（積載量は後のカード）
    int maxLevel = 5;      // 軽い部品は5、ほかは3
    bool light = true;     // 軽い部品（能力値の代わり）か
    float perLevel = 0.1f; // 1段ごとの割合（0.12 なら +12%）
};

/// @brief 熱を使わない軽い部品6種（設計案 4章）。数値は旧 diep 風の能力値の上がり幅を出発点にした既定値。
inline std::array<PartDefinition, kPartCount> DefaultParts()
{
    return {{
        {PartId::Armor, "armor", "装甲板", "最大HP", PartCategory::Body, 1, 5, true, 0.12f},
        {PartId::Ram, "ram", "衝角", "体当たりの威力", PartCategory::Ram, 1, 5, true, 0.20f},
        {PartId::Warhead, "warhead", "弾頭", "弾の威力", PartCategory::Bullet, 1, 5, true, 0.10f},
        {PartId::Loader, "loader", "装填機", "連射", PartCategory::Bullet, 1, 5, true, 0.12f},
        {PartId::Thruster, "thruster", "推進器", "移動の速さ", PartCategory::Dash, 1, 5, true, 0.08f},
        {PartId::Collector, "collector", "回収器", "欠片を引き寄せる範囲", PartCategory::Shard, 1, 5, true, 0.25f},
    }};
}

/// @brief 部品ごとの今の Lv（0は持っていない）。
using PartLevels = std::array<int, kPartCount>;

/// @brief 部品の Lv から、自機の性能の倍率を作る。連射は「1秒あたりの発射数」を上げるので、間隔は割る。
inline TankPartScales MakePartScales(const std::array<PartDefinition, kPartCount>& parts, const PartLevels& levels)
{
    auto bonus = [&](PartId id) {
        const size_t index = static_cast<size_t>(id);
        return 1.0f + parts[index].perLevel * static_cast<float>(levels[index]);
    };
    TankPartScales scales{};
    scales.maxHp = bonus(PartId::Armor);
    scales.bodyDamage = bonus(PartId::Ram);
    scales.bulletDamage = bonus(PartId::Warhead);
    scales.reloadInterval = 1.0f / bonus(PartId::Loader);
    scales.moveSpeed = bonus(PartId::Thruster);
    return scales;
}

/// @brief 回収器による、欠片を引き寄せる範囲の倍率を返す。
inline float CollectorMagnetScale(const std::array<PartDefinition, kPartCount>& parts, const PartLevels& levels)
{
    const size_t index = static_cast<size_t>(PartId::Collector);
    return 1.0f + parts[index].perLevel * static_cast<float>(levels[index]);
}

/// @brief 3択の1枚。part が無効（Count）のときは、修理（HP回復）の札。
struct PartOffer {
    PartId part = PartId::Count;
    int nextLevel = 0;
};

/// @brief 3択の札の説明文を作る（例：「最大HP +12%（合計 +24%）」）。
inline std::string DescribeOffer(const std::array<PartDefinition, kPartCount>& parts, const PartOffer& offer)
{
    if (offer.part == PartId::Count) {
        return "HP を 30% 回復する";
    }
    const PartDefinition& part = parts[static_cast<size_t>(offer.part)];
    char text[128]{};
    std::snprintf(text, sizeof(text), "%s +%d%%\n（合計 +%d%%）", part.stat, static_cast<int>(part.perLevel * 100.0f + 0.5f),
                  static_cast<int>(part.perLevel * 100.0f * static_cast<float>(offer.nextLevel) + 0.5f));
    return text;
}

/// @brief 3択を引く。最大 Lv の部品は出さず、同じ部品は重ねない。1枚目は軽い部品を lightBias 倍出やすくする。
/// @note 引ける部品が3つに足りないときは、残りを修理の札で埋める。
inline std::vector<PartOffer> DrawOffers(const std::array<PartDefinition, kPartCount>& parts, const PartLevels& levels, float lightBias,
                                         std::mt19937& random)
{
    std::vector<size_t> pool;
    for (size_t i = 0; i < kPartCount; ++i) {
        if (levels[i] < parts[i].maxLevel) {
            pool.push_back(i);
        }
    }
    std::vector<PartOffer> offers;
    for (int slot = 0; slot < 3; ++slot) {
        if (pool.empty()) {
            offers.push_back({});
            continue;
        }
        std::vector<float> weights;
        for (size_t index : pool) {
            weights.push_back(slot == 0 && parts[index].light ? (std::max)(1.0f, lightBias) : 1.0f);
        }
        std::discrete_distribution<size_t> pick(weights.begin(), weights.end());
        const size_t chosen = pick(random);
        const size_t index = pool[chosen];
        offers.push_back({static_cast<PartId>(index), levels[index] + 1});
        pool.erase(pool.begin() + static_cast<std::ptrdiff_t>(chosen));
    }
    return offers;
}
} // namespace survivor
