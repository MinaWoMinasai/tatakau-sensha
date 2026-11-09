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
    Armor,          // 装甲板
    Ram,            // 衝角
    Warhead,        // 弾頭
    Loader,         // 装填機
    Thruster,       // 推進器
    Collector,      // 回収器
    Cooler,         // 冷却器
    ShieldCapacity, // シールド容量
    Pierce,         // 貫通
    ChainLightning, // 連鎖放電
    KillBurst,      // 撃破バースト
    DashBurst,      // 衝撃放電
    HeavyWarhead,   // 重弾頭
    HeatVent,       // 排熱砲（Q）
    ResidualHeat,   // 余熱
    RamShield,      // 衝角シールド
    Morale,         // 戦意
    EmergencyPurge, // 緊急放出（Q、使い切り）
    ChainCollapse,  // 連鎖崩壊（合体：貫通＋撃破バースト）
    Thunderclap,    // 雷鳴（合体：連鎖放電＋戦意）
    Rampage,        // 爆走（合体：衝撃放電＋衝角シールド）
    CriticalPoint,  // 臨界点（合体：重弾頭＋余熱）
    Count
};

inline constexpr size_t kPartCount = static_cast<size_t>(PartId::Count);
/// @brief 軽い部品の挙動が変わる段。
inline constexpr int kLightChangeLevel = 3;

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

/// @brief 部品の定義。
/// @note 軽い部品の perLevel は1段（Lv）ごとの上がり幅の割合。ほかの部品は、Lv2 から1段ごとに効果の強さ（effectPower）へ足す量。
struct PartDefinition {
    PartId id = PartId::Armor;
    const char* key = "";  // 設定ファイルでの名前
    const char* name = ""; // 画面に出す名前
    const char* stat = ""; // 軽い部品：1段ごとに上がるもの。ほかの部品：効果の説明
    PartCategory category = PartCategory::Body;
    int weight = 1;        // 積載量に使う重さ
    int maxLevel = 5;      // 軽い部品は5、ほかは3
    bool light = true;     // 軽い部品（能力値の代わり）か
    float perLevel = 0.1f;
    const char* change = ""; // 軽い部品の3段で変わること
    bool qSkill = false;     // Q で使う部品か。Q の枠は1つなので、2つ目は入れ替えになる
    // 合体の部品なら、元の2つ（両方を最大 Lv にすると3択に出る）。合体でなければ Count。
    PartId fuseA = PartId::Count;
    PartId fuseB = PartId::Count;
};

/// @brief 合体の部品か。
inline bool IsFusion(const PartDefinition& part)
{
    return part.fuseA != PartId::Count;
}

/// @brief 部品の既定値（設計案 4章・5.3）。数値は旧 diep 風の能力値の上がり幅と、遠征のカードの強さを出発点にした。
inline std::array<PartDefinition, kPartCount> DefaultParts()
{
    return {{
        {PartId::Armor, "armor", "装甲板", "最大HP", PartCategory::Body, 1, 5, true, 0.12f, "被弾後の無敵が延びる"},
        {PartId::Ram, "ram", "衝角", "体当たりの威力", PartCategory::Ram, 1, 5, true, 0.20f, "車体にトゲ：小さなシェイプは\n触れるだけで壊れる"},
        {PartId::Warhead, "warhead", "弾頭", "弾の威力", PartCategory::Bullet, 1, 5, true, 0.10f, "会心：ときどき威力2倍"},
        {PartId::Loader, "loader", "装填機", "連射", PartCategory::Bullet, 1, 5, true, 0.12f, "撃ち続けるほど連射が速くなる"},
        {PartId::Thruster, "thruster", "推進器", "移動の速さ", PartCategory::Dash, 1, 5, true, 0.08f, "ダッシュの回数 +1"},
        {PartId::Collector, "collector", "回収器", "欠片を引き寄せる範囲", PartCategory::Shard, 1, 5, true, 0.25f, "欠片が自分から寄ってくる"},
        {PartId::Cooler, "cooler", "冷却器", "冷える速さ", PartCategory::Heat, 1, 5, true, 0.20f, "熱の上限 +30"},
        {PartId::ShieldCapacity, "shieldCapacity", "シールド容量", "シールドの上限", PartCategory::Shield, 1, 5, true, 0.25f,
         "回復が始まるまでが半分"},
        {PartId::Pierce, "pierce", "貫通", "弾が敵を1体貫く", PartCategory::Bullet, 1, 3, false, 0.5f, ""},
        {PartId::ChainLightning, "chainLightning", "連鎖放電", "当てた敵から\n別の敵へ放電する", PartCategory::Bullet, 2, 3, false, 0.5f, ""},
        {PartId::KillBurst, "killBurst", "撃破バースト", "倒した敵から\n6方向に小さな弾", PartCategory::Kill, 2, 3, false, 0.5f, ""},
        {PartId::DashBurst, "dashBurst", "衝撃放電", "ダッシュの始めに\n周りへ衝撃波", PartCategory::Dash, 1, 3, false, 0.5f, ""},
        {PartId::HeavyWarhead, "heavyWarhead", "重弾頭", "威力 +60%\n1発ごとの熱 +2", PartCategory::Heat, 1, 3, false, 0.5f, ""},
        {PartId::HeatVent, "heatVent", "排熱砲", "Q：熱を全部放ち、\n熱に比例した太い弾", PartCategory::Heat, 2, 3, false, 0.5f, "", true},
        {PartId::ResidualHeat, "residualHeat", "余熱", "熱が低いほど威力が上がる\n（熱0で +50%）", PartCategory::Heat, 1, 3, false, 0.5f, ""},
        {PartId::RamShield, "ramShield", "衝角シールド", "シールドが多いほど\n体当たりが強い（満タンで +150%）", PartCategory::Shield, 2, 3, false, 0.5f, ""},
        {PartId::Morale, "morale", "戦意", "倒すたびに主砲の威力 +0.1%\n（区画の間ずっと）", PartCategory::Kill, 1, 3, false, 0.5f, ""},
        {PartId::EmergencyPurge, "emergencyPurge", "緊急放出", "Q：一番重い部品を壊し、\n重さに比例した大爆発（1回）", PartCategory::Kill, 0, 3, false, 0.5f, "",
         true},
        {PartId::ChainCollapse, "chainCollapse", "連鎖崩壊", "小さな弾が敵を倒すと、\nもう一度6方向に弾", PartCategory::Kill, 2, 1,
         false, 0.5f, "", false, PartId::Pierce, PartId::KillBurst},
        {PartId::Thunderclap, "thunderclap", "雷鳴", "放電が5体まで届き、\n届く距離 +40%", PartCategory::Bullet, 2, 1, false, 0.5f, "", false,
         PartId::ChainLightning, PartId::Morale},
        {PartId::Rampage, "rampage", "爆走", "ダッシュの衝撃波が大きく強く、\nシールドを半分回復", PartCategory::Dash, 2, 1, false, 0.5f, "", false,
         PartId::DashBurst, PartId::RamShield},
        {PartId::CriticalPoint, "criticalPoint", "臨界点", "オーバーヒートした瞬間に、\n周りへ大爆発", PartCategory::Heat, 1, 1, false, 0.5f, "",
         false, PartId::HeavyWarhead, PartId::ResidualHeat},
    }};
}

/// @brief 合体の部品の重さを、元の2つの重さの合計から1を引いた値にそろえる（設定で元の重さを変えたときも追従する）。
inline void SyncFusionWeights(std::array<PartDefinition, kPartCount>& parts)
{
    for (auto& part : parts) {
        if (IsFusion(part)) {
            part.weight = (std::max)(0, parts[static_cast<size_t>(part.fuseA)].weight + parts[static_cast<size_t>(part.fuseB)].weight - 1);
        }
    }
}

/// @brief 部品ごとの今の Lv（0は持っていない）。
using PartLevels = std::array<int, kPartCount>;

/// @brief 効果を作るときの Lv。持っている合体の部品の、元の2つを最大 Lv として扱う（積載や表示には使わない）。
inline PartLevels EffectiveLevels(const std::array<PartDefinition, kPartCount>& parts, const PartLevels& levels)
{
    PartLevels effective = levels;
    for (size_t i = 0; i < kPartCount; ++i) {
        if (levels[i] > 0 && IsFusion(parts[i])) {
            for (PartId from : {parts[i].fuseA, parts[i].fuseB}) {
                effective[static_cast<size_t>(from)] = parts[static_cast<size_t>(from)].maxLevel;
            }
        }
    }
    return effective;
}

inline int LevelOf(const PartLevels& levels, PartId id)
{
    return levels[static_cast<size_t>(id)];
}

/// @brief 積んでいる部品（Lv1以上）の重さの合計を返す。Lv を上げても重さは変わらない。
inline int LoadOf(const std::array<PartDefinition, kPartCount>& parts, const PartLevels& levels)
{
    int load = 0;
    for (size_t i = 0; i < kPartCount; ++i) {
        if (levels[i] > 0) {
            load += parts[i].weight;
        }
    }
    return load;
}

/// @brief 積んでいる Q の部品を返す。無ければ Count。
inline PartId OwnedQSkill(const std::array<PartDefinition, kPartCount>& parts, const PartLevels& levels)
{
    for (size_t i = 0; i < kPartCount; ++i) {
        if (levels[i] > 0 && parts[i].qSkill) {
            return static_cast<PartId>(i);
        }
    }
    return PartId::Count;
}

/// @brief この札を積むと、今の Q の部品と入れ替えになるか（Q の枠は1つ）。入れ替えになるなら、外れる部品を返す。
inline PartId ReplacedQSkill(const std::array<PartDefinition, kPartCount>& parts, const PartLevels& levels, PartId id)
{
    if (id == PartId::Count || !parts[static_cast<size_t>(id)].qSkill || levels[static_cast<size_t>(id)] > 0) {
        return PartId::Count;
    }
    return OwnedQSkill(parts, levels);
}

/// @brief 合体の札を積むと外れる、元の2つの重さの合計を返す。合体でなければ0。
inline int FusedAwayWeight(const std::array<PartDefinition, kPartCount>& parts, const PartLevels& levels, PartId id)
{
    if (id == PartId::Count || !IsFusion(parts[static_cast<size_t>(id)])) {
        return 0;
    }
    int weight = 0;
    for (PartId from : {parts[static_cast<size_t>(id)].fuseA, parts[static_cast<size_t>(id)].fuseB}) {
        if (levels[static_cast<size_t>(from)] > 0) {
            weight += parts[static_cast<size_t>(from)].weight;
        }
    }
    return weight;
}

/// @brief 合体の札を3択に出せるか（元の2つがどちらも最大 Lv で、合体の部品をまだ持っていない）。
inline bool FusionReady(const std::array<PartDefinition, kPartCount>& parts, const PartLevels& levels, PartId id)
{
    const PartDefinition& part = parts[static_cast<size_t>(id)];
    if (!IsFusion(part) || levels[static_cast<size_t>(id)] > 0) {
        return false;
    }
    return levels[static_cast<size_t>(part.fuseA)] >= parts[static_cast<size_t>(part.fuseA)].maxLevel &&
           levels[static_cast<size_t>(part.fuseB)] >= parts[static_cast<size_t>(part.fuseB)].maxLevel;
}

/// @brief この部品が、持っている合体の部品の元になっているか（合体した後は、元の部品を3択に出さない）。
inline bool FusedInto(const std::array<PartDefinition, kPartCount>& parts, const PartLevels& levels, PartId id)
{
    for (size_t i = 0; i < kPartCount; ++i) {
        if (levels[i] > 0 && IsFusion(parts[i]) && (parts[i].fuseA == id || parts[i].fuseB == id)) {
            return true;
        }
    }
    return false;
}

/// @brief 3択の札を積むと、積載を超えるか。持っている部品の Lv 上げと修理は超えない。Q の入れ替えは、外れる部品の重さを引いて見る。
inline bool ExceedsLoad(const std::array<PartDefinition, kPartCount>& parts, const PartLevels& levels, PartId id, int capacity)
{
    if (id == PartId::Count || levels[static_cast<size_t>(id)] > 0) {
        return false;
    }
    int load = LoadOf(parts, levels);
    const PartId replaced = ReplacedQSkill(parts, levels, id);
    if (replaced != PartId::Count) {
        load -= parts[static_cast<size_t>(replaced)].weight;
    }
    load -= FusedAwayWeight(parts, levels, id);
    return load + parts[static_cast<size_t>(id)].weight > capacity;
}

/// @brief 軽い部品が3段に達しているか。
inline bool HasChange(const PartLevels& levels, PartId id)
{
    return LevelOf(levels, id) >= kLightChangeLevel;
}

/// @brief 軽い部品の倍率（1 + 1段ごとの割合 × Lv）。
inline float LightBonus(const std::array<PartDefinition, kPartCount>& parts, const PartLevels& levels, PartId id)
{
    const size_t index = static_cast<size_t>(id);
    return 1.0f + parts[index].perLevel * static_cast<float>(levels[index]);
}

/// @brief 軽い部品以外の効果の強さ（Lv1 で1、1段ごとに perLevel ずつ上がる）。持っていなければ0。
inline float EffectPower(const std::array<PartDefinition, kPartCount>& parts, const PartLevels& levels, PartId id)
{
    const int level = LevelOf(levels, id);
    return level <= 0 ? 0.0f : 1.0f + parts[static_cast<size_t>(id)].perLevel * static_cast<float>(level - 1);
}

/// @brief 部品の Lv から、自機の性能の倍率を作る。連射は「1秒あたりの発射数」を上げるので、間隔は割る。
inline TankPartScales MakePartScales(const std::array<PartDefinition, kPartCount>& parts, const PartLevels& levels)
{
    TankPartScales scales{};
    scales.maxHp = LightBonus(parts, levels, PartId::Armor);
    scales.bodyDamage = LightBonus(parts, levels, PartId::Ram);
    scales.bulletDamage = LightBonus(parts, levels, PartId::Warhead) * (1.0f + 0.6f * EffectPower(parts, levels, PartId::HeavyWarhead));
    scales.reloadInterval = 1.0f / LightBonus(parts, levels, PartId::Loader);
    scales.moveSpeed = LightBonus(parts, levels, PartId::Thruster);
    scales.extraStamina = HasChange(levels, PartId::Thruster) ? 1.0f : 0.0f;
    return scales;
}

/// @brief 既存のカードの効果を使う部品を、自機の強化へ反映する。強さは effectPower に入れる。
/// @note 合体の部品の元の2つも効かせるには、levels に EffectiveLevels を渡す。
inline void ApplyPartCards(const std::array<PartDefinition, kPartCount>& parts, const PartLevels& levels, TankRunModifiers& modifiers)
{
    auto apply = [&](PartId id, bool& flag, tankrun::CardId card) {
        const float power = EffectPower(parts, levels, id);
        flag = power > 0.0f;
        modifiers.effectPower[tankrun::CardIndex(card)] = flag ? power : 1.0f;
    };
    apply(PartId::Pierce, modifiers.pierce, tankrun::CardId::Pierce);
    apply(PartId::ChainLightning, modifiers.chainLightning, tankrun::CardId::ChainLightning);
    apply(PartId::KillBurst, modifiers.killBurst, tankrun::CardId::KillBurst);
    apply(PartId::DashBurst, modifiers.dashBurst, tankrun::CardId::DashBurst);
}

/// @brief 回収器による、欠片を引き寄せる範囲の倍率を返す。
inline float CollectorMagnetScale(const std::array<PartDefinition, kPartCount>& parts, const PartLevels& levels)
{
    return LightBonus(parts, levels, PartId::Collector);
}

/// @brief 3択の1枚。part が無効（Count）のときは、修理（HP回復）の札。
struct PartOffer {
    PartId part = PartId::Count;
    int nextLevel = 0;
};

/// @brief 3択の札の説明文を作る（例：「最大HP +12%（合計 +24%）」）。3段に上がる札には、変わることも書く。
inline std::string DescribeOffer(const std::array<PartDefinition, kPartCount>& parts, const PartOffer& offer)
{
    if (offer.part == PartId::Count) {
        return "HP を 30% 回復する";
    }
    const PartDefinition& part = parts[static_cast<size_t>(offer.part)];
    char text[192]{};
    if (part.light) {
        std::snprintf(text, sizeof(text), "%s +%d%%\n（合計 +%d%%）", part.stat, static_cast<int>(part.perLevel * 100.0f + 0.5f),
                      static_cast<int>(part.perLevel * 100.0f * static_cast<float>(offer.nextLevel) + 0.5f));
        std::string result = text;
        if (offer.nextLevel == kLightChangeLevel) {
            result += std::string("\n3段：") + part.change;
        } else if (offer.nextLevel < kLightChangeLevel) {
            result += "\n（3段で変化あり）";
        }
        return result;
    }
    std::snprintf(text, sizeof(text), "%s", part.stat);
    std::string result = text;
    if (IsFusion(part)) {
        return result;
    }
    if (offer.nextLevel > 1) {
        std::snprintf(text, sizeof(text), "\n強さ x%.1f", 1.0f + part.perLevel * static_cast<float>(offer.nextLevel - 1));
        result += text;
    }
    return result;
}

/// @brief 3択を引く。最大 Lv の部品は出さず、同じ部品は重ねない。1枚目は軽い部品を lightBias 倍出やすくする。
/// @param premium 真なら軽い部品を出さない（大五角の宝箱から得た、質の高い選択）。
/// @note 引ける部品が3つに足りないときは、残りを修理の札で埋める。
/// @param banned 真の部品は出さない（緊急放出で壊した部品）。nullptr なら全部出せる。
/// @note 合体の札は、元の2つが最大 Lv のときだけ、fusionBias 倍出やすくして混ぜる。合体した後は、元の2つを出さない。
inline std::vector<PartOffer> DrawOffers(const std::array<PartDefinition, kPartCount>& parts, const PartLevels& levels, float lightBias,
                                         std::mt19937& random, bool premium = false, const std::array<bool, kPartCount>* banned = nullptr,
                                         float fusionBias = 3.0f)
{
    std::vector<size_t> pool;
    for (size_t i = 0; i < kPartCount; ++i) {
        const PartId id = static_cast<PartId>(i);
        if (IsFusion(parts[i]) ? !FusionReady(parts, levels, id) : FusedInto(parts, levels, id)) {
            continue;
        }
        if (levels[i] < parts[i].maxLevel && !(premium && parts[i].light) && !(banned && (*banned)[i])) {
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
            weights.push_back(IsFusion(parts[index])                ? (std::max)(1.0f, fusionBias)
                              : slot == 0 && parts[index].light ? (std::max)(1.0f, lightBias)
                                                                : 1.0f);
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
