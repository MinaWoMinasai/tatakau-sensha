#pragma once
#include <string>
#include <vector>
#include "game/ui/session/SurvivorQuadBatch.h"

namespace myEngine {
class Input;
}

namespace gameplay {

/// @brief 画面の操作（キーボード・マウス・コントローラーをまとめた物）。
struct MenuInput {
    myEngine::Vector2 mouse{};
    bool mouseMoved = false, click = false;
    bool up = false, down = false, left = false, right = false;
    bool confirm = false, back = false;
    /// @brief 最後に触ったのがコントローラーか（操作の絵の切り替え）。
    bool pad = false;
};
/// @brief 入力を読む。lastMouse は呼び出し側で持ち、マウスを動かしたかを見る。
MenuInput ReadMenuInput(myEngine::Input& input, myEngine::Vector2& lastMouse);

/// @brief 管の線と文字だけの画面を描く部品（計画書 p11 の R1。モックの形）。板と文字は SurvivorQuadBatch にまとめる。
/// @note 座標は論理座標 1280×720、色は画面の色（sRGB）で渡す。更新の段階で Begin → 部品 → End、描画の段階で Draw。
class NeonMenuKit {
public:
    enum class Shade { Title, Pause, Full };

    void Initialize(size_t capacity = 700);
    void Begin(float alpha = 1.0f);
    void End();
    void Draw();

    /// @brief 画面を暗くする板。
    void Darken(Shade shade);
    void Rect(float left, float top, float width, float height, const myEngine::Vector4& color);
    /// @brief 管の線。width は芯の太さ、glow は光の強さ（0 で光らない）、reach は光の届く幅。
    void Tube(const myEngine::Vector2& a, const myEngine::Vector2& b, float width, const myEngine::Vector4& core, const myEngine::Vector4& glowColor,
              float glow, float reach);
    /// @brief 管の輪（from〜to は 0〜1、上から右回り）。
    void Ring(const myEngine::Vector2& center, float radius, float width, const myEngine::Vector4& core, const myEngine::Vector4& glowColor, float glow,
              float reach, float from = 0.0f, float to = 1.0f);
    /// @brief 文字。戻り値は幅。bold でない文字は細め。
    float Text(const std::string& text, float size, float x, float y, float anchorX, const myEngine::Vector4& color, bool number = false);
    float MeasureText(const std::string& text, float size, bool number = false);
    /// @brief 光る数字・文字（文字の形の光を、まわりへずらして重ねる）。
    float GlowText(const std::string& text, float size, float x, float y, float anchorX, const myEngine::Vector4& color,
                   const myEngine::Vector4& glowColor, float glow);
    /// @brief キーやボタンの絵。round なら丸（コントローラー）。戻り値は幅。
    float Key(const std::string& label, float x, float y, bool round, const myEngine::Vector4& color, float scale = 1.0f);
    /// @brief 強化のアイコン（resources/endless_ui）。
    void Icon(const std::string& name, float x, float y, float size, const myEngine::Vector4& color);
    /// @brief 持ち物の1枠（アイコンと段の点）。name が空なら、空の枠。
    void Cell(const std::string& name, float x, float y, const myEngine::Vector4& color, int tier, int maxTier);
    /// @brief 10段の目盛り。戻り値は幅。
    float Meter(float x, float y, int value, bool lit);

    struct Hint {
        /// @brief キーボードの絵（複数なら並べる）と、コントローラーの絵。
        std::vector<std::string> keys;
        std::string button;
        std::string text;
    };
    /// @brief 左下の操作の案内。
    void Hints(const std::vector<Hint>& hints, bool pad);

    SurvivorQuadBatch& Batch()
    {
        return batch_;
    }
    float GetAlpha() const
    {
        return alpha_;
    }

private:
    myEngine::Vector4 Tint(const myEngine::Vector4& color) const;
    myEngine::Vector4 Light(const myEngine::Vector4& color, float strength) const;
    SurvivorQuadBatch batch_;
    float alpha_ = 1.0f;
};

/// @brief 縦に並ぶ項目（タイトル・ポーズ・結果）。
class NeonMenu {
public:
    struct Item {
        std::string label;
        /// @brief 空でなければ、2回きめて初めて動く（1回目は、この文に替わる）。
        std::string armed;
    };
    /// @brief 並べ直す。top は1つ目の項目の上の端。
    void Open(std::vector<Item> items, float top, float left = 76.8f, int selected = 0);
    /// @brief 入力を受け、決めた項目の番号を返す（無ければ -1）。開いてすぐの入力は受けない。
    int Update(float dt, const MenuInput& input);
    void Queue(NeonMenuKit& kit);
    int GetSelected() const
    {
        return selected_;
    }
    bool IsArmed() const
    {
        return armed_ >= 0;
    }
    /// @brief 2回きめる項目を、元に戻す。
    void Disarm()
    {
        armed_ = -1;
    }
    /// @brief 選ぶ項目が、今の更新で変わったか。
    bool Moved() const
    {
        return moved_;
    }

private:
    std::vector<Item> items_;
    float top_ = 0.0f, left_ = 0.0f, age_ = 0.0f;
    int selected_ = 0, armed_ = -1;
    bool moved_ = false;
};

namespace menucolor {
inline constexpr myEngine::Vector4 kVoid{0.0196f, 0.0196f, 0.0314f, 1.0f};
inline constexpr myEngine::Vector4 kInk{0.933f, 0.949f, 0.941f, 1.0f};
inline constexpr myEngine::Vector4 kDim{0.561f, 0.584f, 0.651f, 1.0f};
inline constexpr myEngine::Vector4 kItem{0.663f, 0.698f, 0.714f, 1.0f};
inline constexpr myEngine::Vector4 kGreen{0.275f, 1.0f, 0.588f, 1.0f};
inline constexpr myEngine::Vector4 kRed{1.0f, 0.235f, 0.431f, 1.0f};
inline constexpr myEngine::Vector4 kCyan{0.235f, 0.863f, 1.0f, 1.0f};
inline constexpr myEngine::Vector4 kGold{1.0f, 0.804f, 0.157f, 1.0f};
inline constexpr myEngine::Vector4 kWhite{1.0f, 1.0f, 1.0f, 1.0f};
/// @brief 管の芯の色（色に白を混ぜる）。
inline myEngine::Vector4 Core(const myEngine::Vector4& c, float white = 0.86f)
{
    return {c.x + (1.0f - c.x) * white, c.y + (1.0f - c.y) * white, c.z + (1.0f - c.z) * white, 1.0f};
}
inline myEngine::Vector4 Alpha(myEngine::Vector4 c, float a)
{
    c.w = a;
    return c;
}
} // namespace menucolor
} // namespace gameplay
