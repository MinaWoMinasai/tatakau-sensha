#pragma once
#include "Collider.h"
#include "CombatCollisionRules.h"
#include <vector>

class Enemy;
class Player;
class PlayerDrone;
class BulletManager;
class EnemyManager;

/// @brief アクターと弾を借用して、どれとどれが接触したかを調べ、走査後に予約弾の生成を反映する。
/// @note 接触した組への処理（ゲームのルール）はCombatCollisionRulesが行う。
class CollisionManager {
public:
    /// @brief 対象一覧を作り直して全ペアを調べ、成立した接触を通知して走査後に予約弾を生成する。
    /// @note player・bulletManagerはnullptr不可。enemy・enemyManagerのnullptrは対象なし。
    /// 弾の移動・地形衝突・特殊戦闘更新の後に呼び、走査中は借用対象を削除しない。
    void CheckAllCollisions(Player* player, Enemy* enemy, BulletManager* bulletManager, EnemyManager* enemyManager = nullptr);

    /// @brief 形状・属性・命中履歴を確認し、接触が成立すれば双方へ通知する。
    /// @param colliderA 接触候補の借用先。nullptr不可。
    /// @param colliderB 接触候補の借用先。nullptr不可。
    /// @note ダッシュ衝撃や装甲反射は通常通知を置き換える。CheckAllCollisionsの走査外で単独に呼ぶ場合は管理クラスへの射撃強化通知を行わない。
    void CheckCollisionPair(Collider* colliderA, Collider* colliderB);

    /// @brief 自機・利用可能なドローン・敵・弾の借用ポインターを内部一覧の末尾へ追加する。
    /// @note 既存一覧は消去しない。CheckAllCollisionsは呼び出し前に消去する。nullptrの条件は同関数と同じ。
    void SetColliders(Player* player, Enemy* enemy, BulletManager* bulletManager, EnemyManager* enemyManager = nullptr);

    /// @brief 最後に登録した衝突対象一覧への読み取り専用参照を返す。要素の所有権は持たない。
    /// @note 弾・敵・ドローンの削除後は要素を利用しない。次の走査で一覧を作り直す。
    const std::vector<Collider*>& GetColliders() const
    {
        return colliders_;
    }

private:
    /// @brief マスクと戦闘のルールで除外されなかった組が、形状として接触しているか判定する。
    bool IsTouching(Collider* colliderA, Collider* colliderB) const;

    // 接触の前後に適用する戦闘のルール。
    CombatCollisionRules rules_;
    // コライダーの一覧。毎フレーム作り直すが、容量は使い回す。
    std::vector<Collider*> colliders_;
};
