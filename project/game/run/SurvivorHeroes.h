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
    None,      // 何もしない（空きの枠）
    Sprint,    // 走る：duration 秒、移動の速さを power 倍にする
    Barrier,   // バリア：押している間、扇のバリアを張る（設定の barrier とスタミナを使う）。待ち時間は使わない
    Rocket,    // 爆発する弾：照準の方へ1発。威力は主砲1発の power 倍、当たると半径 radius で爆発する
    HealZone,  // 回復の場：使った場所に半径 radius の場を duration 秒置き、中にいる間は毎秒 最大HPの power 倍を回復する
    Barrage,   // 自動で狙う弾幕：duration 秒、半径 radius の中の敵へ毎秒 rate 発、主砲1発の power 倍の弾を撃つ（熱は使わない）
    DrainBeam, // 吸い付く光線（主武器）：押している間、照準の近くで届く距離 radius の中の敵1体に吸い付き、毎秒 主砲1発の power 倍を与える。
               // 与えた威力1ごとに吸収ゲージを gain 秒ぶん溜める。size は光線の太さ
    SelfHeal, // 吸収ゲージで自分を回復：押している間、ゲージを毎秒1秒ぶん使い、毎秒 最大HPの power 倍を回復する。ゲージの上限は duration 秒
    Phase,    // 消える移動：duration 秒、無敵で敵をすり抜け、移動の速さを power 倍にする
    BounceOrb, // 跳ねる球：duration 秒、毎秒 rate の速さで進み、敵に触れると次の近い敵へ跳ねる。触れている敵に毎秒 主砲1発の power 倍。
               // size は球の半径
    Charge,    // 突進：duration 秒、移動の速さを power 倍にし、触れた敵を rate の強さで押し飛ばして 主砲1発の size 倍を与える
    Shockwave, // 衝撃波：自機のまわり半径 radius の敵を rate の強さで押し返し、主砲1発の power 倍を与える
    Fortress,  // 大きな盾（Q）：duration 秒、自機のまわり半径 radius に全周のバリア（敵の弾を消し、敵を縁まで押し出す）。
               // 終わりに半径 radius×1.6 で爆発し、主砲1発の power 倍 ＋ 止めた弾1つごとに rate 倍を与える
    Lance,     // 太い光線（Q）：duration 秒、向いている方へ長さ radius・太さ size の光線を出し、触れた敵すべてに毎秒 主砲1発の power 倍。
               // 与えた威力1ごとに吸収ゲージを gain 秒ぶん溜める
    BurstGun,  // 拡散連射（主武器。計画書 p7 の E5）：押している間（自動で撃つなら いつも）、毎秒 rate 発で duration 発を撃ち、撃ち終えたら
               // cooldown 秒待つ。弾は向いている方から ±radius 度ばらけ、威力は主砲1発の power 倍、大きさは size 倍。
               // 強化は7段（SurvivorShop.h の BurstGunAt）で、貫通・待ち時間・連射・爆発・覚醒（燃える）が付く
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
    static constexpr std::array<const char*, static_cast<size_t>(AbilityKind::Count)> kKeys{
        "none", "sprint", "barrier", "rocket", "healZone", "barrage", "drainBeam", "selfHeal", "phase", "bounceOrb", "charge", "shockwave", "fortress", "lance", "burstGun"};
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
    return kind == AbilityKind::Barrier || kind == AbilityKind::SelfHeal || kind == AbilityKind::DrainBeam;
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
    /// @brief 1秒あたりの発射数（弾幕）・速さ（跳ねる球）。
    float rate = 0.0f;
    /// @brief 太さ・大きさ（光線の太さ・跳ねる球の半径。ワールド単位）。
    float size = 0.0f;
    /// @brief 与えた威力1ごとに溜まる吸収ゲージの秒（吸い付く光線・太い光線）。
    float gain = 0.0f;
};

/// @brief 機体（ヒーロー）。主武器は weapons のキーで指し、技は4つの枠に部品を入れる（計画書 p4 の4章）。
struct HeroDefinition {
    std::string key;
    std::string name;
    /// @brief 主武器（左クリック）。設定の weapons のキー。弾倉とリロードは使わず、熱だけで撃つ。
    std::string primary = "cannon";
    /// @brief 主武器の1発の熱の倍率の上書き（0 なら武器の値のまま）。弾を数えないので、撃ち続けられる長さはこれで決まる。
    float primaryHeatScale = 0.0f;
    /// @brief 主武器を技の部品にする（吸い付く光線）。kind が None なら primary の武器で撃つ。
    /// 部品のときも primary の武器は車体と砲塔の見た目に使い、弾は撃たない。
    AbilityDefinition primaryAbility{};
    std::array<AbilityDefinition, kAbilitySlotCount> abilities{};
    /// @brief Q のゲージが満タンになるまでに要る量（与えたダメージ＋回復した量）。
    float ultimateCharge = 600.0f;
    /// @brief 最大HP（0 なら自機の値のまま）。
    int maxHp = 0;
    /// @brief 解放に要る解放ポイント（0 なら最初から使える。計画書 p4 の K6）。
    int unlockCost = 0;
    /// @brief 最初に解放する機体か（まだなら、次に解放できるのはこの機体だけ）。
    bool unlockFirst = false;
};

/// @brief 機体の設定。enabled なら、武器の枠の遊び方の上で機体を使う（枠は主武器1つ、ホイールとリロードなし）。
struct HeroesConfig {
    bool enabled = true;
    /// @brief 出撃する機体（K6 で拠点の選ぶ画面にする）。
    std::string start = "soldier";
    std::vector<HeroDefinition> heroes;
    /// @brief 機体の移動の速さの倍率（計画書 p7。敵とほぼ同じ速さにする。1 で今までと同じ）。
    float moveScale = 1.0f;
    /// @brief 機体の歩きの最高速度（ワールド単位/秒）。0 より大きければ moveScale の代わりにこれを使う（計画書 p7。自機の土台の速さ
    /// （毎秒17.4ほど）によらず、毎秒いくつと直接決める）。技とパークの倍率は、これに掛ける。
    float walkSpeed = 0.0f;
    /// @brief 押さなくても向いている方へ主砲を撃ち続けるか（計画書 p7。HoloCure と同じ。遊んでいる間も F で切り替え）。
    bool autoFire = false;
    /// @brief 欠片でレベルが上がり、戦闘中に技の強化とパークの3択を選ぶか（計画書 p7 の E3。HoloCure と同じ。偽なら今までどおり欠片はレベルポイント）。
    bool levelUps = false;
    /// @brief レベルアップの3択に副武器（勝手に動く武器。SurvivorSubWeapons.h）も並べるか（計画書 p7 の E4。levelUps のときだけ）。
    bool subWeapons = false;
    /// @brief 右クリック・Shift・E の技を使わず、主武器・副武器・Q（コントローラーは B）だけで遊ぶか（計画書 p7 の E5。ヴァンサバに寄せる）。
    /// 技の強化の候補からも外し、外した技に関わるパーク（機体専用と「連鎖の冷却」）も並べない。
    bool onlyUltimate = false;
    /// @brief パークの段の上限（計画書 p8 の F1。HoloCure のアイテムと同じく段で強くなる）。1 なら今までどおり段なし。
    /// 段ごとに、パークの強さ（shop.perks の value）が足される（Lv3 なら value×3）。
    int perkMaxTier = 1;
    /// @brief レベルアップの3択に機体専用スキル（SurvivorHeroSkills.h。計画書 p8 の F2）も並べるか（levelUps のときだけ）。
    bool heroSkills = false;
    /// @brief 持てる副武器・パークの数（計画書 p8 の F1）。0 なら上限なし。埋まったら、持っている物の強化だけ並ぶ。
    int subWeaponSlots = 0;
    int perkSlots = 0;
    /// @brief 選ぶ物が無くなったときの埋め草（計画書 p8 の F1）。3択が3つに満たなければ足す。0 ならその埋め草は出さない。
    /// fillerHeal は最大HPに対する回復の割合（傷も同じだけ治す）、fillerMaxHp は最大HPの増え方、fillerPower は威力の増え方（0.05 で +5%）。
    float fillerHeal = 0.0f;
    int fillerMaxHp = 0;
    float fillerPower = 0.0f;
    /// @brief 途中の回復（計画書 p8 の F5）：敵が光るスパナを落とす確率（中ボスは必ず）・拾ったときの回復（最大HPに対する割合。
    /// 中ボスの物は1.5倍。傷も同じだけ治す）・消えるまでの秒（中ボスの物は消えない。0 なら消えない）・床に置ける数。healDropHeal が0なら落とさない。
    float healDropChance = 0.0f;
    float healDropHeal = 0.0f;
    float healDropLife = 45.0f;
    int healDropMax = 6;
};

/// @brief 設定ファイルが無いときの機体。ソルジャー型（K1）・モイラ型（K2）・タンク型（K7。最初に解放する）。
inline std::vector<HeroDefinition> DefaultHeroes()
{

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
    soldier.maxHp = 100;

    // モイラ型（計画書 p4 の3章）：近づいて吸い取る。攻める → 吸収ゲージが溜まる → 生き残る。
    // 群れへの答えは跳ねる球、硬い敵への答えは吸い付く光線。
    HeroDefinition moira;
    moira.key = "moira";
    moira.name = "モイラ型";
    moira.primary = "beam";
    moira.primaryAbility = {AbilityKind::DrainBeam, "吸収ビーム", 0.0f, 0.0f, 2.6f, 8.0f, 0.0f, 0.22f, 0.012f};
    moira.abilities[static_cast<size_t>(AbilitySlot::Secondary)] = {AbilityKind::SelfHeal, "自分を回復", 0.0f, 6.0f, 0.06f};
    moira.abilities[static_cast<size_t>(AbilitySlot::Mobility)] = {AbilityKind::Phase, "消える移動", 6.0f, 0.8f, 1.6f};
    moira.abilities[static_cast<size_t>(AbilitySlot::Skill)] = {AbilityKind::BounceOrb, "跳ねる球", 10.0f, 4.0f, 1.4f, 12.0f, 9.0f, 1.1f};
    moira.abilities[static_cast<size_t>(AbilitySlot::Ultimate)] = {
        AbilityKind::Lance, "太い光線", 0.0f, 4.0f, 4.0f, 20.0f, 0.0f, 1.4f, 0.006f};
    moira.ultimateCharge = 1800.0f;
    moira.maxHp = 90;

    // タンク型（計画書 p4 の3章、K7）：受け止めて押し返す。最初に解放する機体。
    // 群れへの答えは衝撃波、硬い敵への答えは重い砲。
    HeroDefinition tank;
    tank.key = "tank";
    tank.name = "タンク型";
    tank.primary = "cannon";
    tank.abilities[static_cast<size_t>(AbilitySlot::Secondary)] = {AbilityKind::Barrier, "バリア"};
    tank.abilities[static_cast<size_t>(AbilitySlot::Mobility)] = {AbilityKind::Charge, "突進", 7.0f, 0.35f, 3.0f, 0.0f, 0.7f, 2.0f};
    tank.abilities[static_cast<size_t>(AbilitySlot::Skill)] = {AbilityKind::Shockwave, "衝撃波", 10.0f, 0.0f, 3.0f, 6.0f, 0.8f};
    tank.abilities[static_cast<size_t>(AbilitySlot::Ultimate)] = {AbilityKind::Fortress, "大きな盾", 0.0f, 6.0f, 4.0f, 5.0f, 0.6f};
    tank.ultimateCharge = 1800.0f;
    tank.maxHp = 150;
    tank.unlockCost = 1500;
    tank.unlockFirst = true;
    return {soldier, moira, tank};
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
