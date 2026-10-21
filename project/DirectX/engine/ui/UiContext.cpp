#include "UiContext.h"
#include "DynamicTexture.h"
#include "WinApp.h"

#include <algorithm>
#include <cmath>

namespace myEngine {

namespace {

// 自動で振る領域の番号は、ボタンの番号（文字列のハッシュ）と重ならないよう最上位ビットを立てる。
constexpr uint64_t kAutoIdBit = 1ull << 63;

} // namespace

bool UiContext::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t atlasPageSize)
{
    screenSize_ = {float(WinApp::kClientWidth), float(WinApp::kClientHeight)};
    batch_.SetScreenSize(screenSize_);
    return atlas_.Initialize(dxCommon, srvManager, atlasPageSize) && batch_.Initialize(dxCommon, srvManager);
}

void UiContext::SetScreenSize(const Vector2& size)
{
    screenSize_ = size;
    batch_.SetScreenSize(size);
}

int UiContext::RegisterTextStyle(const GlyphAtlas::Style& style)
{
    return atlas_.RegisterStyle(style);
}

uint64_t UiContext::HashId(std::string_view id)
{
    // FNV-1a。0 と自動の番号の範囲は避ける。
    uint64_t hash = 1469598103934665603ull;
    for (const char c : id) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 1099511628211ull;
    }
    hash &= ~kAutoIdBit;
    return hash == 0 ? 1 : hash;
}

void UiContext::BeginFrame(const UiInput& input)
{
    previousMouseDown_ = input_.mouseDown;
    input_ = input;
    regions_.clear();
    nextAutoId_ = 1;
    tooltip_.clear();
    tooltipStyle_ = -1;
    batch_.Clear();
    // ボタンの上でないところで離したら、押している途中をやめる。
    if (!input_.mouseDown && !previousMouseDown_) {
        activeId_ = 0;
    }
}

void UiContext::AddHitRegion(uint64_t id, const UiRect& rect)
{
    regions_.emplace_back(id, rect);
}

void UiContext::Panel(const UiRect& rect, const Vector4& color)
{
    batch_.AddRect(rect, color);
    AddHitRegion(kAutoIdBit | nextAutoId_++, rect);
}

void UiContext::Text(const Vector2& position, std::string_view text, int style, const Vector4& color, float wrapWidth)
{
    const GlyphAtlas::TextLayout layout = atlas_.Layout(style, text, wrapWidth);
    for (const GlyphAtlas::PlacedGlyph& placed : layout.glyphs) {
        const GlyphAtlas::Glyph& glyph = *placed.glyph;
        DynamicTexture* page = atlas_.GetPageTexture(glyph.page);
        // 新しいページはまだ GPU のテクスチャが無く、描画に使う SRV の番号も決まっていないので、ここで送って作る。
        // 既にあるページに増えた文字は、EndFrame の Upload でまとめて送る（どちらも描画より前の Update の中）。
        if (!page) {
            atlas_.Upload();
            page = atlas_.GetPageTexture(glyph.page);
        }
        if (!page) {
            continue;
        }
        // 文字の画像は画素の境目に合わせて置き、にじみを抑える。
        const UiRect rect{std::round(position.x + placed.position.x), std::round(position.y + placed.position.y), glyph.size.x, glyph.size.y};
        batch_.AddQuad(rect, glyph.uvMin, glyph.uvMax, color, page->GetSrvHandleGPU());
    }
}

void UiContext::TextCentered(const UiRect& rect, std::string_view text, int style, const Vector4& color)
{
    const Vector2 size = atlas_.MeasureText(style, text);
    Text({rect.x + (rect.width - size.x) * 0.5f, rect.y + (rect.height - size.y) * 0.5f}, text, style, color);
}

Vector2 UiContext::MeasureText(std::string_view text, int style, float wrapWidth)
{
    return atlas_.MeasureText(style, text, wrapWidth);
}

bool UiContext::Button(std::string_view id, const UiRect& rect, std::string_view label, int style, bool enabled, const UiButtonStyle& look)
{
    const uint64_t key = HashId(id);
    AddHitRegion(key, rect);
    const bool hovered = enabled && hoveredId_ == key && rect.Contains(input_.mouse);
    const bool pressedNow = input_.mouseDown && !previousMouseDown_;
    const bool releasedNow = !input_.mouseDown && previousMouseDown_;
    bool clicked = false;
    if (hovered && pressedNow) {
        activeId_ = key;
    }
    if (releasedNow && activeId_ == key) {
        clicked = hovered;
        activeId_ = 0;
    }
    if (!enabled && activeId_ == key) {
        activeId_ = 0;
    }

    const Vector4& background = !enabled ? look.disabled : (activeId_ == key && hovered) ? look.pressed : hovered ? look.hovered : look.normal;
    batch_.AddRect(rect, background);
    TextCentered(rect, label, style, enabled ? look.text : look.disabledText);
    return clicked;
}

void UiContext::Tooltip(std::string_view text, int style)
{
    tooltip_ = std::string(text);
    tooltipStyle_ = style;
}

void UiContext::EndFrame()
{
    lastTooltipRect_ = {};
    if (!tooltip_.empty() && tooltipStyle_ >= 0) {
        // マウスの右下に置き、画面からはみ出すなら左・上へずらす。
        constexpr float kPadding = 6.0f;
        constexpr float kOffset = 18.0f;
        const Vector2 size = atlas_.MeasureText(tooltipStyle_, tooltip_);
        UiRect rect{input_.mouse.x + kOffset, input_.mouse.y + kOffset, size.x + kPadding * 2.0f, size.y + kPadding * 2.0f};
        if (rect.x + rect.width > screenSize_.x) {
            rect.x = input_.mouse.x - kOffset - rect.width;
        }
        if (rect.y + rect.height > screenSize_.y) {
            rect.y = input_.mouse.y - kOffset - rect.height;
        }
        rect.x = std::clamp(rect.x, 0.0f, (std::max)(0.0f, screenSize_.x - rect.width));
        rect.y = std::clamp(rect.y, 0.0f, (std::max)(0.0f, screenSize_.y - rect.height));
        batch_.AddRect(rect, tooltipBackground);
        Text({rect.x + kPadding, rect.y + kPadding}, tooltip_, tooltipStyle_, tooltipText);
        lastTooltipRect_ = rect;
    }

    // 次のフレームのために、マウスの下のいちばん手前の領域を決める（後に登録したものほど手前）。
    hoveredId_ = 0;
    for (auto it = regions_.rbegin(); it != regions_.rend(); ++it) {
        if (it->second.Contains(input_.mouse)) {
            hoveredId_ = it->first;
            break;
        }
    }
    // 新しい文字を描いたページを GPU へ送る（描画の前、Update の中で行う）。
    atlas_.Upload();
}

} // namespace myEngine
