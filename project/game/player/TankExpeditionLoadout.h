#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

// Per-expedition progression. No renderer or persistent progression state.
/// @brief 遠征の修理・整備による効果と費用を表す。
class TankExpeditionMaintenance {
public:
    static constexpr int kStatCount = 3;
    static constexpr int kMaxRank = 3;
    static constexpr int kRewardRoomCount = 4;

    /// @brief 部屋を付与する。
    bool AwardRoom(int clearedRoom)
    {
        if (clearedRoom < 1 || clearedRoom > kRewardRoomCount || awardedRooms_[clearedRoom - 1])
            return false;
        awardedRooms_[clearedRoom - 1] = true;
        ++points_;
        return true;
    }
    /// @brief 必要な費用を保持している値から消費する。
    bool Spend(int stat)
    {
        if (stat < 0 || stat >= kStatCount || points_ <= 0 || ranks_[stat] >= kMaxRank)
            return false;
        ++ranks_[stat];
        --points_;
        return true;
    }
    /// @brief 消費した値を返却する。
    bool Refund(int stat)
    {
        if (stat < 0 || stat >= kStatCount || ranks_[stat] <= 0)
            return false;
        --ranks_[stat];
        ++points_;
        return true;
    }
    /// @brief 保持しているポイント数を返す。
    int Points() const
    {
        return points_;
    }
    /// @brief 現在の成長段階を返す。
    int Rank(int stat) const
    {
        return stat >= 0 && stat < kStatCount ? ranks_[stat] : 0;
    }
    /// @brief 移動速度へ掛ける倍率を返す。
    float MoveScale() const
    {
        return 1.0f + 0.12f * static_cast<float>(ranks_[0]);
    }
    /// @brief 拡大率を再読込または再装填する。
    float ReloadScale() const
    {
        return 1.0f - 0.14f * static_cast<float>(ranks_[1]);
    }
    /// @brief 拡大率へダメージを適用する。
    float DamageScale() const
    {
        return 1.0f - 0.08f * static_cast<float>(ranks_[2]);
    }
    /// @brief 防御条件に従って受けるダメージを軽減する。
    uint32_t MitigateDamage(uint32_t amount) const
    {
        if (amount == 0 || ranks_[2] == 0)
            return amount;
        // Double precision keeps even UINT32_MAX input defined. Round to the
        // nearest whole HP; armor cannot turn a damaging hit into zero damage.
        const double scaled = static_cast<double>(amount) * (1.0 - 0.08 * ranks_[2]);
        return scaled < 1.0 ? 1u : static_cast<uint32_t>(scaled + 0.5);
    }

private:
    int points_ = 0;
    std::array<int, kStatCount> ranks_{};
    std::array<bool, kRewardRoomCount> awardedRooms_{};
};

/// @brief 整備メニューで選べる項目の内容と費用を表す。
struct RunMaintenanceChoice {
    int id = 0;
    std::string name;
    std::string description;
    int rank = 0;
    int maxRank = TankExpeditionMaintenance::kMaxRank;
    bool canSpend = false;
};

/// @brief 遠征で選ぶ専門系統の性能補正と効果を表す。
struct TankExpeditionSpecialization {
    const char* id;
    const char* starter;
    const char* name;
    const char* description;
    int barrels;
    float fanAngle;
    bool alternate;
    float reloadScale;
    float damageScale;
    float speedScale;
    float randomSpread;
    bool reflect;
    int drones;
};

// Multipliers are relative to the authored starter; these never mutate its JSON.
inline constexpr std::array<TankExpeditionSpecialization, 6> kTankExpeditionSpecializations{
    {{"exp_twin_fan", "Twin", "トライファン",
      "3砲を扇状に同時発射\n広い角度の敵をまとめて狙う\n集中命中より面の制圧\n主軸・改造・整備を引き継ぎ", 3, 20.0f, false, 2.4f, 1.0f,
      1.0f, 0.0f, false, 0},
     {"exp_twin_lance", "Twin", "ツインランス", "2砲を正面へ同時発射\n高精度・弾速45%増\n1発の威力25%増\n主軸・改造・整備を引き継ぎ", 2,
      0.0f, false, 2.0f, 1.25f, 1.45f, 0.0f, false, 0},
     {"exp_machine_storm", "MachineGun", "ストームガン",
      "2砲を交互に高速連射\n発射間隔30%短縮\n広い散布・1発の威力15%減\n主軸・改造・整備を引き継ぎ", 2, 0.0f, true, 0.7f, 0.85f, 1.0f, 24.0f,
      false, 0},
     {"exp_machine_bank", "MachineGun", "バンクショット",
      "高精度の壁反射弾\n1発の威力30%増\n発射間隔40%増・弾速20%増\n主軸・改造・整備を引き継ぎ", 1, 0.0f, false, 1.4f, 1.3f, 1.2f, 0.0f,
      true, 0},
     {"exp_overseer_swarm", "Overseer", "スウォーム",
      "基本8機の軽射撃ドローン\n各機の発射間隔10%短縮\n各弾の威力20%減\n主軸・改造の追加機も有効", 2, 0.0f, false, 0.9f, 0.8f, 1.1f, 10.0f,
      false, 8},
     {"exp_overseer_artillery", "Overseer", "アーティラリー",
      "基本3機の重射撃ドローン\n各弾の威力2.7倍・高精度\n各機の発射間隔15%増\n追加機は主軸・改造ごとに1機", 1, 0.0f, false, 1.15f, 2.7f,
      1.55f, 0.0f, false, 3}}};

/// @brief 戦車遠征Specializationを検索する。
inline const TankExpeditionSpecialization* FindTankExpeditionSpecialization(std::string_view id)
{
    for (const auto& specialization : kTankExpeditionSpecializations) {
        if (id == specialization.id)
            return &specialization;
    }
    return nullptr;
}

/// @brief 遠征で利用可能なドローン数の上限を返す。
inline int TankExpeditionDroneLimit(const TankExpeditionSpecialization* specialization, int baseLimit, bool droneModifier, bool droneCore)
{
    const int base = specialization && specialization->drones > 0 ? specialization->drones : (baseLimit < 6 ? baseLimit : 6);
    const int extra = specialization && specialization->drones == 3 ? 1 : 2;
    return (base < 0 ? 0 : base) + (droneModifier ? extra : 0) + (droneCore ? extra : 0);
}
