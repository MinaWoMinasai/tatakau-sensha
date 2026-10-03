#pragma once
#include <vector>
#include <memory>
#include "TrailManager.h"
#include "Bullet.h"

class Stage;
class Enemy;
class EnemyManager;

/// @brief 弾と軌跡を所有し、更新・描画を行う。衝突通知で予約した追加弾は走査後に生成する。
class BulletManager {
public:
    enum class BuildEventKind {
        Chain,
        Detonation,
        Burst,
        Return,
        Reflection
    };
    /// @brief 射撃強化の演出用に、イベントの種類・端点・強さを保持する。
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
    /// @brief 射撃強化の命中・発動・追加弾の登録・帰還開始・反射予約の累積件数を保持する。
    struct ShooterStats {
        uint64_t chainHits = 0, detonations = 0, burstTriggers = 0, burstChildren = 0, returns = 0, reflections = 0;
    };
    /// @brief ClearAll以降の射撃強化の集計への読み取り専用参照を返す。
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
    /// @note 所有権は移らない。nullptrは参照先なしを表す。利用中は各参照先を有効に保つ。
    /// ClearAllで参照を解除する。地形の参照先はUpdateで設定する。
    void SetCombatContext(Player* player, Enemy* boss, EnemyManager* enemies)
    {
        combatPlayer_ = player;
        combatBoss_ = boss;
        combatEnemies_ = enemies;
    }
    /// @brief 自機弾の命中後に射撃強化を処理し、連鎖・起爆のダメージ適用と破裂弾の予約を行う。
    /// @param killed 元の衝突でtargetが撃破されたか。
    /// @note 元の衝突通知の後に呼ぶ。破裂子弾、ドローン弾、資源への命中は対象外。
    void NotifyPlayerHit(Bullet& bullet, Collider& target, bool killed);
    /// @brief 装甲反射で発生する弾を衝突走査後の生成へ予約する。
    /// @note 元の弾は変更しない。既に装甲反射済み、速度がほぼ0、または予約列が48件以上なら予約しない。
    void QueueArmorReflection(const Bullet& bullet, const cg2::Vector3& targetPosition);
    /// @brief 管理配列に残る弾の所有者別内訳を表す。死亡済みで削除待ちの弾も含む。
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
    /// @brief ClearAll以降の自機の弾による成長イベントの集計を返す。敵弾は含めない。
    /// @note FlushPendingSplitsで弾のイベントを取り込んだ時点の値への読み取り専用参照。
    const GrowthStats& GetGrowthStats() const
    {
        return growthStats_;
    }
    /// @brief 特殊命中イベントの未処理分を取り出し、内部の保留分を消費済みにする。
    /// @note FlushPendingSplitsで弾から回収した分を返す。保留は最大64件。
    std::vector<Bullet::SpecialImpact> ConsumeSpecialImpacts()
    {
        auto events = std::move(specialImpacts_);
        specialImpacts_.clear();
        return events;
    }

    /// @brief 弾の軌跡を管理する描画資源を初期化する。
    /// @note 描画基盤を準備してから呼び、渡した利用先を利用期間中有効に保つ。既存の弾は消去しない。
    void Initialize(cg2::DirectXCommon* dxCommon, cg2::Object3dCommon* object3dCommon);
    /// @brief 弾の所有権を受け取り、追加条件を満たす場合に管理配列へ登録する。
    /// @param bullet 所有権を渡す弾。空のunique_ptrなら何もしない。
    /// @note 渡す弾のUsesRunProjectileRules()がtrueの場合だけ、同じ所有者の生存弾が
    /// kMaxRunProjectilesPerOwner以上なら追加を拒否する。集計には同じ所有者の通常弾も含む。
    /// 拒否した弾はこの呼び出し内で破棄する。所有権の受け渡しは登録成功を保証せず、結果も返さない。
    /// 衝突の走査中には呼ばず、追加弾を予約して走査後に反映する。
    void Add(std::unique_ptr<Bullet> bullet);
    static constexpr size_t kMaxRunProjectilesPerOwner = 240;
    /// @brief 予約した破裂・反射・分裂弾を追加し、弾の成長・特殊命中イベントを回収する。
    /// @note 個々の衝突コールバックから呼ばない。走査中の配列変更を避けるための境界。
    void FlushPendingSplits();
    /// @brief 弾・軌跡・予約・戦闘イベント・集計・マーキングを消去し、戦闘参照先を解除する。
    /// @note アクターやステージの置き換え前に、フレームの間で呼ぶ。
    void ClearAll();

    /// @brief 弾と軌跡を更新し、地形衝突・予約弾の反映・死亡弾の削除を順に行う。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Update(Stage& stage, float deltaTime);
    /// @brief 各弾に互換用のDrawを呼ぶ。現在、弾本体はこの経路で描画しない。
    void Draw();
    /// @brief 軌跡を描画する。
    void DrawTrails(const cg2::Matrix4x4& viewProjection);

    /// @brief 衝突判定に使う弾のポインター一覧を返す。
    /// @note 一覧はコピーで、弾は借用する。死亡済みの弾も含むため利用時に状態を確認する。
    /// 所有権は移らない。次の更新・削除・ClearAllをまたいで保持しない。
    std::vector<Bullet*> GetBulletPtrs() const;
    /// @brief 管理中の弾の総数を返す。
    /// @note 管理配列の要素数。死亡済みで次のUpdateによる削除を待つ弾も含む。
    size_t GetBulletCount() const
    {
        return bullets_.size();
    }
    /// @brief 管理中の弾数を、自機・敵・敵対する経験値敵の所有者別に集計して値で返す。
    /// @note IsDeadは検査せず、死亡済みで削除待ちの弾も数える。
    BulletCounts GetBulletCounts() const;
    /// @brief 弾の軌跡設定への読み取り専用参照を返す。
    const BulletTrailSettings& GetTrailSettings() const
    {
        return trailSettings_;
    }
    /// @brief 軌跡設定をコピーし、検査対象の数値の範囲を補正する。非有限の検査対象値は各既定値に戻す。
    /// @note 色の各成分は、この関数では検査・補正しない。
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
    /// @brief 生存しHPが正のボスと、生存している資源以外の経験値敵の借用ポインター一覧を返す。
    std::vector<Collider*> LiveCombatTargets() const;
    /// @brief 2点間の内部を最大128分割して地形との接触を調べ、遮るサンプルがなければtrueを返す。
    /// @note 地形の参照先が未設定ならtrue。端点の検査や厳密な線分交差判定は行わない。
    bool HasClearLink(const cg2::Vector3& from, const cg2::Vector3& to) const;
    /// @brief 対応するボス・経験値敵へ射撃強化のダメージを適用する。
    /// @return この適用で撃破した場合true。falseでもダメージを適用している場合がある。
    /// @note nullptrや対応外の型、死亡済み・資源の経験値敵には適用しない。
    bool DamageBuildTarget(Collider* target, uint32_t damage, const cg2::Vector3& source);
    /// @brief 撃破時の破裂能力が有効なら、破裂子弾を予約し発動を集計する。
    /// @note 破裂子弾からの再発動、または予約列が42件を超える場合は予約しない。
    void QueueKillBurst(const Bullet& bullet, const cg2::Vector3& position);
    /// @brief 演出イベントを保留列へ追加する。既に64件あれば追加しない。
    void BuildEventAt(BuildEventKind kind, const cg2::Vector3& start, const cg2::Vector3& end, float strength = 1);
    std::vector<std::unique_ptr<Bullet>> bullets_;
    std::unique_ptr<cg2::TrailManager> trailManager_;
    BulletTrailSettings trailSettings_;
    GrowthStats growthStats_{};
    std::vector<Bullet::SpecialImpact> specialImpacts_;
};
