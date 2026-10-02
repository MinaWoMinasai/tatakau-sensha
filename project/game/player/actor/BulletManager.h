#pragma once
#include <vector>
#include <memory>
#include "TrailManager.h"
#include "Bullet.h"

class Stage;
class Enemy;
class EnemyManager;

/// @brief 弾の所有・更新・描画と衝突後の追加生成を管理する。衝突走査中に弾配列を増やさない。
class BulletManager {
public:
    enum class BuildEventKind {
        Chain,
        Detonation,
        Burst,
        Return,
        Reflection
    };
    /// @brief 射撃系の強化による攻撃イベントを演出へ渡す。
    struct BuildEvent {
        BuildEventKind kind;
        cg2::Vector3 start{}, end{};
        float strength = 1;
    };
    /// @brief マーキング対象の位置と蓄積数を表示へ渡す。
    struct MarkVisual {
        cg2::Vector3 position;
        int stacks = 0;
    };
    /// @brief 射撃系の強化が実際に発動した回数を集計する。
    struct ShooterStats {
        uint64_t chainHits = 0, detonations = 0, burstTriggers = 0, burstChildren = 0, returns = 0, reflections = 0;
    };
    /// @brief 射撃系の強化が発動した回数の集計を返す。
    const ShooterStats& GetShooterStats() const
    {
        return shooterStats_;
    }
    /// @brief 射撃系の強化イベントの未処理分を取り出し、内部の保留分を消費済みにする。
    std::vector<BuildEvent> ConsumeBuildEvents()
    {
        auto out = std::move(buildEvents_);
        buildEvents_.clear();
        return out;
    }
    /// @brief マーキングされた対象の位置と蓄積数を表示用に返す。
    std::vector<MarkVisual> GetMarkVisuals() const;
    /// @brief 戦闘時に参照する自機・ボス・敵の管理先を設定する。
    void SetCombatContext(Player* player, Enemy* boss, EnemyManager* enemies)
    {
        combatPlayer_ = player;
        combatBoss_ = boss;
        combatEnemies_ = enemies;
    }
    /// @brief 自機の弾の命中を射撃強化の発動と集計へ反映する。
    void NotifyPlayerHit(Bullet& bullet, Collider& target, bool killed);
    /// @brief 装甲反射で発生する弾を衝突走査後の生成へ予約する。
    void QueueArmorReflection(const Bullet& bullet, const cg2::Vector3& targetPosition);
    /// @brief 所有者ごとの弾数を表す。
    struct BulletCounts {
        size_t player = 0;
        size_t enemy = 0;
        size_t hostileExpEnemy = 0;
    };
    /// @brief 弾の挙動による成長イベントの累積件数を保持する。
    struct GrowthStats {
        uint64_t wallBounces = 0;
        uint64_t actorPierces = 0;
        uint64_t impactSplits = 0;
        uint64_t splitChildrenSpawned = 0;
    };
    // Actual player-owned growth events since ClearAll (enemy shots excluded).
    /// @brief ClearAll以降の自機の弾による成長イベントの集計を返す。敵弾は含めない。
    const GrowthStats& GetGrowthStats() const
    {
        return growthStats_;
    }
    /// @brief 特殊命中イベントの未処理分を取り出し、内部の保留分を消費済みにする。
    std::vector<Bullet::SpecialImpact> ConsumeSpecialImpacts()
    {
        auto events = std::move(specialImpacts_);
        specialImpacts_.clear();
        return events;
    }

    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(cg2::DirectXCommon* dxCommon, cg2::Object3dCommon* object3dCommon);
    /// @brief 弾の所有権を受け取り、更新・描画の対象へ追加する。
    /// @param bullet 追加する弾。呼び出し後の所有者はBulletManager。
    void Add(std::unique_ptr<Bullet> bullet);
    static constexpr size_t kMaxRunProjectilesPerOwner = 240;
    // Called after a complete collision pass, never from an individual callback.
    /// @brief 予約した分裂・反射の弾を、衝突走査が全て終わってから追加する。
    /// @note 個々の衝突コールバックから呼ばない。走査中の配列変更を避けるための境界。
    void FlushPendingSplits();
    // Call between frames, before moving actors or replacing the stage.
    /// @brief 弾・軌跡と戦闘イベントを消去する。
    /// @note アクターやステージの置き換え前に、フレームの間で呼ぶ。
    void ClearAll();

    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Update(Stage& stage, float deltaTime);
    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    void Draw();
    /// @brief 軌跡を描画する。
    void DrawTrails(const cg2::Matrix4x4& viewProjection);

    /// @brief 衝突判定に使う弾のポインター一覧を返す。
    /// @note 所有権は移らない。次の更新・削除・ClearAllをまたいで保持しない。
    std::vector<Bullet*> GetBulletPtrs() const;
    /// @brief 管理中の弾の総数を返す。
    size_t GetBulletCount() const
    {
        return bullets_.size();
    }
    /// @brief 管理中の弾の総数を返す。
    BulletCounts GetBulletCounts() const;
    /// @brief 弾の軌跡の設定を返す。
    const BulletTrailSettings& GetTrailSettings() const
    {
        return trailSettings_;
    }
    /// @brief 弾の軌跡の設定を設定する。
    void SetTrailSettings(const BulletTrailSettings& settings);
    /// @brief 管理中の軌跡の本数を返す。
    size_t GetTrailInstanceCount() const;
    /// @brief 描画可能軌跡が存在するか判定する。
    bool HasDrawableTrails() const
    {
        return trailManager_ && trailManager_->HasDrawableInstances();
    }
    /// @brief 軌跡の描画件数・頂点数の集計を返す。軌跡管理がなければ空の集計を返す。
    cg2::TrailManager::DrawStats GetTrailDrawStats() const
    {
        return trailManager_ ? trailManager_->GetDrawStats() : cg2::TrailManager::DrawStats{};
    }

private:
    /// @brief 衝突走査が終わってから追加する弾の生成情報を保持する。
    struct PendingShot {
        cg2::Vector3 position{}, velocity{};
        uint32_t damage = 1;
        bool reflection = false;
    };
    std::vector<PendingShot> pendingBuildShots_;
    std::vector<BuildEvent> buildEvents_;
    tankshooter::MarkLedger marks_{};
    ShooterStats shooterStats_{};
    Player* combatPlayer_ = nullptr;
    Enemy* combatBoss_ = nullptr;
    EnemyManager* combatEnemies_ = nullptr;
    Stage* combatStage_ = nullptr;
    /// @brief 現在生存している戦闘対象のポインター一覧を返す。
    std::vector<Collider*> LiveCombatTargets() const;
    /// @brief 2点間の射線が地形に遮られていないか判定する。
    bool HasClearLink(const cg2::Vector3& from, const cg2::Vector3& to) const;
    /// @brief 強化ビルド対象へダメージを適用する。
    bool DamageBuildTarget(Collider* target, uint32_t damage, const cg2::Vector3& source);
    /// @brief 撃破破裂を後で処理するために予約する。
    void QueueKillBurst(const Bullet& bullet, const cg2::Vector3& position);
    /// @brief イベントAtを組み立てる。
    void BuildEventAt(BuildEventKind kind, const cg2::Vector3& start, const cg2::Vector3& end, float strength = 1);
    std::vector<std::unique_ptr<Bullet>> bullets_;
    std::unique_ptr<cg2::TrailManager> trailManager_;
    BulletTrailSettings trailSettings_;
    GrowthStats growthStats_{};
    std::vector<Bullet::SpecialImpact> specialImpacts_;
};
