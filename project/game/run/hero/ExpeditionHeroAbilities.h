#pragma once
#include "game/run/hero/HeroAbilityCore.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <string_view>
#include <unordered_map>

namespace gameplay {
/// @brief 遠征で機体（ヒーロー）の技を動かす（計画書 p5 の H3）。技の処理は土台の HeroAbilityCore と育つ戦車の試作で同じものを使う。
/// @note マップの前に機体を選ぶと Start が呼ばれ、戦闘の部屋の中だけ技を進める。ショップの強化の段とパークは遠征には無い（土台の既定の0）。
/// ボスは遠征のネオンボス（world の enemy_、奥行きの遭遇）で、ボス戦の間だけ技の狙う先に入れる。
class ExpeditionHeroAbilities : public HeroAbilityCore {
public:
    explicit ExpeditionHeroAbilities(GameWorld& world) : HeroAbilityCore(world) {}

    /// @brief 育つ戦車の試作と同じ設定（survivor_proto.json）から、バリアとスタミナ・武器・形態を受け取る。Start の前に呼ぶ。
    void Configure(const survivor::SpawnConfig& config)
    {
        barrier_ = config.barrier;
        weapons_ = config.weapons;
        forms_ = config.forms;
        heat_ = config.heat;
    }
    /// @brief 機体で遊んでいるときの熱の設定（試作と同じ。機体の主武器はこの熱で釣り合わせてある）。機体でなければ nullptr。
    /// @note 遠征の熱（ExpeditionOverdrive）は、これを土台にして札の効果を重ねる。
    const PlayerHeat::Config* HeroHeatConfig() const
    {
        return active_ ? &heat_ : nullptr;
    }
    /// @brief 選んだ機体で始める：主武器と最大HPを入れて満タンにし、技の状態を始めに戻し、与えたダメージで Q のゲージが溜まるようにする。
    void Start(const survivor::HeroDefinition& hero);
    /// @brief 機体で遊んでいるか。
    bool IsActive() const
    {
        return active_;
    }
    /// @brief 部屋に入ったとき、効いている技（回復の場・球・光線・盾など）を消す。待ち時間と Q のゲージは持ち越す。
    void OnRoomStart();
    /// @brief 戦闘の部屋の中だけ、入力を読み、技を進める。dt は固定ステップの秒。
    void Update(float dt);
    /// @brief 技の見た目とバリアを、ネオンの格子のパスに積む。
    void QueueWorldVisuals();
    /// @brief 自動で撃つか（F キーで切り替える。最初は調整ファイルの heroAutoFire）。
    bool IsAutoFire() const
    {
        return autoFire_;
    }
    /// @brief 自動確認：ボットが入力の代わりに技を使う（照準と移動は Player::SetDemoInput）。
    void SetBotEnabled(bool enabled)
    {
        bot_ = enabled;
    }
    /// @brief 自動確認と記録の数字。
    nlohmann::json MakeReport() const;
    /// @brief 戦闘の部屋で技を進めた秒の合計（自動確認）。
    float PlayClock() const
    {
        return clock_;
    }
    /// @brief 技がボスを狙える状態か（自動確認）。
    bool BossTargetActive() const
    {
        return HeroBossTargetActive();
    }

    // 技の強化（覚醒）。工房で買う。0 は主武器、1〜4 は右クリック・Shift・E・Q。1段目は威力、2段目は待ち時間、3段目で挙動が変わる。
    /// @brief 今の機体（強化を効かせた後）。
    const survivor::HeroDefinition& CurrentHero() const
    {
        return Hero();
    }
    /// @brief target の技を、まだ強くできるか（空きの枠でなく、3段に届いていない）。
    bool CanUpgrade(int target) const;
    /// @brief target の技を1段強くし、機体の値と主武器を入れ直す。
    void BuyUpgrade(int target);
    int UpgradeTier(int target) const override
    {
        return target >= 0 && target < static_cast<int>(survivor::kUpgradeTargets) ? tiers_[static_cast<size_t>(target)] : 0;
    }
    /// @brief 工房に並べるときの強化の id（"hero_upgrade_0" など。強化の札の id とぶつからない）。
    static std::string UpgradeOfferId(int target)
    {
        return "hero_upgrade_" + std::to_string(target);
    }
    /// @brief 工房の id が技の強化なら、対象（0〜4）を返す。違えば -1。
    static int ParseUpgradeOfferId(const std::string& id)
    {
        constexpr std::string_view prefix = "hero_upgrade_";
        if (id.size() != prefix.size() + 1 || id.compare(0, prefix.size(), prefix) != 0) {
            return -1;
        }
        const int target = id.back() - '0';
        return target >= 0 && target < static_cast<int>(survivor::kUpgradeTargets) ? target : -1;
    }
    /// @brief 技の強化の値段（今の段ごと。遠征の通貨。共通の札が 24〜78 なので、その間から上に置く。遊んで調整する）。
    static int UpgradeOfferPrice(int currentTier)
    {
        static constexpr std::array<int, survivor::kMaxUpgradeTier> kPrices{40, 70, 110};
        return kPrices[static_cast<size_t>((std::clamp)(currentTier, 0, survivor::kMaxUpgradeTier - 1))];
    }

protected:
    bool HeroActive() const override
    {
        return active_;
    }
    bool HeroBossTargetActive() const override;
    float HeroClock() const override
    {
        return clock_;
    }
    const survivor::SpawnConfig::BarrierConfig& HeroBarrierSettings() const override
    {
        return barrier_;
    }
    /// @brief 遠征の調整ファイル（tankExpeditionBalance.json）の heroMoveScale。敵とほぼ同じ速さにして、振り切れないようにする。
    float HeroBaseMoveScale() const override;

private:
    /// @brief 戦闘の部屋で、止めていないか（ポーズ・マップの確認・詳細の画面・場面の移り変わりの間は止める）。
    bool IsPlaying() const;
    /// @brief 左クリック・右クリック・Shift・E・Q を読んで技を使う。ボットのときはボットの入力。
    void ReadInput();
    /// @brief ボットの入力：近い敵（かボス）を狙って撃ち続け、使える技はすぐ使う。
    void UpdateBot(float dt);
    /// @brief 機体の主武器（weapons の primary）を、試作と同じ形で自機に入れる：武器 → 形態 → 自機の機体設定（砲の数・威力・連射・弾の色・熱）。
    /// 機関砲の撃ち続けると速くなる加速も入れる。見つからなければ遠征のふつうの砲のまま。
    void ApplyPrimaryWeapon(const survivor::HeroDefinition& hero);

    bool active_ = false;
    float clock_ = 0.0f;
    // 自動で撃つ（HoloCure と同じ：押さなくても向いている方へ撃ち続け、向きと移動だけを操作する）。
    bool autoFire_ = true;
    /// @brief 選んだときの機体（強化を効かせる前）と、技ごとの強化の段。
    survivor::HeroDefinition baseHero_{};
    std::array<int, survivor::kUpgradeTargets> tiers_{};
    survivor::SpawnConfig::BarrierConfig barrier_{};
    std::vector<survivor::WeaponDefinition> weapons_;
    std::array<survivor::FormDefinition, survivor::kFormCount> forms_{};
    PlayerHeat::Config heat_{};
    /// @brief 入れた主武器の名前（記録）。
    std::string primaryName_;
    // 与えたダメージの合計（記録）。
    uint64_t damageDealt_ = 0;
    // ボット：押したことにする枠・左右の押し続け・バリアの切り替えの時計。
    bool bot_ = false;
    std::array<bool, survivor::kAbilitySlotCount> botPress_{};
    bool botPrimary_ = false, botSecondary_ = false;
    float botClock_ = 0.0f;
    // 速さの測り（自動確認）：自機が動いた距離と秒、動いている敵の速さの合計と数・一番速い敵（ワールド単位/秒）。
    myEngine::Vector3 lastPlayerPosition_{};
    bool hasLastPlayerPosition_ = false;
    float playerMoveDistance_ = 0.0f, playerMoveSeconds_ = 0.0f;
    std::unordered_map<uint64_t, myEngine::Vector3> lastEnemyPositions_;
    double enemySpeedSum_ = 0.0;
    uint64_t enemySpeedSamples_ = 0;
    std::vector<float> enemySpeedHistogram_;
};
} // namespace gameplay
