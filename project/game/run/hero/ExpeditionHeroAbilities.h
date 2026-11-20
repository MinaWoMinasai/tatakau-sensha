#pragma once
#include "game/run/hero/HeroAbilityCore.h"
#include <nlohmann/json.hpp>

namespace gameplay {
/// @brief 遠征で機体（ヒーロー）の技を動かす（計画書 p5 の H3）。技の処理は土台の HeroAbilityCore と育つ戦車の試作で同じものを使う。
/// @note マップの前に機体を選ぶと Start が呼ばれ、戦闘の部屋の中だけ技を進める。ショップの強化の段とパークは遠征には無い（土台の既定の0）。
/// ボスは遠征のネオンボス（world の enemy_、奥行きの遭遇）で、ボス戦の間だけ技の狙う先に入れる。
class ExpeditionHeroAbilities : public HeroAbilityCore {
public:
    explicit ExpeditionHeroAbilities(GameWorld& world) : HeroAbilityCore(world) {}

    /// @brief バリアとスタミナの設定（育つ戦車の試作と同じ survivor_proto.json の barrier）。Start の前に呼ぶ。
    void Configure(const survivor::SpawnConfig::BarrierConfig& barrier)
    {
        barrier_ = barrier;
    }
    /// @brief 選んだ機体で始める：最大HPを入れて満タンにし、技の状態を始めに戻し、与えたダメージで Q のゲージが溜まるようにする。
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

private:
    /// @brief 戦闘の部屋で、止めていないか（ポーズ・マップの確認・詳細の画面・場面の移り変わりの間は止める）。
    bool IsPlaying() const;
    /// @brief 左クリック・右クリック・Shift・E・Q を読んで技を使う。ボットのときはボットの入力。
    void ReadInput();
    /// @brief ボットの入力：近い敵（かボス）を狙って撃ち続け、使える技はすぐ使う。
    void UpdateBot(float dt);

    bool active_ = false;
    float clock_ = 0.0f;
    survivor::SpawnConfig::BarrierConfig barrier_{};
    // 与えたダメージの合計（記録）。
    uint64_t damageDealt_ = 0;
    // ボット：押したことにする枠・左右の押し続け・バリアの切り替えの時計。
    bool bot_ = false;
    std::array<bool, survivor::kAbilitySlotCount> botPress_{};
    bool botPrimary_ = false, botSecondary_ = false;
    float botClock_ = 0.0f;
};
} // namespace gameplay
