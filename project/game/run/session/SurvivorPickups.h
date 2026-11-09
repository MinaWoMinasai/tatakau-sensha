#pragma once
#include "Struct.h"
#include <cstdint>
#include <random>
#include <vector>

namespace myEngine {
class NeonGridRenderer;
}

namespace gameplay {
/// @brief 育つ戦車の試作の経験値の欠片。撃破で落とし、回収範囲に入ると自機へ吸い寄せ、触れると拾う。
/// @note 数が多いので、1つずつの Object3d にはしない。ネオンの格子のパスに小さな塗りの多角形として積む。
class SurvivorPickups {
public:
    /// @brief 欠片の設定。値はワールド単位・秒。
    struct Config {
        float magnetRadius = 5.0f;
        float collectRadius = 1.2f;
        float pullAcceleration = 70.0f;
        float pullMaxSpeed = 28.0f;
        int valuePerShard = 30;
        size_t maxShards = 600;
    };

    void SetConfig(const Config& config)
    {
        config_ = config;
    }

    const Config& GetConfig() const
    {
        return config_;
    }

    /// @brief value の経験値を、valuePerShard ごとの欠片に分けて position の周りに落とす。
    /// @note 上限を超える分は、既にある欠片に足す（見た目の数は増やさない）。
    void Drop(const myEngine::Vector3& position, uint32_t value, std::mt19937& random);
    /// @brief 欠片を動かし、拾った経験値の合計を返す。
    /// @param magnetScale 回収範囲の倍率（部品で広げる）。
    /// @param homing 真なら、落ちて0.6秒たった欠片は距離によらず寄ってくる（回収器の3段）。
    int Update(float dt, const myEngine::Vector3& player, float magnetScale = 1.0f, bool homing = false);
    /// @brief 画面の近くにある欠片を、カメラを向いた小さな菱形として積む。
    void Queue(myEngine::NeonGridRenderer& renderer, const myEngine::Vector3& cameraRight, const myEngine::Vector3& cameraUp,
               const myEngine::Vector3& viewCenter, float halfWidth, float halfHeight) const;

    /// @brief 全部の欠片を消す。
    void Clear()
    {
        shards_.clear();
    }

    size_t GetCount() const
    {
        return shards_.size();
    }

    /// @brief position から maxDistance 以内で最も近い欠片の位置を返す。無ければ false。
    bool FindNearest(const myEngine::Vector3& position, float maxDistance, myEngine::Vector3& out) const;
    /// @brief 落ちている欠片の経験値の合計を返す。
    int GetPendingValue() const;

private:
    struct Shard {
        myEngine::Vector3 position{};
        myEngine::Vector3 velocity{};
        int value = 0;
        float age = 0.0f;
        bool attracted = false;
    };

    Config config_;
    std::vector<Shard> shards_;
};
} // namespace gameplay
