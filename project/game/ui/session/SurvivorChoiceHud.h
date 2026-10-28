#pragma once
#include <array>
#include <memory>
#include <vector>
#include "Sprite.h"
#include "TextLabel.h"
#include "game/run/SurvivorParts.h"

namespace gameplay {
/// @brief 育つ戦車の試作の、毎レベルの3択の画面と、貯まった選択の数の印。
/// @note 表示だけを持つ。開閉・抽選・選んだ部品の反映は SurvivorRunController が行う。
class SurvivorChoiceHud {
public:
    /// @brief 札の板と文字を作る。
    void Initialize();
    /// @brief 3択の中身を差し替える。offers が3枚より少ないときは、残りの札を出さない。
    void SetOffers(const std::vector<survivor::PartOffer>& offers, const std::array<survivor::PartDefinition, survivor::kPartCount>& parts);
    /// @brief 貯まった選択の数の表示を更新する。
    void SetPending(int pending, bool open);
    /// @brief マウスが乗っている札の番号（0〜2）を返す。無ければ -1。開いている間だけ意味がある。
    int HitTest(const myEngine::Vector2& mouse) const;
    /// @brief 開いている間の札の明るさを、マウスの位置に合わせて更新する。
    void Update(const myEngine::Vector2& mouse);
    /// @brief open なら3択を、閉じていて選択が貯まっていれば右下の印を描く。
    void Draw(bool open, int pending);

private:
    static constexpr float kCardTop = 230.0f, kCardWidth = 368.0f, kCardHeight = 230.0f;

    /// @brief i 枚目の札の左端の x を返す。
    static float CardLeft(size_t i)
    {
        return 64.0f + static_cast<float>(i) * 388.0f;
    }

    std::unique_ptr<myEngine::Sprite> dimmer_;
    std::array<std::unique_ptr<myEngine::Sprite>, 3> cards_;
    std::array<std::unique_ptr<myEngine::TextLabel>, 3> titles_;
    std::array<std::unique_ptr<myEngine::TextLabel>, 3> bodies_;
    std::unique_ptr<myEngine::TextLabel> heading_;
    std::unique_ptr<myEngine::TextLabel> footer_;
    std::unique_ptr<myEngine::Sprite> badgePanel_;
    std::unique_ptr<myEngine::TextLabel> badge_;
    size_t offerCount_ = 0;
    int hovered_ = -1;
};
} // namespace gameplay
