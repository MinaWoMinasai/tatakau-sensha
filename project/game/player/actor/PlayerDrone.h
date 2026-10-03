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

    /// @brief 遠征では護衛中に設定した射撃を行い、旧方式ではマウス入力で射撃する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Attack(float deltaTime = 1.0f / 60.0f);
    /// @brief 遠征射撃を有効にし、射撃設定をコピーする。1回の弾数は1に固定する。
    /// @param reloadSeconds 発射間隔（秒）。最低0.05秒に補正する。
    void ConfigureRunAttack(const AttackParam& param, float reloadSeconds);
    /// @brief 射撃ごとに小数の余りを繰り越す威力を設定する。負値は0、1未満ならこの補正を使わない。
    void SetRunExactDamage(float amount)
    {
        runExactDamage_ = (std::max)(0.0f, amount);
    }
    /// @brief マウス入力に代わる照準のワールド座標と射撃入力を設定し、入力の上書きを有効にする。
    void SetRunInput(const cg2::Vector3& target, bool attack)
    {
        runInputOverride_ = true;
        runAimTarget_ = target;
        runWantsAttack_ = attack;
    }
    /// @brief 自機のワールド座標へ加える追従先のオフセットを設定する。
    void SetRunFollowOffset(const cg2::Vector3& offset)
    {
        runFollowOffset_ = offset;
    }
    /// @brief 追従の基準速度・追い付き速度の上限・指数補間の応答係数を設定する。
    /// @note speed・catchupは60FPS相当の基準1フレームの移動量、responseは毎秒の応答係数。
    void SetRunFollowTuning(float speed, float catchup, float response)
    {
        runFollowSpeed_ = speed;
        runCatchupSpeed_ = catchup;
        runFollowResponse_ = response;
    }
    /// @brief 射撃の待ち時間を解除し、次の護衛射撃を要求する。入力の上書き中はそちらの射撃入力を使う。
    void RallyRunAttack()
    {
        runShotCooldown_ = 0.0f;
        runRallyShotPending_ = true;
    }
    /// @brief 生成する弾へ渡す自機の借用先とドローンの添字を設定する。所有権は移らない。
    void SetRunOwner(Player* player, int index)
    {
        runAttackParam_.sourcePlayer = player;
        runAttackParam_.sourceDroneIndex = index;
    }
    /// @brief 護衛中なら対象のワールド座標を記録して突撃の予告を開始し、trueを返す。
    /// @note 開始できなければ対象位置も変更しない。bombがtrueなら到着後に再構築へ進む。
    bool StartRunMission(const cg2::Vector3& target, bool bomb)
    {
        if (!mission_.Start(bomb))
            return false;
        missionTarget_ = target;
        return true;
    }
    /// @brief 現在の任務状態への読み取り専用参照を返す。
    const tankspecial::DroneMission& GetRunMission() const
    {
        return mission_;
    }
    /// @brief 任務開始時に記録した対象のワールド座標への読み取り専用参照を返す。
    const cg2::Vector3& GetRunMissionTarget() const
    {
        return missionTarget_;
    }
    /// @brief 死亡中・再構築中でなければtrue。新しい任務を開始できるかとは別の判定。
    bool IsRunAvailable() const
    {
        return !isDead_ && mission_.Available();
    }
    /// @brief 遠征任務衝撃の未処理分を取り出し、内部の保留分を消費済みにする。
    bool ConsumeRunMissionImpact()
    {
        return mission_.ConsumeImpact();
    }
    /// @brief 再構築完了の有無を返し、保留フラグを解除する。
    bool ConsumeRunRebuilt()
    {
        const bool value = rebuilt_;
        rebuilt_ = false;
        return value;
    }

    /// @brief 遠征射撃の入力上書き中は指定したワールド座標へ、それ以外はマウスの照準へ向きを更新する。
    void RotateToMouse(cg2::Camera* viewProjection);

    /// @brief 初期化
    /// @param velocity 初期の移動速度。
    /// @param position 初期座標
    void Initialize(const cg2::Vector3& position, const cg2::Vector3& velocity);

    /// @brief 任務を進めて自機または突撃先へ移動し、地形との衝突を解決して射撃する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Update(cg2::Camera* viewProjection, Stage& stage, const cg2::Vector3& playerPosition, float deltaTime = 1.0f / 60.0f);

    /// @brief 描画
    void Draw();
    /// @brief ネオン表示を設定する。
    /// @note 有効なら通常モデルをDrawで描かず、シーン側で機体と同じ制作データのネオン形状を使う。
    void SetNeonVisual(bool enabled)
    {
        neonVisual_ = enabled;
    }
    /// @brief ネオンによる機体描画を使用するか判定する。
    bool UsesNeonVisual() const
    {
        return neonVisual_;
    }
    /// @brief 死亡・再構築と無敵時間の点滅条件から、現在の表示可否を返す。
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

    /// @brief 互換用の空処理。現在はスプライトを描画しない。
    void DrawSprite();

    /// @brief 成立した接触を受け、対象の属性に応じてHPと押し出し速度を更新する。
    /// @note otherはnullptr不可。再構築中と資源は対象外。HP減少時に無敵時間は検査しない。
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

    /// @brief ワールド座標を設定し、描画オブジェクトへ反映する。
    void SetWorldPosition(const cg2::Vector3& pos)
    {
        worldTransform_.translate = pos;
        object_->SetTransform(worldTransform_);
        object_->Update();
    }

    /// @brief AABBを返す。
    cg2::AABB GetAABB();

    /// @brief 無敵時間が切れていればHPを1減らし、2秒の無敵時間を設定する。死亡判定は後の更新で行う。
    void Damage();

    /// @brief 死亡フラグを設定する。既に死亡中なら何もしない。所有側の配列からは削除しない。
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
    /// @brief 最大HPを返す。
    int GetMaxHp() const
    {
        return kMaxHp;
    }

    /// @brief 通常射撃と遠征射撃で使う弾管理先を借用する。射撃中は有効な管理先を保つ。
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
