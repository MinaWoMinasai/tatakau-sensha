#pragma once
#include <array>
#include <memory>
#include <string>
#include <utility>
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
    /// @param premium 真なら、宝箱から得た質の高い選択だと見出しに出す。
    /// @param levels 今の部品の Lv。新しく積む札に、重さと積載（今→積んだ後／上限）を書く。
    /// @param capacity 今の積載量。超える札には「何かを外して積む」と書く。
    void SetOffers(const std::vector<survivor::PartOffer>& offers, const std::array<survivor::PartDefinition, survivor::kPartCount>& parts,
                   bool premium, const survivor::PartLevels& levels, int capacity);
    /// @brief 部品以外の選択（形態など）の札を出す。見出しと下の案内も差し替え、次の SetOffers まで SetPending は案内を変えない。
    void SetCards(const std::string& heading, const std::string& footer, const std::vector<std::pair<std::string, std::string>>& cards);
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
    /// @brief 札の高さ。部品以外の選択は説明が長いので高くする。
    float CardHeight() const
    {
        return customCards_ ? 270.0f : kCardHeight;
    }

    /// @brief i 枚目の札の左端の x を返す。3枚より少ないときは画面の中央に寄せる。
    float CardLeft(size_t i) const
    {
        const float first = offerCount_ >= 3 ? 64.0f : (1280.0f - static_cast<float>(offerCount_) * 388.0f + 20.0f) * 0.5f;
        return first + static_cast<float>(i) * 388.0f;
    }
    /// @brief 札の板と文字を、今の枚数に合わせた位置へ置き直す。
    void LayoutCards();

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
    bool customCards_ = false;
};
} // namespace gameplay
