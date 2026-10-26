#pragma once
#include "Struct.h"
#include <atomic>
#include <cmath>
#include <type_traits>

// 判定図形
enum class ColliderShape {
    Sphere,
    Capsule
};

/// @brief コライダーを持つゲームオブジェクトの種類。キャストせずに相手の型を判定するために使う。
/// @note 値ごとに対応する型は1つだけで、その型はさらに継承しない。型の側は kColliderKind で自分の値を示す。
enum class ColliderKind {
    Player,      // 自機
    PlayerDrone, // 自機のドローン
    Bullet,      // 弾（自機・敵の両方）
    ExpEnemy,    // 通常の敵
    Enemy,       // ボス
};

/// @brief 衝突形状・属性・識別子を保持し、衝突した相手をゲームオブジェクトへ通知する。
class Collider {
public:
    virtual ~Collider() = default;

    /// @brief 衝突識別子を返す。
    uint64_t GetCollisionId() const
    {
        return identity_.value;
    }

    // 半径を取得
    virtual float GetRadius() const
    {
        return radius_;
    }

    // 半径を設定
    void SetRadius(float radius)
    {
        if (std::isfinite(radius) && radius >= 0.0f) {
            radius_ = radius;
        }
    }

    /// @brief ワールド座標の取得
    virtual myEngine::Vector3 GetWorldPosition() const = 0;

    /// @brief 衝突判定
    virtual void OnCollision(Collider* other) = 0;

    // 衝突属性(自分)を取得
    uint32_t GetCollisionAttribute() const
    {
        return collisionAttribute_;
    }

    // 衝突属性(自分)を設定
    void SetCollisionAttribute(uint32_t attribute)
    {
        collisionAttribute_ = attribute;
    }

    // 衝突マスク(相手)を取得
    uint32_t GetCollisionMask() const
    {
        return collisionMask_;
    }

    // 衝突マスク(相手)を設定
    void SetCollisionMask(uint32_t mask)
    {
        collisionMask_ = mask;
    }

    // 図形セット
    ColliderShape GetShape() const
    {
        return shape_;
    }

    /// @brief 形状を設定する。
    void SetShape(ColliderShape shape)
    {
        shape_ = shape;
    }

    /// @brief カプセルを設定する。
    void SetCapsule(const myEngine::Segment& seg, float r)
    {
        if (!std::isfinite(r) || r < 0.0f) {
            return;
        }
        segment_ = seg;
        capsuleRadius_ = r;
    }

    /// @brief カプセルの軸の線分を返す。
    const myEngine::Segment& GetSegment() const
    {
        return segment_;
    }

    /// @brief カプセル半径を返す。
    float GetCapsuleRadius() const
    {
        return capsuleRadius_;
    }

    /// @brief 命中強度を返す。
    float GetHitPower() const
    {
        return hitPower_;
    }

    /// @brief 命中強度を設定する。
    void SetHitPower(float power)
    {
        hitPower_ = power;
    }

    /// @brief ダメージを返す。
    uint32_t GetDamage() const
    {
        return damage_;
    };

    /// @brief ダメージを設定する。
    void SetDamage(uint32_t damage)
    {
        damage_ = damage;
    }

    /// @brief このコライダーを持つゲームオブジェクトの種類を返す。
    ColliderKind GetColliderKind() const
    {
        return kind_;
    }

protected:
    /// @brief 派生クラスが自分の種類を指定して作る。
    explicit Collider(ColliderKind kind) : kind_(kind) {}

private:
    ColliderKind kind_;

    // 貫通弾が生きている間に、アクターのメモリが同じアドレスで再利用されることがある。
    // コピーには新しい識別子を割り当てる。状態を代入しても、アクターの ID は変わらない。
    /// @brief コライダーを世代付きで識別する。破棄後に同じ領域を使っても元の相手と混同しない。
    struct Identity {
        static inline std::atomic<uint64_t> next{1};
        uint64_t value = next.fetch_add(1, std::memory_order_relaxed);
        /// @brief 新しい識別子を発行する。
        Identity() = default;

        /// @brief コピー元とは別の識別子を発行する。同じ対象として命中履歴へ登録しない。
        Identity(const Identity&) : Identity() {}

        /// @brief 自身の識別子を保つ。状態の代入でアクターの同一性を変えない。
        Identity& operator=(const Identity&)
        {
            return *this;
        }
    };

    Identity identity_{};
    // 衝突半径
    float radius_ = 0.8f;

    // 衝突属性
    uint32_t collisionAttribute_ = 0xffffffff;
    // 衝突マスク
    uint32_t collisionMask_ = 0xffffffff;

    // 衝突図形
    ColliderShape shape_ = ColliderShape::Sphere;

    myEngine::Segment segment_;
    float capsuleRadius_ = 0.0f;
    float hitPower_ = 1.0f;

    // 与えるダメージ
    uint32_t damage_ = 0;
};

/// @brief 種類が一致すれば派生クラスのポインターへ変換し、違えばnullptrを返す。dynamic_castの代わりに使う。
/// @tparam T 変換先の型。const付きでもよい。kColliderKind を持つこと。
template <class T, class C> T* ColliderCast(C* collider)
{
    return collider && collider->GetColliderKind() == std::remove_cv_t<T>::kColliderKind ? static_cast<T*>(collider) : nullptr;
}
