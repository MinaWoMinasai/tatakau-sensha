#pragma once
#include <cstddef>

namespace tankrun {
/// @brief ランで得られるカード。並びは kEffectIds などの名前の表と旧来の数値 ID に対応するので、変えない。
enum class CardId {
    Ricochet,
    Heavy,
    Rapid,
    Thrusters,
    Capacitor,
    Repair,
    Drones,
    Pierce,
    ScatterShot,
    Homing,
    DashBurst,
    Overdrive,
    MeleeBlade,
    BladeReach,
    ImpactDrive,
    PerfectDodge,
    DroneFocus,
    DroneGuard,
    MeleeTempo,
    FinisherCharge,
    RailCannon,
    DroneLaserLink,
    SlashWave,
    ParryBlade,
    ExtraBarrel1,
    ExtraBarrel2,
    FanMount,
    AlternatingFire,
    HeavyDroneCore,
    LightBladeActuator,
    HeavyBladeEdge,
    ChainLightning,
    MarkDetonation,
    BoomerangShell,
    KillBurst,
    DroneCharge,
    DroneRebuildBomb,
    TargetPainter,
    AutonomousSpread,
    DashSlash,
    SpinBlade,
    WallSmash,
    // 遠征の熱を使う、派手で代償の大きい強化（集団戦向け）。
    ThermalNova,     // 熱核解放：オーバーヒートの瞬間に大爆発
    ChainDetonation, // 誘爆コア：倒した敵が爆発して連鎖する
    NovaBarrage,     // 全方位斉射：4回に1回、全周へ弾幕
    Overclock,       // 臨界駆動：熱が高いほど威力が上がる
    Singularity,     // 重力崩壊：ダッシュで敵を吸い寄せて爆発
    Count
};
constexpr std::size_t CardCount = static_cast<std::size_t>(CardId::Count);

/// @brief カードを、カードごとの配列の添字に変換する。
constexpr std::size_t CardIndex(CardId card)
{
    return static_cast<std::size_t>(card);
}
} // namespace tankrun
