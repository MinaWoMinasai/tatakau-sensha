#pragma once
#include <algorithm>

/// @brief 育つ戦車の試作の、撃つたびに上がる熱と、熱のあとに来るオーバーヒート。
/// @note 既定では無効。無効の間は CanFire が常に true で、熱も上がらない（既存のモードの射撃は変わらない）。
class PlayerHeat {
public:
    struct Config {
        bool enabled = false;
        /// @brief 熱の上限。これを超えるとオーバーヒートする。
        float maxHeat = 100.0f;
        /// @brief 1回の発射で上がる熱。
        float heatPerShot = 3.0f;
        /// @brief 最後に撃ってからこの秒数が経つと、速く冷え始める。
        float coolDelay = 0.5f;
        /// @brief 撃っていない間に毎秒冷える量。
        float coolPerSecond = 40.0f;
        /// @brief 撃ち続けている間にも毎秒冷える量。基本の連射と釣り合わせるためのもの。
        float coolPerSecondWhileFiring = 0.0f;
        /// @brief オーバーヒートで撃てなくなる秒数。明けると熱は0に戻る。
        float overheatSeconds = 2.0f;
    };

    void Configure(const Config& config)
    {
        config_ = config;
        Reset();
    }

    /// @brief 設定だけを替える。今の熱とオーバーヒートは保ち、熱は新しい上限までに抑える。
    void Reconfigure(const Config& config)
    {
        config_ = config;
        heat_ = (std::min)(heat_, config_.maxHeat);
    }

    void Reset()
    {
        heat_ = 0.0f;
        sinceShot_ = config_.coolDelay;
        overheatTimer_ = 0.0f;
    }

    void Update(float deltaTime)
    {
        if (!config_.enabled || deltaTime <= 0.0f) {
            return;
        }
        if (overheatTimer_ > 0.0f) {
            overheatTimer_ -= deltaTime;
            if (overheatTimer_ <= 0.0f) {
                overheatTimer_ = 0.0f;
                heat_ = 0.0f;
            }
            return;
        }
        sinceShot_ += deltaTime;
        const float rate = sinceShot_ >= config_.coolDelay ? config_.coolPerSecond : config_.coolPerSecondWhileFiring;
        heat_ = (std::max)(0.0f, heat_ - rate * deltaTime);
    }

    /// @brief 今撃てるか。オーバーヒートの間だけ false。
    bool CanFire() const
    {
        return !config_.enabled || overheatTimer_ <= 0.0f;
    }

    /// @brief 1回の発射の熱を足す。上限を超えたらオーバーヒートにする。
    void AddShot(float scale = 1.0f)
    {
        AddHeat(config_.heatPerShot * scale);
    }

    /// @brief 熱を amount だけ足す（ため撃ちなど、1発の熱と違う量を使う技向け）。上限を超えたらオーバーヒートにする。
    void AddHeat(float amount)
    {
        if (!config_.enabled) {
            return;
        }
        heat_ += amount;
        sinceShot_ = 0.0f;
        if (heat_ > config_.maxHeat) {
            heat_ = config_.maxHeat;
            overheatTimer_ = config_.overheatSeconds;
            ++overheatCount_;
        }
    }

    bool IsEnabled() const
    {
        return config_.enabled;
    }
    bool IsOverheated() const
    {
        return overheatTimer_ > 0.0f;
    }
    float GetHeat() const
    {
        return heat_;
    }
    float GetMaxHeat() const
    {
        return config_.maxHeat;
    }
    /// @brief 熱の割合（0〜1）。オーバーヒートの間は、明けるまでの残りの割合。
    float GetRatio() const
    {
        if (overheatTimer_ > 0.0f) {
            return config_.overheatSeconds > 0.0f ? overheatTimer_ / config_.overheatSeconds : 0.0f;
        }
        return config_.maxHeat > 0.0f ? heat_ / config_.maxHeat : 0.0f;
    }
    int GetOverheatCount() const
    {
        return overheatCount_;
    }

private:
    Config config_{};
    float heat_ = 0.0f;
    float sinceShot_ = 0.0f;
    float overheatTimer_ = 0.0f;
    int overheatCount_ = 0;
};
