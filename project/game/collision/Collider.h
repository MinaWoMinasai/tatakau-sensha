#pragma once
#include "Struct.h"
#include <atomic>
#include <cmath>

// 判定図形
enum class ColliderShape {
    Sphere,
    Capsule
};

/// @brief 衝突形状・属性・識別子を保持し、衝突した相手をゲームオブジェクトへ通知する。
class Collider {

public:
    /// @brief この型の終了処理を行う。所有している資源の寿命を終了させる。
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
        if (std::isfinite(radius) && radius >= 0.0f)
            radius_ = radius;
    }

    /// @brief ワールド座標の取得
    virtual cg2::Vector3 GetWorldPosition() const = 0;

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

    /// @brief 状態Gardを設定する。
    void SetIsGard(bool flag)
    {
        isAttack_ = flag;
    }
    /// @brief 状態攻撃を返す。
    bool GetIsAttack()
    {
        return isAttack_;
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
    void SetCapsule(const cg2::Segment& seg, float r)
    {
        if (!std::isfinite(r) || r < 0.0f)
            return;
        segment_ = seg;
        capsuleRadius_ = r;
    }
    /// @brief 区切りを返す。
    const cg2::Segment& GetSegment() const
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

private:
    // Actor allocations may reuse an address while a piercing projectile lives.
    // Copies get a new identity; assigning state does not change an actor's ID.
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

    bool isAttack_ = false;

    // 衝突図形
    ColliderShape shape_ = ColliderShape::Sphere;

    cg2::Segment segment_;
    float capsuleRadius_ = 0.0f;
    float hitPower_ = 1.0f;

    // 与えるダメージ
    uint32_t damage_ = 0;
};
