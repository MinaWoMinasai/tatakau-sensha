#pragma once
#include "game/weapon/CombatTypes.h"
#include "Calculation.h"
#include "Bullet.h"
#include "BulletManager.h"

/// @brief 機体の発射設定から弾を作り、BulletManagerへ所有権を渡す。
class AttackController {
public:
    /// @brief 発射先の弾管理クラスを借用する。所有権は移らず、発射中は有効な管理先を保つ。
    void SetBulletManager(BulletManager* manager)
    {
        bulletManager_ = manager;
    }

    /// @brief 中心位置から、所有者別の位置補正と射撃設定に従って弾を生成する。
    /// @note 先に有効な弾管理先を設定する。baseDirは有限で長さが0.0001より大きい方向を渡す。
    /// 速度には正規化した方向、位置補正には元のbaseDirを使う。Addの登録成否は返さない。
    void Fire(const cg2::Vector3& origin, const cg2::Vector3& baseDir, const AttackParam& param, BulletOwner owner);

    /// @brief 指定した銃口のワールド座標から弾を生成する。中心からの位置補正は行わない。
    /// @note 弾管理先と方向の条件、登録成否の扱いはFireと同じ。
    void FireFromMuzzle(const cg2::Vector3& muzzlePosition, const cg2::Vector3& baseDir, const AttackParam& param, BulletOwner owner);

private:
    /// @brief 拡散方向と発射位置を求め、弾の基本性能・成長・命中元を設定して管理先へ渡す。
    /// @param originIsMuzzle originを補正済みの銃口としてそのまま使うか。
    void FireInternal(const cg2::Vector3& origin, const cg2::Vector3& baseDir, const AttackParam& param, BulletOwner owner,
                      bool originIsMuzzle);

    BulletManager* bulletManager_ = nullptr;
};
