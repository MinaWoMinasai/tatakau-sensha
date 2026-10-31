#pragma once
#include "game/player/PlayerClassConfig.h"
#include "game/run/SurvivorParts.h"
#include <array>
#include <cstdio>
#include <string>
#include <vector>

namespace survivor {
/// @brief 育つ戦車の試作の形態。Twin・LongBarrel は Lv5 で選ぶ第1段、ほかは親から進化する第2段（FormDefinition::stage・parent）。
/// @note 並びは保存や設定の名前（key）と結び付けないので、途中に足してよい。
enum class FormId {
    Twin,
    LongBarrel,
    Triple,     // 三連（連装から）
    MachineGun, // 機関砲（連装から）
    Thunder,    // 雷砲（連装から、鍵：連鎖放電）
    Sniper,     // 狙撃（長砲身から）
    Ricochet,   // 反射砲（長砲身から）
    Beam,       // 光線砲（長砲身から、鍵：貫通）
    Count
};
inline constexpr size_t kFormCount = static_cast<size_t>(FormId::Count);

/// @brief 育つ戦車の試作の、区画の始めに選ぶ特性（D3）。数値は survivor_proto.json の traits。
enum class TraitId {
    Standard,       // 標準：変化なし
    CoolingFurnace, // 冷却炉：熱の上限が増え、冷える速さが落ちる
    Count
};
inline constexpr size_t kTraitCount = static_cast<size_t>(TraitId::Count);

/// @brief 特性の設定での名前（遊んだ記録にも使う）。
inline const char* TraitKey(TraitId id)
{
    return id == TraitId::CoolingFurnace ? "coolingFurnace" : "standard";
}

/// @brief 育つ戦車の試作の、大五角の宝箱から得るレリック（D4）。重さ0で、積載を使わない。数値は survivor_proto.json の relics。
enum class RelicId {
    OneShot,     // 一発屋：威力が大きく上がり、連射が大きく落ちる
    FixedTurret, // 不動砲台：止まっている間は威力が上がり、ダッシュできない
    Count
};
inline constexpr size_t kRelicCount = static_cast<size_t>(RelicId::Count);

/// @brief レリックの設定での名前（遊んだ記録にも使う）。
inline const char* RelicKey(RelicId id)
{
    return id == RelicId::FixedTurret ? "fixedTurret" : "oneShot";
}

/// @brief レリックの画面に出す名前。
inline const char* RelicName(RelicId id)
{
    return id == RelicId::FixedTurret ? "不動砲台" : "一発屋";
}

/// @brief 特性の画面に出す名前。
inline const char* TraitName(TraitId id)
{
    return id == TraitId::CoolingFurnace ? "冷却炉" : "標準";
}

/// @brief 形態の数値。倍率は元の機体設定（sourceClass）に対して掛けるのではなく、そのまま使う。
/// @note stage 1 は Lv5（formLevel）で選ぶ形態、stage 2 は親（parent）の形態から stage2Level で進化する形態。
/// key が Count でなければ、その部品が最大 Lv（合体の元も含む）のときだけ候補に出る（鍵付きの進化）。
struct FormDefinition {
    const char* key = "";
    const char* name = "";
    /// @brief 札に出す一言（何が変わるか）。
    const char* summary = "";
    /// @brief 元にする playerClasses.json の機体。
    const char* sourceClass = "Basic";
    int stage = 1;
    FormId parent = FormId::Count;
    PartId keyPart = PartId::Count;
    /// @brief 長砲身の系統（1本の長い砲身・五角の車体・右クリックのため撃ち）か。
    bool longLine = false;
    /// @brief 砲門を扇形に開く角度（度）。0 なら元の機体の向きのまま。3門なら -角度・0・+角度。
    float fanAngle = 0.0f;
    /// @brief 弾のばらつき（度）。負なら元の機体のまま。
    float spreadDeg = -1.0f;
    /// @brief 撃ち続けるほど連射が速くなる（装填機の3段と同じ加速を、形態が最初から持つ）。
    bool fireRamp = false;
    /// @brief 主砲で倒した敵と、markHits 回（宿敵は markHits+1 回）当てた敵を、周りごと爆破する（BulletManager の印）。
    bool markDetonation = false;
    int markHits = 2;
    /// @brief 主砲の弾が壁で跳ね返る回数。
    int wallBounces = 0;
    /// @brief 連鎖放電の届く数に足す数と、届く距離の倍率（雷砲。合体の雷鳴とは足し合わせる）。
    int chainExtraTargets = 0;
    float chainRadiusScale = 1.0f;
    /// @brief ため撃ちの弾の太さの倍率と、貫く数に足す数（光線砲）。
    float railWidth = 1.0f;
    int railPierce = 0;
    float reloadScale = 1.0f;
    float damageScale = 1.0f;
    float speedScale = 1.0f;
    /// @brief 主砲1発の熱の倍率。
    float heatScale = 1.0f;
    /// @brief 砲身の長さの倍率。
    float barrelLength = 1.0f;
    /// @brief 右クリックのため撃ちを持つか。持つなら威力の強さと熱を使う。
    bool railCharge = false;
    float railPower = 1.0f;
    float railHeatBase = 10.0f;
    float railHeatPerCharge = 25.0f;
};

/// @brief 形態の既定値。survivor_proto.json の forms で上書きする。第2段の数値は仮（固有の能力は V1〜V3 で足す）。
inline std::array<FormDefinition, kFormCount> DefaultForms()
{
    std::array<FormDefinition, kFormCount> forms{};
    auto form = [&](FormId id) -> FormDefinition& {
        return forms[static_cast<size_t>(id)];
    };
    auto& twin = form(FormId::Twin);
    twin.key = "twin";
    twin.name = "連装";
    twin.summary = "砲が2本になり、同時に撃つ";
    twin.sourceClass = "Twin";
    twin.reloadScale = 0.9f;
    twin.damageScale = 0.7f;
    twin.heatScale = 1.1f;
    auto& longBarrel = form(FormId::LongBarrel);
    longBarrel.key = "longBarrel";
    longBarrel.name = "長砲身";
    longBarrel.summary = "長い砲身で、速く遠くまで届く弾";
    longBarrel.longLine = true;
    longBarrel.reloadScale = 1.35f;
    longBarrel.damageScale = 1.8f;
    longBarrel.speedScale = 1.6f;
    longBarrel.heatScale = 1.2f;
    longBarrel.barrelLength = 1.7f;
    longBarrel.railCharge = true;

    // 第2段：連装の系統。
    auto& triple = form(FormId::Triple);
    triple = twin;
    triple.key = "triple";
    triple.name = "三連";
    triple.summary = "3門を扇形に開いて撃つ";
    triple.sourceClass = "Triple";
    triple.stage = 2;
    triple.parent = FormId::Twin;
    triple.reloadScale = 1.0f;
    triple.damageScale = 0.62f;
    triple.heatScale = 1.15f;
    triple.fanAngle = 16.0f;
    auto& machineGun = form(FormId::MachineGun);
    machineGun = twin;
    machineGun.key = "machineGun";
    machineGun.name = "機関砲";
    machineGun.summary = "弾がばらける高速連射";
    machineGun.sourceClass = "MachineGun";
    machineGun.stage = 2;
    machineGun.parent = FormId::Twin;
    machineGun.reloadScale = 0.55f;
    machineGun.damageScale = 0.45f;
    machineGun.heatScale = 0.7f;
    machineGun.spreadDeg = 12.0f;
    machineGun.fireRamp = true;
    auto& thunder = form(FormId::Thunder);
    thunder = twin;
    thunder.key = "thunder";
    thunder.name = "雷砲";
    thunder.summary = "放電が6体まで、1.5倍の距離に届く";
    thunder.stage = 2;
    thunder.parent = FormId::Twin;
    thunder.keyPart = PartId::ChainLightning;
    thunder.damageScale = 0.75f;
    thunder.chainExtraTargets = 4;
    thunder.chainRadiusScale = 1.5f;

    // 第2段：長砲身の系統。
    auto& sniper = form(FormId::Sniper);
    sniper = longBarrel;
    sniper.key = "sniper";
    sniper.name = "狙撃";
    sniper.summary = "重い一撃。倒した敵と、2回当てた敵は\n周りごと爆発";
    sniper.stage = 2;
    sniper.parent = FormId::LongBarrel;
    sniper.reloadScale = 1.6f;
    sniper.damageScale = 2.6f;
    sniper.speedScale = 2.0f;
    sniper.heatScale = 1.3f;
    sniper.barrelLength = 2.0f;
    sniper.markDetonation = true;
    auto& ricochet = form(FormId::Ricochet);
    ricochet = longBarrel;
    ricochet.key = "ricochet";
    ricochet.name = "反射砲";
    ricochet.summary = "弾が壁で2回跳ね返る";
    ricochet.stage = 2;
    ricochet.parent = FormId::LongBarrel;
    ricochet.reloadScale = 1.2f;
    ricochet.damageScale = 1.6f;
    ricochet.speedScale = 1.4f;
    ricochet.barrelLength = 1.6f;
    ricochet.wallBounces = 2;
    auto& beam = form(FormId::Beam);
    beam = longBarrel;
    beam.key = "beam";
    beam.name = "光線砲";
    beam.summary = "ため撃ちが太い光線になり、多くの敵を貫く";
    beam.stage = 2;
    beam.parent = FormId::LongBarrel;
    beam.keyPart = PartId::Pierce;
    beam.damageScale = 1.9f;
    beam.speedScale = 1.8f;
    beam.barrelLength = 1.9f;
    beam.railPower = 1.6f;
    beam.railWidth = 2.2f;
    beam.railPierce = 5;
    return forms;
}

/// @brief 元の機体設定から、形態の機体設定を作る。
inline PlayerClassConfig BuildFormConfig(const PlayerClassConfig& source, const FormDefinition& form, FormId id)
{
    (void)id;
    PlayerClassConfig config = source;
    config.id = std::string("Survivor_") + form.key;
    config.displayName = form.name;
    config.requiredRank = 1 + form.stage;
    config.usesDrone = false;
    config.reloadScale = form.reloadScale;
    config.bulletDamageScale = form.damageScale;
    config.bulletSpeedScale = form.speedScale;
    if (form.longLine) {
        // 1本の長い砲身。まっすぐ飛ばす。
        if (config.barrels.size() > 1) {
            config.barrels.resize(1);
        }
        config.randomSpread = false;
        config.spreadAngleDeg = 0.0f;
        config.bodyShape = PlayerBodyShape::Pentagon;
    } else {
        config.bodyShape = PlayerBodyShape::Box;
    }
    if (form.spreadDeg >= 0.0f) {
        config.randomSpread = form.spreadDeg > 0.0f;
        config.spreadAngleDeg = form.spreadDeg;
    }
    // 扇形：砲門を中央からの順で -角度・0・+角度 に向ける（横に並んだ砲門の位置はそのまま）。
    if (form.fanAngle > 0.0f && config.barrels.size() >= 2) {
        const float step = 2.0f * form.fanAngle / static_cast<float>(config.barrels.size() - 1);
        for (size_t i = 0; i < config.barrels.size(); ++i) {
            config.barrels[i].angleDeg = -form.fanAngle + step * static_cast<float>(i);
        }
    }
    for (auto& barrel : config.barrels) {
        barrel.scale.x *= form.barrelLength;
        barrel.muzzleForward *= form.barrelLength;
        if (form.longLine) {
            barrel.offset.y = 0.0f;
            barrel.angleDeg = 0.0f;
        }
    }
    config.specialActionId = form.railCharge ? "rail_charge" : "perfect_dodge";
    return config;
}

/// @brief 形態の札の説明文。
inline std::string DescribeForm(const FormDefinition& form, FormId id)
{
    (void)id;
    char numbers[160]{};
    std::snprintf(numbers, sizeof(numbers), "威力 x%.2f　発射間隔 x%.2f\n弾速 x%.2f　熱 x%.2f", form.damageScale, form.reloadScale,
                  form.speedScale, form.heatScale);
    std::string text = std::string(form.summary) + "\n";
    text += numbers;
    text += form.railCharge ? "\n右長押し：ため撃ち（貫通・熱）\nダッシュは Shift" : "\n右クリック・Shift：ダッシュ";
    return text;
}

/// @brief 形態を、設定での名前から探す。無ければ Count。
inline FormId FindForm(const std::array<FormDefinition, kFormCount>& forms, const std::string& key)
{
    for (size_t i = 0; i < kFormCount; ++i) {
        if (key == forms[i].key) {
            return static_cast<FormId>(i);
        }
    }
    return FormId::Count;
}

/// @brief 次に選べる形態の候補を返す。current が Count（基本砲）なら第1段、そうでなければ current の子。
/// 鍵付きの形態は、鍵の部品が最大 Lv のときだけ入れる（levels には合体の元も含めた Lv を渡す）。
inline std::vector<FormId> FormCandidates(const std::array<FormDefinition, kFormCount>& forms, FormId current,
                                          const std::array<PartDefinition, kPartCount>& parts, const PartLevels& levels)
{
    std::vector<FormId> candidates;
    for (size_t i = 0; i < kFormCount; ++i) {
        const FormDefinition& form = forms[i];
        const bool next = current == FormId::Count ? form.stage == 1 : form.stage == 2 && form.parent == current;
        if (!next) {
            continue;
        }
        if (form.keyPart != PartId::Count &&
            levels[static_cast<size_t>(form.keyPart)] < parts[static_cast<size_t>(form.keyPart)].maxLevel) {
            continue;
        }
        candidates.push_back(static_cast<FormId>(i));
    }
    return candidates;
}
} // namespace survivor
