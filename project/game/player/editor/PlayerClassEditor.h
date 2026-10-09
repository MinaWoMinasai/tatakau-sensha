#pragma once
#include "game/player/ui/PlayerUiState.h"

/// @brief PlayerClassEditorの表示と入力を担当し、必要な自機の状態を借用する。
class PlayerClassEditor {
public:
    using BodyShape = Player::BodyShape;
    using PlayerStats = Player::PlayerStats;
    /// @brief 自機と画面状態を接続する。
    PlayerClassEditor(Player& player, PlayerUiState& ui) : player_(player), ui_(ui) {}
    using RunCombatSnapshot = Player::RunCombatSnapshot;
    using MineDropEvent = Player::MineDropEvent;
    using BalanceConfig = Player::BalanceConfig;
    using SpecialCombatEvent = Player::SpecialCombatEvent;
    using TargetLockVisual = Player::TargetLockVisual;
    using DroneAbilityVisual = Player::DroneAbilityVisual;
    using SpecialCombatStats = Player::SpecialCombatStats;
    using LaserShotEvent = Player::LaserShotEvent;
    using NeonBarrelLayout = Player::NeonBarrelLayout;
    using BarrelModel = Player::BarrelModel;
    using MeleeSlashEvent = Player::MeleeSlashEvent;
    using UiProfileStats = Player::UiProfileStats;
    using WallSmashTarget = Player::WallSmashTarget;
    using DroneLaserLink = Player::DroneLaserLink;
    using NeonBodyLayout = Player::NeonBodyLayout;
    using DashImpactEvent = Player::DashImpactEvent;
#if defined(USE_IMGUI) && !defined(NDEBUG)
    using UpgradeHudDebugSnapshot = Player::UpgradeHudDebugSnapshot;
#endif
    /// @brief 機体設定の選択・複製・編集・JSON保存を行う制作画面を表示する。
    /// @note USE_IMGUIが有効な構成で表示する。戦闘の更新と分けて呼ぶ。
    void DrawPlayerClassEditor();

private:
    Player& player_;
    PlayerUiState& ui_;
};
