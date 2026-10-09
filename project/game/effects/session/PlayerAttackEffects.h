#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief PlayerAttackEffectsの処理を担当し、同じプレイの共有状態を借用する。
class PlayerAttackEffects {
public:
    /// @brief 借用するワールドを設定する。
    explicit PlayerAttackEffects(GameWorld& world) : world_(world) {}
    /// @brief 自機ネオンAfterimagesを更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdatePlayerNeonAfterimages(float deltaTime);

    /// @brief 発射イベントの壁までの表示を登録し、その時点の対象へレーザーダメージを適用する。
    /// @note 方向がほぼ0なら登録・ダメージ適用を行わない。残存表示から追加の命中判定は行わない。
    void SpawnPlayerLaser(const Player::LaserShotEvent& event);

    /// @brief レーザーの表示寿命だけを進め、期限切れの表示を消去する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdatePlayerLasers(float deltaTime);

    /// @brief 自機Lasersを後で処理するために予約する。
    void QueuePlayerLasers(const cg2::Vector3& cameraForward);

    /// @brief 発射イベントから地雷の位置・威力・待機時間・寿命を登録する。
    void SpawnPlayerMine(const Player::MineDropEvent& event);

    /// @brief 地雷の待機時間・寿命を進め、寿命切れまたは待機後の近接で起爆し、終了した爆発表示を消去する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdatePlayerMines(float deltaTime);

    /// @brief 添字の地雷の範囲ダメージを適用し、爆発表示を登録して地雷を削除する。
    /// @param index 現在の地雷配列の添字。範囲外なら何もしない。
    void DetonatePlayerMine(size_t index);

    /// @brief 自機Minesを後で処理するために予約する。
    void QueuePlayerMines(const cg2::Vector3& cameraRight, const cg2::Vector3& cameraUp, const cg2::Vector3& cameraForward);

    /// @brief 斬撃イベントから予備動作・振り・回復動作と命中履歴を持つ斬撃状態を登録する。
    /// @note 方向がほぼ0なら登録しない。この時点では命中ダメージを適用しない。
    void SpawnPlayerMeleeSlash(const Player::MeleeSlashEvent& event);

    /// @brief 自機に追従する斬撃の時間・命中・押し飛ばし・軌跡を処理し、期限切れの斬撃を消去する。
    /// @note 同じ斬撃の命中履歴にある対象へは再適用しない。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdatePlayerMeleeSlashes(float deltaTime);

    /// @brief 自機近接攻撃Slashesを後で処理するために予約する。
    void QueuePlayerMeleeSlashes();

    /// @brief 強化/特殊戦闘のイベントを消費して演出へ写し、既存演出の経過時間と特殊弾の表示情報を更新する。
    /// @note 攻撃結果は適用済み。表示上限でもイベントは消費し、ダメージを再適用しない。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateSpecialCombatPresentation(float deltaTime);

    /// @brief 特殊戦闘画面演出を後で処理するために予約する。
    void QueueSpecialCombatPresentation();

    /// @brief 自機近接攻撃軌跡設定を作成して返す。
    cg2::TrailConfig MakePlayerMeleeTrailConfig(const PlayerMeleeSlash& slash, float alphaScale = 1.0f) const;

    /// @brief 振りの進行度を0～1へ制限して刃外側の軌跡の2端点を計算し、base/tipへ書き込む。
    void ComputePlayerMeleeBladeSection(const PlayerMeleeSlash& slash, float progress, cg2::Vector3& base, cg2::Vector3& tip) const;

private:
    GameWorld& world_;
};
} // namespace gameplay
