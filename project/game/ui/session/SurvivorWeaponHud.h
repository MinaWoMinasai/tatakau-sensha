#pragma once
#include <array>
#include <memory>
#include <string>
#include "Sprite.h"
#include "TextLabel.h"

namespace gameplay {
/// @brief 育つ戦車の試作の、右下の武器の輪と弾の数（計画書 p3 の W1）。
/// @note 輪は枠を120度ずつ並べ、構えている枠が左（画面の中央側）に来るように回る。弾の数は 0〜9 と「/」の画像を始めに1回だけ作り、
/// 板に貼り替えて使い回す（撃つたびに TextLabel の SetText で画像を作らないため）。文字や板を変えるのは更新の段階だけ。
class SurvivorWeaponHud {
public:
    static constexpr size_t kSlots = 3;

    /// @brief 枠に入っている武器の表示。empty なら「（空き）」。
    struct Slot {
        bool empty = true;
        std::string name;
        std::string ammo;
        myEngine::Vector4 color{1.0f, 1.0f, 1.0f, 1.0f};
    };

    /// @brief その時の状態。reloadRatio・swapRatio は進み（0〜1、負なら行っていない）。
    struct State {
        size_t active = 0;
        int magazine = 0;
        /// @brief 弾倉の大きさ。0 ならエネルギー弾（弾倉なし。予備だけを大きく出す）。
        int magazineMax = 0;
        int reserve = 0;
        float reloadRatio = -1.0f;
        float swapRatio = -1.0f;
        /// @brief 枠ごとの熱の割合（0〜1）と、オーバーヒートしているか。
        std::array<float, kSlots> heat{};
        std::array<bool, kSlots> overheated{};
    };

    /// @brief 輪の板・数字の画像・決まった文字を作る。
    void Initialize();
    /// @brief 枠の武器を渡す（名前の文字を作り直すので、更新の段階で、武器が変わったときだけ呼ぶ）。
    void SetSlots(const std::array<Slot, kSlots>& slots);
    /// @brief 輪を回し、弾の数と状態の表示を並べ直す。更新の段階で毎フレーム呼ぶ。
    void Update(float dt, const State& state);
    void Draw();

private:
    /// @brief 数字の列を、left から並べた板に貼る。使った板の数を返す。
    size_t LayoutNumber(size_t firstSprite, const std::string& text, float left, float centerY, float scale, const myEngine::Vector4& color);

    std::array<Slot, kSlots> slots_{};
    State state_{};
    // 輪の回り具合（度）。構えている枠が左に来る角度へ、なめらかに寄せる。
    float angle_ = 180.0f;
    bool angleReady_ = false;
    std::unique_ptr<myEngine::Sprite> marker_;
    // 枠ごとの熱の棒（名前の下）。
    std::array<std::unique_ptr<myEngine::Sprite>, kSlots> heatBars_;
    // 輪の形を示す小さな点。
    std::array<std::unique_ptr<myEngine::Sprite>, 30> ringDots_;
    std::unique_ptr<myEngine::Sprite> progressTrack_;
    std::unique_ptr<myEngine::Sprite> progressFill_;
    std::array<std::unique_ptr<myEngine::TextLabel>, kSlots> nameLabels_;
    std::array<std::unique_ptr<myEngine::TextLabel>, kSlots> ammoLabels_;
    std::unique_ptr<myEngine::TextLabel> reloadingLabel_;
    std::unique_ptr<myEngine::TextLabel> swappingLabel_;
    std::unique_ptr<myEngine::TextLabel> emptyLabel_;
    std::unique_ptr<myEngine::TextLabel> noReserveLabel_;
    // 0〜9 と「/」（10番目）の画像。
    std::array<std::unique_ptr<myEngine::TextLabel>, 11> glyphLabels_;
    std::array<std::string, 11> glyphPaths_{};
    std::array<myEngine::Vector2, 11> glyphSizes_{};
    // 弾の数の板（弾倉3桁・「/」・予備4桁）。
    std::array<std::unique_ptr<myEngine::Sprite>, 8> numberSprites_;
    size_t usedNumberSprites_ = 0;
    bool showProgress_ = false;
};
} // namespace gameplay
