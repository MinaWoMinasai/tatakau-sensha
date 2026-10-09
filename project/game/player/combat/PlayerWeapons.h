#pragma once
#include "Player.h"

/// @brief 自機の射撃・特殊攻撃とドローンへの指示を担当する。
class PlayerWeapons {
public:
    using SpecialEventKind = Player::SpecialEventKind;
    /// @brief 戦闘主体である自機を借用する。
    explicit PlayerWeapons(Player& player) : player_(player) {}
    using PlayerClassConfig = ::PlayerClassConfig;
    using MineDropEvent = Player::MineDropEvent;
    using WallSmashTarget = Player::WallSmashTarget;
    using MeleeSlashEvent = Player::MeleeSlashEvent;
    using LaserShotEvent = Player::LaserShotEvent;
    /// @brief ドローンの命中をロックへ蓄積し、今回適用する補正後のダメージを返す。
    /// @param drone 命中元ドローンの添字。0～31は蓄積に使い、負値は処理対象外。
    /// @param target 借用する命中対象。nullptr、死亡済み、資源、対応外の型は処理対象外。
    /// @param originalDamage ロック補正を掛ける前のダメージ。
    /// @return 能力・装備系統が対象外、または記録枠が確保できなければoriginalDamage。
    /// 対象の記録がある場合はロック倍率を掛けて四捨五入し、最低1としたダメージ。
    /// @note ロック成立時は集計と演出イベントも更新する。添字32以上は蓄積しないが既存ロックの倍率は適用する。
    /// HPへの適用は呼び出し側が行う。衝突側は弾のダメージを一時的に置き換え、衝突処理後に元へ戻す。
    uint32_t NotifyDroneHit(int drone, Collider* target, uint32_t originalDamage);

    /// @brief 対象の現在のロックによるダメージ倍率を返す。対象・能力・装備系統が該当しなければ1。
    /// @param boss ボス用の倍率を使うか。対象の型判定は呼び出し側で行う。
    float GetDroneTargetDamageScale(const Collider* target, bool boss) const;

    /// @brief 近接攻撃で押し出した経験値敵について、1秒以内の壁衝突を追加ダメージの候補として記録する。
    /// @param target 借用する対象。nullptr、死亡済み、資源、能力・装備系統が対象外なら記録しない。
    /// @param strength 0.5～2に制限して記録する強さ。現在の壁衝突ダメージ計算には使わない。
    /// @note 記録枠がない場合は何もしない。同じ対象の未処理の壁衝突があれば上書きしない。
    void ArmWallSmash(ExpEnemy* target, float strength = 1);

    /// @brief チャージ演出用に先頭砲塔の銃口のワールド座標を返す。設定がなければ照準方向の既定位置。
    cg2::Vector3 GetRailChargeMuzzle() const;

    /// @brief 追加能力・接続レーザー・斬撃波・パリィを更新し、必要なダメージ・弾・イベントを生成する。
    /// @param dt この処理で進める経過時間（秒）。
    /// @note 自機・敵・弾の移動と地形衝突の後、アクター・弾の衝突処理の前に1回呼ぶ。
    /// boss・enemiesのnullptrは対象なし。bulletsのnullptr、遠征補正無効、死亡中、dtが0以下なら
    /// レーザー接続一覧を消去した後に戻る。
    void UpdateSpecialCombat(Stage& stage, BulletManager* bullets, Enemy* boss, EnemyManager* enemies, float dt);

    /// @brief 誘導・ドローン照準に使う対象のワールド座標を、先頭48件までコピーする。
    void SetRunHomingTargets(const std::vector<cg2::Vector3>& targets);

    /// @brief 入力・装備・発射待ち時間に従って主攻撃を処理し、反動と実行イベントを更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    /// @note 初期化後に有効な弾管理先を渡す。内部でAddが拒否した場合も、攻撃の集計は登録弾数を保証しない。
    void Attack(BulletManager* BulletManager, float deltaTime);

    /// @brief ドローン数と待ち時間の条件を満たせば、ドローンを1機生成して弾管理先を設定する。
    /// @note この関数自体は弾を発射しない。ドローンの射撃は各ドローンの更新で行う。
    void DroneShoot(BulletManager* BulletManager);

    /// @brief Smasherの押下中の突進力を蓄積し、入力を離したときに突進速度を設定する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Smash(float deltaTime);

    /// @brief レール砲の入力を蓄積し、離したときに弾を追加して待ち時間・反動・集計を更新する。
    /// @param dt この処理で進める経過時間（秒）。
    /// @note 発射待ち中やbulletsがnullptrならチャージをリセットする。Addの登録成功を確認するAPIではない。
    void AttackRailCannon(BulletManager* bullets, bool pressed, float dt);

    /// @brief 遠征の初期射撃装備へ追加砲身・扇状配置・交互射撃の補正を反映し、必要なら描画装備を作り直す。
    void RefreshAdditiveArmaments();

    /// @brief EMP・ドローン任務待ち時間・ロック・回転斬撃・ダッシュ斬撃・壁衝突候補を初期状態へ戻す。
    void ResetAdditionalAbilities();

    /// @brief ドローン任務とロック、ダッシュ斬撃・回転斬撃・壁衝突ダメージを更新する。
    /// @param dt この処理で進める経過時間（秒）。
    /// @note UpdateSpecialCombatから呼ぶ。bulletsは現在未使用で、敵のnullptrは対象なしを表す。
    void UpdateAdditionalAbilities(Stage& stage, BulletManager* bullets, Enemy* boss, EnemyManager* enemies, float dt);

    /// @brief 近接系統のフィニッシュ後、入力継続とスタミナの条件を満たせば回転斬撃を開始する。
    /// @return 開始してスタミナ・攻撃待ち時間・演出イベントを更新した場合true。
    bool TryStartSpinBlade(bool pressed);

    /// @brief 現在の機体・遠征補正・EMPなどから、ドローンの射撃性能・発射間隔・追従設定を更新する。
    void ConfigureRunDrone(PlayerDrone& drone) const;

    /// @brief 発射条件へ現在の射撃強化・反射・耐久度・貫通などを反映する。
    /// @param applyFan 互換用の未使用引数。
    /// @note 遠征補正無効でも弾数を1、分裂数を0に設定する。自機の扇状配置は砲身で表す。
    void ApplyRunProjectileRules(AttackParam& param, bool applyFan = true) const;

    /// @brief 遠征の加速効果と突撃系統のダッシュ後の補正を含む、発射間隔に掛ける倍率を返す。
    float GetRunFireIntervalScale() const;

    /// @brief 登録した対象位置と誘導能力に従って、往路の自機弾の向きを更新する。弾の移動は行わない。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateRunProjectiles(BulletManager* bulletManager, float deltaTime);

    /// @brief 機体設定に従って弾・レーザー・地雷・近接攻撃を生成または予約し、発射待ち時間を更新する。
    /// @param baseReload 基準の発射間隔（秒）。
    /// @param recoilDir 発射後の反動方向の出力先。
    /// @param recoilPower 発射後の反動の大きさの出力先。ドローン方式では0にする。
    /// @return 攻撃処理を進めた場合true。待ち時間・上限などで処理しない場合false。
    /// @note 有効な弾管理先が必要。trueでも、ドローン上限やAddの拒否などにより生成弾数が増えるとは限らない。
    bool FireConfiguredClass(const PlayerClassConfig& config, BulletManager* bulletManager, float baseReload, cg2::Vector3& recoilDir,
                             float& recoilPower);

    /// @brief 機体設定と再使用待ち時間から特殊行動を選んで開始する。開始した場合true。
    bool TryActivateSpecialAction();

    /// @brief スタミナと移動方向の条件を満たせばダッシュを開始し、trueを返す。
    /// @note ジャスト回避の成立は後の衝突通知で判定する。この関数は回避成功を保証しない。
    bool ActivatePerfectDodge(const PlayerClassConfig& config);

    /// @brief 使用可能な近接装備とスタミナがあれば、剣カウンターの受付時間を設定してtrueを返す。
    bool ActivateSaberCounter(const PlayerClassConfig& config);

    /// @brief 最初の使用可能な近接装備で反撃を予約し、受付時間の解除・無敵時間・スロー要求を設定する。
    void TriggerSaberCounter(const PlayerClassConfig& config);

private:
    Player& player_;
};
