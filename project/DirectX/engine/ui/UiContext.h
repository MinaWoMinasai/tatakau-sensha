#pragma once
#include "GlyphAtlas.h"
#include "UiBatch.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace myEngine {

/// @brief UI へ渡す、1フレーム分のマウスの状態（論理座標）。
struct UiInput {
    Vector2 mouse = {-1.0f, -1.0f};
    // 左ボタンを押しているか。
    bool mouseDown = false;
};

/// @brief ボタンの見た目。
struct UiButtonStyle {
    Vector4 normal = UiColor(0x3A4656, 0.95f);
    Vector4 hovered = UiColor(0x52637A, 0.95f);
    Vector4 pressed = UiColor(0x2A3340, 0.95f);
    Vector4 disabled = UiColor(0x2C3036, 0.7f);
    Vector4 text = UiColor(0xF2F2F2);
    Vector4 disabledText = UiColor(0x8A8F96);
};

/// @brief 即時モードのゲーム内 UI（ImGui と同じく、毎フレーム「描きたいもの」を関数で呼ぶ）。
/// Release でも使える（ImGui は使わない）。文字は GlyphAtlas、描画は UiBatch で行う。
///
/// 使い方（Update の中）:
///   ui.BeginFrame(input);
///   ui.Panel(rect, color);
///   if (ui.Button("pause", rect, "停止", style)) { ... }
///   ui.EndFrame();                 // 新しい文字を GPU へ送る
///   if (!ui.WantsMouse()) { ゲーム画面の操作（カメラなど） }
/// DrawSprite の中で ui.Draw()。
///
/// 当たり判定: 後から呼んだもの（手前）が優先される。どれの上にマウスがあるかは前のフレームの結果を使う（1フレーム遅れ）。
/// ボタンは「押して、同じボタンの上で離す」とクリックになる。
class UiContext {
public:
    /// @brief 文字のアトラスと描画の仕組みを用意する。
    bool Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t atlasPageSize = 1024);

    /// @brief 文字の見た目を登録する（GlyphAtlas::RegisterStyle）。失敗したら -1。
    int RegisterTextStyle(const GlyphAtlas::Style& style);

    void BeginFrame(const UiInput& input);
    void EndFrame();

    /// @brief 塗りつぶしの四角形。マウスを受け止める（後ろの部品や、UI の後ろのゲーム画面には届かない）。
    void Panel(const UiRect& rect, const Vector4& color);
    /// @brief 文字を描く（マウスは受け止めない）。wrapWidth が 0 より大きければ折り返す。
    void Text(const Vector2& position, std::string_view text, int style, const Vector4& color, float wrapWidth = 0.0f);
    /// @brief rect の中の中央に文字を描く。
    void TextCentered(const UiRect& rect, std::string_view text, int style, const Vector4& color);
    /// @brief 文字の大きさ。
    Vector2 MeasureText(std::string_view text, int style, float wrapWidth = 0.0f);
    /// @brief ボタン。クリックされたフレームに true を返す。id はボタンごとに違う文字列にする。
    bool Button(std::string_view id, const UiRect& rect, std::string_view label, int style, bool enabled = true,
                const UiButtonStyle& look = {});
    /// @brief マウスの近くに説明を出す。このフレームの最後（いちばん手前）に描く。画面からはみ出さないようにずらす。
    void Tooltip(std::string_view text, int style);

    /// @brief マウスが UI の上にあるか、UI のボタンを押している途中か。true の間は、ゲーム画面の操作にマウスを使わない。
    bool WantsMouse() const
    {
        return hoveredId_ != 0 || activeId_ != 0;
    }

    /// @brief 積んだ UI を描く。DrawSprite の中で呼ぶ。
    void Draw()
    {
        batch_.Draw();
    }

    GlyphAtlas& GetAtlas()
    {
        return atlas_;
    }

    UiBatch& GetBatch()
    {
        return batch_;
    }

    /// @brief 前の EndFrame で描いた説明の四角形（テスト・確認用）。説明が無ければ幅 0。
    const UiRect& GetLastTooltipRect() const
    {
        return lastTooltipRect_;
    }

    /// @brief 画面の大きさ（論理座標）。
    void SetScreenSize(const Vector2& size);

    // 説明の見た目。
    Vector4 tooltipBackground = UiColor(0x14181E, 0.92f);
    Vector4 tooltipText = UiColor(0xF2F2F2);

private:
    /// @brief マウスを受け止める領域を登録する（後のものほど手前）。
    void AddHitRegion(uint64_t id, const UiRect& rect);
    static uint64_t HashId(std::string_view id);

    GlyphAtlas atlas_;
    UiBatch batch_;
    Vector2 screenSize_;
    UiInput input_;
    bool previousMouseDown_ = false;
    // 前のフレームで、マウスの下のいちばん手前にあった領域（0 なら無し）。
    uint64_t hoveredId_ = 0;
    // 押している途中のボタン（0 なら無し）。
    uint64_t activeId_ = 0;
    // このフレームの領域（呼んだ順）。
    std::vector<std::pair<uint64_t, UiRect>> regions_;
    uint64_t nextAutoId_ = 1;
    // このフレームの説明（最後に描く）。
    std::string tooltip_;
    int tooltipStyle_ = -1;
    UiRect lastTooltipRect_;
};

} // namespace myEngine
