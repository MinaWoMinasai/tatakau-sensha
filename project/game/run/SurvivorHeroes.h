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

/// @brief 技の部品の種類（計画書 p4 の4章）。K0 では走る・バリアだけ。ほかは K1 以降で足す。
enum class AbilityKind {
    None,    // 何もしない（空きの枠）
    Sprint,  // 走る：duration 秒、移動の速さを power 倍にする
    Barrier, // バリア：押している間、扇のバリアを張る（設定の barrier とスタミナを使う）。待ち時間は使わない
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
    static constexpr std::array<const char*, static_cast<size_t>(AbilityKind::Count)> kKeys{"none", "sprint", "barrier"};
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
    /// @brief 種類ごとの強さ（走るなら速さの倍率）。
    float power = 1.0f;
};

/// @brief 機体（ヒーロー）。主武器は weapons のキーで指し、技は4つの枠に部品を入れる（計画書 p4 の4章）。
struct HeroDefinition {
    std::string key;
    std::string name;
    /// @brief 主武器（左クリック）。設定の weapons のキー。弾倉とリロードは使わず、熱だけで撃つ。
    std::string primary = "cannon";
    std::array<AbilityDefinition, kAbilitySlotCount> abilities{};
    /// @brief Q のゲージが満タンになるまでに要る量（与えたダメージ＋回復した量）。
    float ultimateCharge = 600.0f;
};

/// @brief 機体の設定。enabled なら、武器の枠の遊び方の上で機体を使う（枠は主武器1つ、ホイールとリロードなし）。
struct HeroesConfig {
    bool enabled = true;
    /// @brief 出撃する機体（K6 で拠点の選ぶ画面にする）。
    std::string start = "prototype";
    std::vector<HeroDefinition> heroes;
};

/// @brief 設定ファイルが無いときの機体。K0 の仮の機体（単発の砲・バリア・走る）だけ。
inline std::vector<HeroDefinition> DefaultHeroes()
{
    HeroDefinition prototype;
    prototype.key = "prototype";
    prototype.name = "試作機";
    prototype.primary = "cannon";
    prototype.abilities[static_cast<size_t>(AbilitySlot::Secondary)] = {AbilityKind::Barrier, "バリア", 0.0f, 0.0f, 1.0f};
    prototype.abilities[static_cast<size_t>(AbilitySlot::Mobility)] = {AbilityKind::Sprint, "走る", 8.0f, 3.0f, 1.5f};
    prototype.ultimateCharge = 600.0f;
    return {prototype};
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
