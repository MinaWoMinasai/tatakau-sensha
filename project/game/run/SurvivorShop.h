#pragma once
#include <array>
#include <string>

namespace survivor {
/// @brief 波の間のショップで買えるパーク（計画書 p4 の K4）。共通の6つと、機体ごとの3つ。挙動が変わる物を中心にする。
enum class PerkId {
    Lifesteal,    // 共通：与えたダメージの value 倍を回復する
    KillCooldown, // 共通：倒すたびに、技の待ち時間を value 秒縮める
    Armor,        // 共通：最大HPを value 増やす
    Speed,        // 共通：移動の速さを value 倍だけ上げる（0.12 なら +12%）
    Magnet,       // 共通：欠片を引き寄せる範囲を value 倍だけ広げる
    UltCharge,    // 共通：Q のゲージの溜まり方を value 倍だけ増やす
    Coolant,      // ソルジャー型：主武器の1発の熱を value 倍にする
    RocketRefund, // ソルジャー型：小型ロケットで倒すたびに、ロケットの待ち時間を value 秒縮める
    FieldMedic,   // ソルジャー型：回復の場で回復した量の value 倍だけ、傷も治る
    BeamKill,     // モイラ型：吸収ビームで倒すたびに、吸収ゲージを value 秒ぶん溜める
    PhaseHeal,    // モイラ型：消える移動の終わりに、最大HPの value 倍を回復する
    OrbAbsorb,    // モイラ型：跳ねる球の与えた威力でも、吸収ビームの value 倍だけ吸収ゲージが溜まる
    ChargeShock,  // タンク型：突進の終わりに、半径 value の小さな衝撃波を出す
    BarrierHeal,  // タンク型：バリアで弾を止めるたびに、HP を value 回復する
    Quake,        // タンク型：衝撃波で押し返した数だけ、衝撃波の待ち時間を value 秒縮める
    Count
};
inline constexpr size_t kPerkCount = static_cast<size_t>(PerkId::Count);

/// @brief パークの名前・説明・使える機体（空なら全機体）と、最初の値段と強さ。値段と強さは設定の shop.perks で変えられる。
struct PerkInfo {
    const char* key;
    const char* name;
    const char* text;
    const char* hero;
    int price;
    float value;
};

inline const PerkInfo& GetPerkInfo(PerkId id)
{
    static const std::array<PerkInfo, kPerkCount> kPerks{{
        {"lifesteal", "吸い取る装甲", "与えたダメージの一部で回復する", "", 250, 0.02f},
        {"killCooldown", "連鎖の冷却", "倒すたびに、技の待ち時間が縮む", "", 200, 0.25f},
        {"armor", "厚い装甲", "最大HPが増える", "", 150, 20.0f},
        {"speed", "軽い足回り", "移動が速くなる", "", 150, 0.12f},
        {"magnet", "欠片の磁石", "欠片を遠くから引き寄せる", "", 150, 1.0f},
        {"ultCharge", "見せ場の予感", "Q のゲージが早く溜まる", "", 200, 0.25f},
        {"coolant", "冷却材", "連射砲の熱が溜まりにくい", "soldier", 300, 0.75f},
        {"rocketRefund", "ロケットの再装填", "小型ロケットで倒すたびに、ロケットの待ち時間が縮む", "soldier", 300, 1.0f},
        {"fieldMedic", "野戦の手当て", "回復の場で回復すると、傷も治る", "soldier", 300, 0.5f},
        {"beamKill", "吸い尽くす", "吸収ビームで倒すと、吸収ゲージが溜まる", "moira", 300, 0.5f},
        {"phaseHeal", "消える癒し", "消える移動の終わりに回復する", "moira", 300, 0.08f},
        {"orbAbsorb", "跳ねる吸収", "跳ねる球の威力でも吸収ゲージが溜まる", "moira", 300, 0.5f},
        {"chargeShock", "突進の衝撃", "突進の終わりに、小さな衝撃波を出す", "tank", 300, 3.5f},
        {"barrierHeal", "盾の手当て", "バリアで弾を止めるたびに、HP が少し戻る", "tank", 300, 1.0f},
        {"quake", "地鳴り", "衝撃波で押し返した数だけ、衝撃波の待ち時間が縮む", "tank", 300, 0.3f},
    }};
    return kPerks[static_cast<size_t>(id)];
}

/// @brief 設定での名前からパークを探す。無ければ Count。
inline PerkId FindPerk(const std::string& key)
{
    for (size_t i = 0; i < kPerkCount; ++i) {
        if (key == GetPerkInfo(static_cast<PerkId>(i)).key) {
            return static_cast<PerkId>(i);
        }
    }
    return PerkId::Count;
}

/// @brief 技の強化の対象：主武器と4つの技の枠（0 が主武器、1〜4 が右クリック・Shift・E・Q）。
inline constexpr size_t kUpgradeTargets = 5;
/// @brief 技の強化の段の上限。3段目で挙動が変わる。
inline constexpr int kMaxUpgradeTier = 3;

/// @brief ショップの設定（計画書 p4 の K4）。
/// @note 並ぶ数は offerCount（修理はいつも別に並ぶ）。引き直しは rerollCost で、引くたびに rerollStep 高くなる（波ごとに戻る）。
/// 技の強化の値段は段ごとに upgradePrices。パークの値段と強さは perks（PerkInfo の最初の値を上書きする）。
struct ShopConfig {
    int offerCount = 5;
    int rerollCost = 50;
    int rerollStep = 25;
    std::array<int, kMaxUpgradeTier> upgradePrices{120, 250, 400};
    std::array<int, kPerkCount> perkPrices = [] {
        std::array<int, kPerkCount> prices{};
        for (size_t i = 0; i < kPerkCount; ++i) {
            prices[i] = GetPerkInfo(static_cast<PerkId>(i)).price;
        }
        return prices;
    }();
    std::array<float, kPerkCount> perkValues = [] {
        std::array<float, kPerkCount> values{};
        for (size_t i = 0; i < kPerkCount; ++i) {
            values[i] = GetPerkInfo(static_cast<PerkId>(i)).value;
        }
        return values;
    }();
};
} // namespace survivor
