#pragma once
#include <array>
#include <string>
#include <vector>

namespace survivor {
/// @brief 機体の技の枠（計画書 p4 の0章）。ボタンの意味は全機体で同じにする。
enum class AbilitySlot {
    Secondary, // 右クリック：もう一つの攻撃か守り
    Mobility,  // Shift：移動の技
    Skill,     // E：技
    Ultimate,  // Q：アルティメット（ゲージが満タンで使える）
    Count
};
inline constexpr size_t kAbilitySlotCount = static_cast<size_t>(AbilitySlot::Count);

/// @brief 技の部品の種類（計画書 p4 の4章）。
enum class AbilityKind {
    None,     // 何もしない（空きの枠）
    Sprint,   // 走る：duration 秒、移動の速さを power 倍にする
    Barrier,  // バリア：押している間、扇のバリアを張る（設定の barrier とスタミナを使う）。待ち時間は使わない
    Rocket,   // 爆発する弾：照準の方へ1発。威力は主砲1発の power 倍、当たると半径 radius で爆発する
    HealZone, // 回復の場：使った場所に半径 radius の場を duration 秒置き、中にいる間は毎秒 最大HPの power 倍を回復する
    Barrage,  // 自動で狙う弾幕：duration 秒、半径 radius の中の敵へ毎秒 rate 発、主砲1発の power 倍の弾を撃つ（熱は使わない）
    Count
};

/// @brief 枠の設定での名前。
inline const char* AbilitySlotKey(AbilitySlot slot)
{
    static constexpr std::array<const char*, kAbilitySlotCount> kKeys{"secondary", "mobility", "skill", "ultimate"};
    return kKeys[static_cast<size_t>(slot)];
}

/// @brief 枠のボタンの名前（画面と案内に出す）。
inline const char* AbilitySlotButton(AbilitySlot slot)
{
    static constexpr std::array<const char*, kAbilitySlotCount> kButtons{"右クリック", "Shift", "E", "Q"};
    return kButtons[static_cast<size_t>(slot)];
}

/// @brief 技の種類の設定での名前。
inline const char* AbilityKindKey(AbilityKind kind)
{
    static constexpr std::array<const char*, static_cast<size_t>(AbilityKind::Count)> kKeys{"none",   "sprint",   "barrier",
                                                                                            "rocket", "healZone", "barrage"};
    return kKeys[static_cast<size_t>(kind)];
}

/// @brief 設定での名前から技の種類を探す。無ければ None。
inline AbilityKind FindAbilityKind(const std::string& key)
{
    for (size_t i = 0; i < static_cast<size_t>(AbilityKind::Count); ++i) {
        if (key == AbilityKindKey(static_cast<AbilityKind>(i))) {
            return static_cast<AbilityKind>(i);
        }
    }
    return AbilityKind::None;
}

/// @brief 押している間だけ効く種類か（待ち時間ではなく資源を使う）。
inline bool IsHeldAbility(AbilityKind kind)
{
    return kind == AbilityKind::Barrier;
}

/// @brief 1つの技（部品＋数値）。どの値を使うかは種類で決まる。
struct AbilityDefinition {
    AbilityKind kind = AbilityKind::None;
    /// @brief 画面と案内に出す名前。
    std::string name;
    /// @brief 使ってから次に使えるまでの秒（Q は使わず、ゲージで決まる）。
    float cooldown = 0.0f;
    /// @brief 効いている秒（走るなど）。
    float duration = 0.0f;
    /// @brief 種類ごとの強さ（走るなら速さの倍率、弾なら主砲1発に対する威力の倍率、回復の場なら毎秒の回復の割合）。
    float power = 1.0f;
    /// @brief 種類ごとの半径（爆発・回復の場・弾幕の狙う範囲。ワールド単位）。
    float radius = 0.0f;
    /// @brief 1秒あたりの発射数（弾幕）。
    float rate = 0.0f;
};

/// @brief 機体（ヒーロー）。主武器は weapons のキーで指し、技は4つの枠に部品を入れる（計画書 p4 の4章）。
struct HeroDefinition {
    std::string key;
    std::string name;
    /// @brief 主武器（左クリック）。設定の weapons のキー。弾倉とリロードは使わず、熱だけで撃つ。
    std::string primary = "cannon";
    /// @brief 主武器の1発の熱の倍率の上書き（0 なら武器の値のまま）。弾を数えないので、撃ち続けられる長さはこれで決まる。
    float primaryHeatScale = 0.0f;
    std::array<AbilityDefinition, kAbilitySlotCount> abilities{};
    /// @brief Q のゲージが満タンになるまでに要る量（与えたダメージ＋回復した量）。
    float ultimateCharge = 600.0f;
};

/// @brief 機体の設定。enabled なら、武器の枠の遊び方の上で機体を使う（枠は主武器1つ、ホイールとリロードなし）。
struct HeroesConfig {
    bool enabled = true;
    /// @brief 出撃する機体（K6 で拠点の選ぶ画面にする）。
    std::string start = "soldier";
    std::vector<HeroDefinition> heroes;
};

/// @brief 設定ファイルが無いときの機体。K0 の仮の機体（試作機）と、ソルジャー型（K1）。
inline std::vector<HeroDefinition> DefaultHeroes()
{
    HeroDefinition prototype;
    prototype.key = "prototype";
    prototype.name = "試作機";
    prototype.primary = "cannon";
    prototype.abilities[static_cast<size_t>(AbilitySlot::Secondary)] = {AbilityKind::Barrier, "バリア"};
    prototype.abilities[static_cast<size_t>(AbilitySlot::Mobility)] = {AbilityKind::Sprint, "走る", 8.0f, 3.0f, 1.5f};
    prototype.ultimateCharge = 600.0f;

    // ソルジャー型（計画書 p4 の3章）：連射砲は熱だけで撃つ。群れへの答えはロケット、硬い敵への答えは連射砲。
    HeroDefinition soldier;
    soldier.key = "soldier";
    soldier.name = "ソルジャー型";
    soldier.primary = "machineGun";
    soldier.primaryHeatScale = 1.0f;
    soldier.abilities[static_cast<size_t>(AbilitySlot::Secondary)] = {AbilityKind::Rocket, "小型ロケット", 6.0f, 0.0f, 6.0f, 3.0f};
    soldier.abilities[static_cast<size_t>(AbilitySlot::Mobility)] = {AbilityKind::Sprint, "全力で走る", 8.0f, 3.0f, 1.5f};
    soldier.abilities[static_cast<size_t>(AbilitySlot::Skill)] = {AbilityKind::HealZone, "回復の場", 15.0f, 5.0f, 0.04f, 4.0f};
    soldier.abilities[static_cast<size_t>(AbilitySlot::Ultimate)] = {AbilityKind::Barrage, "弾幕", 0.0f, 5.0f, 0.8f, 18.0f, 14.0f};
    soldier.ultimateCharge = 2000.0f;
    return {prototype, soldier};
}

/// @brief キーから機体の番号を探す。無ければ -1。
inline int FindHero(const std::vector<HeroDefinition>& heroes, const std::string& key)
{
    for (size_t i = 0; i < heroes.size(); ++i) {
        if (heroes[i].key == key) {
            return static_cast<int>(i);
        }
    }
    return -1;
}
} // namespace survivor
