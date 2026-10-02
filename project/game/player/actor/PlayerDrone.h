#pragma once
#include "game/weapon/CombatTypes.h"
#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <list>
#include "Calculation.h"
#include "Collider.h"
#include "CollisionConfig.h"
#include "Object3d.h"
#include "Sprite.h"
#include "AttackController.h"
#include "PlayerDrone.h"
#include "game/player/TankSpecialCombat.h"

class Stage;

/// @brief 自機に属するドローン1体の追従・攻撃・衝突・描画を管理する。
class PlayerDrone : public Collider {

public:
    /// @brief デストラクタ
    ~PlayerDrone();

    /// @brief 攻撃
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Attack(float deltaTime = 1.0f / 60.0f);
    /// @brief 遠征攻撃を利用条件を設定する。
    void ConfigureRunAttack(const AttackParam& param, float reloadSeconds);
    /// @brief 遠征Exactダメージを設定する。
    void SetRunExactDamage(float amount)
    {
        runExactDamage_ = (std::max)(0.0f, amount);
    }
    /// @brief 遠征入力を設定する。
    void SetRunInput(const cg2::Vector3& target, bool attack)
    {
        runInputOverride_ = true;
        runAimTarget_ = target;
        runWantsAttack_ = attack;
    }
    /// @brief 遠征追従差分を設定する。
    void SetRunFollowOffset(const cg2::Vector3& offset)
    {
        runFollowOffset_ = offset;
    }
    /// @brief 遠征追従調整値を設定する。
    void SetRunFollowTuning(float speed, float catchup, float response)
    {
        runFollowSpeed_ = speed;
        runCatchupSpeed_ = catchup;
        runFollowResponse_ = response;
    }
    /// @brief 遠征中の攻撃強化を現在の攻撃へ反映する。
    void RallyRunAttack()
    {
        runShotCooldown_ = 0.0f;
        runRallyShotPending_ = true;
    }
    /// @brief 遠征所有者を設定する。
    void SetRunOwner(Player* player, int index)
    {
        runAttackParam_.sourcePlayer = player;
        runAttackParam_.sourceDroneIndex = index;
    }
    /// @brief 遠征任務を開始する。
    bool StartRunMission(const cg2::Vector3& target, bool bomb)
    {
        if (!mission_.Start(bomb))
            return false;
        missionTarget_ = target;
        return true;
    }
    /// @brief 遠征任務を返す。
    const tankspecial::DroneMission& GetRunMission() const
    {
        return mission_;
    }
    /// @brief 遠征任務対象を返す。
    const cg2::Vector3& GetRunMissionTarget() const
    {
        return missionTarget_;
    }
    /// @brief 遠征利用可能であるか判定する。
    bool IsRunAvailable() const
    {
        return !isDead_ && mission_.Available();
    }
    /// @brief 遠征任務衝撃の未処理分を取り出し、内部の保留分を消費済みにする。
    bool ConsumeRunMissionImpact()
    {
        return mission_.ConsumeImpact();
    }
    /// @brief 遠征Rebuiltの未処理分を取り出し、内部の保留分を消費済みにする。
    bool ConsumeRunRebuilt()
    {
        const bool value = rebuilt_;
        rebuilt_ = false;
        return value;
    }

    /// @brief マウスの方を向く
    void RotateToMouse(cg2::Camera* viewProjection);

    /// @brief 初期化
    /// @param velocity 初期の移動速度。
    /// @param position 初期座標
    void Initialize(const cg2::Vector3& position, const cg2::Vector3& velocity);

    /// @brief 更新
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Update(cg2::Camera* viewProjection, Stage& stage, const cg2::Vector3& playerPosition, float deltaTime = 1.0f / 60.0f);

    /// @brief 描画
    void Draw();
    // The arena renders companions with the same authored neon geometry as tanks.
    /// @brief ネオン表示を設定する。
    void SetNeonVisual(bool enabled)
    {
        neonVisual_ = enabled;
    }
    /// @brief ネオンによる機体描画を使用するか判定する。
    bool UsesNeonVisual() const
    {
        return neonVisual_;
    }
    /// @brief 表示表示中であるか判定する。
    bool IsVisualVisible() const;
    /// @brief 照準方向を返す。
    const cg2::Vector3& GetAimDirection() const
    {
        return dir;
    }
    /// @brief ネオン銃口フラッシュ比率を返す。
    float GetNeonMuzzleFlashRatio() const
    {
        if (!runAttackEnabled_ || runShotCooldown_ <= 0.0f)
            return 0.0f;
        const float flashSeconds = (std::min)(0.075f, runReloadSeconds_);
        return (std::clamp)((runShotCooldown_ - runReloadSeconds_ + flashSeconds) / flashSeconds, 0.0f, 1.0f);
    }

    /// @brief スプライト描画
    void DrawSprite();

    /// @brief 衝突判定
    void OnCollision(Collider* other) override;

    // ワールド座標を取得

    // 半径
    static inline const float kRadius = 1.0f;

    /// @brief ワールド座標での位置を返す。
    cg2::Vector3 GetWorldPosition() const override;

    /// @brief 移動を返す。
    cg2::Vector3 GetMove()
    {
        return velocity_;
    }
    /// @brief 速度を設定する。
    void SetVelocity(const cg2::Vector3& v)
    {
        velocity_ = v;
    }

    // セッター
    void SetWorldPosition(const cg2::Vector3& pos)
    {
        worldTransform_.translate = pos;
        object_->SetTransform(worldTransform_);
        object_->Update();
    }

    /// @brief AABBを返す。
    cg2::AABB GetAABB();

    /// @brief 指定ダメージを戦闘状態へ反映する。
    void Damage();

    /// @brief 死亡状態にし、以降の攻撃・衝突などの対象から外す。
    void Die();

    /// @brief 死亡であるか判定する。
    bool IsDead() const
    {
        return isDead_;
    }
    /// @brief 現在のHPを返す。
    int GetHp() const
    {
        return hp_;
    }
    /// @brief 最大値HPを返す。
    int GetMaxHp() const
    {
        return kMaxHp;
    }

    /// @brief 攻撃制御弾管理を設定する。
    void SetAttackControllerBulletManager(BulletManager* bulletManager)
    {
        attackController_.SetBulletManager(bulletManager);
        runBulletManager_ = bulletManager;
    }

private:
    static inline const int kMaxHp = 10;

    // ワールド変換データ
    cg2::Transform worldTransform_;

    // モデル
    std::unique_ptr<cg2::Object3d> object_;
    // テクスチャハンドル
    uint32_t textureHandle_ = 0u;

    // キーボード入力
    cg2::Input* input_ = nullptr;

    // 弾
    cg2::Vector3 dir;

    // キャラクターの移動速さ
    float kCharacterSpeed = 0.2f;
    cg2::Vector3 move_;

    const int kBulletTime = 30;
    int bulletCoolTime = 0;
    bool runAttackEnabled_ = false;
    AttackParam runAttackParam_{};
    float runExactDamage_ = 0, runDamageRemainder_ = 0;
    float runReloadSeconds_ = 0.5f;
    float runShotCooldown_ = 0.0f;
    bool runRallyShotPending_ = false;
    BulletManager* runBulletManager_ = nullptr;
    bool runInputOverride_ = false, runWantsAttack_ = false;
    cg2::Vector3 runAimTarget_{}, runFollowOffset_{};
    float runFollowSpeed_ = 0.25f, runCatchupSpeed_ = 0.62f, runFollowResponse_ = 5.0f;

    // キャラクターの当たり判定サイズ
    static inline const float kWidth = 1.6f;
    static inline const float kHeight = 1.6f;

    cg2::Vector3 velocity_{0, 0, 0}; // 現在速度
    cg2::Vector3 inputDir_{0, 0, 0}; // 入力方向

    float maxSpeed_ = 0.25f; // 最高速度
    float accel_ = 0.7f;     // 加速
    float decel_ = 3.5f;     // 減速（ブレーキ）

    cg2::Transform hpTransform_;

    int hp_ = kMaxHp;

    // 無敵時間
    float invincibleTimer_ = 0.0f;

    bool isDead_ = false;
    bool neonVisual_ = false;
    tankspecial::DroneMission mission_{};
    cg2::Vector3 missionTarget_{};
    bool rebuilt_ = false;

    // 攻撃コントローラ
    AttackController attackController_;

    float angle_ = 0.0f;
};
