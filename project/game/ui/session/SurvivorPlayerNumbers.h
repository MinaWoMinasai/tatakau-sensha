#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "Sprite.h"
#include "TextLabel.h"

namespace gameplay {
/// @brief 自機の上に出す、被弾（赤い「-12」）・回復（緑の「+20」）・立ち直り（金色の「+50」）の数字と、被弾したときの画面の縁の赤（計画書 p9 の H3）。
/// @note 敵へのダメージの数字とは別に持つ（敵の数字が多いときに、自機の数字が押し出されないように）。数字の画像は始めに1回だけ作る。
class SurvivorPlayerNumbers {
public:
    enum class Kind { Hurt, Heal, Revive };
    struct Config {
        /// @brief 数字の大きさの倍率と、縁の赤の強さ（0 で出さない）。
        float scale = 1.0f;
        float edge = 0.6f;
    };

    void Initialize();
    void SetConfig(const Config& config)
    {
        config_ = config;
    }
    /// @brief 数字を出す。ratio は最大HPに対する割合（大きいほど数字も縁の赤も大きい）。続けて受けた被弾は、ごく短い間だけ1つにまとめる。
    void Spawn(Kind kind, int amount, float ratio);
    /// @brief 数字を進め、自機の画面の位置 anchor に合わせて板を置く。更新の段階で呼ぶ。
    void Update(float dt, const myEngine::Vector2& anchor);
    void Draw();
    void Clear();
    /// @brief これまでに出した数字の数（自動確認用）。種類ごと。
    uint32_t GetSpawnedCount(Kind kind) const
    {
        return spawned_[static_cast<size_t>(kind)];
    }
    /// @brief 一番新しい数字の、出てからの秒（無ければ負）。自動確認の写真用。
    float GetNewestAge(Kind kind) const;

private:
    static constexpr size_t kKinds = 3;
    static constexpr size_t kGlyphs = 11; // 0〜9 と符号
    static constexpr size_t kMaxEntries = 8;
    static constexpr size_t kSpriteCount = 40;
    struct Entry {
        Kind kind = Kind::Hurt;
        int amount = 0;
        float ratio = 0.0f;
        float age = 0.0f;
        float offsetX = 0.0f;
    };

    Config config_{};
    std::array<std::array<std::unique_ptr<myEngine::TextLabel>, kGlyphs>, kKinds> labels_;
    std::array<std::array<std::string, kGlyphs>, kKinds> paths_{};
    std::array<std::array<myEngine::Vector2, kGlyphs>, kKinds> sizes_{};
    std::vector<Entry> entries_;
    std::vector<std::unique_ptr<myEngine::Sprite>> sprites_;
    std::array<std::unique_ptr<myEngine::Sprite>, 4> edges_; // 上・下・左・右
    size_t usedSprites_ = 0;
    float edgeTimer_ = 0.0f;
    float edgePower_ = 0.0f;
    std::array<uint32_t, kKinds> spawned_{};
    uint32_t sequence_ = 0;
};
} // namespace gameplay
