#pragma once
#include "Collider.h"

class BulletManager;

/// @brief 当たり判定で使う戦闘のルールと、接触が成立したときの処理をまとめる。
/// @note CollisionManagerはマスクと形状で接触を調べ、弾・敵・自機など型ごとの扱いはこのクラスへ任せる。
class CombatCollisionRules {
public:
    /// @brief 反射弾の予約と射撃強化の通知に使う弾管理を設定する。nullptrならそれらを行わない。
    void SetBulletManager(BulletManager* bulletManager)
    {
        bulletManager_ = bulletManager;
    }

    /// @brief 形状を調べる前に、死亡済みの弾・敵や命中済みの組を除外する。調べる必要があればtrue。
    bool CanTest(Collider* colliderA, Collider* colliderB) const;

    /// @brief 特殊弾・帰還弾が、前回位置からの移動区間で相手に触れたか判定する。
    bool HitsBySweep(Collider* colliderA, Collider* colliderB) const;

    /// @brief 接触が成立した組に戦闘のルールを適用し、通常の接触なら双方へ通知する。
    /// @note ダッシュ衝撃・床コア・装甲反射は通常の通知を置き換える。ドローンの命中補正は今回の接触だけに使う。
    void Resolve(Collider* colliderA, Collider* colliderB);

private:
    BulletManager* bulletManager_ = nullptr;
};
