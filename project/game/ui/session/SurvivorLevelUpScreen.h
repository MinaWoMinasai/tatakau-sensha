#pragma once
#include <array>
#include <string>
#include <vector>
#include "game/ui/session/SurvivorQuadBatch.h"
#include "game/ui/session/SurvivorUpgradePreview.h"

namespace gameplay {
/// @brief 無限の場の強化の画面（計画書 p9 の L1、モックどおり）：右に縦3段の札、左に選んでいる強化の動くプレビュー、左下に今の持ち物。
/// Tab の持ち物の画面も、同じ部品で出す（札が小さくなり2列。選んでいる物の効果を左下に出す）。
/// @note 板と文字は SurvivorQuadBatch にまとめる（Sprite を多く持つと、出していない間も重くなるため）。
class SurvivorLevelUpScreen {
public:
    static constexpr size_t kMaxCards = 16;
    struct Card {
        std::string icon;
        myEngine::Vector4 color{1.0f, 1.0f, 1.0f, 1.0f};
        std::string name;
        std::string level;
        std::string badge;
        std::string effect;
        /// @brief プレビューの段と、覚醒するか。
        int tier = 1;
        bool awaken = false;
    };
    struct Slot {
        /// @brief 空ならまだ何も入っていない枠。
        std::string icon;
        myEngine::Vector4 color{1.0f, 1.0f, 1.0f, 1.0f};
        int tier = 0;
        int maxTier = 0;
    };
    struct Group {
        std::string name;
        std::vector<Slot> slots;
    };
    struct View {
        bool inventory = false;
        int level = 1;
        /// @brief 続けて選べる回数（1なら出さない）。
        int pending = 1;
        std::vector<Card> cards;
        std::vector<Group> groups;
    };
    struct Input {
        myEngine::Vector2 mouse{};
        bool mouseMoved = false;
        bool click = false;
        int move = 0;
        bool confirm = false;
        /// @brief 数字キーで選んだ札（負なら無し）。
        int number = -1;
    };

    /// @brief 描く領域を作り、絵を読み込む（2回目からは何もしない）。
    void Initialize();
    void Open(const View& view);
    /// @brief 画面を進め、決めた札の番号を返す（無ければ -1。持ち物の画面では決められない）。
    int Update(float dt, const Input& input);
    void Draw();
    size_t GetSelected() const
    {
        return selected_;
    }
    void Select(size_t index);
    /// @brief 全部のアイコンの名前（自動確認で、プレビューを1つずつ見るため）。
    static const std::vector<std::string>& IconNames();
    /// @brief 自動確認用：プレビューだけを key に替えて、seconds 秒ぶん進める。
    void ShowPreview(const std::string& key, int tier, bool awaken, float seconds);
    /// @brief 開いてから、まだ入力を受けない間か（連打で間違って選ばないように）。
    bool IsLocked() const
    {
        return age_ < kLockSeconds;
    }
    float GetAge() const
    {
        return age_;
    }

private:
    static constexpr float kLockSeconds = 0.3f;
    struct Rect {
        float x = 0.0f, y = 0.0f, width = 0.0f, height = 0.0f;
        bool Contains(const myEngine::Vector2& p) const
        {
            return p.x >= x && p.x <= x + width && p.y >= y && p.y <= y + height;
        }
    };
    Rect CardRect(size_t index) const;
    void Panel(const Rect& rect, const myEngine::Vector4& color);
    void Glow(const std::string& icon, float x, float y, float size, const myEngine::Vector4& color);
    /// @brief 文字を積み、幅を返す。
    float Text(const std::string& text, const myEngine::TextStyle& style, float x, float y, float anchorX, const myEngine::Vector4& color);
    void Layout(float dt);

    View view_{};
    size_t selected_ = 0;
    float age_ = 0.0f;
    float time_ = 0.0f;
    std::array<float, kMaxCards> lift_{};
    SurvivorUpgradePreview preview_;
    SurvivorQuadBatch batch_;
};
} // namespace gameplay
