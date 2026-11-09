#pragma once
#include <array>
#include <memory>
#include <string>
#include <vector>
#include "Sprite.h"
#include "TextLabel.h"
#include "game/run/SurvivorParts.h"

namespace gameplay {
/// @brief 育つ戦車の試作の、積んでいる部品と積載の表示（左下）と、積載を超えたときに外す部品を選ぶ画面。
/// @note 表示だけを持つ。外す処理と抽選は SurvivorRunController が行う。
class SurvivorLoadoutHud {
public:
    /// @brief 板と文字を作る。
    void Initialize();
    /// @brief 左下の積載の表示を作り直す。
    void SetLoadout(const std::array<survivor::PartDefinition, survivor::kPartCount>& parts, const survivor::PartLevels& levels,
                    int capacity);
    /// @brief 外す部品を選ぶ画面の中身を作り直す。持っている部品を並べる。
    /// @param incoming 積もうとしている部品。見出しに「あと何空けるか」を書く。
    /// @param refundPerLevel 外したときに落とす経験値（Lv あたり）。行に書く。
    void SetRemoval(const std::array<survivor::PartDefinition, survivor::kPartCount>& parts, const survivor::PartLevels& levels,
                    int capacity, survivor::PartId incoming, int refundPerLevel);
    /// @brief 外す画面で、row 番目の行の部品を返す。範囲外なら Count。
    survivor::PartId RowPart(size_t row) const
    {
        return row < rowParts_.size() ? rowParts_[row] : survivor::PartId::Count;
    }
    /// @brief 外す画面の行の数。
    size_t RowCount() const
    {
        return rowParts_.size();
    }
    /// @brief マウスが乗っている行の番号を返す。無ければ -1。
    int HitTest(const myEngine::Vector2& mouse) const;
    /// @brief 外す画面の行の明るさを、マウスの位置に合わせて更新する。
    void Update(const myEngine::Vector2& mouse);
    /// @brief 左下の積載の表示を描く。
    void DrawLoadout();
    /// @brief 外す部品を選ぶ画面を描く。
    void DrawRemoval();

private:
    static constexpr size_t kMaxRows = 14;
    static constexpr float kRowWidth = 540.0f, kRowHeight = 46.0f, kRowGap = 8.0f, kRowsTop = 222.0f;
    /// @brief row 番目の行の左上。7行ごとに右の列へ移る。
    myEngine::Vector2 RowPosition(size_t row) const;

    std::unique_ptr<myEngine::Sprite> loadoutPanel_;
    std::unique_ptr<myEngine::TextLabel> loadoutText_;
    float loadoutHeight_ = 0.0f;

    std::unique_ptr<myEngine::Sprite> dimmer_;
    std::unique_ptr<myEngine::TextLabel> heading_;
    std::unique_ptr<myEngine::TextLabel> footer_;
    std::array<std::unique_ptr<myEngine::Sprite>, kMaxRows> rows_;
    std::array<std::unique_ptr<myEngine::TextLabel>, kMaxRows> rowTexts_;
    std::vector<survivor::PartId> rowParts_;
    int hovered_ = -1;
};
} // namespace gameplay
