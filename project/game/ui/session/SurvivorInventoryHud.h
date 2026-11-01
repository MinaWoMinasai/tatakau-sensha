#pragma once
#include <array>
#include <memory>
#include <string>
#include <vector>
#include "Sprite.h"
#include "TextLabel.h"

namespace gameplay {
/// @brief 育つ戦車の試作の、E で開く持ち物の画面（計画書 p3 の W4）。左に装備（武器3・シールド1・補助1）、右に出撃中の袋。
/// 出撃前の拠点の画面（W5）も同じ並びで使う（右の欄を拠点の武器の一覧にし、選んでいる行に印を付ける）。
/// @note 戦闘中は見るだけ（付け替えは休憩部屋の W6）。開いている間は区画を止める（止めるのは SurvivorRunController）。
/// 文字は開いたときに1回だけ作る（更新の段階で Show を呼ぶ）。描くだけの段階では作り直さない。
class SurvivorInventoryHud {
public:
    static constexpr size_t kWeaponCards = 3;
    /// @brief 袋の欄に並べられる行の数（越える分は呼ぶ側で「ほか N」にまとめる）。
    static constexpr size_t kBagLines = 22;

    /// @brief 色つきの1行。
    struct Line {
        std::string text;
        myEngine::Vector4 color{0.85f, 0.92f, 1.0f, 1.0f};
    };
    /// @brief 装備の欄の1枚。empty なら title に「（空き）」を添えて出す（値の行は出さない）。
    struct Card {
        bool empty = true;
        bool active = false;
        std::string title;
        std::string detail;
        myEngine::Vector4 color{1.0f, 1.0f, 1.0f, 1.0f};
    };
    struct View {
        /// @brief 見出し・操作の案内・右の欄の見出し。
        std::string title = "持ち物";
        std::string help = "E で閉じる　／　開いている間は止まる　／　付け替え・整理・売るのは休憩部屋で";
        std::string bagTitle = "出撃中の袋（倒れると全部失う）";
        /// @brief 右の欄で選んでいる行（-1 なら印を出さない）。
        int cursorLine = -1;
        std::array<Card, kWeaponCards> weapons{};
        Card shield{};
        Card support{};
        std::vector<Line> bag;
    };

    /// @brief 板と文字の入れ物を作る。
    void Initialize();
    /// @brief 中身を入れ替える（文字を作り直すので、更新の段階で、開いたときだけ呼ぶ）。
    void Show(const View& view);
    void Draw();

private:
    /// @brief 色が変わったときだけ書体を替えてから、文字を入れる。
    static void SetLine(myEngine::TextLabel& label, const std::string& text, const myEngine::Vector4& color, float size, bool bold);

    std::unique_ptr<myEngine::Sprite> backdrop_;
    std::unique_ptr<myEngine::Sprite> bagPanel_;
    std::unique_ptr<myEngine::Sprite> cursor_;
    int cursorLine_ = -1;
    // 装備の欄の板（武器3・シールド・補助）と、構えている武器の左の帯。
    std::array<std::unique_ptr<myEngine::Sprite>, kWeaponCards + 2> cardPanels_;
    std::array<std::unique_ptr<myEngine::Sprite>, kWeaponCards + 2> cardStripes_;
    std::array<std::unique_ptr<myEngine::TextLabel>, kWeaponCards + 2> cardTitles_;
    std::array<std::unique_ptr<myEngine::TextLabel>, kWeaponCards + 2> cardDetails_;
    std::array<bool, kWeaponCards + 2> cardEmpty_{};
    std::unique_ptr<myEngine::TextLabel> title_;
    std::unique_ptr<myEngine::TextLabel> help_;
    std::unique_ptr<myEngine::TextLabel> equipHeader_;
    std::unique_ptr<myEngine::TextLabel> bagHeader_;
    std::array<std::unique_ptr<myEngine::TextLabel>, kBagLines> bagLines_;
    size_t bagLineCount_ = 0;
};
} // namespace gameplay
