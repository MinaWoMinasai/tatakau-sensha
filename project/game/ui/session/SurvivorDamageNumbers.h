#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "Sprite.h"
#include "TextLabel.h"

namespace gameplay {
/// @brief 育つ戦車の試作の、敵に当てたときに出るダメージの数字（ネオン風）。
/// @note 数字の画像は 0〜9 を始めに1回だけ作り、遊んでいる間は使い回す（TextLabel の SetText は文字列ごとに画像を作るので使わない）。
/// 同じ敵へ短い間に続けて当てた分は、1つの数字にまとめて足す（連鎖や破裂で画面が数字で埋まらないように）。
class SurvivorDamageNumbers {
public:
    struct Config {
        bool enabled = true;
        /// @brief 同時に出す数字の上限。超えたら一番古いものを消して出す。
        int maxActive = 40;
        /// @brief 1つの数字が出ている秒数。
        float lifeSeconds = 0.7f;
        /// @brief 文字の大きさの倍率。
        float scale = 1.0f;
        /// @brief 主砲1発の威力のこの倍以上なら、大きな数字（金色）にする。
        float bigRatio = 2.5f;
        /// @brief 同じ敵への命中を1つにまとめる秒数（0 でまとめない）。
        float mergeSeconds = 0.15f;
        /// @brief 会心の数字の大きさ（ふつうに対する倍。計画書 p9 の H2）。
        float critScale = 1.5f;
        /// @brief ふつうの数字の濃さ、大きな数字の大きさ（倍）、自機のまわりで薄くする半径（0 で無し）と、そこで残す濃さ（計画書 p10 の18章）。
        float alpha = 1.0f;
        float bigScale = 1.35f;
        float nearRadius = 0.0f;
        float nearAlpha = 0.0f;
    };

    /// @brief 0〜9 の数字の画像と、数字を並べる板を作る。
    void Initialize();
    void SetConfig(const Config& config)
    {
        config_ = config;
    }
    /// @brief 数字を出す（衝突処理の途中から呼ばれてよい。ここでは控えるだけ）。
    /// @param id 当てた相手の衝突ID。同じ相手への続けての命中をまとめるのに使う。
    /// @param bigThreshold この量以上なら大きな数字にする。
    /// @param critical 会心（金色で大きく、後ろに「!」を付ける）。
    void Spawn(uint64_t id, const myEngine::Vector3& world, uint32_t amount, float bigThreshold, bool critical = false);
    /// @brief 数字を進め、画面の位置と見た目を板へ反映する。更新の段階で呼ぶ（描画中には板を書き換えない）。
    /// @param player 自機の位置。近くの数字を薄くするのに使う。
    void Update(float dt, const std::function<myEngine::Vector2(const myEngine::Vector3&)>& worldToScreen, const myEngine::Vector3& player);
    void Draw();
    void Clear();
    /// @brief これまでに出した数字の数（自動確認用）。
    uint32_t GetSpawnedCount() const
    {
        return spawned_;
    }

private:
    struct Entry {
        uint64_t id = 0;
        myEngine::Vector3 world{};
        uint32_t amount = 0;
        float age = 0.0f;
        bool big = false;
        bool critical = false;
    };
    static constexpr size_t kSpriteCount = 240;

    Config config_{};
    // 数字の画像は3組：[0] ふつう（水色の縁）、[1] 大きな数字（橙の縁）、[2] 会心（金色の太い縁。計画書 p9 の H2）。
    // 中は白で、ネオン管のように見せる。会心には「!」を付ける。
    std::array<std::array<std::unique_ptr<myEngine::TextLabel>, 10>, 3> digitLabels_;
    std::array<std::array<std::string, 10>, 3> digitPaths_{};
    std::array<std::array<myEngine::Vector2, 10>, 3> digitSizes_{};
    std::unique_ptr<myEngine::TextLabel> bangLabel_;
    std::string bangPath_;
    myEngine::Vector2 bangSize_{};
    uint32_t criticals_ = 0;

public:
    /// @brief これまでに出した会心の数字の数（自動確認用）。
    uint32_t GetCriticalCount() const
    {
        return criticals_;
    }

private:
    std::vector<Entry> entries_;
    std::vector<std::unique_ptr<myEngine::Sprite>> sprites_;
    size_t usedSprites_ = 0;
    uint32_t spawned_ = 0;
};
} // namespace gameplay
